/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "timing_state.h"
namespace Snatcher {
namespace {
void put(std::vector<uint8_t>& out,uint64_t value,unsigned bytes){while(bytes)out.push_back(uint8_t(value>>(--bytes*8)));}
uint64_t get(const uint8_t* data,size_t& cursor,unsigned bytes){uint64_t value=0;while(bytes--)value=value<<8|data[cursor++];return value;}
bool valid(const TimingSnapshot& state,std::string& error){
 GameClock clock;NarrativeDelay delay;
 if(!clock.restore(state.clock,error)||!delay.restore(state.delay,error))return false;
 bool owned=state.delay.active||state.delay.finished;
 if(owned?(!state.delayToken.id||!state.delayToken.generation):(state.delayToken.id||state.delayToken.generation)){error="timing delay token ownership inconsistent";return false;}return true;
}
}
bool encodeTimingState(const TimingSnapshot& state,NativeSaveSection& section,std::string& error){
 error.clear();if(!valid(state,error))return false;
 NativeSaveSection out(kSaveTimers);out.required=true;const uint8_t header[]={'S','N','T','M',1,0,0,0};out.bytes.assign(header,header+8);
 put(out.bytes,state.clock.numerator,4);put(out.bytes,state.clock.denominator,4);put(out.bytes,state.clock.fraction,8);put(out.bytes,state.clock.ticks,8);
 put(out.bytes,state.delay.target,2);put(out.bytes,state.delay.elapsed,4);put(out.bytes,(state.delay.active?1:0)|(state.delay.finished?2:0),1);put(out.bytes,0,1);
 put(out.bytes,state.delayToken.id,8);put(out.bytes,state.delayToken.generation,4);section=out;return true;
}
bool decodeTimingState(const NativeSaveSection& section,TimingSnapshot& state,std::string& error){
 error.clear();const std::vector<uint8_t>& b=section.bytes;
 if(section.id!=kSaveTimers||section.version!=1||b.size()!=52||b[0]!='S'||b[1]!='N'||b[2]!='T'||b[3]!='M'||b[4]!=1||b[5]||b[6]||b[7]||b[38]>2||b[39]){error="timing payload header/version/extent/flags";return false;}
 TimingSnapshot next;size_t c=8;next.clock.numerator=uint32_t(get(b.data(),c,4));next.clock.denominator=uint32_t(get(b.data(),c,4));next.clock.fraction=get(b.data(),c,8);next.clock.ticks=get(b.data(),c,8);
 next.delay.target=uint16_t(get(b.data(),c,2));next.delay.elapsed=uint32_t(get(b.data(),c,4));unsigned flags=unsigned(get(b.data(),c,1));++c;next.delay.active=(flags&1)!=0;next.delay.finished=(flags&2)!=0;next.delayToken.id=get(b.data(),c,8);next.delayToken.generation=uint32_t(get(b.data(),c,4));
 if(!valid(next,error))return false;state=next;return true;
}
bool restoreTimingState(const TimingSnapshot& state,const EventSnapshot& events,const std::vector<EventRebind>& bindings,GameClock& clock,NarrativeDelay& delay,EventToken& token,std::string& error){
 error.clear();if(!valid(state,error)||!validateEventSnapshot(events,error))return false;EventToken rebound;
 if(state.delayToken.id){
  const SemanticEvent* event=nullptr;for(const SemanticEvent& e:events.pending)if(e.token==state.delayToken)event=&e;
  if(!event||event->kind!=kCommandEvent||event->cancelled||event->completed!=state.delay.finished){error="timing delay has no matching scheduler disposition";return false;}
  unsigned matches=0;for(const EventRebind& binding:bindings)if(binding.previous==state.delayToken){rebound=binding.current;++matches;}
  if(matches!=1||!rebound.id||!rebound.generation||rebound.generation==state.delayToken.generation){error="timing delay requires one fresh scheduler token";return false;}
 }
 GameClock stagedClock;NarrativeDelay stagedDelay;if(!stagedClock.restore(state.clock,error)||!stagedDelay.restore(state.delay,error))return false;
 clock=stagedClock;delay=stagedDelay;token=rebound;return true;
}
}
