/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../animation.h"
#include <cstdio>
namespace {
int checks=0,failures=0;
void check(bool v,const char *s){++checks;if(!v){++failures;std::fprintf(stderr,"FAIL animation: %s\n",s);}}
}
namespace {
struct Owner {unsigned calls,randomCalls;uint16_t lower,span;Owner():calls(0),randomCalls(0),lower(0),span(0){} };
Snatcher::AnimationCommandResult handle(void *p,const Snatcher::AnimationCommand& c,std::string&){Owner& owner=*static_cast<Owner*>(p);if(c.opcode!=0xa2)return Snatcher::kAnimationCommandUnsupported;return ++owner.calls==1?Snatcher::kAnimationCommandBlocked:Snatcher::kAnimationCommandAdvance;}
Snatcher::AnimationCommandResult retireSelf(void *p,const Snatcher::AnimationCommand& c,std::string&){Owner& owner=*static_cast<Owner*>(p);++owner.calls;return c.opcode==0x95?Snatcher::kAnimationCommandRetireSelf:Snatcher::kAnimationCommandUnsupported;}
bool randomDuration(void *p,uint16_t lower,uint16_t span,uint16_t& out,std::string&){Owner& owner=*static_cast<Owner*>(p);++owner.randomCalls;owner.lower=lower;owner.span=span;out=uint16_t(lower+(uint32_t(0x8000)*span>>16));return true;}
}
int runAnimationTests(){
 using namespace Snatcher;checks=failures=0;std::string e;AnimationPlayer a;
 const uint8_t basic[]={1,0,0,2,2,0,0,1,0x80,0,0,0};
 check(a.start(basic,sizeof(basic),0,5,-3,e),"start");check(a.tick(e)==kAnimationFrame&&a.state().frameIndex==1&&a.state().remaining==2,"first update");
 a.pause(true);check(a.tick(e)==kAnimationPaused&&a.state().remaining==2,"pause does not advance");a.pause(false);
 check(a.tick(e)==kAnimationFrame&&a.state().frameIndex==1&&a.state().remaining==1,"duration hold");
 check(a.tick(e)==kAnimationFrame&&a.state().frameIndex==2,"duration boundary");check(a.tick(e)==kAnimationFinished&&!a.state().active&&a.state().x16==0,"terminal clears actor");check(a.tick(e)==kAnimationFinished,"terminal idempotent");
 const uint8_t repeat[]={3,0,0,1,0x82,2,0,0,4,0,0,1,0x80,0,0,0};
 check(a.start(repeat,sizeof(repeat),0,0,0,e),"repeat start");for(unsigned i=0;i<3;++i)check(a.tick(e)==kAnimationFrame&&a.state().frameIndex==3,"bounded repeat count");check(a.tick(e)==kAnimationFrame&&a.state().frameIndex==4,"repeat exits at equality");check(a.tick(e)==kAnimationFinished,"repeat terminal");
 const uint8_t jump[]={0x81,2,0,0,0x83,0,0,0,7,0,0,1,0x80,0,0,0};
 check(a.start(jump,sizeof(jump),0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.state().frameIndex==7,"jump uses record identity");
 const uint8_t cycle[]={0x81,0,0,0};check(a.start(cycle,sizeof(cycle),0,0,0,e)&&a.tick(e)==kAnimationError&&!e.empty(),"immediate cycle explicit error");std::string prior=e;check(a.tick(e)==kAnimationError&&e==prior,"error cannot silently advance");
 const uint8_t unknown[]={0x83,0,0,0,1,0,0,1};check(a.start(unknown,sizeof(unknown),0,0,0,e)&&a.tick(e)==kAnimationUnsupported,"random control unsupported");check(a.tick(e)==kAnimationUnsupported&&a.state().recordIndex==0,"unsupported remains latched");
 const uint8_t random[]={1,1,0,3};check(a.start(random,sizeof(random),0,0,0,e)&&a.tick(e)==kAnimationUnsupported,"random normal duration unsupported");
 const uint8_t hide[]={0x97,0,0,2,8,0,0,1,0x80,0,0,0};check(a.start(hide,sizeof(hide),0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.state().hidden,"timed invisible");check(a.tick(e)==kAnimationFrame&&a.state().hidden,"invisibility retained");check(a.tick(e)==kAnimationFrame&&!a.state().hidden&&a.state().frameIndex==8,"normal restores visibility");
 const uint8_t motion[]={0x86,128,0,1,0x87,0,255,255,1,0,0,2,0x84,0,0,10,2,0,0,1,0x80,0,0,0};
 check(a.start(motion,sizeof(motion),0,0,2,e)&&a.tick(e)==kAnimationFrame&&a.state().x16==98304&&a.state().y16==65536,"fixed velocity and update");check(a.tick(e)==kAnimationFrame&&a.state().x16==196608&&a.state().y16==0,"velocity each logical update");check(a.tick(e)==kAnimationFrame&&a.state().x16==753664,"integer setter then velocity");
 const uint8_t styles[]={0xa1,0,0x12,0x34,0xa7,0x82,0,0,1,0,0,1};check(a.start(styles,sizeof(styles),0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.state().styleWord==0x1234&&a.state().paletteOverride==0x82,"raw style ownership");
 for(size_t n=0;n<4;++n)check(!a.start(basic,n,0,0,0,e),"short sequence rejected");check(a.start(basic,4,0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.tick(e)==kAnimationFrame&&a.tick(e)==kAnimationError,"truncated next record");
 const uint8_t zero[]={1,0,0,0,2,0,0,1};check(a.start(zero,sizeof(zero),0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.tick(e)==kAnimationFrame&&a.state().frameIndex==2,"zero duration still yields one update");
 Owner retired;const uint8_t selfRetire[]={0x8c,0,0,12,0x98,0,0,0,0x95,5,0,0,1,1,0,3};
 check(a.start(selfRetire,sizeof(selfRetire),0,0,0,e)&&a.tick(e,retireSelf,&retired,randomDuration)==kAnimationSelfRetired,"typed self retirement returns after handler without following authored records");
 check(!a.state().active&&a.takeCompletionMarkers()==0&&retired.calls==1&&retired.randomCalls==0,"self retirement clears actor-owned markers and does not invent random/completion activity");
 std::vector<ScrollRequest> priorScroll=a.takeScrollRequests();check(priorScroll.size()==1&&priorScroll[0].value==12,"self retirement retains already-produced independent scroll request");check(a.tick(e,retireSelf,&retired)==kAnimationFinished&&retired.calls==1,"self retirement is inactive on later updates");
 a.cancel();check(!a.state().active&&a.tick(e)==kAnimationFinished,"cancel clears ownership");
 const uint8_t scroll[]={0x8c,0,0,12,0x8d,7,255,216,0x8e,1,255,128,0x8f,0,0,128,1,0,0,2};
 check(a.start(scroll,sizeof(scroll),0,0,0,e)&&a.tick(e)==kAnimationFrame,"scroll controls emit before frame");std::vector<ScrollRequest> requests=a.takeScrollRequests();
 check(requests.size()==4&&requests[0].kind==ScrollRequest::kPosition&&requests[0].plane==0&&requests[0].axis==kScrollX&&requests[0].value==12,"scroll position request");
 check(requests[1].plane==1&&requests[1].axis==kScrollY&&requests[1].value==-40,"nonzero plane selector and signed position");check(requests[2].kind==ScrollRequest::kVelocity&&requests[2].plane==1&&requests[2].axis==kScrollX&&requests[2].value==-128&&requests[3].value==128,"signed8.8 velocity request");
 check(a.takeScrollRequests().empty()&&a.tick(e)==kAnimationFrame,"requests drain exactly once");
 const uint8_t scrollEnd[]={0x8d,0,255,192,0x80,0,0,0};check(a.start(scrollEnd,sizeof(scrollEnd),0,0,0,e)&&a.tick(e)==kAnimationFinished&&a.takeScrollRequests().size()==1,"terminal preserves prior scene requests");
 check(a.start(scroll,sizeof(scroll),0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.tick(e)==kAnimationError&&!e.empty(),"unconsumed requests fail explicitly");
 const uint8_t completion[]={0x98,0,0,0,0x9e,0,0,0,1,0,0,1,0x80,0,0,0};check(a.start(completion,sizeof(completion),0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.takeCompletionMarkers()==1&&a.takeCompletionMarkers()==0,"completion marker ownership and verified advance control");
 const uint8_t terminalMarker[]={0x98,0,0,0,0x80,0,0,0};check(a.start(terminalMarker,sizeof(terminalMarker),0,0,0,e)&&a.tick(e)==kAnimationFinished&&a.takeCompletionMarkers()==1,"terminal preserves completion marker");
 const uint8_t revealWait[]={7,0,0,1,0x97,0,0,1,0xa2,0,0,0,8,0,0,2};Owner revealOwner;
 check(a.start(revealWait,sizeof(revealWait),0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.state().frameIndex==7,"wait visibility retains authored visible frame");
 check(a.tick(e)==kAnimationFrame&&a.state().hidden&&a.state().frameIndex==7,"97 hides without replacing cached frame");
 check(a.tick(e,handle,&revealOwner)==kAnimationFrame&&!a.state().hidden&&a.state().frameIndex==7&&a.state().recordIndex==2&&revealOwner.calls==1,"blocked control after97 reveals cached frame on the same update");
 check(a.tick(e,handle,&revealOwner)==kAnimationFrame&&!a.state().hidden&&a.state().frameIndex==8&&revealOwner.calls==2,"wait completion enters next frame without a fabricated delay");
 const uint8_t blocked[]={0xa2,0,0,0,1,0,0,2,0x98,0,0,0,0x80,0,0,0};Owner owner;
 check(a.start(blocked,sizeof(blocked),0,0,0,e)&&a.tick(e,handle,&owner)==kAnimationFrame&&a.state().recordIndex==0&&owner.calls==1,"external wait retains exact record");
 check(a.tick(e,handle,&owner)==kAnimationFrame&&a.state().recordIndex==1&&owner.calls==2&&a.state().remaining==2,"external wait resumes through specified completion");
 check(a.tick(e,handle,&owner)==kAnimationFrame&&a.tick(e,handle,&owner)==kAnimationFinished&&a.takeCompletionMarkers()==1,"resumed actor emits original marker once");
 const uint8_t randomOwner[]={1,1,5,5,2,7,2,6,0x80,0,0,0};Owner rng;
 check(a.start(randomOwner,sizeof(randomOwner),0,0,0,e)&&a.tick(e,nullptr,&rng,randomDuration)==kAnimationFrame&&a.state().remaining==5&&rng.randomCalls==1&&rng.span==0,"zero-span duration still consumes shared draw");
 a.pause(true);check(a.tick(e,nullptr,&rng,randomDuration)==kAnimationPaused&&rng.randomCalls==1,"pause consumes no shared random draw");a.pause(false);
 for(unsigned i=0;i<4;++i)check(a.tick(e,nullptr,&rng,randomDuration)==kAnimationFrame&&rng.randomCalls==1,"duration hold consumes no extra draw");
 check(a.tick(e,nullptr,&rng,randomDuration)==kAnimationFrame&&a.state().frameIndex==2&&a.state().remaining==4&&rng.randomCalls==2&&rng.lower==2&&rng.span==4,"one random duration draw per entered record");
 const uint8_t restart[]={0,0,0,1,0x82,2,0,0,1,0,0,1,0x80,0,0,0};check(a.start(restart,sizeof(restart),0,5,-3,e)&&a.tick(e)==kAnimationFrame&&a.tick(e)==kAnimationFrame,"loop owner before restart");a.setStyleWord(0x304);a.setPaletteOverride(0x82);a.setVelocity(kScrollX,65536);check(a.restartSequence(e)&&a.state().recordIndex==0xffff&&a.state().remaining==0xffff&&a.state().styleWord==0x304&&a.state().paletteOverride==0x82&&a.state().x16==5*65536,"restart retains frame style palette motion ownership");check(a.tick(e)==kAnimationFrame&&a.tick(e)==kAnimationFrame&&a.tick(e)==kAnimationFrame&&a.state().frameIndex==1,"restart preserves loop-repeat history");check(a.tick(e)==kAnimationFinished&&!a.restartSequence(e),"inactive animation cannot fabricate restart ownership");Owner blockedOwner;check(a.start(blocked,sizeof(blocked),0,0,0,e)&&a.tick(e,handle,&blockedOwner)==kAnimationFrame&&!a.restartSequence(e),"restart cannot discard unresolved external wait");
 AnimationClockCapture clock;check(!applyAnimationClockCapture(0,clock,e)&&!clock.captureKnown,"clock control cannot invent unknown source clock");clock.clockKnown=true;clock.sourceTicks=65530;check(!applyAnimationClockCapture(1,clock,e)&&!clock.captureKnown,"elapsed clock requires actual prior capture");check(applyAnimationClockCapture(0,clock,e)&&clock.captured==65530,"zero argument captures exact source word");clock.sourceTicks=3;check(applyAnimationClockCapture(255,clock,e)&&clock.captured==9,"every nonzero argument uses wrapping word elapsed");clock.sourceTicks=25;check(applyAnimationClockCapture(1,clock,e)&&clock.captured==16,"elapsed operation replaces shared capture instead of retaining initial timestamp");
 const uint8_t stochastic[]={0,0,0,1,0x83,4,8,0,1,0,0,1,0x80,0,0,0};Owner repeatOwner;check(a.start(stochastic,sizeof(stochastic),0,0,0,e)&&a.tick(e,handle,&repeatOwner,randomDuration)==kAnimationFrame,"random repeat frame setup");for(unsigned i=0;i<4;++i)check(a.tick(e,handle,&repeatOwner,randomDuration)==kAnimationFrame&&a.state().frameIndex==0,"source stochastic repeat keeps timed loop");check(a.tick(e,handle,&repeatOwner,randomDuration)==kAnimationFrame&&a.state().frameIndex==1&&repeatOwner.randomCalls==1&&repeatOwner.lower==2&&repeatOwner.span==4,"source stochastic repeat seed lower2 and byte-difference span once");check(a.start(stochastic,sizeof(stochastic),0,0,0,e)&&a.tick(e)==kAnimationFrame&&a.tick(e)==kAnimationUnsupported,"random repeat without shared owner rejects");
 std::printf("animation: %d checks, %d failures\n",checks,failures);return failures;
}
