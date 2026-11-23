/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_FM_PUBLICATION_H
#define SNATCHER_FM_PUBLICATION_H
#include <cstdint>
#include <deque>
#include <string>
namespace Snatcher {
// Game command publication, distinct from voice execution and synthesis.
// Main4F96/5058 admit requests; Main5106 consumes one per service. The
// priority barrier belongs to the most recently admitted effect request,
// not to a voice, and expires when that exact queue entry is consumed.
class FmPublicationQueue {
 friend struct FmCheckpointCodec;
 struct Request {uint8_t command;bool priorityOwner;Request(uint8_t c=0):command(c),priorityOwner(false){}};
 std::deque<Request> _requests;uint8_t _priority;
public:
 FmPublicationQueue():_priority(0){}
 void clear(){_requests.clear();_priority=0;}
 size_t size()const{return _requests.size();}
 uint8_t priority()const{return _priority;}
 bool post(uint8_t command,uint8_t priority,bool& admitted,std::string& error){
  error.clear();admitted=false;if(!command||(command>=123&&command<241))return true;
  const bool effect=command>=50&&command<63;
  if(effect&&priority<_priority)return true;
  // The original ring has32 slots with equality denoting empty. Guard a
  // native overrun rather than silently aliasing a full ring to empty.
  if(_requests.size()>=31){error="Native FM publication ring capacity exhausted";return false;}
  if(effect){for(auto& request:_requests)request.priorityOwner=false;_priority=priority;}
  _requests.push_back(Request(command));_requests.back().priorityOwner=effect;admitted=true;return true;
 }
 bool take(uint8_t& command){
  if(_requests.empty())return false;const Request request=_requests.front();_requests.pop_front();
  if(request.priorityOwner)_priority=0;command=request.command;return true;
 }
};
}
#endif
