/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../startup.h"
#include <cstdio>
#include <stdexcept>
namespace {unsigned checks;void check(bool value,const char *reason){++checks;if(!value)throw std::runtime_error(reason);}}
int runStartupTests(){using namespace Snatcher;StartupCoordinator startup;StartupRequest request,reset,initialize;std::string error;
 check(startup.phase()==kStartupDormant&&startup.poll(kStartupPresentationComplete,request,error)&&request.kind==kStartupNoRequest,"unstarted coordinator cannot inject a story");
 check(startup.startFresh(error)&&startup.phase()==kStartupWaitingPresentation,"fresh request waits on original presenter");
 for(unsigned i=0;i<100;++i){check(startup.poll(kStartupPresentationNotStarted,request,error)&&request.kind==kStartupPumpPresentation&&request.id==0&&!startup.pending(),"not-started status pumps actual owner without completion");check(!startup.complete(request,error),"presentation pump is not a completed post-intro reset");}
 for(unsigned i=0;i<1000;++i)check(startup.poll(kStartupPresentationRunning,request,error)&&request.kind==kStartupNoRequest&&startup.phase()==kStartupWaitingPresentation,"long-running original intro never becomes a synthetic timeout");
 check(!startup.poll(StartupPresentationState(99),request,error)&&startup.phase()==kStartupWaitingPresentation,"unknown source status rejected");
 startup.pause(true);check(startup.poll(kStartupPresentationComplete,request,error)&&request.kind==kStartupNoRequest,"pause cannot advance to reset");startup.pause(false);
 check(startup.poll(kStartupPresentationComplete,reset,error)&&reset.kind==kStartupPostIntroReset&&reset.id&&startup.pending()&&startup.phase()==kStartupWaitingPostIntroReset,"original terminal state issues owned reset");
 check(startup.poll(kStartupPresentationNotStarted,request,error)&&request.id==reset.id&&request.generation==reset.generation&&request.kind==reset.kind,"pending reset is stable without reissuing work");
 request=reset;++request.id;check(!startup.complete(request,error)&&startup.pending(),"wrong request identity cannot complete reset");request=reset;request.kind=kStartupInitializeFreshStory;check(!startup.complete(request,error)&&startup.pending(),"initialization cannot bypass reset owner");
 startup.pause(true);check(!startup.complete(reset,error)&&startup.pending(),"pause preserves actual pending ownership");startup.pause(false);
 check(startup.complete(reset,error)&&startup.phase()==kStartupWaitingFreshInitialization&&!startup.pending(),"real reset completion permits initialization phase");check(!startup.complete(reset,error),"duplicate reset completion rejected");
 check(startup.poll(kStartupPresentationComplete,initialize,error)&&initialize.kind==kStartupInitializeFreshStory&&initialize.id>reset.id,"fresh story initialization is separate owned operation");
 check(startup.cancel(error)&&startup.phase()==kStartupDormant&&!startup.pending()&&!startup.complete(initialize,error),"cancellation invalidates initialization");
 check(startup.startFresh(error)&&startup.poll(kStartupPresentationComplete,request,error)&&request.generation>initialize.generation&&request.id>initialize.id,"new game never reuses old ownership");
 check(!startup.complete(reset,error)&&!startup.complete(initialize,error),"old callbacks never advance new game");
 check(startup.complete(request,error)&&startup.poll(kStartupPresentationComplete,initialize,error)&&startup.complete(initialize,error)&&startup.phase()==kStartupStoryReady,"actual reset and actual fresh initialization reach scenario-ready boundary");
 for(unsigned i=0;i<100;++i)check(startup.poll(kStartupPresentationComplete,request,error)&&request.kind==kStartupNoRequest,"ready state never repeats original initialization");check(!startup.complete(initialize,error),"once-only initialization completion");
 std::printf("startup ownership contracts: %u checks, 0 failures\n",checks);return 0;
}

int main(){try{return runStartupTests();}catch(const std::exception& e){std::fprintf(stderr,"startup test failure: %s\n",e.what());return 1;}}
