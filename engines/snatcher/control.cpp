#include "control.h"
namespace Snatcher {
namespace {bool fail(std::string &e,const char*s){e=s;return false;}}
ControlState::ControlState(uint16_t cursor):_current(cursor),_pending(0xffff),_depth(0),_activationMode(false){}
bool validateControlSnapshot(const ControlSnapshot& state,std::string& error){
 error.clear();if(state.available.size()>31||state.selected.size()>15||state.activations.size()>8||state.history.size()>3||state.current==0xffff)return fail(error,"control snapshot capacity/current cursor");
 uint16_t previous=0;for(uint16_t depth:state.activations){if(depth<=previous||depth>state.selected.size())return fail(error,"control snapshot activation ownership");previous=depth;}
 for(uint16_t cursor:state.history)if(cursor==0xffff)return fail(error,"control snapshot reserved history cursor");return true;
}
bool ControlState::snapshot(ControlSnapshot& state,std::string& error)const{
 error.clear();if(!atBoundary(error))return false;ControlSnapshot next;next.available=_available;next.selected=_selected;next.activations=_activations;next.history=_history;next.current=_current;next.pending=_pending;next.activationMode=_activationMode;
 if(!validateControlSnapshot(next,error))return false;state=next;return true;
}
bool ControlState::restore(const ControlSnapshot& state,std::string& error){
 if(!validateControlSnapshot(state,error))return false;_available=state.available;_selected=state.selected;_activations=state.activations;_history=state.history;_current=state.current;_pending=state.pending;_activationMode=state.activationMode;_depth=0;_choiceScopes.clear();return true;
}
unsigned ControlState::activationFloor()const{return _activations.empty()?0:_activations.back();}
bool ControlState::gate()const{
 if(!_available.empty()||hasPendingContinuation())return false;
 if(!_activationMode)return _depth==_selected.size();
 return _activations.empty()||_activations.back()<_depth;
}
ChoiceDecision ControlState::beginChoice(uint16_t ref,std::string &e){
 e.clear();if(hasPendingContinuation())return kChoicePending;
 if(_depth>_selected.size()){fail(e,"Choice traversal exceeds selected path");return kChoiceError;}
 if(_depth==_selected.size()){
  if(_available.size()>=31){fail(e,"Available choice capacity exceeded");return kChoiceError;}
  _available.push_back(ref);return kChoiceOffered;
 }
 if(_selected[_depth]!=ref)return kChoiceSkipped;
 _choiceScopes.push_back(_depth);++_depth;return kChoiceEntered;
}
bool ControlState::endChoice(std::string&e){
 e.clear();if(_choiceScopes.empty()||_depth!=_choiceScopes.back()+1)return fail(e,"Choice body scope mismatch");
 if(_depth==_selected.size()&&_available.empty())_activationMode=true;
 _depth=_choiceScopes.back();_choiceScopes.pop_back();return true;
}
bool ControlState::atBoundary(std::string&e)const{
 if(_depth||!_choiceScopes.empty())return fail(e,"Response requires a completed traversal");return true;
}
bool ControlState::respond(unsigned choice,std::string&e){
 e.clear();if(!atBoundary(e))return false;
 if(!choice||choice>_available.size())return fail(e,"Choice ordinal is outside available choices");
 if(_selected.size()>=15)return fail(e,"Selected choice path capacity exceeded");
 _selected.push_back(_available[choice-1]);_available.clear();return true;
}
bool ControlState::cancel(std::string&e){
 e.clear();if(!atBoundary(e))return false;unsigned floor=activationFloor();
 if(floor>_selected.size())return fail(e,"Activation floor exceeds selected path");
 if(_selected.size()!=floor)_selected.pop_back();_available.clear();return true;
}
bool ControlState::emptyPass(std::string&e){
 e.clear();if(!atBoundary(e))return false;unsigned floor=activationFloor();
 if(floor>_selected.size())return fail(e,"Activation floor exceeds selected path");
 _selected.resize(floor);_available.clear();return true;
}
ActivationDecision ControlState::enterActivation(std::string&e){
 e.clear();if(!gate()){fail(e,"Activation is gated");return kActivationError;}
 // Source17048..1704E returns before both the push and inline body when
 // the previous activation floor is equal to or above the current depth.
 if(activationFloor()>=_depth)return kActivationSkipped;
 if(_activations.size()>=8){fail(e,"Activation depth capacity exceeded");return kActivationError;}
 _activations.push_back(uint16_t(_depth));return kActivationEntered;
}
bool ControlState::leaveActivation(std::string&e){
 e.clear();if(!gate())return fail(e,"Activation removal is gated");
 if(_activations.empty())return fail(e,"Activation stack is empty");_activations.pop_back();return true;
}
bool ControlState::requestContinuation(uint16_t cursor,std::string&e){
 e.clear();if(cursor==0xffff)return fail(e,"Reserved pending continuation sentinel");
 if(hasPendingContinuation())return fail(e,"A continuation is already pending");_pending=cursor;return true;
}
bool ControlState::applyContinuation(std::string&e){
 e.clear();if(!atBoundary(e))return false;
 if(!hasPendingContinuation())return fail(e,"No continuation is pending");
 if(_history.size()==3)_history.erase(_history.begin());_history.push_back(_current);
 _current=_pending;_pending=0xffff;return true;
}
bool ControlState::replaceContinuation(uint16_t cursor,std::string&e){e.clear();if(cursor==0xffff)return fail(e,"Reserved pending continuation sentinel");_pending=cursor;return true;}
bool ControlState::beginPass(std::string&e){
 e.clear();if(!atBoundary(e))return false;_activationMode=false;return true;
}
}
