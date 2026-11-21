/* SPDX-License-Identifier: GPL-3.0-or-later */
// Independently specified physical grammar. Provenance: Phase 2 research.
// These are format contracts, not original executable translations/tables.
#include "scenario.h"
#include <algorithm>
#include <sstream>
namespace Snatcher {
const Node *Scenario::at(uint32_t offset) const {std::map<uint32_t,size_t>::const_iterator i=byOffset.find(offset);return i==byOffset.end()?nullptr:&nodes[i->second];}
bool schemaFor(uint8_t op,Schema &s) {
 switch(op) {
 case 0x00:case 0x0b:case 0x2c:case 0x2e:case 0x30:case 0x42:case 0x43:s=Schema(4);break;
 case 0x01:case 0x07:case 0x32:s=Schema(4,1);break;
 case 0x02:case 0x12:case 0x16:case 0x17:case 0x1c:case 0x29:case 0x2a:s=Schema(1,3);break;
 case 0x03:case 0x09:case 0x33:s=Schema(1,1);break;
 case 0x04:case 0x05:case 0x0a:case 0x0d:case 0x0f:case 0x3d:case 0x41:case 0x45:s=Schema(6);break;
 case 0x06:case 0x1a:case 0x25:case 0x31:s=Schema(4,4);break;
 case 0x08:case 0x1b:case 0x1d:case 0x3e:case 0x3f:case 0x40:s=Schema(1,4);break;
 case 0x0c:s=Schema(5,7);break;
 case 0x0e:case 0x13:case 0x14:case 0x2f:s=Schema(1);break;
 case 0x10:s=Schema(4,5);break;
 case 0x11:case 0x28:s=Schema(4,3);break;
 case 0x15:case 0x3c:case 0x4a:s=Schema(3);break;
 case 0x18:s=Schema(4,4,8);break;
 case 0x19:s=Schema(1,4,8);break;
 case 0x1e:s=Schema(3,3);break;case 0x1f:case 0x34:s=Schema(3,4);break;
 case 0x20:s=Schema(3,2);break;case 0x21:s=Schema(6,5);break;
 case 0x22:s=Schema(6,3);break;case 0x23:s=Schema(6,6);break;case 0x24:s=Schema(6,4);break;
 case 0x26:s=Schema(4,8);break;case 0x27:s=Schema(4,7);break;
 case 0x2b:case 0x3b:s=Schema(5);break;case 0x2d:s=Schema();break;
 case 0x35:s=Schema(3,7);break;case 0x36:s=Schema(7,4);break;case 0x37:s=Schema(7,7);break;
 case 0x38:s=Schema(2,4);break;case 0x39:case 0x3a:case 0x46:case 0x47:case 0x48:case 0x49:s=Schema(7);break;
 case 0x44:s=Schema(2);break;default:return false;
 }return true;
}
static bool word(const uint8_t *p,size_t limit,size_t off,uint16_t& v) {if(off>limit||limit-off<2)return false;v=uint16_t(p[off]<<8|p[off+1]);return true;}
struct Frame {Node node;Schema schema;unsigned next;Frame():next(0){} };
bool parseTerm(const uint8_t *p,size_t size,size_t start,size_t limit,std::vector<Node>& out,size_t& end,std::string& error) {
 out.clear();error.clear();end=start;
 if(!p||limit>size||start>=limit||limit>65536) {error="invalid scenario extent";return false;}
 std::vector<Frame> stack;std::vector<Node> staged;size_t cursor=start;bool begin=true;
 while(true) {
  if(begin) {
   if(cursor>=limit) {error="truncated construct";return false;}
   Frame f;f.node.offset=uint32_t(cursor);f.node.opcode=p[cursor++];
   if(!schemaFor(f.node.opcode,f.schema)){error="unsupported descriptor identity";return false;}
   if(f.schema.field[1]&8||f.schema.field[2]&8) {
    uint16_t ref;if(!word(p,limit,cursor,ref)){error="truncated shared reference";return false;}
    f.node.hasReference=true;f.node.reference=ref;f.node.fields.push_back(Field{8,uint32_t(cursor),ref});cursor+=2;
    if(f.node.opcode==0x26)f.schema=Schema(4);
    else if(f.node.opcode==0x18||f.node.opcode==0x19)f.schema=Schema(f.schema.field[0],4);
    else {error="unclassified shared form";return false;}
   }
   stack.push_back(f);begin=false;
  }
  Frame &f=stack.back();
  if(f.next==3) {
   f.node.end=uint32_t(cursor);uint32_t offset=f.node.offset;staged.push_back(f.node);stack.pop_back();
   if(stack.empty()){out.swap(staged);end=cursor;return true;}
   stack.back().node.children.push_back(offset);continue;
  }
  uint8_t kind=f.schema.field[f.next++];
  if(kind==0||kind==7)continue;
  if(kind==4){begin=true;continue;}
  uint16_t value;if(kind>6||!word(p,limit,cursor,value)){error="truncated or unsupported operand";return false;}
  f.node.fields.push_back(Field{kind,uint32_t(cursor),value});cursor+=2;
 }
}
bool parseScenario(const uint8_t *p,size_t size,Scenario& out,std::string& error) {
 out=Scenario();error.clear();if(!p||!size||size>65536){error="invalid scenario size";return false;}
 out.size=size;size_t arena=std::min(size,size_t(0x3800)),codeEnd=arena;
 while(codeEnd&&p[codeEnd-1]==255)--codeEnd;
 out.fillBytes=arena-codeEnd;size_t cursor=0;
 while(cursor<codeEnd) {
  std::vector<Node> staged;size_t end=cursor;std::string reason;
  if(!parseTerm(p,size,cursor,codeEnd,staged,end,reason)){out.unresolved=reason;break;}
  for(size_t i=0;i<staged.size();++i){out.byOffset[staged[i].offset]=out.nodes.size();out.nodes.push_back(staged[i]);}
  cursor=end;
 }
 out.codeBytes=cursor;out.unknownCode=codeEnd-cursor;
 size_t textStart=arena;cursor=arena;
 while(cursor<size){if(p[cursor++]==255){++out.textRecords;textStart=cursor;}}
 out.textBytes=textStart-arena;out.unknownText=size-textStart;
 // Unknowns are preserved in accounting; parsing does not invent their behavior.
 return true;
}
bool validateLocalTargets(const Scenario& s,std::string& error) {
 error.clear();for(size_t i=0;i<s.nodes.size();++i){const Node& n=s.nodes[i];
  if(n.hasReference&&!s.at(n.reference)){error="shared/conditional target is not a decoded boundary";return false;}
  for(size_t j=0;j<n.fields.size();++j){const Field& f=n.fields[j];
   if(f.type==2&&(size_t(f.value)+0x3800>=s.size)){error="text reference outside resource";return false;}
   if(f.type==5&&(n.opcode==0x0c||n.opcode==0x2b)&&!s.at(f.value)){error="continuation target is not a boundary";return false;}
  }
 }return true;
}
bool isPureExpression(uint8_t op) {switch(op){case 1:case 2:case 3:case 6:case 7:case 8:case 9:case 0x10:case 0x11:case 0x12:case 0x16:case 0x17:case 0x28:case 0x29:case 0x2a:case 0x2e:case 0x2f:case 0x31:case 0x32:case 0x33:return true;default:return false;}}
bool validateCrossTargets(const std::vector<Scenario>& all,size_t& count,std::string& error) {
 count=0;error.clear();for(size_t i=0;i<all.size();++i)for(size_t j=0;j<all[i].nodes.size();++j){const Node& n=all[i].nodes[j];
  if(n.opcode!=0x0b&&n.opcode!=0x2c)continue;
  if(n.children.size()!=1){error="cross-resource request missing operand";return false;}
  const Node *pair=all[i].at(n.children[0]);
  if(!pair||pair->opcode!=0x21||pair->fields.size()!=2){error="unsupported dynamic cross-resource destination";return false;}
  unsigned id=pair->fields[0].value;
  if(id<0x39||id>0x5f||id-0x39>=all.size()||!all[id-0x39].at(pair->fields[1].value)){error="invalid cross-resource target boundary";return false;}++count;
 }return true;
}
bool readPacked(const std::vector<uint8_t>& state,uint16_t selector,uint16_t& value,std::string& error) {
 size_t bit=size_t(selector>>7)*8+((selector>>4)&7),width=(selector&15)+1;value=0;error.clear();
 if(bit+width>state.size()*8){error="packed selector outside supplied state";return false;}
 for(size_t i=0;i<width;++i){size_t b=bit+i;value=uint16_t(value<<1|((state[b/8]>>(7-b%8))&1));}return true;
}
bool writePacked(std::vector<uint8_t>& state,uint16_t selector,uint16_t value,std::string& error) {
 size_t bit=size_t(selector>>7)*8+((selector>>4)&7),width=(selector&15)+1;error.clear();
 if(bit+width>state.size()*8){error="packed selector outside supplied state";return false;}
 for(size_t i=0;i<width;++i){size_t b=bit+i;uint8_t mask=uint8_t(1u<<(7-b%8));state[b/8]=uint8_t((state[b/8]&~mask)|(((value>>(width-1-i))&1)?mask:0));}return true;
}
bool evaluate(const Scenario& s,uint32_t offset,const std::vector<uint8_t>& state,uint16_t& value,std::string& error,StateReadObserver observer,void *context,ExpressionResolver resolver) {
 struct EvalFrame {const Node *node;Schema schema;unsigned next;size_t word,child;std::vector<uint16_t> args;EvalFrame(const Node *n):node(n),next(0),word(0),child(0){schemaFor(n->opcode,schema);}};
 error.clear();value=0;std::vector<EvalFrame> stack;uint32_t begin=offset;bool enter=true;size_t steps=0;
 while(++steps<=s.nodes.size()*8+8){
  if(enter){const Node *n=s.at(begin);if(!n){error="missing expression node";return false;}if(!isPureExpression(n->opcode)){uint16_t result=0;if(!resolver||!resolver(context,*n,result,error)){if(error.empty()){std::ostringstream message;message<<"unsupported expression node "<<begin;error=message.str();}return false;}if(stack.empty()){value=result;return true;}stack.back().args.push_back(result);enter=false;continue;}stack.push_back(EvalFrame(n));enter=false;}
  EvalFrame& frame=stack.back();const Node *n=frame.node;
  if(frame.next<3){uint8_t kind=frame.schema.field[frame.next++];if(!kind||kind==7)continue;
   if(kind==4){if(frame.child>=n->children.size()){error="missing expression child";return false;}begin=n->children[frame.child++];enter=true;continue;}
   if(frame.word>=n->fields.size()||(kind!=1&&kind!=3&&kind!=5)||n->fields[frame.word].type!=kind){error="unsupported expression operand";return false;}
   uint16_t operand=n->fields[frame.word++].value;
   if(kind==1){uint16_t selector=operand;if(!readPacked(state,selector,operand,error))return false;if(observer)observer(context,n->offset,selector,operand);}
   frame.args.push_back(operand);continue;
  }
  const std::vector<uint16_t>& args=frame.args;
  uint16_t r=0;uint8_t op=n->opcode;bool unary=(op==0x2e||op==0x2f);
  if(args.size()!=(unary?1u:2u)){error="malformed expression arity";return false;}
  uint16_t a=args[0],b=unary?0:args[1];
  switch(op){case 1:case 2:case 3:r=uint16_t(uint32_t(a)+b);break;case 6:case 7:case 8:case 9:r=a&b;break;
   case 0x31:case 0x32:case 0x33:r=a|b;break;case 0x2e:case 0x2f:r=a==0?0xffff:0;break;
   case 0x10:case 0x11:case 0x12:r=b==a?0xffff:0;break;case 0x16:r=b<a?0xffff:0;break;
   case 0x17:r=b<=a?0xffff:0;break;case 0x28:case 0x29:r=b>a?0xffff:0;break;case 0x2a:r=b>=a?0xffff:0;break;
   default:error="unsupported expression";return false;}
  if(frame.word!=n->fields.size()||frame.child!=n->children.size()){error="extra expression operands";return false;}
  stack.pop_back();if(stack.empty()){value=r;return true;}stack.back().args.push_back(r);
 }error="expression traversal cycle/budget";return false;
}
}
