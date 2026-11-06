/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_HISTORY_VALIDATION_H
#define NOX_HISTORY_VALIDATION_H
#include "nox/backoff_validation.h"
namespace Nox {
bool NoxEngine::validateSaveHistory(Common::String &error) {
 auto fail=[&](const char *message){error=message;return false;};
 if(ConfMan.getInt("nox_history_validation")==8)return validateBackoffEndpoint(error);
 if(ConfMan.getInt("nox_history_validation")==6||ConfMan.getInt("nox_history_validation")==7){
  const bool recoveredTerminal=ConfMan.getInt("nox_history_validation")==7;
  auto &ce=Common::CEAchievements();const bool linked=ConfMan.getBool("nox_history_linked");
  if(_targetName!="nox-gameplay-test"||linked==ce.getLinkToken().empty())return fail("Offline/link diagnostic credential phase differs");
  Common::String ownerA,ownerB;
  auto loadLabel=[&](const char *label,Common::String &owner){
   for(const auto &save:getMetaEngine()->listSaves(_targetName.c_str(),false))if(save.getDescription()==label){
    const Common::String file=getSaveStateName(save.getSaveSlot());Saves::Entry entry;Common::Array<byte> data;
    if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,data)||loadGameState(save.getSaveSlot()).getCode()!=Common::kNoError)return false;
    Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);owner=entry.metadata.session;
    return _runtime->saveState(state)&&state.size()==data.size()-entry.offset&&!memcmp(state.getData(),data.data()+entry.offset,state.size())&&_runtimeSession==owner&&_saveMode==entry.metadata.mode&&_audio->numQueuedStreams()==0;
   }return false;
  };
  if(!loadLabel(recoveredTerminal?"Recovered session world":"Session A slot1",ownerA))return fail("Offline/link A world changed");
  if(!linked&&!recoveredTerminal){
   // Exercise the existing assistance setting, then disable it. Its history is
   // permanent for A; B must remain clean after the ordinary setting is off.
   ce.setCheatsEnabled(true,_targetName);ce.noteDisqualifyingAssistance("cheats.enabled");
   ce.setCheatsEnabled(false,_targetName);ce.setChallengeModeEnabled(true);
   if(!loadLabel("Session A slot1",ownerA))return fail("Offline older clean A load failed");
  }
  bool assisted=false;for(const auto &flag:ce.getAssistanceFlags())if(flag==(recoveredTerminal?"unknown_save_history":"cheats.enabled"))assisted=true;
  if(!linked&&recoveredTerminal&&!ce.endActivePlaythrough("completed"))return fail("Recovered session could not complete offline");
  if(!assisted||!loadLabel("Session B slot1",ownerB)||ownerA==ownerB||!ce.getAssistanceFlags().empty())return fail("Offline/link permanent assistance crossed sessions");
  if(!linked&&recoveredTerminal){
   if(!ce.setPlaythroughStatusForTarget(_targetName,ownerB,"abandoned")||!ce.setPlaythroughArchivedForTarget(_targetName,ownerB,true))return fail("Older local session could not archive offline");
  }
  if(linked){
   // One submission drains successful requests through existing callbacks.
   // Fail on the first idle pending queue instead of hammering a failed API.
   ce.retryQueuedEvents();const uint32 deadline=_system->getMillis()+15000;
   while(ce.isSyncBusy()&&_system->getMillis()<deadline)_system->delayMillis(20);
   if(ce.isSyncBusy()||ce.getPendingEventCount())return fail("Offline/link queue did not finish its single submission chain");
   Common::String serverA,serverB;for(const auto &record:ce.listPlaythroughsForTarget(_targetName)){
    if(record.clientPlaythroughId==ownerA){serverA=record.playthroughId;if(recoveredTerminal&&record.status!="completed")return fail("Start acknowledgement revived completed session");if(!record.casual)return fail("Linked A lost permanent Casual");}
    if(record.clientPlaythroughId==ownerB){serverB=record.playthroughId;if(recoveredTerminal&&record.status!="archived")return fail("Start acknowledgement revived archived session");if(record.casual)return fail("Linked B inherited Casual");}
   }
   if(serverA.empty()||serverB.empty()||serverA==serverB)return fail("Linked server identities missing or shared");
  }else if(!ce.getPendingEventCount()||ce.isSyncBusy())return fail("Offline history not durably queued without transport");
  Common::OutSaveFile *out=getSaveFileManager()->openForSaving(linked?"nox-offline-linked.tsv":"nox-offline-local.tsv",false);if(!out)return fail("Cannot write offline/link evidence");
  out->writeString(Common::String::format("PASS\tlinked=%u\tworlds_exact=1\tsessions=2\tpermanent_assistance_isolated=1\n",linked?1:0));out->finalize();bool ok=!out->err();delete out;return ok;
 }
 if(_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("History validation requires isolated offline target");
 auto &ce=Common::CEAchievements(); auto &manager=*getSaveFileManager();
 const int scenario=ConfMan.getInt("nox_history_validation");
 const bool cold=ConfMan.getBool("nox_history_cold");
 const Common::String aFile=ConfMan.get("nox_history_a_file"), bFile=ConfMan.get("nox_history_b_file");
 const Common::String ownerA=ConfMan.get("nox_history_a_owner");
 auto read=[&](const Common::String &name){Common::InSaveFile *in=manager.openForLoading(name);Common::String data;if(in){data=in->readString(0,in->size());delete in;}return data;};
 auto hash=[&](const Common::String &name){Common::InSaveFile *in=manager.openForLoading(name);Common::String data;if(in){data=Common::computeStreamMD5AsString(*in);delete in;}return data;};
 auto handle=[&](const Common::String &name){for(const auto &save:getMetaEngine()->listSaves(_targetName.c_str(),false))if(getSaveStateName(save.getSaveSlot())==name)return save.getSaveSlot();return -1;};
 int aSlot=handle(aFile),bSlot=handle(bFile);
 if(bSlot<0||loadGameState(bSlot).getCode()!=Common::kNoError||!ce.getAssistanceFlags().empty())return fail("Cannot load clean reference session B");
 const Common::String ownerB=_runtimeSession;
 Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES);if(!_runtime->saveState(before))return fail("Cannot capture reference world");
 if(!cold){
  if(aSlot<0||!ce.hasCurrentSaveHistory(aFile))return fail("History baseline is not current");
  const Common::String prefix=aFile.substr(0,aFile.size()-3);
  Common::String sidecars[2];for(uint g=0;g<2;++g)sidecars[g]=read(SnapshotStore(manager,prefix).filename(g)+".ce-achievements.json");
  for(uint g=0;g<2;++g){
   Common::String name=SnapshotStore(manager,prefix).filename(g)+".ce-achievements.json";
   if(scenario==1){if(!manager.removeSavefile(name))return fail("Cannot prepare absent history");continue;}
   Common::String data=scenario==2?"{":scenario==3?"{\"client_playthrough_id\":\"different-owner\"}":scenario==4?sidecars[1-g]:"{\"client_playthrough_id\":\""+ownerA+"\",\"tainted\":false,\"popup_seen\":[],\"progress\":{\"nox_m2_state_owner_probe\":999}}";
   Common::OutSaveFile *out=manager.openForSaving(name,false);if(!out)return fail("Cannot prepare bounded history case");out->writeString(data);out->finalize();bool ok=!out->err();delete out;if(!ok)return fail("History preparation write failed");
  }
 }
 Common::StringArray names,hashes;for(const auto &name:manager.listSavefiles("nox-ce-*")){names.push_back(name);hashes.push_back(hash(name));}
 const Common::String registry=hash("ce-playthroughs.json");
 if(scenario<=3){
  if(!cold&&loadGameState(aSlot).getCode()==Common::kNoError)return fail("Invalid history accepted through cached handle");
  for(const auto &entry:Saves::entries(manager,_targetName))if(entry.metadata.session==ownerA)return fail("Invalid committed generation remained visible");
  Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES);
  if(!_runtime->saveState(after)||before.size()!=after.size()||memcmp(before.getData(),after.getData(),before.size())||_runtimeSession!=ownerB||!ce.getAssistanceFlags().empty()||registry!=hash("ce-playthroughs.json"))return fail("Rejected history changed live world or registry");
 }else{
  aSlot=handle(aFile);if(aSlot<0||ce.hasCurrentSaveHistory(aFile)||loadGameState(aSlot).getCode()!=Common::kNoError||_runtimeSession!=ownerA)return fail("Legacy/stale world did not retain its session");
  bool unknown=false;for(const auto &flag:ce.getAssistanceFlags())if(flag=="unknown_save_history")unknown=true;
  if(!unknown||ce.getPopupProgress("nox_m2_state_owner_probe")!=0)return fail("Uncertain history acquired trusted progress");
  bool casual=false;for(const auto &record:ce.listPlaythroughsForTarget(_targetName))if(record.clientPlaythroughId==ownerA)casual=record.casual;
  if(!casual)return fail("Uncertain history did not become permanently Casual");
  Common::Array<byte> payload;Saves::Entry entry;Common::MemoryWriteStreamDynamic actual(DisposeAfterUse::YES);
  if(!Saves::readGeneration(manager,aFile.substr(0,aFile.size()-3),aFile.lastChar()-'0',_targetName,entry,payload)||!_runtime->saveState(actual)||actual.size()!=payload.size()-entry.offset||memcmp(actual.getData(),payload.data()+entry.offset,actual.size())||_saveMode!=entry.metadata.mode)return fail("Uncertain history changed saved world/mode");
  bSlot=handle(bFile);if(bSlot<0||loadGameState(bSlot).getCode()!=Common::kNoError||!ce.getAssistanceFlags().empty())return fail("Uncertain history contaminated session B");
 }
 for(uint i=0;i<names.size();++i)if(hash(names[i])!=hashes[i])return fail("History load rewrote a save or sidecar");
 Common::OutSaveFile *out=manager.openForSaving(cold?"nox-history-cold.tsv":"nox-history.tsv",false);if(!out)return fail("Cannot write history evidence");
 out->writeString(Common::String::format("PASS\tscenario=%d\tcold=%u\trejected=%u\tconservative_casual=%u\tother_session_clean=1\tfiles_unchanged=1\n",scenario,cold?1:0,scenario<=3?1:0,scenario>3?1:0));out->finalize();bool ok=!out->err();delete out;return ok;
}
}
#endif
