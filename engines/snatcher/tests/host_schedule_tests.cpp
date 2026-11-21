/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../game_clock.h"
#include "../timing_state.h"
#include "../credits.h"
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace Snatcher;
static unsigned checks=0;
static void check(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(int argc,char** argv){
 std::string error;GameClock clock,normal;uint32_t done=0,expected=0;uint64_t suspended=0;
 check(clock.configure(599227434,10000000,error)&&normal.configure(599227434,10000000,error),"source rational cadence");
 for(unsigned i=0;i<1000;++i){
  auto before=clock.snapshot();std::vector<uint16_t> counters;
  check(clock.advanceHost(10,4,[&](uint16_t counter){counters.push_back(counter);return true;},done,suspended,error),"ordinary visit");
  check(normal.advance(10,false,expected,error)&&done==expected&&!suspended,"normal scheduling unchanged");
  check(clock.snapshot().ticks==normal.snapshot().ticks&&clock.snapshot().fraction==normal.snapshot().fraction,"normal time partition invariance");
  if(done)check(counters.front()==uint16_t(before.ticks+1),"consecutive update identity");
 }
 const uint64_t prior=clock.snapshot().ticks;
 check(clock.advanceHost(60000,4,[&](uint16_t counter){return clock.snapshot().ticks==uint64_t(prior+done);},done,suspended,error)&&done==4&&suspended>3500,"minute stall bounded; workers see committed clock");
 check(clock.snapshot().ticks==prior+4,"host stall never jumps script clock");
 check(clock.advanceHost(0,4,[](uint16_t){return true;},done,suspended,error)&&!done&&!suspended,"no residual catchup debt");
 // A scene/new-game owner can reset the same clock from inside a worker.
 unsigned visits=0;
 check(clock.advanceHost(1000,4,[&](uint16_t){++visits;return clock.configure(60,1,error);},done,suspended,error)&&done==1&&visits==1&&!clock.snapshot().ticks,"scene reset retires previous batch");
 check(clock.advanceHost(17,4,[&](uint16_t counter){return counter==1;},done,suspended,error)&&done==1,"new context starts at its own first tick");
 // Authored waits and independent completion events remain sequential, even
 // when host time is repeatedly late. No deadline/event is synthesized away.
 NarrativeDelay delay;delay.start(9);EventScheduler events;EventToken audio;
 check(events.create(kAudioEvent,Cursor(63,1),audio,error)&&events.complete(audio,false,error),"audio finishes independently of host scheduler");
 std::vector<unsigned> authored;unsigned simulated=0,hostPolls=0;
 for(unsigned host=0;host<3;++host){++hostPolls;check(clock.advanceHost(5000,4,[&](uint16_t){++simulated;authored.push_back(simulated);delay.tick(false);return true;},done,suspended,error)&&done<=4,"input polling between bounded visits");}
 check(hostPolls==3&&simulated==12&&authored.front()==1&&authored.back()==12&&delay.finished(),"ordered authored updates and wait completion retained");
 SemanticEvent completion;check(events.take(completion)&&completion.token==audio&&!events.take(completion),"audio completion delivered exactly once");
 // Existing semantic timer archive remains sufficient: no host backlog exists
 // at save boundaries, and host timestamps/device positions are not serialized.
 TimingSnapshot saved;saved.clock=clock.snapshot();NativeSaveSection section;TimingSnapshot decoded;
 check(encodeTimingState(saved,section,error)&&section.bytes.size()==52&&decodeTimingState(section,decoded,error),"timer save schema unchanged");
 GameClock restored;check(restored.restore(decoded.clock,error),"restore bounded clock");
 std::vector<uint16_t> a,b;uint32_t other;uint64_t otherSuspended;
 check(clock.advanceHost(31,4,[&](uint16_t tick){a.push_back(tick);return true;},done,suspended,error)&&restored.advanceHost(31,4,[&](uint16_t tick){b.push_back(tick);return true;},other,otherSuspended,error)&&a==b&&clock.snapshot().fraction==restored.snapshot().fraction,"save/resume continuation exact");
 auto pause=clock.snapshot();check(clock.advance(3600000,true,expected,error)&&!expected&&clock.snapshot().ticks==pause.ticks&&clock.snapshot().fraction==pause.fraction,"pause never accrues catchup");
 auto before=clock.snapshot();check(!clock.advanceHost(1000,0,[](uint16_t){return true;},done,suspended,error)&&clock.snapshot().ticks==before.ticks&&clock.snapshot().fraction==before.fraction,"invalid budget transactional");
 GameClockState wrapped;wrapped.numerator=60;wrapped.denominator=1;wrapped.ticks=65534;check(clock.restore(wrapped,error),"source counter wrap owner");
 std::vector<uint16_t> wrap;check(clock.advanceHost(100,4,[&](uint16_t tick){wrap.push_back(tick);return true;},done,suspended,error),"bounded source counter wrap");
 check(wrap==std::vector<uint16_t>({65535,0,1,2})&&clock.snapshot().ticks==65538,"source16-bit wrap does not skip semantic updates");
 before.ticks=UINT64_MAX;check(clock.restore(before,error)&&!clock.advanceHost(1000,4,[](uint16_t){return true;},done,suspended,error)&&clock.snapshot().fraction==before.fraction,"overflow transactional");
 check(clock.configure(60,1,error)&&!clock.advanceHost(1000,4,[](uint16_t){return false;},done,suspended,error)&&done==1&&clock.snapshot().ticks==1,"worker failure cannot precommit unexecuted updates");
 if(argc==2){
  std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
  for(bool alternate:{false,true}){
   CreditsTimeline credits;GameClock host;check(credits.start(bytes,alternate,error)&&host.configure(60,1,error),"original credits under bounded scheduler");
   CreditsInput playback;playback.ready=true;playback.clockKnown=true;playback.relativeClock=0x99007400;unsigned expectedScene=0,completed=0,hostVisits=0;
   while(!credits.finished()&&hostVisits++<1000){
    check(host.advanceHost(60000,4,[&](uint16_t){
     // Late playback deadlines never advance the credits record index directly.
     // Source handlers retain their sequence, authored holds and input gate.
     if(credits.index()==17){playback.cdPlaying=false;playback.cdIdleOrPaused=true;}
     playback.startPressed=credits.waitingStart();std::vector<CreditsEvent> actions;
     check(credits.tick(playback,actions,error),"late original credits worker");
     for(const auto& action:actions){if(action.kind==kCreditsStartTrack){playback.cdPlaying=true;playback.cdIdleOrPaused=false;}
      if(action.kind==kCreditsInstallScene){check(action.value==expectedScene,"late credits preserves scene order");++expectedScene;}
      if(action.kind==kCreditsApplicationComplete)++completed;}
     return true;
    },done,suspended,error)&&done<=4,"late credits yields host service");
   }
   // Alternate source branch jumps from scene8 to the final input gate.
   check(credits.finished()&&completed==1&&expectedScene==(alternate?9u:10u),"late original credits reaches its authored ending once");
  }
 }
 std::printf("host scheduling: %u checks, 0 failures\n",checks);return 0;
}
