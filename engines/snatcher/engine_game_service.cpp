/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine.h"
#include "game_service_backend.h"
#include "narrative_state.h"
#include "original_input.h"
#include "common/config-manager.h"
#include "common/system.h"
#include "engines/ce_achievements.h"
#include "engine_save.h"
namespace Snatcher {
bool SnatcherEngine::ensureGameStorage(std::string& error){
 error.clear();if(_gameStorageScanned)return true;
 auto* manager=_system->getSavefileManager();if(!manager){error="Original game storage has no CE backend";return false;}
 if(!scanGameStorage(*manager,ConfMan.getActiveDomainName(),_gameServices,error))return false;
 _gameStorageScanned=true;return true;
}
bool SnatcherEngine::captureOriginalGameSlot(const RuntimeRequest& request,std::string& error){
 for(const auto& saved:_gameSaveCaptures)if(saved.token==request.token)return true;
 if(_gameSaveCaptures.size()>=64){error="Original save capture capacity";return false;}
 if(!capturePresentationBookmark(request.second,_bookmark,error))return false;
 GameSaveCapture capture;capture.token=request.token;capture.bookmark=_bookmark;capture.sourceContextSupported=!_fixture&&_interactionMode<=1;capture.mono=_pcmMono;capture.inputSettings=_originalInputSettings;capture.inputSettings.mono=_pcmMono;
 DialogueLayout layout;if(!_scene.dialogueLayout(layout,error))return false;capture.mappingKnown=layout.substitutionKnown;capture.controllerMapping=layout.substitution;
 uint64_t liveTicks=_clock.snapshot().ticks;if(liveTicks>UINT64_MAX-_gameElapsedOrigin){error="Original elapsed-time owner overflow";return false;}uint64_t ticks=_gameElapsedOrigin+liveTicks;
 if(ticks/(UINT64_C(60)*60*60)>99){error="Original elapsed-time hours exceed the recovered decimal field";return false;}
 capture.elapsed.frames=uint8_t(ticks%60);capture.elapsed.seconds=uint8_t((ticks/60)%60);capture.elapsed.minutes=uint8_t((ticks/(60*60))%60);capture.elapsed.hours=uint8_t(ticks/(UINT64_C(60)*60*60));
 _gameSaveCaptures.push_back(capture);return true;
}
bool SnatcherEngine::writeOriginalGameSlot(const RuntimeRequest& request,std::string& error){
 error.clear();if(request.first<1||request.first>4||request.queue!=30||!request.operands.empty()){error="Invalid original game slot request";return false;}
 // 14F12 falls through to 14F70, the same capture used by native11. Capture is
 // a real shared side effect even if the eventual storage write fails.
 const GameSaveCapture* captured=nullptr;for(const auto& item:_gameSaveCaptures)if(item.token==request.token)captured=&item;
 if(!captured){error="Original game slot has no source-ordered presentation capture";return false;}
 if(!captured->sourceContextSupported){error="Original save record requires the recovered standard controller context";return false;}
 if(!captured->mappingKnown){error="Original save record requires known controller mapping";return false;}
 if(!ensureGameStorage(error))return false;
 const GameSaveContext& capture=request.gameSave;
 if(!capture.known||capture.packed.size()!=352||!capture.progressionKnown){error="Original save request has no source-time story capture";return false;}
 const Scenario* scenario=request.owner.resource>=57&&size_t(request.owner.resource-57)<_scenarios.size()?&_scenarios[request.owner.resource-57]:nullptr;
 if(!scenario||!scenario->at(capture.entry)){error="Original save continuation is not a decoded source entry";return false;}
 GameSaveRecord record;record.disc="69e1a081e2bcc8da5a033433b3e7b91c5fda8d7043300d35550be81a83ec900b";
 for(const auto& source:_scenarios)record.scenarioStamps.push_back(narrativeScenarioStamp(source));
 record.packed=capture.packed;record.resource=request.owner.resource;record.resumeEntry=capture.entry;record.progression=capture.progression;record.cursorHistory=capture.history;
 record.controllerMapping=captured->controllerMapping;
 // Source-time anchors persist even after Options detects a disconnected gun.
 // Mouse aiming is an independent CE preference, never a fake attachment.
 StartupSettings input=captured->inputSettings;input.controlMapping=captured->controllerMapping;captureOriginalInputSettings(input,record);record.contextKnown=true;record.elapsed=captured->elapsed;
 const auto& source=captured->bookmark;auto& bookmark=record.bookmark;bookmark.known=source.valid&&source.pcmKnown;bookmark.presentation=source.presentation;bookmark.pcm=source.stream;bookmark.music=source.music;bookmark.ambient=uint8_t(source.ambient);bookmark.interaction=uint8_t(source.interaction);bookmark.quiet=source.quiet;bookmark.suppressed=source.suppressed;bookmark.actors=source.actors;
 GameSaveTransaction transaction;if(!beginGameSaveWrite(_gameServices,request.first,transaction,error))return false;
 record.counter=transaction.stagedCounter;
 captureSaveThumbnail(record.thumbnail);
 GameSaveOutcome outcome;std::string ioError;
 if(!publishGameSave(*_system->getSavefileManager(),ConfMan.getActiveDomainName(),request.first,record,outcome,ioError)){std::string ignored;abortGameSaveWrite(_gameServices,transaction,ignored);error=ioError;return false;}
 if(!finishGameSaveWrite(_gameServices,transaction,outcome,error))return false;
 if(outcome==kGameSaveCommitted)recordNativeSaveCreated(*_system->getSavefileManager(),Common::String::format("%s.%03u",ConfMan.getActiveDomainName().c_str(),request.first),int(request.first),Common::String::format("Internal Save Slot %u",request.first));
 _saveError=ioError;_trace.record(request.owner,"original_game_slot_result",request.first,_gameServices.lastWriteResult);
 bool completed=_events.complete(request.token,false,error);
 if(completed)for(auto item=_gameSaveCaptures.begin();item!=_gameSaveCaptures.end();++item)if(item->token==request.token){_gameSaveCaptures.erase(item);break;}
 return completed;
}
bool SnatcherEngine::readCompatibleOriginalSlot(unsigned slot,GameSaveRecord& record,std::string& error,Common::String* restoredName){
 std::vector<uint64_t> stamps;for(const auto& scenario:_scenarios)stamps.push_back(narrativeScenarioStamp(scenario));
 const std::string disc="69e1a081e2bcc8da5a033433b3e7b91c5fda8d7043300d35550be81a83ec900b";
 auto& manager=*_system->getSavefileManager();const auto& target=ConfMan.getActiveDomainName();
 if(readGameSave(manager,target,slot,disc,stamps,record,error,restoredName))return true;
 Scenario previous;
 if(!alternateSaveScenario(_resources,previous,error))return false;
 stamps[25]=narrativeScenarioStamp(previous);GameSaveRecord staged;
 if(!readGameSave(manager,target,slot,disc,stamps,staged,error,restoredName))return false;
 NarrativeSnapshot continuation;continuation.scenarioStamps=staged.scenarioStamps;continuation.cursor=continuation.entry=Cursor(staged.resource,staged.resumeEntry);
 if(!rebindNarrativeScenario(continuation,25,previous,_scenarios[25],error))return false;
 staged.scenarioStamps=continuation.scenarioStamps;record=staged;return true;
}
bool SnatcherEngine::startupStorage(StartupStorage& storage,std::string& error){
 if(!ensureGameStorage(error))return false;
 StartupStorage next;next.known=_gameServices.availability!=kGameStorageUnknown;next.available=_gameServices.availability==kGameStorageReady;
 for(unsigned i=0;i<4;++i){auto& slot=next.slots[i];slot.known=_gameServices.slots[i].known;slot.present=_gameServices.slots[i].occupied;
  if(slot.present){GameSaveRecord record;if(!readCompatibleOriginalSlot(i+1,record,error))return false;slot.metadataKnown=true;slot.counter=record.counter;slot.hours=record.elapsed.hours;slot.minutes=record.elapsed.minutes;if(record.progression>2){error="Original slot progression is outside the source title catalog";return false;}slot.progression=uint8_t(record.progression);}
 }
 if(!validateStartupStorage(next,error))return false;storage=next;return true;
}
bool SnatcherEngine::readStartupGameSlot(unsigned slot,std::string& error){
 GameSaveRecord record;Common::String restoredName;
 if(!readCompatibleOriginalSlot(slot,record,error,&restoredName))return false;
 PcmRequest cached;if(record.bookmark.pcm!=0xffff&&(!planPcmRequest(_catalog,record.bookmark.pcm,cached,error)||cached.kind!=kLoadResidentBank)){error="Original slot PCM cache is not a resident bank";return false;}
 StartupSettings settings=originalInputSettings(record);
 if(!_startup.acknowledgeLoad(slot,settings,error))return false;
 Common::CEAchievements().recordSaveLoaded(restoredName,int(slot));
 _startupRecord=record;_startupRecordSlot=slot;_startupRecordReady=true;
 _trace.record(Cursor(record.resource,record.resumeEntry),"original_game_slot_read",slot,record.counter);return true;
}
bool SnatcherEngine::beginOriginalGameResume(const GameSaveRecord& record,const StartupSettings& settings,std::string& error){
 GameSaveRecord saved=record;
 if(!validateGameSaveRecord(saved,error))return false;
 if(!_scene.validateModuleEnvironmentHistory(saved.bookmark.ambient,error))return false;
 if(!originalInputResumeSupported(settings,error))return false;
 if(saved.bookmark.ambient>9||saved.bookmark.interaction>1){error="Original slot requires an unrecovered presentation policy";return false;}
 if(saved.resource<57||size_t(saved.resource-57)>=_scenarios.size()||!_scenarios[saved.resource-57].at(saved.resumeEntry)){error="Original slot continuation is not decoded source";return false;}
 PcmRequest cached;if(saved.bookmark.pcm!=0xffff&&(!planPcmRequest(_catalog,saved.bookmark.pcm,cached,error)||cached.kind!=kLoadResidentBank)){error="Original slot PCM cache is not a resident bank";return false;}
 if(!startNative(Cursor(saved.resource,saved.resumeEntry),false,error)||!_runtime->startGameCheckpoint(Cursor(saved.resource,saved.resumeEntry),saved.packed,saved.progression,saved.cursorHistory,saved.bookmark.presentation,error)||!_scene.setControllerMapping(uint8_t(settings.controlMapping),error))return false;
 _gameElapsedOrigin=uint64_t(saved.elapsed.hours)*60*60*60+uint64_t(saved.elapsed.minutes)*60*60+uint64_t(saved.elapsed.seconds)*60+saved.elapsed.frames;
 _gameServices.counterKnown=_gameServices.resultKnown=true;_gameServices.saveCounter=saved.counter;_gameServices.lastWriteResult=0;_gameStorageScanned=false;
 _originalInputSettings=settings;_pcmMono=settings.mono;_fm.setMono(_pcmMono);_fm.setPercussionMono(_pcmMono);
 const auto& bookmark=saved.bookmark;_bookmark.valid=bookmark.known;_bookmark.pcmKnown=bookmark.known;_bookmark.presentation=bookmark.presentation;_bookmark.stream=bookmark.pcm;_bookmark.music=bookmark.music;_bookmark.ambient=bookmark.ambient;_bookmark.interaction=bookmark.interaction;_bookmark.quiet=bookmark.quiet;_bookmark.suppressed=bookmark.suppressed;_bookmark.actors=bookmark.actors;
 if(!restoreBookmarkPolicies(error))return false;
 _originalResumeJob=RuntimeRequest();_originalResumeJob.kind=kRequestPresentationBookmark;_originalResumeJob.owner=Cursor(saved.resource,saved.resumeEntry);_originalResumeJob.first=1;
 if(!_events.create(kCommandEvent,_originalResumeJob.owner,_originalResumeJob.token,error))return false;
 _originalResumePending=true;_startupActive=false;_startupCompleted=false;
 _trace.record(_originalResumeJob.owner,"original_game_resume_started",saved.counter,saved.bookmark.presentation);return true;
}
bool SnatcherEngine::updateOriginalGameResume(std::string& error){
 if(!_originalResumePending){error="Original game resume has no owner";return false;}
 // startNative defers shared graphics initialization until blanking settles.
 // A resumed checkpoint already owns narrative state/events: initialize the
 // graphics, but never run the fresh-game packed clear/event transition.
 if(_commonGraphicsInitPending){
  if(_scene.graphicsServiceBusy()||_scene.resourceChangePending())return true;
  std::vector<uint8_t> sub;if(!_resources.readFile("SUBCODE.BIN",sub,error)||!_scene.queueScenarioGraphicsInitialization(sub.data(),sub.size(),error,true))return false;
  _commonGraphicsInitPending=false;return true;
 }
 if(!updateBookmark(_originalResumeJob,error))return false;
 if(_events.awaiting(_originalResumeJob.token))return true;
 SemanticEvent completed;if(!_events.take(completed)||!(completed.token==_originalResumeJob.token)||completed.cancelled){error="Original game resume completion association mismatch";return false;}
 _originalResumePending=false;_startupCompleted=true;_startupRecordReady=false;_startupRecordSlot=0;
 _trace.record(completed.owner,"original_game_resume_completed");return true;
}
bool SnatcherEngine::updateReturnTitle(const RuntimeRequest& request,std::string& error){
 error.clear();if(!_returnTitleToken.id||!(_returnTitleToken==request.token)){error="Return-to-title request owner mismatch";return false;}
 if(!_returnTitlePhase){
  // Source1670C appends queue2 only after the old action buffer has drained.
  // Later script requests in the same pass must still receive their real jobs.
  for(const auto& other:_runtime->requests())if(!(other.token==request.token)&&_events.awaiting(other.token))return true;
  std::vector<uint8_t> sub;PaletteEffect fade;const size_t packet=0x100d8-0xd400;
  if(!_resources.readFile("SUBCODE.BIN",sub,error)||!readPaletteEffect(sub.data(),sub.size(),packet,packet+4,fade,error,0xd400)||!_scene.beginApplicationFade(fade,error))return false;
  _returnTitleRemaining=48;_returnTitlePhase=1;_trace.record(request.owner,"application_fade_started",48,fade.delay);return true;
 }
 if(_returnTitlePhase==1){if(_returnTitleRemaining)--_returnTitleRemaining;if(!_returnTitleRemaining){_returnTitlePhase=2;_returnTitleReady=true;_trace.record(request.owner,"application_fade_completed");}}
 // This is a non-returning coordinator handoff. Actual application teardown
 // invalidates its old generation; no completed story/audio event is invented.
 return true;
}
}
