/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_clock.h"
#include <limits>
namespace Snatcher {
namespace {bool valid(const GameClockState& s){return s.numerator&&s.denominator&&s.numerator<=1000000000u&&s.denominator<=1000000000u&&uint64_t(s.numerator)<=uint64_t(s.denominator)*1000&&s.fraction<uint64_t(s.denominator)*1000;}}
bool GameClock::configure(uint32_t n,uint32_t d,std::string& error){GameClockState next;next.numerator=n;next.denominator=d;return restore(next,error);}
bool GameClock::restore(const GameClockState& state,std::string& error){error.clear();if(!valid(state)){error="game clock frequency/fraction invalid";return false;}_state=state;++_revision;return true;}
bool GameClock::prepareHost(uint32_t ms,uint32_t limit,uint32_t& updates,uint64_t& suspended,std::string& error){
 error.clear();updates=0;suspended=0;
 if(!valid(_state)||!limit||limit>64){error="bounded game clock cadence/budget invalid";return false;}
 const uint64_t scaled=uint64_t(ms)*_state.numerator;
 if(std::numeric_limits<uint64_t>::max()-_state.fraction<scaled){error="bounded game clock accumulator overflow";return false;}
 const uint64_t total=scaled+_state.fraction,denominator=uint64_t(_state.denominator)*1000,due=total/denominator;
 const uint32_t admitted=uint32_t(due>limit?limit:due);
 if(std::numeric_limits<uint64_t>::max()-_state.ticks<admitted){error="bounded game clock update count overflow";return false;}
 _state.fraction=total%denominator;updates=admitted;suspended=due-admitted;return true;
}
bool GameClock::advance(uint32_t ms,bool paused,uint32_t& updates,std::string& error){
 error.clear();updates=0;if(!valid(_state)){error="game clock cadence not established";return false;}if(paused)return true;
 uint64_t scaled=uint64_t(ms)*_state.numerator;if(std::numeric_limits<uint64_t>::max()-_state.fraction<scaled){error="game clock accumulator overflow";return false;}
 uint64_t total=scaled+_state.fraction,denominator=uint64_t(_state.denominator)*1000,due=total/denominator;if(due>std::numeric_limits<uint32_t>::max()||std::numeric_limits<uint64_t>::max()-_state.ticks<due){error="game clock update count overflow";return false;}
 _state.fraction=total%denominator;_state.ticks+=due;updates=uint32_t(due);return true;
}
}
