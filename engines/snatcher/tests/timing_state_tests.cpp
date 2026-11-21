/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../timing_state.h"
#include <cstdio>
#include <cstdlib>
using namespace Snatcher;
namespace {unsigned checks=0;void require(bool value){++checks;if(!value){std::fprintf(stderr,"timing state failure %u\n",checks);std::exit(1);}}}
int main(){
 std::string error;GameClock clock;require(clock.configure(599227434,10000000,error));uint32_t updates;require(clock.advance(17,false,updates,error));
 EventScheduler scheduler;EventToken original;require(scheduler.create(kCommandEvent,Cursor(63,100),original,error));NarrativeDelay delay;delay.start(0);for(unsigned i=0;i<123;++i)require(!delay.tick(false));
 TimingSnapshot saved;saved.clock=clock.snapshot();saved.delay=delay.snapshot();saved.delayToken=original;NativeSaveSection section;require(encodeTimingState(saved,section,error));require(section.bytes.size()==52&&section.id==kSaveTimers&&section.required);
 TimingSnapshot decoded;require(decodeTimingState(section,decoded,error));require(decoded.clock.fraction==saved.clock.fraction&&decoded.clock.ticks==saved.clock.ticks&&decoded.delay.elapsed==123&&decoded.delayToken==original);
 EventSnapshot savedEvents=scheduler.snapshot();std::vector<EventRebind> rebind;require(scheduler.restore(savedEvents,rebind,error));GameClock restored;NarrativeDelay wait;EventToken token;
 require(restoreTimingState(decoded,savedEvents,rebind,restored,wait,token,error));require(token==rebind[0].current&&!(token==original));require(!scheduler.complete(original,false,error));
 for(unsigned i=123;i<65535;++i)require(!wait.tick(false));require(!wait.tick(true));require(wait.tick(false));require(scheduler.complete(token,false,error));require(!scheduler.complete(token,false,error));SemanticEvent event;require(scheduler.take(event)&&event.token==token&&!event.cancelled);require(!scheduler.take(event));
 uint32_t a,b;require(clock.advance(983,false,a,error));require(restored.advance(983,false,b,error));require(a==b&&clock.snapshot().fraction==restored.snapshot().fraction&&clock.snapshot().ticks==restored.snapshot().ticks);
 // Every truncation and trailing byte fails without partially replacing output.
 for(size_t size=0;size<52;++size){NativeSaveSection bad=section;bad.bytes.resize(size);TimingSnapshot before=decoded;require(!decodeTimingState(bad,decoded,error));require(decoded.delay.elapsed==before.delay.elapsed&&decoded.delayToken==before.delayToken);}
 NativeSaveSection bad=section;bad.bytes.push_back(0);require(!decodeTimingState(bad,decoded,error));
 const unsigned reserved[]={4,5,6,7,38,39};for(unsigned offset:reserved){bad=section;bad.bytes[offset]=255;require(!decodeTimingState(bad,decoded,error));}
 bad=section;bad.bytes[8]=0xff;require(!decodeTimingState(bad,decoded,error));bad=section;bad.version=2;require(!decodeTimingState(bad,decoded,error));bad=section;bad.id=kSaveEvents;require(!decodeTimingState(bad,decoded,error));
 // Cross-owner mismatch is rejected transactionally, leaving live time intact.
 const uint64_t priorTicks=restored.snapshot().ticks;const uint32_t priorElapsed=wait.elapsed();EventToken priorToken=token;
 std::vector<EventRebind> missing;require(!restoreTimingState(saved,savedEvents,missing,restored,wait,token,error));require(restored.snapshot().ticks==priorTicks&&wait.elapsed()==priorElapsed&&token==priorToken);
 EventSnapshot wrong=savedEvents;wrong.pending[0].kind=kAudioEvent;require(!restoreTimingState(saved,wrong,rebind,restored,wait,token,error));wrong=savedEvents;wrong.pending[0].cancelled=true;wrong.pending[0].completed=true;wrong.ready.push_back(original);require(!restoreTimingState(saved,wrong,rebind,restored,wait,token,error));
 auto duplicate=rebind;duplicate.push_back(rebind[0]);require(!restoreTimingState(saved,savedEvents,duplicate,restored,wait,token,error));
 // Idle and completed local owners have distinct serialized dispositions.
 TimingSnapshot idle;idle.clock=saved.clock;require(encodeTimingState(idle,section,error));require(decodeTimingState(section,decoded,error));EventSnapshot empty;require(restoreTimingState(decoded,empty,missing,restored,wait,token,error));require(!token.id&&!wait.active()&&!wait.finished());
 idle.delayToken=original;require(!encodeTimingState(idle,section,error));saved.delay.active=false;saved.delay.finished=true;saved.delay.elapsed=65536;require(encodeTimingState(saved,section,error));require(decodeTimingState(section,decoded,error));
 EventScheduler finished;EventToken finishedToken;require(finished.create(kCommandEvent,Cursor(63,100),finishedToken,error));require(finished.complete(finishedToken,false,error));auto snapshot=finished.snapshot();require(finished.restore(snapshot,rebind,error));require(restoreTimingState(decoded,snapshot,rebind,restored,wait,token,error));require(wait.finished());
 std::printf("timing state: %u checks, 0 failures\n",checks);return 0;
}
