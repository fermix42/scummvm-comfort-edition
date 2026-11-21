/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "events.h"
#include <limits>
namespace Snatcher {
bool validateEventSnapshot(const EventSnapshot& state,std::string& error){
 error.clear();if(!state.generation||!state.nextIdentity||state.pending.size()>kNativeEventCapacity||state.ready.size()>state.pending.size()){error="event snapshot header/capacity";return false;}
 for(size_t i=0;i<state.pending.size();++i){const SemanticEvent& event=state.pending[i];if(!event.token.id||event.token.id>=state.nextIdentity||event.token.generation!=state.generation||event.kind<kDialogueEvent||event.kind>kCommandEvent||(event.cancelled&&!event.completed)){error="event snapshot identity/kind/disposition";return false;}
  for(size_t j=0;j<i;++j)if(state.pending[j].token==event.token){error="duplicate event snapshot identity";return false;}
  size_t posted=0;for(const EventToken& token:state.ready)if(token==event.token)++posted;if(posted!=(event.completed?1u:0u)){error="event snapshot completion queue mismatch";return false;}
 }
 for(const EventToken& token:state.ready){bool found=false;for(const SemanticEvent& event:state.pending)if(event.token==token)found=true;if(!found){error="event snapshot orphan completion";return false;}}
 return true;
}
EventSnapshot EventScheduler::snapshot()const{EventSnapshot result;result.generation=_generation;result.nextIdentity=_next;result.paused=_paused;result.pending=_pending;result.ready.assign(_ready.begin(),_ready.end());return result;}
bool EventScheduler::restore(const EventSnapshot& state,std::vector<EventRebind>& rebind,std::string& error){
 rebind.clear();if(!validateEventSnapshot(state,error))return false;
 uint32_t generation=_generation>state.generation?_generation:state.generation;if(generation==std::numeric_limits<uint32_t>::max()){error="event restore generation exhausted";return false;}++generation;
 uint64_t next=_next>state.nextIdentity?_next:state.nextIdentity;if(std::numeric_limits<uint64_t>::max()-next<state.pending.size()){error="event restore identity exhausted";return false;}
 std::vector<SemanticEvent> pending=state.pending;std::deque<EventToken> ready;std::vector<EventRebind> mapped;
 for(SemanticEvent& event:pending){EventRebind pair;pair.previous=event.token;pair.current=EventToken(next++,generation);event.token=pair.current;mapped.push_back(pair);}
 for(const EventToken& posted:state.ready)for(const EventRebind& pair:mapped)if(pair.previous==posted)ready.push_back(pair.current);
 _generation=generation;_next=next;_paused=state.paused;_pending.swap(pending);_ready.swap(ready);rebind.swap(mapped);return true;
}
bool EventScheduler::create(EventKind kind,Cursor owner,EventToken& token,std::string& error){
 error.clear();token=EventToken();if(_pending.size()>=kNativeEventCapacity||_next==std::numeric_limits<uint64_t>::max()){error="event capacity/identity exhausted";return false;}
 SemanticEvent e;e.token=EventToken(_next++,_generation);e.owner=owner;e.kind=kind;e.completed=e.cancelled=false;_pending.push_back(e);token=e.token;return true;
}
bool EventScheduler::complete(EventToken token,bool cancelled,std::string& error){
 error.clear();for(size_t i=0;i<_pending.size();++i)if(_pending[i].token==token){if(_pending[i].completed){error="duplicate event completion";return false;}_pending[i].completed=true;_pending[i].cancelled=cancelled;_ready.push_back(token);return true;}
 error="stale or unknown event token";return false;
}
bool EventScheduler::awaiting(EventToken token)const{for(const SemanticEvent& event:_pending)if(event.token==token)return !event.completed;return false;}
bool EventScheduler::take(SemanticEvent& event){
 if(_paused||_ready.empty())return false;EventToken token=_ready.front();_ready.pop_front();
 for(size_t i=0;i<_pending.size();++i)if(_pending[i].token==token){event=_pending[i];_pending.erase(_pending.begin()+i);return true;}return false;
}
bool EventScheduler::transition(std::string& error){error.clear();if(_generation==std::numeric_limits<uint32_t>::max()){error="event generation exhausted";return false;}++_generation;_pending.clear();_ready.clear();return true;}
void SemanticTrace::record(Cursor c,const char *category,uint32_t a,uint32_t b){if(!_enabled)return;if(_entries.size()==256){_entries.pop_front();++_dropped;}TraceEntry e;e.sequence=_next++;e.cursor=c;e.category=category;e.a=a;e.b=b;_entries.push_back(e);}
}
