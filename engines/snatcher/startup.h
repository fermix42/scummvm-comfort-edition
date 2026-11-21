/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_STARTUP_H
#define SNATCHER_STARTUP_H
#include <cstdint>
#include <string>
namespace Snatcher {
enum StartupPresentationState {kStartupPresentationNotStarted,kStartupPresentationRunning,kStartupPresentationComplete};
enum StartupPhase {kStartupDormant,kStartupWaitingPresentation,kStartupWaitingPostIntroReset,kStartupWaitingFreshInitialization,kStartupStoryReady};
enum StartupRequestKind {kStartupNoRequest,kStartupPumpPresentation,kStartupPostIntroReset,kStartupInitializeFreshStory};
struct StartupRequest {
 StartupRequestKind kind;uint64_t generation,id;
 StartupRequest():kind(kStartupNoRequest),generation(0),id(0){}
};
// Semantic ownership recovered from the source story coordinator. The separate
// game-specific title/intro presenter must supply its real state; no elapsed
// time, captured frame sequence or scenario injection completes it here.
class StartupCoordinator {
public:
 StartupCoordinator();
 bool startFresh(std::string& error);
 bool cancel(std::string& error);
 void pause(bool paused){_paused=paused;}
 bool poll(StartupPresentationState presentation,StartupRequest& request,std::string& error);
 bool complete(const StartupRequest& request,std::string& error);
 StartupPhase phase()const{return _phase;}
 bool pending()const{return _pending.kind!=kStartupNoRequest;}
private:
 StartupPhase _phase;uint64_t _generation,_nextId;bool _paused;StartupRequest _pending;
 bool issue(StartupRequestKind kind,StartupRequest& request,std::string& error);
};
}
#endif
