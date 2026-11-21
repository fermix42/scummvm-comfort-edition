/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "animation.h"
#include <sstream>
#include <limits>
namespace Snatcher {
namespace {
uint16_t word(const uint8_t *p){return uint16_t(uint16_t(p[0])<<8|p[1]);}
int16_t signedWord(const uint8_t *p){uint16_t v=word(p);return int16_t(v<0x8000?int(v):int(v)-65536);}
int32_t fixed(int16_t integer,uint8_t fraction=0){return int32_t(integer)*65536+int32_t(fraction)*256;}
// Unsigned addition defines original16.16 wrap without signed-overflow UB.
int32_t wrapAdd(int32_t a,int32_t b){uint32_t v=uint32_t(a)+uint32_t(b);return v<=0x7fffffff?int32_t(v):int32_t(int64_t(v)-0x100000000LL);}
}
bool applyAnimationClockCapture(uint8_t argument,AnimationClockCapture& state,std::string& error){
 error.clear();if(!state.clockKnown||(argument&&!state.captureKnown)){error="animation clock capture requires verified coordinator/history context";return false;}state.captured=argument?uint16_t(unsigned(state.sourceTicks)-unsigned(state.captured)):state.sourceTicks;state.captureKnown=true;return true;
}
AnimationPlayer::AnimationPlayer():_sequence(0),_first(true),_paused(false),_halted(false),_blocked(false),_repeat(0),_haltResult(kAnimationError),_completionMarkers(0){}
void AnimationPlayer::cancel(){_state=AnimationState();_resource.clear();_sequence=0;_first=true;_paused=false;_halted=false;_blocked=false;_repeat=0;_haltError.clear();_scrollRequests.clear();_completionMarkers=0;}
bool AnimationPlayer::start(const uint8_t *data,size_t size,size_t sequence,int16_t x,int16_t y,std::string &error){
 cancel();error.clear();if(!data||size>16*1024*1024||sequence>size||size-sequence<4){error="animation sequence outside bounded resource";return false;}
 _resource.assign(data,data+size);_sequence=sequence;_state.active=true;_state.x16=fixed(x);_state.y16=fixed(y);return true;
}
bool AnimationPlayer::restartSequence(std::string& error){error.clear();if(!_state.active||_resource.empty()||_halted||_blocked||!_scrollRequests.empty()){error="animation restart requires active specified ownership without pending controls";return false;}_state.recordIndex=0xffff;_state.remaining=0xffff;_first=true;return true;}
bool AnimationPlayer::selectRecord(unsigned record,std::string& error){
 error.clear();if(!_state.active||_halted||!_scrollRequests.empty()||record>0xffff||_sequence>_resource.size()||size_t(record)>=(_resource.size()-_sequence)/4){error="animation record selection requires active bounded ownership";return false;}
 _state.recordIndex=record;_first=false;_blocked=false;return true;
}
AnimationResult AnimationPlayer::fail(AnimationResult result,const char *message,std::string &error){std::ostringstream out;out<<message<<" at sequence record "<<_state.recordIndex<<" (resource offset "<<currentOffset()<<")";error=out.str();_halted=true;_haltResult=result;_haltError=error;return result;}
AnimationResult AnimationPlayer::tick(std::string &error,AnimationCommandHandler handler,void *owner,AnimationRandomHandler random){
 error.clear();if(_halted){error=_haltError;return _haltResult;}if(!_state.active)return kAnimationFinished;if(_paused)return kAnimationPaused;
 if(!_scrollRequests.empty())return fail(kAnimationError,"unconsumed semantic scroll requests",error);
 uint16_t remaining=uint16_t(_state.remaining-1);
 bool advance=_first||_blocked||remaining==0||(remaining&0x8000)!=0;
 if(!advance){
  size_t offset=currentOffset();if(offset>_resource.size()||_resource.size()-offset<4)return fail(kAnimationError,"truncated held animation record",error);
  // F8C8..F8EA: a held97 preserves the flag; every other held record
  // clears it. An external hide therefore does not outlive a normal hold.
  if(_resource[offset]!=0x97)_state.hidden=false;
  _state.remaining=remaining;_state.x16=wrapAdd(_state.x16,_state.velocityX16);_state.y16=wrapAdd(_state.y16,_state.velocityY16);return kAnimationFrame;
 }
 unsigned next=_first?0:_blocked?_state.recordIndex:uint16_t(_state.recordIndex+1);_first=false;_blocked=false;
 // This guards malformed immediate-control cycles; it is an explicit error,
 // never a guessed loop exit. Original valid visible loops yield a timed record.
 for(unsigned budget=0;budget<4096;++budget){
  // F8F6 clears hidden before each dispatch, including blocked-control retries.
  // A wait following97 reveals the retained frame, without advancing it.
  _state.hidden=false;
  _state.recordIndex=next;size_t offset=currentOffset();
  if(offset>_resource.size()||_resource.size()-offset<4)return fail(kAnimationError,"truncated animation record",error);
  const uint8_t *p=&_resource[offset];uint8_t op=p[0];
  if(op<0x80||op==0x97){
   uint16_t duration=word(p+2);if(p[1]){if(!random)return fail(kAnimationUnsupported,"random animation duration requires verified RNG context",error);uint16_t lower=p[2],span=uint16_t(int(p[3])-int(p[2]));if(!random(owner,lower,span,duration,error))return fail(kAnimationError,error.empty()?"shared animation random owner failed":error.c_str(),error);}
   _state.remaining=duration;_state.hidden=op==0x97;if(op<0x80)_state.frameIndex=op;
   _state.x16=wrapAdd(_state.x16,_state.velocityX16);_state.y16=wrapAdd(_state.y16,_state.velocityY16);return kAnimationFrame;
  }
  if(handler&&op!=0x80&&op!=0x81&&op!=0x82&&op!=0x83&&op!=0x97&&op!=0x98&&op!=0x9e&&op!=0xa0&&op!=0xa1&&op!=0xa7&&!(op>=0x8c&&op<=0x8f)){
   AnimationCommand command={op,p[1],word(p+2),_sequence,offset};AnimationCommandResult result=handler(owner,command,error);
   if(result==kAnimationCommandAdvance){++next;continue;}
   if(result==kAnimationCommandRetireSelf){std::vector<ScrollRequest> pending;pending.swap(_scrollRequests);cancel();_scrollRequests.swap(pending);return kAnimationSelfRetired;}
   if(result==kAnimationCommandBlocked){_blocked=true;_state.x16=wrapAdd(_state.x16,_state.velocityX16);_state.y16=wrapAdd(_state.y16,_state.velocityY16);return kAnimationFrame;}
   if(result==kAnimationCommandUnsupported&&!error.empty())return fail(kAnimationUnsupported,error.c_str(),error);
   if(result==kAnimationCommandError)return fail(kAnimationError,error.empty()?"scene animation command failed":error.c_str(),error);
  }
  switch(op){
  case 0x80:{unsigned markers=_completionMarkers;std::vector<ScrollRequest> pending;pending.swap(_scrollRequests);cancel();_scrollRequests.swap(pending);_completionMarkers=markers;return kAnimationFinished;}
  case 0x81:next=p[1];break;
  case 0x82:if(_repeat==p[1]){_repeat=0;++next;}else{++_repeat;next=p[3];}break;
  case 0x83:{if(!_repeat){if(!random)return fail(kAnimationUnsupported,"random animation repeat requires verified RNG context",error);uint16_t count=0;if(!random(owner,2,uint8_t(p[2]-p[1]),count,error))return fail(kAnimationError,error.empty()?"shared animation repeat RNG failed":error.c_str(),error);_repeat=uint8_t(count);}if(_repeat==p[2]){_repeat=0;++next;}else{++_repeat;next=p[3];}break;}
  case 0x84:_state.x16=fixed(signedWord(p+2))+int32_t(uint32_t(_state.x16)&0xffff);++next;break;
  case 0x85:_state.y16=fixed(signedWord(p+2))+int32_t(uint32_t(_state.y16)&0xffff);++next;break;
  case 0x86:_state.velocityX16=fixed(signedWord(p+2),p[1]);++next;break;
  case 0x87:_state.velocityY16=fixed(signedWord(p+2),p[1]);++next;break;
  case 0x8c:case 0x8d:case 0x8e:case 0x8f:{ScrollRequest request={op<0x8e?ScrollRequest::kPosition:ScrollRequest::kVelocity,p[1]?1u:0u,(op&1)?kScrollY:kScrollX,signedWord(p+2)};_scrollRequests.push_back(request);++next;break;}
  case 0x98:++_completionMarkers;++next;break;
  case 0x9e:++next;break; // Verified original FC08 explicitly advances.
  case 0xa0:case 0xa1:_state.styleWord=word(p+2);++next;break;
  case 0xa7:_state.paletteOverride=p[1];++next;break;
  default:return fail(kAnimationUnsupported,"unsupported animation control",error);
  }
 }
 return fail(kAnimationError,"animation immediate-control cycle/budget",error);
}
}
