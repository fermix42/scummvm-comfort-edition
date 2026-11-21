/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_CREDITS_H
#define SNATCHER_CREDITS_H
#include <cstdint>
#include <string>
#include <vector>
namespace Snatcher {
struct CreditsInput {
 bool cdPlaying,cdIdleOrPaused,clockKnown,ready,paused,startPressed;
 uint32_t relativeClock;
 CreditsInput():cdPlaying(false),cdIdleOrPaused(false),clockKnown(false),ready(false),paused(false),startPressed(false),relativeClock(0){}
};
enum CreditsEventKind {kCreditsStartTrack,kCreditsInstallScene,kCreditsPalette,kCreditsResetRelativeClock,kCreditsApplicationComplete};
struct CreditsEvent {CreditsEventKind kind;uint32_t value;CreditsEvent(CreditsEventKind k,uint32_t v=0):kind(k),value(v){}};
// Finite native application timeline; data records are not CPU instructions.
// The engine owns real transport, scene/transfer readiness and input edges.
class CreditsTimeline {
public:
 CreditsTimeline();
 bool start(const std::vector<uint8_t>& sourceBytes,bool originalSaveCountNonzero,std::string& error);
 bool tick(const CreditsInput& input,std::vector<CreditsEvent>& events,std::string& error);
 bool active()const{return _active;}
 bool finished()const{return _finished;}
 int index()const{return _index;}
 unsigned countdown()const{return _countdown;}
 bool waitingStart()const;
 uint64_t sourceStamp()const{return _sourceStamp;}
private:
 friend class EngineStateCodec;
 struct Record {uint32_t handler,deadline;};
 std::vector<Record> _records;
 bool _active,_finished,_requested,_alternate;
 int _index;unsigned _countdown;uint64_t _sourceStamp;
 bool tickOwned(const CreditsInput&,std::vector<CreditsEvent>&,std::string&);
};
}
#endif
