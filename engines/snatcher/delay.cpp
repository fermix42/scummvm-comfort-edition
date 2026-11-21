/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "delay.h"
namespace Snatcher {
void NarrativeDelay::start(uint16_t target){_state=NarrativeDelaySnapshot();_state.target=target;_state.active=true;}
bool NarrativeDelay::tick(bool paused){
 if(_state.active&&!paused){++_state.elapsed;if(uint16_t(_state.elapsed)==_state.target){_state.active=false;_state.finished=true;}}
 return _state.finished;
}
bool NarrativeDelay::restore(const NarrativeDelaySnapshot& state,std::string& error){
 error.clear();uint32_t duration=state.target?state.target:65536;
 bool valid=state.active?!state.finished&&state.elapsed<duration:
  state.finished?state.elapsed==duration:state.target==0&&state.elapsed==0;
 if(!valid){error="inconsistent narrative delay snapshot";return false;}
 _state=state;return true;
}
}
