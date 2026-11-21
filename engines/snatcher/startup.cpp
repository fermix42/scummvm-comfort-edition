/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "startup.h"
#include <limits>
namespace Snatcher {
StartupCoordinator::StartupCoordinator():_phase(kStartupDormant),_generation(0),_nextId(1),_paused(false){}
bool StartupCoordinator::startFresh(std::string& error){
 error.clear();if(_generation==std::numeric_limits<uint64_t>::max()){error="startup generation exhausted";return false;}
 ++_generation;_pending=StartupRequest();_phase=kStartupWaitingPresentation;_paused=false;return true;
}
bool StartupCoordinator::cancel(std::string& error){
 error.clear();if(_generation==std::numeric_limits<uint64_t>::max()){error="startup generation exhausted";return false;}
 ++_generation;_pending=StartupRequest();_phase=kStartupDormant;return true;
}
bool StartupCoordinator::issue(StartupRequestKind kind,StartupRequest& request,std::string& error){
 if(_nextId==std::numeric_limits<uint64_t>::max()){error="startup request identity exhausted";return false;}
 _pending.kind=kind;_pending.generation=_generation;_pending.id=_nextId++;request=_pending;return true;
}
bool StartupCoordinator::poll(StartupPresentationState presentation,StartupRequest& request,std::string& error){
 error.clear();request=StartupRequest();if(presentation!=kStartupPresentationNotStarted&&presentation!=kStartupPresentationRunning&&presentation!=kStartupPresentationComplete){error="unknown startup presentation state";return false;}
 if(_paused||_phase==kStartupDormant||_phase==kStartupStoryReady)return true;
 if(pending()){request=_pending;return true;}
 if(_phase==kStartupWaitingPresentation){
  if(presentation==kStartupPresentationNotStarted){request.kind=kStartupPumpPresentation;request.generation=_generation;return true;}
  if(presentation==kStartupPresentationRunning)return true;
  // Only the actual presentation owner's terminal state permits EC5A's reset.
  if(!issue(kStartupPostIntroReset,request,error))return false;_phase=kStartupWaitingPostIntroReset;return true;
 }
 if(_phase==kStartupWaitingFreshInitialization)return issue(kStartupInitializeFreshStory,request,error);
 error="startup phase has no owning request";return false;
}
bool StartupCoordinator::complete(const StartupRequest& request,std::string& error){
 error.clear();if(_paused){error="startup completion is paused";return false;}
 if(!pending()||request.kind!=_pending.kind||!request.id||request.id!=_pending.id||request.generation!=_generation){error="stale, duplicate or unowned startup completion";return false;}
 if(_phase==kStartupWaitingPostIntroReset&&request.kind==kStartupPostIntroReset)_phase=kStartupWaitingFreshInitialization;
 else if(_phase==kStartupWaitingFreshInitialization&&request.kind==kStartupInitializeFreshStory)_phase=kStartupStoryReady;
 else{error="startup completion does not match phase";return false;}
 _pending=StartupRequest();return true;
}
}
