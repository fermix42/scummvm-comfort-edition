/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../scenario.h"
#include <cstdio>
#include <fstream>
#include <iterator>
int runGraphicsTests(); int runPcmTests(); int runDiscTests(); int runControlTests();
static int failures=0,checks=0;
static void check(bool ok,const char *s){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL scenario: %s\n",s);}}
static void put(std::vector<uint8_t>& p,uint16_t v){p.push_back(uint8_t(v>>8));p.push_back(uint8_t(v));}
static void observe(void *p,uint32_t,uint16_t selector,uint16_t){static_cast<std::vector<uint16_t>*>(p)->push_back(selector);}
static std::vector<uint8_t> expression(uint8_t op,uint16_t a,uint16_t b){
 Snatcher::Schema s;Snatcher::schemaFor(op,s);std::vector<uint8_t> p(1,op);
 for(int i=0;i<3;++i){uint8_t k=s.field[i];if(!k||k==7)continue;
  if(k==4){p.push_back(2);put(p,0x100f);put(p,i?b:a);} // packed state zero plus literal
  else put(p,k==1?(i?0x080f:0x000f):(i?b:a));
 }return p;
}
static int scenarioTests(){
 using namespace Snatcher;std::string err;std::vector<uint8_t> state(352,0);uint16_t v;
 check(writePacked(state,0x000f,0xffff,err),"write16");check(readPacked(state,0x000f,v,err)&&v==0xffff,"read16");
 check(writePacked(state,0x0013,0,err)&&state[0]==0x87&&state[1]==0xff,"overlap preserves neighbors");
 check(!readPacked(state,0xffff,v,err),"reject selector outside state");
 const uint8_t ids[]={1,2,3,6,7,8,9,0x10,0x11,0x12,0x16,0x17,0x28,0x29,0x2a,0x2e,0x2f,0x31,0x32,0x33};
 for(size_t j=0;j<sizeof(ids);++j)for(unsigned variant=0;variant<4;++variant){
  uint16_t a=variant==0?0:variant==1?0xffff:variant==2?1:0x8000,b=variant==0?0:variant==1?2:variant==2?0xffff:0x7fff;
  state.assign(352,0);Schema schema;schemaFor(ids[j],schema);
  // Nested expressions use selector0 as zero; direct selectors carry operands.
  if(schema.field[0]==1)writePacked(state,0x000f,a,err);
  if(schema.field[1]==1)writePacked(state,0x080f,b,err);
  std::vector<uint8_t> p=expression(ids[j],a,b);
  // Nested addition uses a distinct zero selector, preserving direct operands.
  Scenario s;size_t end=0;bool parsed=parseTerm(&p[0],p.size(),0,p.size(),s.nodes,end,err);for(size_t q=0;q<s.nodes.size();++q)s.byOffset[s.nodes[q].offset]=q;
  check(parsed&&end==p.size(),"pure grammar");
  uint16_t want=0;switch(ids[j]){case 1:case 2:case 3:want=uint16_t(a+b);break;case 6:case 7:case 8:case 9:want=a&b;break;case 0x10:case 0x11:case 0x12:want=a==b?0xffff:0;break;case 0x16:want=b<a?0xffff:0;break;case 0x17:want=b<=a?0xffff:0;break;case 0x28:case 0x29:want=b>a?0xffff:0;break;case 0x2a:want=b>=a?0xffff:0;break;case 0x2e:case 0x2f:want=!a?0xffff:0;break;default:want=a|b;}
  check(evaluate(s,0,state,v,err)&&v==want,"pure expression exact arithmetic");
 }
 std::vector<uint8_t> ordered=expression(8,0,0x1234);Scenario orderedScenario;size_t orderedEnd;check(parseTerm(ordered.data(),ordered.size(),0,ordered.size(),orderedScenario.nodes,orderedEnd,err),"mixed operand fixture");for(size_t i=0;i<orderedScenario.nodes.size();++i)orderedScenario.byOffset[orderedScenario.nodes[i].offset]=i;state.assign(352,0);writePacked(state,0x000f,0x00ff,err);std::vector<uint16_t> reads;
 check(evaluate(orderedScenario,0,state,v,err,observe,&reads)&&v==0x34,"mixed operand value");check(reads==std::vector<uint16_t>({0x000f,0x100f}),"packed left read precedes nested right read");
 std::vector<uint8_t> unary(10000,0x2e);unary.push_back(0x2f);put(unary,0);Scenario unaryScenario;check(parseTerm(unary.data(),unary.size(),0,unary.size(),unaryScenario.nodes,orderedEnd,err),"deep pure expression grammar");for(size_t i=0;i<unaryScenario.nodes.size();++i)unaryScenario.byOffset[unaryScenario.nodes[i].offset]=i;state.assign(352,0);check(evaluate(unaryScenario,0,state,v,err)&&v==0xffff,"deep expression uses iterative ordered frames");
 uint8_t bad[]={0x4b},trunc[]={2,0};Scenario s;
 check(parseScenario(bad,1,s,err)&&s.unknownCode==1&&!s.unresolved.empty(),"unknown retained");
 check(!evaluate(s,0,state,v,err),"unknown cannot execute");
 check(parseScenario(trunc,2,s,err)&&s.unknownCode==2,"truncation retained");
 uint8_t ref[]={0x26,0,4,0x2d,0x2d};check(parseScenario(ref,5,s,err)&&!s.unknownCode&&s.at(0)->children.size()==1&&s.at(0)->end==4&&validateLocalTargets(s,err),"shared tail owns inline head");
 ref[2]=2;parseScenario(ref,5,s,err);check(!validateLocalTargets(s,err),"reject shared target inside operand");
 std::vector<uint8_t> deep(10000,0);deep.push_back(0x2d);std::vector<Node> parsedNodes;size_t end=0;check(parseTerm(&deep[0],deep.size(),0,deep.size(),parsedNodes,end,err)&&end==deep.size()&&parsedNodes.size()==10001,"deep nesting uses bounded iterative parser");
 std::vector<Scenario> all(39);uint8_t call[]={0x0b,0x21,0,0x3a,0,0},ret[]={0x2d};parseScenario(call,sizeof(call),all[0],err);parseScenario(ret,1,all[1],err);size_t count=0;
 check(validateCrossTargets(all,count,err)&&count==1,"cross-resource decoded boundary");all[1].byOffset.clear();check(!validateCrossTargets(all,count,err),"cross-resource missing boundary rejected");
 std::printf("scenario: %d checks, %d failures\n",checks,failures);return failures;
}
int main(int argc,char **argv){
 if((argc==3||argc==4)&&std::string(argv[1])=="--scenarios"){
  size_t totals[6]={};std::vector<Snatcher::Scenario> scenarios;for(int i=0;i<39;++i){char name[32];std::snprintf(name,sizeof(name),"/SP%02d.BIN",i);std::ifstream f((std::string(argv[2])+name).c_str(),std::ios::binary);std::vector<uint8_t> p((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());Snatcher::Scenario s;std::string err;
   if(!f||!Snatcher::parseScenario(p.empty()?0:&p[0],p.size(),s,err)||!Snatcher::validateLocalTargets(s,err)){std::fprintf(stderr,"%s: %s\n",name,err.c_str());return 1;}
   size_t v[]={s.size,s.codeBytes,s.textBytes,s.fillBytes,s.unknownCode,s.unknownText};for(int k=0;k<6;++k)totals[k]+=v[k];
   std::printf("SP%02d\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",i,v[0],v[1],v[2],v[3],v[4],v[5],s.nodes.size());
   if(argc==4){std::string outname=std::string(argv[3])+name+".tsv";std::ofstream o(outname.c_str());if(!o)return 2;
    for(size_t k=0;k<s.nodes.size();++k){const Snatcher::Node& n=s.nodes[k];o<<n.offset<<'\t'<<n.end<<'\t'<<unsigned(n.opcode)<<'\t'<<(n.hasReference?int(n.reference):-1)<<'\t';for(size_t q=0;q<n.fields.size();++q)o<<unsigned(n.fields[q].type)<<':'<<n.fields[q].offset<<':'<<n.fields[q].value<<',';o<<'\t';for(size_t q=0;q<n.children.size();++q)o<<n.children[q]<<',';o<<'\n';}
   }scenarios.push_back(s);
  }size_t count=0;std::string reason;if(!Snatcher::validateCrossTargets(scenarios,count,reason)||count!=383){std::fprintf(stderr,"Cross-resource: %s (%zu)\n",reason.c_str(),count);return 1;}
  std::printf("CROSS\t%zu\n",count);std::printf("TOTAL\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\n",totals[0],totals[1],totals[2],totals[3],totals[4],totals[5]);return 0;
 }
 return scenarioTests()+runGraphicsTests()+runPcmTests()+runDiscTests()+runControlTests()?1:0;
}
