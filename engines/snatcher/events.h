/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_EVENTS_H
#define SNATCHER_EVENTS_H
#include <cstdint>
#include <deque>
#include <string>
#include <vector>
namespace Snatcher {
struct Cursor {uint16_t resource;uint32_t node;Cursor(uint16_t r=0,uint32_t n=0):resource(r),node(n){} bool operator==(const Cursor& b)const{return resource==b.resource&&node==b.node;}};
struct EventToken {uint64_t id;uint32_t generation;EventToken(uint64_t i=0,uint32_t g=0):id(i),generation(g){} bool operator==(const EventToken& b)const{return id==b.id&&generation==b.generation;}};
// Native producer bound accommodates the original 512-byte action buffer.
// Versioned persistence retains all 256 legal owners; legacy V1 keeps 64.
constexpr size_t kNativeEventCapacity=256;

enum EventKind {kDialogueEvent,kAudioEvent,kAnimationEvent,kCommandEvent};
struct SemanticEvent {EventToken token;Cursor owner;EventKind kind;bool completed,cancelled;};
struct EventSnapshot {uint32_t generation;uint64_t nextIdentity;bool paused;std::vector<SemanticEvent> pending;std::vector<EventToken> ready;EventSnapshot():generation(1),nextIdentity(1),paused(false){} };
struct EventRebind {EventToken previous,current;};
bool validateEventSnapshot(const EventSnapshot& state,std::string& error);
// Host completions are posted/polled on the engine thread. No callbacks execute
// game logic. Logical delivery order is completion-post order, not token order.
class EventScheduler {
public:
 EventScheduler():_generation(1),_next(1),_paused(false){}
 bool create(EventKind kind,Cursor owner,EventToken& token,std::string& error);
 bool complete(EventToken token,bool cancelled,std::string& error);
 bool take(SemanticEvent& event);
 bool awaiting(EventToken token)const;
 EventSnapshot snapshot()const;
 // Owners must cancel old work and reissue unresolved jobs using these tokens.
 // Restoration never completes an unresolved event or reconstructs owner jobs.
 bool restore(const EventSnapshot& state,std::vector<EventRebind>& rebind,std::string& error);
 bool transition(std::string& error);
 void pause(bool paused){_paused=paused;}bool paused()const{return _paused;}
 size_t pending()const{return _pending.size();}uint32_t generation()const{return _generation;}
private:
 uint32_t _generation;uint64_t _next;bool _paused;
 std::vector<SemanticEvent> _pending;std::deque<EventToken> _ready;
};
struct TraceEntry {uint64_t sequence;Cursor cursor;std::string category;uint32_t a,b;};
class SemanticTrace {
public:
 SemanticTrace():_enabled(false),_next(1),_dropped(0){}
 void enable(bool enabled){_enabled=enabled;}bool enabled()const{return _enabled;}
 void record(Cursor cursor,const char *category,uint32_t a=0,uint32_t b=0);
 const std::deque<TraceEntry>& entries()const{return _entries;}uint64_t dropped()const{return _dropped;}
private:bool _enabled;uint64_t _next,_dropped;std::deque<TraceEntry> _entries;
};
}
#endif
