/* SPDX-License-Identifier: GPL-3.0-or-later */
// Independent semantic contracts: original handlers16E08,16E52/6E,16F94,
//17018,1702E,17078..170F8,1720C,172C0 and Phase2/3 research. No guest execution.
#include "runtime.h"
#include <sstream>
namespace Snatcher {
NarrativeRuntime::NarrativeRuntime(const std::vector<Scenario>& s,EventScheduler& e,SemanticTrace& t):_scenarios(s),_events(e),_trace(t),_random(nullptr),_queries(nullptr),_progressionSection(0),_lastPresentation(0xffff),_bookmarkedPresentation(0xffff),_bookmarkKnown(false),_progressionKnown(false),_randomRebindRequired(false),_hasPending(false),_menuSelectionResetPending(false),_status(kRuntimeIdle){}
void NarrativeRuntime::observeRead(void *context,uint32_t node,uint16_t selector,uint16_t value){NarrativeRuntime& runtime=*static_cast<NarrativeRuntime*>(context);runtime._trace.record(Cursor(runtime._cursor.resource,node),"state_read",selector,value);}
const Scenario *NarrativeRuntime::scenario(uint16_t id)const{return id>=0x39&&id-0x39<_scenarios.size()?&_scenarios[id-0x39]:nullptr;}
const Node *NarrativeRuntime::node(Cursor c)const{const Scenario *s=scenario(c.resource);return s?s->at(c.node):nullptr;}
bool NarrativeRuntime::fail(const char *reason,bool unsupported){std::ostringstream text;text<<"resource "<<_cursor.resource<<" node "<<_cursor.node;const Node *at=node(_cursor);if(at){text<<" opcode "<<unsigned(at->opcode);for(size_t i=0;i<at->fields.size();++i)text<<" operand"<<i<<"="<<at->fields[i].value;}text<<": "<<reason;_diagnostic=text.str();_status=unsupported?kRuntimeUnsupported:kRuntimeError;_trace.record(_cursor,unsupported?"unsupported":"error");return false;}
bool NarrativeRuntime::push(Cursor c,uint8_t action,uint32_t saved){if(_work.size()>=256)return fail("native continuation capacity");if(!action&&!node(c))return fail("destination is not a decoded node");_work.push_back(Work(c,action,saved));return true;}
bool NarrativeRuntime::start(Cursor entry,const std::vector<uint8_t>& state,std::string& error){
 error.clear();if(!node(entry)||state.size()!=352){error="invalid scenario entry or packed-state extent";return false;}
 if(!_events.transition(error))return false;_cursor=entry;_packed=state;_progressionSection=0;_lastPresentation=0xffff;_bookmarkedPresentation=0xffff;_bookmarkKnown=false;_progressionKnown=false;_randomRebindRequired=false;_control=ControlState(uint16_t(entry.node));_entry=entry;_locals.clear();_returns.clear();_work.clear();_requests.clear();_hasPending=false;_menuSelectionResetPending=false;_diagnostic.clear();_status=kRuntimeRunning;_trace.record(entry,"entry",_events.generation());return push(entry);
}
bool NarrativeRuntime::startGameCheckpoint(Cursor entry,const std::vector<uint8_t>& packed,uint16_t progression,const std::vector<uint16_t>& history,uint16_t presentation,std::string& error){
 error.clear();ControlState fresh(uint16_t(entry.node));ControlSnapshot control;
 if(!fresh.snapshot(control,error))return false;control.history=history;
 if(!validateControlSnapshot(control,error)||(presentation>>8)>=97){if(error.empty())error="Original game checkpoint presentation is invalid";return false;}
 for(uint16_t previous:history){bool found=false;for(const auto& source:_scenarios)if(source.at(previous)){found=true;break;}if(!found){error="Original game checkpoint history is not decoded source";return false;}}
 if(!start(entry,packed,error)||!_control.restore(control,error))return false;
 _progressionSection=progression;_progressionKnown=true;_lastPresentation=_bookmarkedPresentation=presentation;_bookmarkKnown=true;
 return true;
}
bool NarrativeRuntime::request(RequestKind kind,const Node& n,uint16_t first,uint16_t second){
 if(_requests.size()>=kNativeEventCapacity)return fail("request queue capacity");RuntimeRequest r;r.kind=kind;r.owner=_cursor;r.first=first;r.second=second;
 EventKind event=kind==kRequestDialogue?kDialogueEvent:kind==kRequestPresentation?kAnimationEvent:(kind==kRequestActorAction||kind>=kRequestCommand)?kCommandEvent:kAudioEvent;std::string error;
 if(!_events.create(event,_cursor,r.token,error))return fail(error.c_str());_requests.push_back(r);_trace.record(_cursor,"request",uint32_t(kind),first);_trace.record(_cursor,"event_created",uint32_t(r.token.id),uint32_t(r.token.id>>32));return true;
}
bool decodeNarrativeQuery(uint16_t field,NarrativeQuery& query){
 switch(field){
 case 0x79e0:query=kQueryResponseStatus;break;case 0x79e2:query=kQueryResponseWord0;break;case 0x79e4:query=kQueryResponseWord1;break;case 0x79e6:query=kQueryResponseWord2;break;case 0x79e8:query=kQueryResponseWord3;break;
 case 0x7908:query=kQueryShootingShots;break;case 0x790a:query=kQueryShootingTypeSetHits;break;case 0x790c:query=kQueryShootingTypeClearHits;break;case 0x790e:query=kQueryShootingMisses;break;case 0x7910:query=kQueryShootingCounter;break;case 0x7916:query=kQueryShootingResult;break;case 0x7918:query=kQueryShootingInputToggle;break;case 0x7972:query=kQueryAlternateShootingInput;break;
 case 0x96dc:query=kQueryMenuContext;break;case 0xcdac:query=kQueryStorageStatus;break;default:return false;
 }return true;
}
bool decodeNarrativeSetting(uint16_t field,NarrativeSetting& setting){
 switch(field){case 0x971a:setting=kSettingTextEntrySession;break;case 0x9718:setting=kSettingTextEntryLimit;break;case 0x971c:setting=kSettingDialogueTop;break;case 0x971e:setting=kSettingPhoneEntryPolicy;break;case 0x78e2:setting=kSettingShootingParameter;break;case 0x79ee:setting=kSettingAmbientPresentation;break;case 0xffff:setting=kSettingQuietPcm;break;default:return false;}return true;
}
bool NarrativeRuntime::resolveExpression(void *context,const Node& n,uint16_t& value,std::string& error){
 NarrativeRuntime& r=*static_cast<NarrativeRuntime*>(context);error.clear();value=0;
 if(n.opcode==0x42){
  if(!r._control.gate()){error="native query outside enabled traversal requires inherited evaluator word";return false;}
  const Node* tuple=n.children.size()==1?r.node(Cursor(r._cursor.resource,n.children[0])):nullptr;NarrativeQuery query;
  if(!tuple||tuple->opcode!=0x23||tuple->fields.size()!=2||!tuple->children.empty()||tuple->fields[0].value!=5){error="native query expression constructor unsupported";return false;}
  if(!decodeNarrativeQuery(tuple->fields[1].value,query)){error="native query semantic field unresolved";return false;}
  if(!r._queries){error="native query requires its typed subsystem owner";return false;}
  uint16_t result=0;if(!r._queries->read(query,result,error)){if(error.empty())error="native query subsystem value unavailable";return false;}
  value=result;r._trace.record(Cursor(r._cursor.resource,n.offset),"subsystem_query",uint32_t(query),value);return true;
 }

 if(n.fields.size()!=1||!n.children.empty()){error="contextual expression operand shape";return false;}
 uint8_t index=uint8_t(n.fields[0].value);
 if(n.opcode==0x15){const std::vector<uint16_t>& history=r._control.cursorHistory();value=index&&index<=history.size()?history[history.size()-index]:0xffff;}
 else if(n.opcode==0x3b||n.opcode==0x3c){if(n.fields[0].type==5){value=0;for(uint32_t cursor:r._locals)if(cursor==n.fields[0].value){value=1;break;}}
  else if(n.fields[0].type==3){value=index==0?uint16_t(r._entry.node):index<=r._locals.size()?uint16_t(r._locals[r._locals.size()-index]):0xffff;}
  else{error="contextual query operand type";return false;}}
 else return false;
 r._trace.record(Cursor(r._cursor.resource,n.offset),"context_query",n.fields[0].value,value);return true;
}
bool NarrativeRuntime::command(const Node& n,uint16_t queue,const std::vector<uint16_t>& operands){
 if(!request(kRequestCommand,n,queue,operands.empty()?0:operands[0]))return false;
 RuntimeRequest& r=_requests.back();r.queue=queue;r.operands=operands;
 _trace.record(_cursor,"action_queue",queue,uint32_t(operands.size()));return true;
}
bool NarrativeRuntime::nativeAction(const Node& n){
 const Node *tuple=n.opcode==0x41?&n:n.children.size()==1?node(Cursor(_cursor.resource,n.children[0])):nullptr;
 if(!tuple||tuple->fields.empty())return fail("native subtype operands",true);
 uint16_t subtype=tuple->fields[0].value;_trace.record(_cursor,"native_subtype",subtype,tuple->opcode);
 if(subtype==5){uint16_t ignored;std::string error;if(!resolveExpression(this,n,ignored,error))return fail(error.c_str(),true);return true;}

 if(n.opcode==0x41){
  if(subtype==0||subtype==2||subtype==3)return command(n,subtype==0?4:subtype==2?33:31,std::vector<uint16_t>());
  if(subtype==12){if(tuple->fields.size()!=1)return fail("packed reset operand extent");_packed[53]&=0xf8;for(size_t i=54;i<_packed.size();++i)_packed[i]=0;_trace.record(_cursor,"packed_reset_suffix",53,299);return true;}
 }else if(tuple->opcode==0x24&&tuple->fields.size()==1&&tuple->children.size()==1&&subtype==6){
  const Node *pair=node(Cursor(_cursor.resource,tuple->children[0]));
  if(pair&&pair->opcode==0x22&&pair->fields.size()==2&&pair->children.empty()&&pair->fields[0].value==0x79f0){_progressionSection=pair->fields[1].value;_progressionKnown=true;_trace.record(_cursor,"progression_section",_progressionSection);return true;}
  NarrativeSetting setting;if(pair&&(pair->opcode==0x22||pair->opcode==0x23)&&pair->fields.size()==2&&pair->children.empty()&&decodeNarrativeSetting(pair->fields[0].value,setting))return request(kRequestSemanticSetting,n,uint16_t(setting),pair->fields[1].value);
 }else if(tuple->fields.size()==2&&tuple->children.empty()){
  if(tuple->opcode==0x22&&subtype==1){uint16_t slot=tuple->fields[1].value;if(slot<1||slot>4)return fail("original save slot outside four-slot service",true);if(!request(kRequestGameSave,n,slot,_lastPresentation))return false;RuntimeRequest& save=_requests.back();save.queue=30;save.gameSave.known=true;save.gameSave.entry=uint16_t(_locals.empty()?_entry.node:_locals.front());save.gameSave.progressionKnown=_progressionKnown;save.gameSave.progression=_progressionSection;save.gameSave.packed=_packed;save.gameSave.history=_control.cursorHistory();_bookmarkedPresentation=_lastPresentation;_bookmarkKnown=true;_trace.record(_cursor,"action_queue",30,slot);return true;}
  // Source encounter entry sets the mode and clears the encounter's counters
  // before queue18 begins it. The native request retains this setup operand;
  // the owning encounter consumer must apply it, never infer a default mode.
  if(tuple->opcode==0x23&&subtype==4)return command(n,24,std::vector<uint16_t>(1,tuple->fields[1].value));
  if((tuple->opcode==0x22||tuple->opcode==0x23)&&subtype>=9&&subtype<=11){uint16_t operand=tuple->fields[1].value;if(!request(subtype==9?kRequestCoordinatorControl:subtype==10?kRequestInteractionMode:kRequestPresentationBookmark,n,operand,subtype==11&&!operand?_lastPresentation:0))return false;if(subtype==11){if(!operand){_bookmarkedPresentation=_lastPresentation;_bookmarkKnown=true;}else if(_bookmarkKnown)_lastPresentation=_bookmarkedPresentation;}return true;}
  if(tuple->opcode==0x22&&subtype==7){std::vector<uint16_t> operands(1,tuple->fields[1].value==0?0:1);return command(n,29,operands);}
  if(tuple->opcode==0x22&&subtype==8&&tuple->fields[1].value==0){if(!request(kRequestActorAction,n,1,2))return false;_requests.back().queue=9;_trace.record(_cursor,"actor_action",1,2);return true;}
 }else if(tuple->opcode==0x24&&tuple->fields.size()==1&&tuple->children.size()==1&&(subtype==8||subtype==13)){
  const Node *head=node(Cursor(_cursor.resource,tuple->children[0]));const Node *pair=head&&head->children.size()==1?node(Cursor(_cursor.resource,head->children[0])):nullptr;
  if(head&&head->opcode==0x1f&&head->fields.size()==1&&pair&&pair->children.empty()&&pair->fields.size()==2&&pair->opcode==(subtype==8?0x22:0x1e)){
   std::vector<uint16_t> operands;if(subtype==13)operands.push_back(head->fields[0].value);operands.push_back(pair->fields[0].value);operands.push_back(pair->fields[1].value);return command(n,subtype==8?21:27,operands);
  }
 }
 std::ostringstream message;message<<"native subtype "<<subtype<<" consumer/context not implemented";return fail(message.str().c_str(),true);
}
bool NarrativeRuntime::branch(const Node& n){std::string error;uint16_t result=0;bool packed=n.opcode==0x19||n.opcode==0x1b;
 if(packed){size_t index=n.hasReference?1:0;if(n.fields.size()<=index||!readPacked(_packed,n.fields[index].value,result,error))return fail("invalid packed predicate");observeRead(this,n.offset,n.fields[index].value,result);}
 else if(n.children.empty()||!evaluate(*scenario(_cursor.resource),n.children[0],_packed,result,error,observeRead,this,resolveExpression))return fail(error.c_str(),true);
 _trace.record(_cursor,"predicate",result);if(result){size_t index=packed?0:1;if(index>=n.children.size())return fail("missing true arm");_trace.record(_cursor,"branch",n.children[index]);return push(Cursor(_cursor.resource,n.children[index]));}
 if(n.hasReference){_trace.record(_cursor,"branch",n.reference);return push(Cursor(_cursor.resource,n.reference));}return true;
}
bool NarrativeRuntime::logicalSkip(Cursor start,uint32_t& end){
 end=start.node;const Scenario *s=scenario(start.resource);if(!s)return fail("logical skip resource");std::vector<std::pair<Schema,unsigned> > stack;uint32_t cursor=start.node;bool begin=true;size_t steps=0;
 while(++steps<s->nodes.size()*8+8){if(begin){const Node *n=s->at(cursor);if(!n)return fail("logical skip boundary");if(n->hasReference){cursor=n->reference;continue;}Schema schema;if(!schemaFor(n->opcode,schema))return fail("logical skip descriptor");stack.push_back(std::make_pair(schema,0));++cursor;begin=false;}
  if(stack.back().second==3){stack.pop_back();if(stack.empty()){end=cursor;return true;}continue;}
  uint8_t kind=stack.back().first.field[stack.back().second++];if(kind==4){begin=true;continue;}if(kind&&kind!=7){if(kind>6||cursor>s->codeBytes||s->codeBytes-cursor<2)return fail("logical skip word bounds");cursor+=2;}
 }return fail("logical skip cycle/budget");
}
bool NarrativeRuntime::listItems(Cursor cursor,std::vector<Cursor>& items){items.clear();std::vector<uint32_t> seen;while(true){if(seen.size()>=256)return fail("logical list capacity/cycle");for(uint32_t off:seen)if(off==cursor.node)return fail("logical list cycle");seen.push_back(cursor.node);const Node *n=node(cursor);if(!n)return fail("logical list boundary");bool cell=n->opcode==0x25||n->opcode==0x26||n->opcode==0x27;Cursor head=cursor;if(cell){if(n->children.empty())return fail("logical list missing head");head.node=n->children[0];}const Node *h=node(head);if(!h)return fail("logical list head boundary");unsigned repeats=1;if(cell&&(h->opcode==0x34||h->opcode==0x35)){if(h->fields.size()!=1)return fail("repeat operand");repeats=h->fields[0].value&255;if(!repeats)return fail("zero repeat list count",true);}if(items.size()+repeats>255)return fail("logical list count exceeds byte domain");for(unsigned i=0;i<repeats;++i)items.push_back(head);if(!cell||n->opcode==0x27)return true;if(n->opcode==0x26){if(!n->hasReference)return fail("missing shared list tail");cursor.node=n->reference;}else if(!logicalSkip(head,cursor.node))return false;}}
RuntimeStatus NarrativeRuntime::run(size_t budget){
 if(_status!=kRuntimeRunning||_events.paused())return _status;while(!_work.empty()&&budget--){Work w=_work.back();_work.pop_back();_cursor=w.cursor;std::string error;
  if(w.action==1){if(!_control.endChoice(error))fail(error.c_str());if(_status!=kRuntimeRunning)return _status;continue;}
  if(w.action==2){if(_locals.empty()){fail("local return ownership");return _status;}_entry.node=w.saved;_locals.pop_back();_trace.record(_cursor,"local_return",w.saved);continue;}
  if(w.action==4){uint16_t selector=uint16_t(w.saved>>16),value=uint16_t(w.saved);if(!writePacked(_packed,selector,value,error)){fail(error.c_str());return _status;}_trace.record(_cursor,"random_selection_write",selector,value);continue;}
  if(w.action==3){if(_control.availableChoices().empty()){if(!writePacked(_packed,uint16_t(w.saved>>16),uint16_t(w.saved),error)){fail(error.c_str());return _status;}_trace.record(_cursor,"progressive_advance",w.saved);}continue;}
  const Node *p=node(_cursor);if(!p){fail("missing decoded node");return _status;}const Node& n=*p;_trace.record(_cursor,"dispatch",n.opcode);
  switch(n.opcode){
  case 0x30:if(n.children.size()!=1||!push(Cursor(_cursor.resource,n.children[0])))fail("invalid callable wrapper");break;
  case 0x25:case 0x26:case 0x27:{std::vector<Cursor> items;if(!listItems(_cursor,items))break;for(size_t i=items.size();i>0;--i)if(!push(items[i-1]))break;break;}
  case 0x34:if(n.children.size()!=1){fail("invalid repeated list body");break;}push(Cursor(_cursor.resource,n.children[0]));break;
  case 0x2d:case 0x35: _trace.record(_cursor,"return");break;
  case 0x18:case 0x19:case 0x1a:case 0x1b:branch(n);break;
  case 0x38:{if(n.fields.size()!=1||n.children.size()!=1){fail("invalid choice");break;}ChoiceDecision d=_control.beginChoice(n.fields[0].value,error);_trace.record(_cursor,"choice",n.fields[0].value,uint32_t(d));if(d==kChoiceError)fail(error.c_str());else if(d==kChoiceEntered&&push(_cursor,1))push(Cursor(_cursor.resource,n.children[0]));break;}
  case 0x36:case 0x37:if(_control.gate()){ActivationDecision activation=_control.enterActivation(error);if(activation==kActivationError){fail(error.c_str());break;}if(activation==kActivationEntered&&n.opcode==0x36){if(n.children.size()!=1)fail("missing activation body");else push(Cursor(_cursor.resource,n.children[0]));}else if(activation==kActivationSkipped)_trace.record(_cursor,"activation_skipped",_control.depth());}break;
  case 0x39:if(_control.gate()&&!_control.leaveActivation(error))fail(error.c_str());break;
  case 0x40:{if(n.fields.size()!=1||n.children.size()!=1){fail("indexed body operands");break;}uint16_t selected;if(!readPacked(_packed,n.fields[0].value,selected,error)){fail(error.c_str());break;}observeRead(this,n.offset,n.fields[0].value,selected);Cursor at(_cursor.resource,n.children[0]);bool done=false;
   for(unsigned limit=0;limit<256;++limit){const Node *cell=node(at);if(!cell){fail("indexed list boundary");done=true;break;}bool product=cell->opcode>=0x1e&&cell->opcode<=0x27;Cursor head=at;if(product){if(cell->children.empty()){fail("indexed scalar constructor",true);done=true;break;}head.node=cell->children[0];}const Node *body=node(head);if(!body||body->opcode!=0x34||body->fields.size()!=1||body->children.size()!=1){fail("indexed case constructor",true);done=true;break;}if(uint8_t(body->fields[0].value)==uint8_t(selected)){_trace.record(_cursor,"indexed_select",selected,body->children[0]);push(Cursor(at.resource,body->children[0]));done=true;break;}if(!product){_trace.record(_cursor,"indexed_no_match",selected);done=true;break;}if(!logicalSkip(head,at.node)){done=true;break;}}
   if(!done)fail("indexed list cycle/budget");break;}
  case 0x3e:{if(n.fields.size()!=1||n.children.size()!=1){fail("random selection operands");break;}if(!_random){fail("random selection requires shared RNG owner state",true);break;}
   std::vector<Cursor> items;if(!listItems(Cursor(_cursor.resource,n.children[0]),items))break;if(items.empty()){fail("empty random list");break;}uint16_t draw=0;if(!_random->next(draw,error)){fail(error.empty()?"shared RNG draw unavailable":error.c_str(),true);break;}uint16_t selected=uint16_t(1+((uint32_t(draw)*items.size())>>16));_trace.record(_cursor,"random_select",draw,selected);
   if(push(_cursor,4,(uint32_t(n.fields[0].value)<<16)|selected))push(items[selected-1]);break;}
  case 0x3f:{if(n.fields.size()!=1||n.children.size()!=1){fail("progressive selection operands");break;}std::vector<Cursor> items;if(!listItems(Cursor(_cursor.resource,n.children[0]),items))break;uint16_t value;if(!readPacked(_packed,n.fields[0].value,value,error)){fail(error.c_str());break;}observeRead(this,n.offset,n.fields[0].value,value);unsigned index=value&255;if(index>=items.size()){fail("progressive selector outside logical list");break;}_trace.record(_cursor,"progressive_select",index,uint32_t(items.size()));if(index+1<items.size()&&!push(_cursor,3,uint32_t(n.fields[0].value)<<16|(index+1)))break;push(items[index]);break;}
  case 0x0e:case 0x13:case 0x14:case 0x1c:case 0x1d:{if(!_control.gate())break;if(n.fields.empty()){fail("missing mutation operand");break;}uint16_t selector=n.fields[0].value,before=0,value=0;if(!readPacked(_packed,selector,before,error)){fail(error.c_str());break;}_trace.record(_cursor,"state_read",selector,before);
   if(n.opcode==0x14)value=0xffff;else if(n.opcode==0x0e){uint32_t max=(1u<<((selector&15)+1))-1;value=uint16_t(before<max?before+1:before);}else if(n.opcode==0x1c){if(n.fields.size()!=2){fail("missing assignment");break;}value=n.fields[1].value;}else if(n.opcode==0x1d){if(n.children.size()!=1||!evaluate(*scenario(_cursor.resource),n.children[0],_packed,value,error,observeRead,this,resolveExpression)){fail(error.c_str(),true);break;}}
   if(!writePacked(_packed,selector,value,error))fail(error.c_str());else{uint16_t after;readPacked(_packed,selector,after,error);_trace.record(_cursor,"state_write",selector,uint32_t(before)<<16|after);}break;}
  case 0x0c:{if(n.fields.size()!=1||_locals.size()>=16){fail("local call operand/stack");break;}uint32_t saved=_entry.node;_locals.push_back(saved);_entry.node=n.fields[0].value;_trace.record(_cursor,"local_call",_entry.node);if(push(_cursor,2,saved))push(Cursor(_cursor.resource,_entry.node));break;}
  case 0x2b:case 0x2c:case 0x0b:{if(!_control.gate())break;Cursor destination=_cursor;
   if(n.opcode==0x2b){if(n.fields.size()!=1){fail("continuation operand");break;}destination.node=n.fields[0].value;}
   else{const Node *tuple=n.children.size()==1?scenario(_cursor.resource)->at(n.children[0]):nullptr;if(!tuple||tuple->opcode!=0x21||tuple->fields.size()!=2){fail("dynamic resource destination",true);break;}destination=Cursor(tuple->fields[0].value,tuple->fields[1].value);}
   if(!node(destination)){fail("invalid continuation destination");break;}
   if(n.opcode==0x0b){if(_returns.size()>=9){fail("resource continuation stack");break;}_returns.push_back(Cursor(_cursor.resource,_locals.empty()?_entry.node:_locals.front()));}
   if(!_control.requestContinuation(uint16_t(destination.node),error)){fail(error.c_str());break;}_pending=destination;_hasPending=true;_trace.record(_cursor,"continuation",destination.resource,destination.node);break;}
  case 0x3a:if(_returns.empty()){fail("resource continuation underflow");break;}else{Cursor target=_returns.back();_returns.pop_back();if(!_control.replaceContinuation(uint16_t(target.node),error)){fail(error.c_str());break;}_pending=target;_hasPending=true;_trace.record(_cursor,"resource_return",target.resource,target.node);}break;
  case 0x43:case 0x44:{if(!_control.gate())break;uint16_t style=0,text=0;if(n.opcode==0x44){if(n.fields.size()!=1){fail("dialogue operand");break;}text=n.fields[0].value;}
   else{const Node *tuple=n.children.size()==1?scenario(_cursor.resource)->at(n.children[0]):nullptr;if(!tuple||tuple->opcode!=0x20||tuple->fields.size()!=2){fail("dialogue constructor",true);break;}style=tuple->fields[0].value;text=tuple->fields[1].value;}request(kRequestDialogue,n,style,text);break;}
  case 4:case 5:case 0x3d:if(_control.gate()){if(n.fields.size()!=1)fail("request operand");else if(n.opcode==0x3d&&_lastPresentation==n.fields[0].value)_trace.record(_cursor,"presentation_cached",_lastPresentation);else if(request(n.opcode==4?kRequestPcm:n.opcode==5?kRequestEffect:kRequestPresentation,n,n.fields[0].value)&&n.opcode==0x3d)_lastPresentation=n.fields[0].value;}break;
  case 0:case 0x45:if(_control.gate()){
   uint16_t actor=0,action=3;
   if(n.opcode==0x45){if(n.fields.size()!=1){fail("actor wait operand");break;}actor=n.fields[0].value;}
   else{const Node *tuple=n.children.size()==1?node(Cursor(_cursor.resource,n.children[0])):nullptr;if(!tuple||tuple->opcode!=0x23||tuple->fields.size()!=2){fail("actor action tuple context",true);break;}actor=tuple->fields[0].value;action=tuple->fields[1].value;}
   if(actor>=64||action>5){fail("actor action index/subtype outside recovered domain",true);break;}
   if(request(kRequestActorAction,n,actor,action)){_requests.back().queue=9;_trace.record(_cursor,"actor_action",actor,action);}
  }break;
  case 0x0d:case 0x0f:case 0x47:case 0x48:case 0x49:case 0x4a:if(_control.gate()){
   uint16_t queue=n.opcode==0x0d?14:n.opcode==0x0f?28:n.opcode==0x47?19:n.opcode==0x48?11:n.opcode==0x49?25:16;
   bool word=n.opcode==0x0d||n.opcode==0x0f||n.opcode==0x4a;
   if(n.fields.size()!=(word?1u:0u)){fail("action queue operands");break;}
   std::vector<uint16_t> operands;if(word)operands.push_back(n.fields[0].value);command(n,queue,operands);
  }break;
  case 0x41:case 0x42:if(_control.gate())nativeAction(n);break;
  case 0x0a:if(_control.gate()){if(n.fields.size()!=1)fail("FM command operand");else request(kRequestFm,n,n.fields[0].value);}break;
  case 0x46:if(_control.gate())request(kRequestAudioWait,n,0);break;
  default:fail("descriptor semantic contract not implemented",true);break;
  }if(_status!=kRuntimeRunning)return _status;
 }if(!_work.empty()){fail("bounded execution budget exceeded");return _status;}finishPass();return _status;
}
bool NarrativeRuntime::finishPass(){if(_events.pending()){_status=kRuntimeWaiting;return true;}std::string error;bool handoff=_hasPending;if(_hasPending){if(!_control.applyContinuation(error))return fail(error.c_str());_entry=_pending;_hasPending=false;_menuSelectionResetPending=true;_trace.record(_entry,"handoff");}if(!_control.availableChoices().empty()){_status=kRuntimeChoices;return true;}if(!_control.emptyPass(error))return fail(error.c_str());if(handoff){if(!_events.transition(error)||!_control.beginPass(error))return fail(error.c_str());_status=kRuntimeRunning;return push(_entry);}_status=kRuntimePassFinished;return true;}
bool NarrativeRuntime::restart(std::string& error){if(!_control.beginPass(error))return false;_requests.clear();_work.clear();_status=kRuntimeRunning;return push(_entry);}
bool NarrativeRuntime::continueTraversal(std::string& error){
 error.clear();if(_events.paused()||_status!=kRuntimePassFinished||_events.pending()){error="runtime is paused or not at a completed empty traversal";return false;}if(!restart(error))return false;_trace.record(_entry,"traversal_restarted");return true;
}
bool NarrativeRuntime::choose(unsigned choice,std::string& error){error.clear();if(_events.paused()||_status!=kRuntimeChoices){error="runtime is paused or not awaiting a choice";return false;}if(!_control.respond(choice,error))return false;_trace.record(_entry,"choice_selected",choice);return restart(error);}
bool NarrativeRuntime::chooseReference(uint16_t reference,std::string& error){
 error.clear();if(_events.paused()||_status!=kRuntimeChoices){error="runtime is paused or not awaiting a choice";return false;}
 const std::vector<uint16_t>& choices=_control.availableChoices();unsigned ordinal=0;
 for(size_t i=0;i<choices.size();++i)if(choices[i]==reference){if(ordinal){error="command text reference is ambiguous in current context";return false;}ordinal=unsigned(i+1);}
 if(!ordinal){error="command text reference is unavailable in current context";return false;}
 return choose(ordinal,error);
}
bool NarrativeRuntime::cancelChoice(std::string& error){error.clear();if(_events.paused()||_status!=kRuntimeChoices){error="runtime is paused or not awaiting a choice";return false;}if(!_control.cancel(error))return false;_trace.record(_entry,"choice_cancelled");return restart(error);}
bool NarrativeRuntime::resumeCompleted(std::string& error){error.clear();if(_status!=kRuntimeWaiting){error="runtime is not awaiting events";return false;}SemanticEvent event;while(_events.take(event)){_trace.record(event.owner,event.cancelled?"event_cancelled":"event_completed",uint32_t(event.token.id),uint32_t(event.token.id>>32));if(event.cancelled){_status=kRuntimeError;_diagnostic="cancelled event requires an unresolved narrative cancellation contract";error=_diagnostic;return false;}}if(_events.pending())return true;_requests.clear();return finishPass();}
}
