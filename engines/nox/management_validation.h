/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_MANAGEMENT_VALIDATION_H
#define NOX_MANAGEMENT_VALIDATION_H
#include "nox/nox.h"
#include "nox/runtime.h"
#include "nox/saves.h"
#include "gui/saveload-dialog.h"
#include "gui/gui-manager.h"
#include "gui/widget.h"
#include "image/png.h"
namespace Nox {
class ManagementChooser : public GUI::SaveLoadChooserSimple {
public:
 enum Action { Abandon, Archive, ShowArchived, Unarchive, DeleteKeep, DeleteAll };
 ManagementChooser():GUI::SaveLoadChooserSimple(Common::U32String("Load game"),Common::U32String("Load"),false){}
 bool foreground=false;
 void receivedFocus(int x=-1,int y=-1)override{GUI::SaveLoadChooserSimple::receivedFocus(x,y);foreground=true;}
 void lostFocus()override{foreground=false;GUI::SaveLoadChooserSimple::lostFocus();}
 GUI::Widget *button(Action action){
  return action==Abandon?_abandonPlaythroughButton:action==Archive||action==Unarchive?_archivePlaythroughButton:action==ShowArchived?_showArchivedPlaythroughsCheckbox:_deletePlaythroughButton;
 }
 bool contains(const Common::String &owner)const{for(const auto &record:_playthroughs)if(record.clientPlaythroughId==owner)return true;return false;}
 bool showingArchived()const{return _showArchivedPlaythroughs;}
};
static bool managementCapture(const Common::String &name){
 Graphics::Surface overlay;overlay.create(g_system->getOverlayWidth(),g_system->getOverlayHeight(),g_system->getOverlayFormat());g_system->grabOverlay(overlay);
 Common::OutSaveFile *out=g_system->getSavefileManager()->openForSaving(name,false);bool ok=false;
 if(out){ok=Image::writePNG(*out,overlay);out->finalize();ok=ok&&!out->err();delete out;}overlay.free();return ok;
}
// Real mouse input into the ordinary chooser and Delete Run confirmation.
class ManagementInput : public Common::EventSource {
 ManagementChooser &_chooser;ManagementChooser::Action _action;Common::String _target,_owner,_capture;
 Common::EventDispatcher *_dispatcher;uint _phase=0;bool _up=false;Common::Point _click;uint32 _start=0,_next=0;
 bool mouse(Common::Event &event,int x,int y){_click=Common::Point(x,y);_up=true;event.type=Common::EVENT_LBUTTONDOWN;event.mouse=_click;return true;}
public:
 bool passed=false,timedOut=false;
 ManagementInput(ManagementChooser &chooser,ManagementChooser::Action action,const Common::String &target,const Common::String &owner,const Common::String &capture)
  :_chooser(chooser),_action(action),_target(target),_owner(owner),_capture(capture),_dispatcher(g_system->getEventManager()->getEventDispatcher()) {_dispatcher->registerSource(this,false);}
 ~ManagementInput(){_dispatcher->unregisterSource(this);}
 bool allowMapping()const override{return false;}
 bool pollEvent(Common::Event &event)override{
  if(_up){_up=false;event.type=Common::EVENT_LBUTTONUP;event.mouse=_click;return true;}
  const uint32 now=g_system->getMillis();if(!_start){_start=now;_next=now+150;}
  if(now-_start>10000){timedOut=true;event.type=Common::EVENT_QUIT;return true;}
  if(now<_next)return false;_next=now+120;
  const bool active=_chooser.foreground;
  if(!_phase){
   if(!active)return false;
   GUI::Widget *widget=_chooser.button(_action);if(!widget||!widget->isEnabled())return false;
   _phase=1;return mouse(event,widget->getAbsX()+widget->getWidth()/2,widget->getAbsY()+widget->getHeight()/2);
  }
  const bool deleting=_action==ManagementChooser::DeleteKeep||_action==ManagementChooser::DeleteAll;
  if(deleting&&!active){
   // CEDeletePlaythroughDialog uses fixed unscaled bounds(40,40,360,122).
   if(_phase==1&&_action==ManagementChooser::DeleteAll){_phase=2;return mouse(event,210,104);}
   if(_phase<=2){_phase=3;return mouse(event,340,138);}
   return false;
  }
  if(!active)return false;
  Common::String status;bool found=false;
  for(const auto &record:Common::CEAchievements().listPlaythroughsForTarget(_target))if(record.clientPlaythroughId==_owner){found=true;status=record.status;}
  bool done=deleting?!found:_action==ManagementChooser::Abandon||_action==ManagementChooser::Unarchive?status=="abandoned":_action==ManagementChooser::Archive?status=="archived"&&!_chooser.contains(_owner):_chooser.showingArchived()&&_chooser.contains(_owner);
  if(!done)return false;
  passed=managementCapture(_capture);event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_ESCAPE,27);return true;
 }
};
bool NoxEngine::validateSessionManagement(Common::String &error){
 auto fail=[&](const char *message){error=message;return false;};
 if(_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Management validation requires isolated offline target");
 const int scenario=ConfMan.getInt("nox_management_validation");
 Common::SaveFileManager &manager=*getSaveFileManager();const MetaEngine *meta=getMetaEngine();
 const bool cold=ConfMan.hasKey("nox_management_cold")&&ConfMan.getBool("nox_management_cold");
 Common::Array<Saves::Entry> entries=Saves::entries(manager,_targetName);if(entries.size()!=uint(cold?(scenario==2?3:scenario==3?1:2):2))return fail("Management fixture logical save count differs");
 Saves::Entry a,b;bool haveA=false,haveB=false;
 for(const auto &entry:entries){if(entry.metadata.description=="Session A slot1"){a=entry;haveA=true;}if(entry.metadata.description=="Session B slot1"){b=entry;haveB=true;}}
 if((!haveA&&(!cold||scenario!=3))||!haveB)return fail("Original session labels missing");
 auto handle=[&](const Common::String &file){for(const auto &save:meta->listSaves(_targetName.c_str(),false))if(getSaveStateName(save.getSaveSlot())==file)return save.getSaveSlot();return -1;};
 auto digest=[&](const Common::String &name){Common::InSaveFile *in=manager.openForLoading(name);if(!in)return Common::String();Common::String hash=Common::computeStreamMD5AsString(*in);delete in;return hash;};
 Common::StringArray protectedNames,hashes;
 for(const auto &entry:entries)for(uint g=0;g<2;++g){Common::String name=SnapshotStore(manager,entry.prefix).filename(g);protectedNames.push_back(name);hashes.push_back(digest(name));protectedNames.push_back(name+".ce-achievements.json");hashes.push_back(digest(name+".ce-achievements.json"));}
 auto unchanged=[&](bool both){for(uint i=0;i<protectedNames.size();++i)if((both||protectedNames[i].hasPrefix(b.prefix))&&digest(protectedNames[i])!=hashes[i])return false;return true;};
 auto action=[&](ManagementChooser::Action type,const Common::String &owner,bool archived,const char *capture){
  ManagementChooser chooser;chooser.setPreferredPlaythroughFilter(owner,archived);ManagementInput input(chooser,type,_targetName,owner,capture);PauseToken pause=pauseEngine();chooser.run(_targetName,meta);return input.passed&&!input.timedOut;
 };
 auto load=[&](const Saves::Entry &entry){int slot=handle(entry.filename);return slot>=0&&loadGameState(slot).getCode()==Common::kNoError;};
 auto worldEqual=[&](const Saves::Entry &entry){Common::Array<byte> data;Saves::Entry original;Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);return Saves::readGeneration(manager,entry.prefix,entry.generation,_targetName,original,data)&&_runtime->saveState(state)&&state.size()==data.size()-original.offset&&!memcmp(state.getData(),data.data()+original.offset,state.size())&&_saveMode==original.metadata.mode&&_audio->numQueuedStreams()==0;};

 if(cold){
  const Common::String originalA=ConfMan.get("nox_management_original_a");
  const Common::Array<Common::CEPlaythroughRecord> records=Common::CEAchievements().listPlaythroughsForTarget(_targetName);
  if(records.size()!=uint(scenario==3?1:2))return fail("Cold registry count differs");
  bool foundA=false;for(const auto &record:records)if(record.clientPlaythroughId==originalA){foundA=true;if(scenario==1&&record.status!="abandoned")return fail("Archive status failed cold restore");}
  if(foundA!=(scenario==1||scenario==4))return fail("Deleted history revived across restart");
  if(scenario==2){
   bool recovered=false;
   for(const auto &entry:entries)if(entry.metadata.description=="Recovered session world"){
    if(!load(entry)||!worldEqual(entry)||entry.metadata.session==originalA)return fail("Recovered saved world failed cold load");
    Common::Array<byte> data;Saves::Entry original;Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);
    if(!Saves::readGeneration(manager,a.prefix,a.generation,_targetName,original,data)||!_runtime->saveState(state)||state.size()!=data.size()-original.offset||memcmp(state.getData(),data.data()+original.offset,state.size()))return fail("Recovered cold world differs from kept original");
    for(const auto &record:Common::CEAchievements().listPlaythroughsForTarget(_targetName))if(record.clientPlaythroughId==entry.metadata.session)recovered=record.casual;
    bool unknown=false;for(const auto &reason:Common::CEAchievements().getAssistanceFlags())if(reason=="unknown_save_history")unknown=true;
    if(!recovered||!unknown)return fail("Recovered history became clean across restart");
   }
   if(!recovered)return fail("Recovered save missing");
  }
  if(!load(b)||!worldEqual(b)||!Common::CEAchievements().getAssistanceFlags().empty()||!unchanged(true))return fail("Cold other session/state/provenance differs");
  Common::OutSaveFile *out=manager.openForSaving("nox-management-cold.tsv",false);if(!out)return fail("Cannot create cold management report");
  const Common::String report=Common::String::format("PASS\tscenario=%d\tregistry=1\tworld_exact=1\tother_session_clean=1\tfiles_unchanged=1\n",scenario);
  out->write(report.c_str(),report.size());out->finalize();bool ok=!out->err();delete out;return ok;
 }
 if(!load(a)||!worldEqual(a))return fail("Cannot establish actual session A world");
 Common::String owner=a.metadata.session;
 if(scenario==4){
  if(!Common::CEAchievements().startPlaythroughForTarget(_targetName,"nox-archaist","apple2-v119","Empty local session")||!synchronizePlaySession())return fail("Cannot create empty local session");
  owner=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
 }
 if(scenario==1||scenario==4){
  if(!action(ManagementChooser::Abandon,owner,false,"nox-management-abandon.png")||!action(ManagementChooser::Archive,owner,false,"nox-management-archive.png")||!action(ManagementChooser::ShowArchived,owner,false,"nox-management-show-archived.png")||!action(ManagementChooser::Unarchive,owner,true,"nox-management-unarchive.png"))return fail("Actual archive/show/unarchive chooser flow failed");
  if(!unchanged(true))return fail("Archive workflow modified saved worlds or another session");
 }
 if(scenario>=2){
  if(!action(scenario==3?ManagementChooser::DeleteAll:ManagementChooser::DeleteKeep,owner,false,"nox-management-delete.png"))return fail("Actual Delete Run chooser flow failed");
  for(const auto &record:Common::CEAchievements().listPlaythroughsForTarget(_targetName))if(record.clientPlaythroughId==owner)return fail("Deleted session remains registered");
  if(!unchanged(scenario!=3))return fail("Delete modified a kept world or unrelated session");
  if(scenario==3){
   for(const auto &name:protectedNames)if(name.hasPrefix(a.prefix)&&manager.exists(name))return fail("Delete saves left a generation or sidecar");
   if(Saves::entries(manager,_targetName).size()!=1)return fail("Deleted save remained listed");
  }else if(scenario==2){
   if(!Common::CEAchievements().getSavePlaythroughForTargetSlot(_targetName,handle(a.filename),a.filename).empty())return fail("Kept save not grouped as untracked");
   if(!load(a)||!worldEqual(a))return fail("Kept world cannot be recovered unchanged");
   const Common::String recovered=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
   if(recovered.empty()||recovered==owner||recovered==b.metadata.session||_runtimeSession!=recovered)return fail("Recovery revived deleted identity");
   bool casual=false;for(const auto &record:Common::CEAchievements().listPlaythroughsForTarget(_targetName))if(record.clientPlaythroughId==recovered)casual=record.casual;
   const Common::Array<Common::String> reasons=Common::CEAchievements().getAssistanceFlags();
   if(!casual||reasons.size()!=1||reasons[0]!="unknown_save_history"||saveGameState(1,"Recovered session world").getCode()!=Common::kNoError||saveGameState(1,"Recovered session world").getCode()!=Common::kNoError||!unchanged(true))return fail("Recovered world/history/save isolation differs");
  }
 }
 if(!load(b)||!worldEqual(b)||!unchanged(scenario!=3))return fail("Other session cannot load unchanged after management");
 Common::OutSaveFile *out=manager.openForSaving("nox-management.tsv",false);if(!out)return fail("Cannot create management report");
 const Common::String report=Common::String::format("PASS\tscenario=%d\tactual_mouse_actions=1\tconfirmation=%u\tother_session_unchanged=1\tother_world_exact=1\tkept_recovered_casual=%u\n",scenario,scenario>=2?1:0,scenario==2?1:0);
 out->write(report.c_str(),report.size());out->finalize();bool ok=!out->err();delete out;return ok;
}
}
#endif
