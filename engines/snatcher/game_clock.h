/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_GAME_CLOCK_H
#define SNATCHER_GAME_CLOCK_H
#include <cstdint>
#include <string>
namespace Snatcher {
// Game update cadence supplied by a recovered owner. This is a rational time
// accumulator, not a device clock; resource-blocked coordinators do not tick.
struct GameClockState {uint32_t numerator,denominator;uint64_t fraction,ticks;GameClockState():numerator(0),denominator(0),fraction(0),ticks(0){} };
class GameClock {
public:
 bool configure(uint32_t numerator,uint32_t denominator,std::string& error);
 bool advance(uint32_t elapsedMilliseconds,bool paused,uint32_t& updates,std::string& error);
 // Late host time is suspended simulation time, not a debt of authored work.
 // Audio/CD positions remain owned by playback. Commit each logical update
 // before its worker, and retire the batch if that worker resets the clock.
 template<class Worker> bool advanceHost(uint32_t milliseconds,uint32_t limit,Worker worker,uint32_t& completed,uint64_t& suspended,std::string& error){
  completed=0;uint32_t due=0;if(!prepareHost(milliseconds,limit,due,suspended,error))return false;
  const uint64_t revision=_revision;
  for(uint32_t i=0;i<due&&revision==_revision;++i){
   ++_state.ticks;++completed;if(!worker(uint16_t(_state.ticks)))return false;
  }
  return true;
 }
 GameClockState snapshot()const{return _state;}
 bool restore(const GameClockState& state,std::string& error);
private:
 bool prepareHost(uint32_t milliseconds,uint32_t limit,uint32_t& due,uint64_t& suspended,std::string& error);
 GameClockState _state;uint64_t _revision=0;
};
}
#endif
