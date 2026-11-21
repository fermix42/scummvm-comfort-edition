#include "../control.h"
#include <stdexcept>
#include <string>
namespace {
void check(bool ok,const char*what){if(!ok)throw std::runtime_error(std::string("control test failed: ")+what);}
void chooseNext(Snatcher::ControlState &s,uint16_t ref){
 std::string e;check(s.beginPass(e),"begin pass");
 const std::vector<uint16_t> path=s.selectedPath();
 for(size_t i=0;i<path.size();++i)check(s.beginChoice(path[i],e)==Snatcher::kChoiceEntered,"reenter path");
 check(s.beginChoice(ref,e)==Snatcher::kChoiceOffered,"offer next");
 for(size_t i=0;i<path.size();++i)check(s.endChoice(e),"unwind choice");
 check(s.respond(1,e),"select offered reference");
}
}
int runControlTests(){
 using namespace Snatcher;std::string e;ControlState s;
 check(s.gate(),"initial frontier gate");check(s.beginChoice(0x1207,e)==kChoiceOffered,"offer identity");check(!s.gate(),"choices gate");check(!s.respond(0,e)&&!e.empty(),"reject zero ordinal");check(s.respond(1,e)&&s.selectedPath()[0]==0x1207,"ordinal resolves text identity");check(!s.gate(),"depth below selected frontier");check(s.beginChoice(0x1208,e)==kChoiceSkipped,"mismatched identity skips");check(!s.endChoice(e),"unmatched end rejected");check(s.beginChoice(0x1207,e)==kChoiceEntered&&s.depth()==1,"matching body enters");check(!s.cancel(e),"cancel during body rejected");check(s.gate(),"matched frontier gate");check(s.endChoice(e)&&s.activationMode(),"empty body activates after dispatch");check(s.beginPass(e)&&!s.activationMode(),"new pass resets activation mode");
 check(s.beginChoice(0x1207,e)==kChoiceEntered,"enter activation fixture");check(s.enterActivation(e)&&s.activationDepths().back()==1,"push depth");check(s.endChoice(e),"close activation fixture");check(s.cancel(e)&&s.selectedPath().size()==1,"cancel keeps floor");check(s.emptyPass(e)&&s.selectedPath().size()==1,"empty pass keeps floor");
 ControlState h(7);check(!h.applyContinuation(e),"missing pending rejected");check(!h.requestContinuation(0xffff,e),"sentinel rejected");for(unsigned i=0;i<5;++i){check(h.requestContinuation(uint16_t(20+i),e),"request continuation");check(!h.gate(),"pending gates");check(h.beginChoice(9,e)==kChoicePending,"pending choice does not execute");check(!h.requestContinuation(90,e),"second pending rejected");check(h.applyContinuation(e),"apply continuation");}
 check(h.currentCursor()==24&&!h.hasPendingContinuation(),"handoff clears pending");check(h.cursorHistory()==std::vector<uint16_t>({21,22,23}),"history oldest eviction");
 ControlState capacity;for(unsigned i=0;i<31;++i)check(capacity.beginChoice(uint16_t(i),e)==kChoiceOffered,"choice capacity");check(capacity.beginChoice(99,e)==kChoiceError&&!e.empty()&&capacity.availableChoices().size()==31,"choice overflow explicit");check(capacity.cancel(e)&&capacity.availableChoices().empty(),"cancel clears offers");
 ControlState path;for(unsigned i=0;i<15;++i)chooseNext(path,uint16_t(100+i));check(path.selectedPath().size()==15,"path capacity");check(path.beginPass(e),"full path pass");for(unsigned i=0;i<15;++i)check(path.beginChoice(uint16_t(100+i),e)==kChoiceEntered,"full path descent");check(path.beginChoice(999,e)==kChoiceOffered,"offer beyond full path");for(unsigned i=0;i<15;++i)check(path.endChoice(e),"full path unwind");check(!path.respond(1,e)&&path.selectedPath().size()==15&&!e.empty(),"path overflow explicit");check(path.cancel(e)&&path.selectedPath().size()==14,"cancel removes only one");check(path.emptyPass(e)&&path.selectedPath().empty(),"empty pass truncates to zero");
 ControlState rootActivation;check(rootActivation.enterActivation(e)==kActivationSkipped&&e.empty()&&rootActivation.activationDepths().empty(),"source root activation skips without pushing zero");ControlState reused;check(reused.beginChoice(7,e)==kChoiceOffered&&reused.respond(1,e)&&reused.beginPass(e)&&reused.beginChoice(7,e)==kChoiceEntered&&reused.enterActivation(e)==kActivationEntered&&reused.endChoice(e),"source activation first eligible body");check(reused.beginPass(e)&&reused.beginChoice(7,e)==kChoiceEntered&&reused.enterActivation(e)==kActivationSkipped&&e.empty()&&reused.activationDepths().size()==1&&reused.endChoice(e),"source equal activation floor skips without duplicate");
 ControlState activation;
 for(unsigned level=1;level<=9;++level){chooseNext(activation,uint16_t(200+level));check(activation.beginPass(e),"activation capacity pass");for(unsigned i=1;i<=level;++i)check(activation.beginChoice(uint16_t(200+i),e)==kChoiceEntered,"activation path descent");if(level<=8)check(activation.enterActivation(e),"activation capacity push");else check(!activation.enterActivation(e)&&!e.empty()&&activation.activationDepths().size()==8,"activation overflow explicit");for(unsigned i=0;i<level;++i)check(activation.endChoice(e),"activation path unwind");}
 // Original SP01 descriptors at offsets70 and76 identify text refs8 and285.
 // This fixture proves only their nested path contract, not story reachability.
 ControlState route;chooseNext(route,8);chooseNext(route,285);check(route.beginPass(e),"route pass");check(route.beginChoice(8,e)==kChoiceEntered,"SP01 outer reference");check(route.beginChoice(285,e)==kChoiceEntered&&route.gate(),"SP01 inner reference admits body");check(route.endChoice(e)&&route.endChoice(e),"route unwind");return 0;
}
