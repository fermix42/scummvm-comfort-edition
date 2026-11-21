/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_NATIVE_VISUAL_JOBS_H
#define SNATCHER_NATIVE_VISUAL_JOBS_H
#include "scene_renderer.h"
#include <utility>
namespace Snatcher {
// Existing scene queues own persistent requests. This short-lived transaction
// admits their current shared sources, completes native work, then publishes
// allocations once. No wall clock, hardware budget or callback delay exists.
struct NativeVisualRequest {
 bool clearFont=false,clearNames=false,resetLayout=false;
 SceneLayout layout;SceneLayoutPreset preset=kSceneTallLayout;
 std::vector<SceneGraphicsCopy> copies;
 bool empty()const{return !clearFont&&!clearNames&&!resetLayout&&copies.empty();}
};
class NativeVisualCommit {
public:
 enum State {kRequested,kAdmitted,kDataReady,kCompleted,kCommitted,kCancelled,kFailed};
 explicit NativeVisualCommit(const NativeVisualRequest& request):_request(request),_state(kRequested){}
 State state()const{return _state;}
 bool continuationAllowed()const{return _state==kCommitted;}
 bool admit(const SceneGraphicsQueue& sources,std::string& error){
  error.clear();if(_state!=kRequested)return fail(error,"visual job admission has no request");
  _state=kAdmitted;for(const auto& copy:_request.copies){GraphicsTransfer ready;if(!sources.resolveCopy(copy,ready,error)){_state=kFailed;return false;}_ready.push_back(ready);}
  _state=kDataReady;return true;
 }
 bool complete(const SceneRenderer& current,std::string& error){
  error.clear();if(_state!=kDataReady)return fail(error,"visual job data is not ready");
  if(!_request.empty()){
   _completed=current;
   auto clear=[&](uint16_t destination,size_t count){GraphicsTransfer t={};t.destination=destination;t.decoded.assign(count,0);return _completed.applyTransfer(t,error);};
   if((_request.clearFont&&!clear(0,8160))||(_request.clearNames&&(!clear(0xc000,4096)||!clear(0xe000,4096)))){_state=kFailed;return false;}
   if(_request.resetLayout&&(!_completed.configure(_request.layout,error)||!_completed.establishSceneReset(_request.preset,error))){_state=kFailed;return false;}
   for(const auto& transfer:_ready)if(!_completed.applyTransfer(transfer,error)){_state=kFailed;return false;}
  }
  _state=kCompleted;return true;
 }
 bool commit(SceneRenderer& target,std::string& error){
  error.clear();if(_state!=kCompleted)return fail(error,"visual job cannot publish unfinished work");
  // Presentation layout/palette have separate owners; adopt only the complete
  // decoded allocations. Layout reset metadata is committed by SceneRuntime.
  if(!_request.empty()){
   std::swap(target._patterns,_completed._patterns);
   for(unsigned p=0;p<2;++p){target._maps[p].swap(_completed._maps[p]);target._known[p].swap(_completed._known[p]);}
  }
  _state=kCommitted;return true;
 }
 bool cancel(std::string& error){error.clear();if(_state==kCommitted)return fail(error,"committed visual work cannot be cancelled");_ready.clear();_state=kCancelled;return true;}
private:
 NativeVisualRequest _request;State _state;std::vector<GraphicsTransfer> _ready;SceneRenderer _completed;
 bool fail(std::string& error,const char* text){error=text;return false;}
};
// Both adapters use the same consumption readiness. Pointer intents may stay
// bound to their current menu/page until publication; digital edges retain
// their existing source-owned sampling rules.
inline bool nativeStartupInputReady(bool graphicsServiceBusy,bool resourcePending){return !graphicsServiceBusy&&!resourcePending;}
inline bool nativeMenuInputReady(bool visualBusy,bool painting,bool fontPending){return !visualBusy&&!painting&&!fontPending;}
inline bool nativeDialogueInputReady(bool waiting,bool visualBusy,bool fontPending){return waiting&&!visualBusy&&!fontPending;}
}
#endif
