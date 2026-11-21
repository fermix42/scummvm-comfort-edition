/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../delay.h"
#include <cstdio>
#include <stdexcept>
namespace {
unsigned checks;
void check(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"FAILED: %s\n",message);throw std::runtime_error(message);}}
bool equal(const Snatcher::NarrativeDelaySnapshot&a,const Snatcher::NarrativeDelaySnapshot&b){return a.target==b.target&&a.elapsed==b.elapsed&&a.active==b.active&&a.finished==b.finished;}
}
int runDelayTests(){using namespace Snatcher;checks=0;NarrativeDelay wait;std::string error;
 check(!wait.active()&&!wait.finished()&&wait.elapsed()==0&&!wait.tick(false),"idle wait does not advance");
 wait.start(1);check(!wait.tick(true)&&wait.elapsed()==0&&wait.counter()==0,"paused first tick leaves wait active");
 check(wait.tick(false)&&wait.elapsed()==1&&wait.counter()==0&&!wait.active(),"one-tick wait completes and clears source counter");
 for(unsigned i=0;i<100;++i)wait.tick(false);check(wait.elapsed()==1&&wait.finished(),"terminal wait does not complete repeatedly or overflow");
 for(uint16_t target:{uint16_t(2),uint16_t(30),uint16_t(60),uint16_t(100),uint16_t(65535),uint16_t(0)}){
  uint32_t duration=target?target:65536;wait.start(target);
  for(uint32_t i=1;i<duration;++i){if(wait.tick(false))throw std::runtime_error("delay completed early");}
  check(wait.active()&&wait.elapsed()==duration-1&&wait.counter()==uint16_t(duration-1),"counter reaches exact pre-completion value");
  NarrativeDelaySnapshot before=wait.snapshot();for(unsigned i=0;i<17;++i)wait.tick(true);
  check(equal(before,wait.snapshot()),"pause never consumes elapsed ticks near completion");
  check(wait.tick(false)&&wait.elapsed()==duration&&wait.counter()==0,"target equality completes including zero wrap");
  NarrativeDelay restored;check(restored.restore(wait.snapshot(),error)&&restored.finished()&&restored.tick(true)&&restored.elapsed()==duration,"terminal snapshot restoration remains terminal while paused");
 }
 wait.start(100);for(unsigned i=0;i<40;++i)wait.tick(false);NarrativeDelaySnapshot active=wait.snapshot();
 NarrativeDelay restored;check(restored.restore(active,error)&&restored.elapsed()==40&&restored.counter()==40&&!restored.tick(true),"active snapshot retains logical time with caller-owned pause");
 for(unsigned i=0;i<59;++i)restored.tick(false);check(!restored.finished()&&restored.elapsed()==99&&restored.tick(false),"restored wait completes on original boundary");
 wait.start(2);check(wait.elapsed()==0&&!wait.finished()&&wait.active(),"restart resets elapsed and terminal ownership");
 wait.tick(false);wait.reset();check(!wait.tick(false)&&!wait.active()&&!wait.finished()&&wait.elapsed()==0,"owner reset discards local wait without fabricated completion");
 check(restored.restore(wait.snapshot(),error)&&!restored.active()&&!restored.finished(),"idle snapshot restoration");
 wait.start(0);for(unsigned i=0;i<65535;++i)wait.tick(false);NarrativeDelaySnapshot boundary=wait.snapshot();
 check(restored.restore(boundary,error)&&restored.counter()==65535&&restored.tick(false)&&restored.elapsed()==65536,"zero-target active snapshot completes after exact wrap");
 NarrativeDelaySnapshot invalid=active;invalid.active=true;invalid.finished=true;check(!restored.restore(invalid,error),"active-and-finished snapshot rejected");
 NarrativeDelaySnapshot preserved=restored.snapshot();
 invalid=active;invalid.elapsed=100;check(!restored.restore(invalid,error)&&equal(preserved,restored.snapshot()),"active equality snapshot rejected transactionally");
 invalid=active;invalid.elapsed=0xffffffff;check(!restored.restore(invalid,error)&&equal(preserved,restored.snapshot()),"unbounded elapsed snapshot rejected before application");
 invalid=active;invalid.active=false;invalid.finished=true;check(!restored.restore(invalid,error),"early-finished snapshot rejected");
 invalid=NarrativeDelaySnapshot();invalid.target=1;check(!restored.restore(invalid,error),"noncanonical idle target rejected");
 invalid=NarrativeDelaySnapshot();invalid.elapsed=1;check(!restored.restore(invalid,error),"noncanonical idle elapsed rejected");
 invalid=NarrativeDelaySnapshot();invalid.active=true;invalid.elapsed=65536;check(!restored.restore(invalid,error),"zero-target active overflow rejected");
 std::printf("narrative delay: %u checks, 0 failures\n",checks);return 0;
}
#ifdef SNATCHER_DELAY_TEST_MAIN
int main(){return runDelayTests();}
#endif
