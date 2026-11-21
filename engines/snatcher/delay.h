/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_DELAY_H
#define SNATCHER_DELAY_H
#include <cstdint>
#include <string>
namespace Snatcher {
// Semantic wait state only. The narrative scheduler owns its event token and
// the engine owns coordinator cadence and global pause. No wall clock is read.
struct NarrativeDelaySnapshot {
 uint16_t target;uint32_t elapsed;bool active,finished;
 NarrativeDelaySnapshot():target(0),elapsed(0),active(false),finished(false){}
};
// Source contract102AC: increment16-bit count, compare target for equality,
// clear count and advance only on equality. Target0 therefore takes65536 ticks.
class NarrativeDelay {
public:
 NarrativeDelay(){}
 void start(uint16_t target);
 bool tick(bool paused);
 // Owner reset discards this local wait; it does not emit a cancellation or
 // complete a narrative event whose cancellation disposition is unspecified.
 void reset(){_state=NarrativeDelaySnapshot();}
 bool active()const{return _state.active;}
 bool finished()const{return _state.finished;}
 uint16_t target()const{return _state.target;}
 uint32_t elapsed()const{return _state.elapsed;}
 uint16_t counter()const{return _state.active?uint16_t(_state.elapsed):0;}
 NarrativeDelaySnapshot snapshot()const{return _state;}
 bool restore(const NarrativeDelaySnapshot& state,std::string& error);
private:
 NarrativeDelaySnapshot _state;
};
}
#endif
