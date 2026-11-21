/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "event_state.h"
namespace Snatcher {
namespace {
void put(std::vector<uint8_t>& out,uint64_t value,unsigned bytes){while(bytes)out.push_back(uint8_t(value>>(--bytes*8)));}
uint64_t get(const uint8_t* data,size_t& cursor,unsigned bytes){uint64_t value=0;while(bytes--)value=value<<8|data[cursor++];return value;}
}
bool encodeEventState(const EventSnapshot& state,NativeSaveSection& section,std::string& error){
 section=NativeSaveSection(kSaveEvents);if(!validateEventSnapshot(state,error))return false;
 NativeSaveSection staged(kSaveEvents);staged.required=true;const uint8_t magic[]={'S','N','E','V',1,uint8_t(state.paused),0,0};staged.bytes.assign(magic,magic+8);if(state.pending.size()>64)staged.bytes[4]=2;put(staged.bytes,state.generation,4);put(staged.bytes,state.nextIdentity,8);put(staged.bytes,state.pending.size(),2);put(staged.bytes,state.ready.size(),2);
 for(const SemanticEvent& event:state.pending){put(staged.bytes,event.token.id,8);put(staged.bytes,event.owner.resource,2);put(staged.bytes,event.owner.node,4);put(staged.bytes,event.kind,1);put(staged.bytes,(event.completed?1:0)|(event.cancelled?2:0),1);}
 for(const EventToken& token:state.ready)put(staged.bytes,token.id,8);section=staged;return true;
}
bool decodeEventState(const NativeSaveSection& section,EventSnapshot& state,std::string& error){
 state=EventSnapshot();error.clear();const std::vector<uint8_t>& bytes=section.bytes;
 if(section.id!=kSaveEvents||section.version!=1||bytes.size()<24||bytes.size()>24+24*kNativeEventCapacity||bytes[0]!='S'||bytes[1]!='N'||bytes[2]!='E'||bytes[3]!='V'||(bytes[4]!=1&&bytes[4]!=2)||bytes[5]>1||bytes[6]||bytes[7]){error="event payload header/version/extent";return false;}
 EventSnapshot staged;staged.paused=bytes[5]!=0;size_t cursor=8;staged.generation=uint32_t(get(bytes.data(),cursor,4));staged.nextIdentity=get(bytes.data(),cursor,8);size_t count=size_t(get(bytes.data(),cursor,2)),ready=size_t(get(bytes.data(),cursor,2));
 if(count>(bytes[4]==1?64:kNativeEventCapacity)||ready>count||bytes.size()!=24+16*count+8*ready){error="event payload count/extent";return false;}
 for(size_t i=0;i<count;++i){SemanticEvent event;event.token=EventToken(get(bytes.data(),cursor,8),staged.generation);event.owner.resource=uint16_t(get(bytes.data(),cursor,2));event.owner.node=uint32_t(get(bytes.data(),cursor,4));unsigned kind=unsigned(get(bytes.data(),cursor,1)),flags=unsigned(get(bytes.data(),cursor,1));if(kind>kCommandEvent||flags>3){error="event payload kind/flags";return false;}event.kind=EventKind(kind);event.completed=(flags&1)!=0;event.cancelled=(flags&2)!=0;staged.pending.push_back(event);}
 for(size_t i=0;i<ready;++i)staged.ready.push_back(EventToken(get(bytes.data(),cursor,8),staged.generation));if(!validateEventSnapshot(staged,error))return false;state=staged;return true;
}
}
