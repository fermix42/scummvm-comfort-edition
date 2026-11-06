/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_IO_VALIDATION_H
#define NOX_IO_VALIDATION_H
namespace Nox {
// External Windows sharing locks exercise actual filesystem errors. Markers only
// coordinate the isolated test process; no SaveFileManager operation is mocked.
static bool ioWaitForMarker(Common::SaveFileManager &manager,const Common::String &name) {
 const uint32 deadline=g_system->getMillis()+10000;
 while(g_system->getMillis()<deadline){
  Common::StringArray none;manager.updateSavefilesList(none);
  Common::InSaveFile *locked=manager.openForLoading(name);
  if(locked){delete locked;return true;}g_system->delayMillis(10);
 }
 return false;
}
static bool ioWaitForLock(Common::SaveFileManager &manager,const Common::String &target) {
 Common::OutSaveFile *out=manager.openForSaving("nox-io-ready.tsv",false);if(!out)return false;
 out->writeString(target+"\n");out->finalize();bool ok=!out->err();delete out;
 return ok&&ioWaitForMarker(manager,"nox-io-locked.tsv");
}
bool NoxEngine::validateStorageFailures(Common::String &error) {
 auto fail=[&](const char *message){error=message;return false;};
 auto &ce=Common::CEAchievements();auto &manager=*getSaveFileManager();
 if(_targetName!="nox-gameplay-test"||!ce.getLinkToken().empty())return fail("I/O diagnostic requires isolated offline target");
 const int scenario=ConfMan.getInt("nox_io_validation");const bool cold=ConfMan.getBool("nox_io_cold");
 if(scenario<1||scenario>13)return fail("Unknown storage failure case");
 const Common::String aFile=ConfMan.get("nox_io_a_file"),bFile=ConfMan.get("nox_io_b_file");
 Saves::Entry a,b;Common::Array<byte> aData,bData;
 if(!Saves::readGeneration(manager,aFile.substr(0,aFile.size()-3),aFile.lastChar()-'0',_targetName,a,aData)||!Saves::readGeneration(manager,bFile.substr(0,bFile.size()-3),bFile.lastChar()-'0',_targetName,b,bData))return fail("I/O baseline committed saves missing");
 auto handle=[&](const Common::String &name){for(const auto &save:getMetaEngine()->listSaves(_targetName.c_str(),false))if(getSaveStateName(save.getSaveSlot())==name)return save.getSaveSlot();return -1;};
 auto digest=[&](const Common::String &name){Common::InSaveFile *in=manager.openForLoading(name);Common::String hash;if(in){hash=Common::computeStreamMD5AsString(*in);delete in;}return hash;};
 auto equal=[&](const Saves::Entry &entry,const Common::Array<byte> &data){Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);return _runtime->saveState(state)&&state.size()==data.size()-entry.offset&&!memcmp(state.getData(),data.data()+entry.offset,state.size())&&_runtimeSession==entry.metadata.session&&_saveMode==entry.metadata.mode&&_audio->numQueuedStreams()==0;};
 PauseToken paused=pauseEngine();
 if(cold){
  int slot=handle(aFile);if(slot<0||loadGameState(slot).getCode()!=Common::kNoError||!equal(a,aData))return fail("Prior complete A world/enhancements failed cold recovery");
  slot=handle(bFile);if(slot<0||loadGameState(slot).getCode()!=Common::kNoError||!equal(b,bData))return fail("Unrelated B world failed cold recovery");
  if(scenario==13){bool reason=false;for(const auto &flag:ce.getAssistanceFlags())reason=reason||flag=="nox_io_assistance_probe";if(!ConfMan.getBool("ce_active_playthrough_casual",Common::ConfigManager::kApplicationDomain)||!reason)return fail("Assistance reason lost on cold recovery");}
  Common::OutSaveFile *out=manager.openForSaving("nox-io-cold.tsv",false);if(!out)return fail("Cannot write cold I/O evidence");out->writeString("PASS\tprior_world_exact=1\tother_world_exact=1\tmodes=1\taudio_empty=1\n");out->finalize();bool ok=!out->err();delete out;return ok;
 }
 const int aSlot=handle(aFile),bSlot=handle(bFile);if(aSlot<0||bSlot<0)return fail("Cannot pin load handles before access failure");
 const bool liveB=scenario==1||scenario>=5;
 if(loadGameState(liveB?bSlot:aSlot).getCode()!=Common::kNoError||!equal(liveB?b:a,liveB?bData:aData))return fail("Cannot establish live baseline");
 if((scenario==8||scenario==10)&&!ce.endActivePlaythrough("abandoned"))return fail("Cannot establish valid abandoned-to-archive transition");
 const Common::String activeStatus=ConfMan.get("ce_active_playthrough_status",Common::ConfigManager::kApplicationDomain);
 const Common::String registry=digest("ce-playthroughs.json"),queue=digest("ce-achievements-queue.json");
 const Common::String owner=ce.getCurrentPlaythroughForTarget(_targetName);const uint32 probe=ce.getPopupProgress("nox_m2_state_owner_probe");
 Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES);if(!_runtime->saveState(before))return fail("Cannot capture complete live baseline");
 const SaveMode mode=_saveMode;const uint32 playtime=getTotalPlayTime();
 Common::String target=scenario==1?aFile:scenario==2?SnapshotStore(manager,a.prefix).filename(1-a.generation):scenario==4?aFile+".ce-achievements.json":(scenario==6||(scenario>=9&&scenario<=12))?"ce-achievements-queue.json":"ce-playthroughs.json";
 if(scenario!=3&&!ioWaitForLock(manager,target))return fail("External real file lock handshake missing");
 bool rejected=false;
 if(scenario==1)rejected=loadGameState(aSlot).getCode()!=Common::kNoError;
 else if(scenario==2||scenario==3)rejected=saveGameState(a.metadata.slot,a.metadata.description).getCode()!=Common::kNoError;
 else if(scenario==4)rejected=!getMetaEngine()->removeSaveState(_targetName.c_str(),aSlot);
 else if(scenario==13){
  ce.noteDisqualifyingAssistance("nox_io_assistance_probe");
  if(loadGameState(aSlot).getCode()!=Common::kNoError||loadGameState(bSlot).getCode()!=Common::kNoError)return fail("Cannot switch worlds during assistance persistence check");
  bool reason=false;for(const auto &flag:ce.getAssistanceFlags())reason=reason||flag=="nox_io_assistance_probe";
  rejected=ConfMan.getBool("ce_active_playthrough_casual",Common::ConfigManager::kApplicationDomain)&&reason;
 }
 else if(scenario==12){ce.recordSaveLoaded(bFile,b.metadata.slot,b.metadata.description,true);rejected=ce.getLastSyncStatus().contains("history could not be saved");}
 else if(scenario==11){ce.recordSaveCreated(bFile,b.metadata.slot,b.metadata.description);rejected=ce.getLastSyncStatus().contains("history could not be saved");}
 else if(scenario==7||scenario==9)rejected=!ce.endActivePlaythrough("completed");
 else if(scenario==8||scenario==10)rejected=!ce.setPlaythroughArchivedForTarget(_targetName,owner,true);
 else rejected=!ce.startPlaythroughForTarget(_targetName,"nox-archaist","apple2-v119","Must not start when registry is unavailable");
 Common::StringArray none;manager.updateSavefilesList(none);
 Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES);
 const bool live=_runtime->saveState(after)&&before.size()==after.size()&&!memcmp(before.getData(),after.getData(),before.size())&&_saveMode==mode&&getTotalPlayTime()==playtime&&_runtimeSession==owner&&ce.getCurrentPlaythroughForTarget(_targetName)==owner&&ce.getPopupProgress("nox_m2_state_owner_probe")==probe&&ConfMan.get("ce_active_playthrough_status",Common::ConfigManager::kApplicationDomain)==activeStatus;
 const bool history=digest("ce-playthroughs.json")==registry&&(scenario==13||digest("ce-achievements-queue.json")==queue);
 bool prior=true;
 if(scenario!=1){Saves::Entry retained;Common::Array<byte> data;prior=Saves::best(manager,a.prefix,_targetName,retained,data)&&retained.filename==a.filename&&retained.sequence==a.sequence&&data==aData;}
 Common::OutSaveFile *result=manager.openForSaving("nox-io-result.tsv",false);if(!result)return fail("Cannot persist actual I/O result");result->writeString(Common::String::format("RESULT\tscenario=%d\trejected=%u\tlive_unchanged=%u\thistory_unchanged=%u\tprior_complete=%u\tread_recovery_deferred=%u\n",scenario,rejected?1:0,live?1:0,history?1:0,prior?1:0,scenario==1?1:0));result->finalize();bool ok=!result->err();delete result;
 if(!ok)return fail("I/O result write failed");
 if(!ioWaitForMarker(manager,"nox-io-unlocked.tsv"))return fail("External lock was not released before shutdown");
 Common::OutSaveFile *ack=manager.openForSaving("nox-io-release.tsv",false);if(!ack)return fail("Cannot acknowledge released lock");
 ack->writeString("PASS\tlock_released_before_exit=1\n");ack->finalize();ok=!ack->err();delete ack;
 if(!ok||!rejected||!live||!history||!prior)return fail("Observed I/O operation violated error/rollback/recovery acceptance; inspect RESULT");
 return true;
}
}
#endif
