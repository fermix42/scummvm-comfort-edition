/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine.h"
namespace Snatcher {
void SnatcherEngine::resetCredits(){_credits=CreditsTimeline();_creditsData.clear();_creditsToken=EventToken();_creditsPhase=0;_creditsScene=-1;_creditsClockKnown=false;_creditsClock=0;_creditsClockOrigin=0;_creditsGateRgb.clear();_creditsGateTick=0;}
bool SnatcherEngine::startCredits(const RuntimeRequest& request,std::string& error){
 error.clear();if(_creditsPhase){if(!(_creditsToken==request.token)){error="application credits owner conflict";return false;}return true;}
 _creditsToken=request.token;_creditsPhase=1;_coordinatorMode=5;
 _trace.record(request.owner,"application_completion_requested",1);return true;
}
bool SnatcherEngine::installCreditsScene(unsigned index,std::string& error){
 // F5E8 cancels queued programs and retains staging. Its deferred name clear
 // and any dependent transfers are serviced before the next coordinator.
 if(_scene.transferBusy()){error="credits scene has outstanding graphics ownership";return false;}
 if(!_scene.preparePresentationReplacement(error)||!_scene.install(52,_creditsData,index,error))return false;
 _sceneAssets=_scene.assets();_sceneAssetsReady=true;_creditsScene=int(index);
 _trace.record(Cursor(52,index),"credits_scene_installed",index);return true;
}
bool SnatcherEngine::updateCredits(std::string& error){
 error.clear();if(!_creditsPhase)return true;
 bool admitted=false;if(!_scene.advanceCoordinatorClock(error,&admitted))return false;
 if(!admitted)return true;
 if(_creditsPhase==1){
  // E242 waits for actor31, then F560/EEB6 reset only their qualified owners.
  if(_scene.actor(31)->active||_scene.actor(31)->reserved)return true;
  if(!_scene.beginCreditsPresentation(error))return false;
  _commandInput.close();_scene.clearMenuCursor();_dialogueWindow.cancel();_dialogueReady=false;
  _creditsPhase=2;return true;
 }
 if(_creditsPhase==2){
  if(_cd.active()&&!_cd.paused())return true;
  if(!_gameServices.counterKnown||_gameServices.pending){error="Credits requires the settled original-save counter owner";return false;}
  if(!_resources.readResource(52,_creditsData,error))return false;
  if(!_credits.start(_creditsData,_gameServices.saveCounter!=0,error))return false;
  _creditsPhase=3;_trace.record(Cursor(52,0x10d4),"credits_program_started",_gameServices.saveCounter);return true;
 }
 if(_creditsPhase==4){
  // The source reloads its catalog and percussion owners before application
  // restart. Immutable native assets use the same verified resource service.
  std::vector<uint8_t> index,drums;if(!_resources.readResource(56,index,error)||!_resources.readResource(54,drums,error))return false;
  std::vector<PcmCatalogEntry> catalog;if(!parsePcmCatalog(index.data(),index.size(),_pcmData.size(),catalog,error))return false;
  std::vector<PcmEffect> effects;if(!parsePcmEffects(index.data(),index.size(),effects,error))return false;
  _catalog.swap(catalog);_effectCatalog.swap(effects);_drumData.swap(drums);
  if(!_cd.reset(error))return false;_streamOwner.reset();_effectOwner.reset();_audio.reset();_fm.reset();
  if(_events.awaiting(_creditsToken)&&!_events.complete(_creditsToken,false,error))return false;
  // Main's normal application teardown returns to game-owned startup. The
  // console BIOS backup-device selector is delegated to CE portable storage.
  _returnTitleReady=true;_creditsPhase=5;_trace.record(Cursor(95,3691),"credits_application_handoff",1);return true;
 }
 if(_creditsPhase!=3)return true;
 unsigned track=0;uint64_t frames=0;bool position=_cd.presentationPosition(track,frames);
 _creditsClockKnown=position;if(position){uint64_t relative=track==19&&frames>=_creditsClockOrigin?frames-_creditsClockOrigin:frames;if(!encodeCdRelativeClock(relative,_creditsClock,error))return false;}
 CreditsInput input;input.ready=!_scene.transferBusy()&&!_scene.resourceChangePending();
 input.cdPlaying=_cd.active()&&_cdBackend.active()&&!_cd.paused();input.cdIdleOrPaused=!_cd.active()||_cd.paused();input.clockKnown=_creditsClockKnown;input.relativeClock=_creditsClock;input.startPressed=_workerStartPressed;
 std::vector<CreditsEvent> events;if(!_credits.tick(input,events,error))return false;
 for(const auto& event:events){
  if(event.kind==kCreditsStartTrack){AudioToken token;if(!_cd.reset(error)||!_cd.start(event.value,64,AudioOwner(52,uint16_t(_credits.index()<0?0:_credits.index())),_events.generation(),token,error))return false;_creditsClockOrigin=0;_creditsClock=0;_creditsClockKnown=true;_trace.record(Cursor(52,0x10fc),"credits_cd_started",event.value);}
  else if(event.kind==kCreditsResetRelativeClock){unsigned playingTrack;uint64_t playingFrames;if(!_cd.consumedPosition(playingTrack,playingFrames)||playingTrack!=19){error="credits relative clock reset lacks actual transport position";return false;}_creditsClockOrigin=playingFrames;_creditsClock=0;_creditsClockKnown=true;}
  else if(event.kind==kCreditsInstallScene){if(!installCreditsScene(event.value,error))return false;}
  else if(event.kind==kCreditsPalette){PaletteEffect effect;if(!readPaletteEffect(_creditsData.data(),_creditsData.size(),event.value,_creditsData.size(),effect,error,0x28000))return false;_scene.cancelPaletteEffects();if(!_scene.queuePaletteEffect(effect,error))return false;}
  else if(event.kind==kCreditsApplicationComplete){if(_creditsCompletionSerial==UINT64_MAX){error="credits completion identity exhausted";return false;}++_creditsCompletionSerial;_creditsPhase=4;_trace.record(Cursor(52,0x12bc),"credits_program_completed",1);}
 }
 return true;
}
}
