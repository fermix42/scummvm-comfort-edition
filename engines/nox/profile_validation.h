/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_PROFILE_VALIDATION_H
#define NOX_PROFILE_VALIDATION_H
namespace Nox {
bool NoxEngine::validateStateProfile(Common::String &error) {
 auto fail=[&](const char *message){error=message;return false;};
 auto &ce=Common::CEAchievements();auto &manager=*getSaveFileManager();
 const int profile=ConfMan.getInt("nox_state_profile");const bool cold=ConfMan.getBool("nox_profile_cold");
 if(_targetName!="nox-gameplay-test"||!ce.getLinkToken().empty()||profile<1||profile>3)return fail("Owner profiles require isolated offline fixture");
 const Common::String source=ConfMan.get("nox_profile_source"),description=Common::String::format("Owner profile %d",profile);
 int handle=-1;
 for(const auto &save:getMetaEngine()->listSaves(_targetName.c_str(),false))
  if((cold&&save.getDescription()==description)||(!cold&&getSaveStateName(save.getSaveSlot())==source))handle=save.getSaveSlot();
 if(handle<0)return fail("Profile source save missing");
 PauseToken paused=pauseEngine();
 if(loadGameState(handle).getCode()!=Common::kNoError)return fail("Cannot load actual profile baseline");
 const Common::String owner=_runtimeSession;const SaveMode mode=_saveMode;
 auto write=[&](const char *name,const Common::String &text){Common::OutSaveFile *out=manager.openForSaving(name,false);if(!out)return false;out->writeString(text);out->finalize();bool ok=!out->err();delete out;return ok;};
 if(cold){
  Common::MemoryWriteStreamDynamic actual(DisposeAfterUse::YES);if(!_runtime->saveState(actual))return fail("Cannot encode restored profile");
  Common::InSaveFile *expected=manager.openForLoading("nox-profile-runtime.bin");if(!expected)return fail("Profile expected bytes missing");
  Common::Array<byte> bytes;bytes.resize(expected->size());bool ok=expected->read(bytes.data(),bytes.size())==bytes.size()&&!expected->err();delete expected;
  if(!ok||bytes.size()!=actual.size()||memcmp(bytes.data(),actual.getData(),bytes.size())||_audio->numQueuedStreams()||ce.getCurrentPlaythroughForTarget(_targetName)!=owner)return fail("Cold owner profile differs from committed runtime");
  return write("nox-profile-cold.tsv",Common::String::format("PASS\tprofile=%d\truntime_exact=1\taudio_empty=1\towner_exact=1\tmode=%d\n",profile,int(mode)));
 }
 const auto &characters=_runtime->enhancements().characters();uint replies=0;for(const auto &c:characters)replies+=c.replies.size();
 if((profile==1&&replies)||(profile>1&&replies!=9))return fail("Profile journal baseline provenance differs");
 if(profile==3){
  if(_runtime->enhancements().cells().empty())return fail("Profile needs observed map identity");
  const DiscoveryCell cell=_runtime->enhancements().cells()[0];
  for(uint y=0;y<16;++y)for(uint x=0;x<16;++x)
   if(!_runtime->enhancements().discover(cell.resource,cell.mapType,cell.mapId,x,y,false,true,y==0?"Controlled persistence profile annotation":""))return fail("Cannot establish controlled rich owner profile");
  ce.noteDisqualifyingAssistance("nox_controlled_owner_profile");
 }
 const int slot=profile==1?1:2;
 if(saveGameState(slot,description).getCode()!=Common::kNoError||saveGameState(slot,description).getCode()!=Common::kNoError)return fail("Profile durable save failed");
 Common::MemoryWriteStreamDynamic expected(DisposeAfterUse::YES);if(!_runtime->saveState(expected))return fail("Cannot capture profile bytes");
 Common::OutSaveFile *out=manager.openForSaving("nox-profile-runtime.bin",false);if(!out)return fail("Cannot persist diagnostic expected bytes");out->write(expected.getData(),expected.size());out->finalize();bool ok=!out->err();delete out;if(!ok)return fail("Profile expected bytes incomplete");
 if(_runtimeSession!=owner||_saveMode!=mode||ce.getCurrentPlaythroughForTarget(_targetName)!=owner)return fail("Profile save changed session/mode");
 return write("nox-profile-warm.tsv",Common::String::format("PASS\tprofile=%d\towner_exact=1\treplies=%u\tcells=%u\tmode=%d\tprovenance=%s\n",profile,replies,_runtime->enhancements().cells().size(),int(mode),profile==3?"controlled_discovery_overlay_on_observed_world":"observed_journal_in_assisted_adjacency_fixture"));
}
}
#endif
