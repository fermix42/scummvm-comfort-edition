/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../dialogue_window.h"
#include "../credits.h"
#include <iostream>
#include <stdexcept>
using namespace Snatcher;
namespace {
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::vector<uint8_t> unpack(const std::string& text){check(text.size()%2==0,"hex extent");std::vector<uint8_t> out;for(size_t i=0;i<text.size();i+=2)out.push_back(uint8_t(std::stoul(text.substr(i,2),nullptr,16)));return out;}
unsigned word(const std::vector<uint8_t>& b,size_t p){return unsigned(b.at(p))*256+b.at(p+1);}
uint32_t stamp(const std::vector<uint8_t>& b,size_t p){return uint32_t(word(b,p))<<16|word(b,p+2);}
uint32_t packed(unsigned frames){unsigned m=frames/4500,s=frames/75%60,f=frames%75;return uint32_t((m/10)<<28|(m%10)<<24|(s/10)<<20|(s%10)<<16|(f/10)<<12|(f%10)<<8);}
unsigned frames(uint32_t p){return (((p>>28)*10+((p>>24)&15))*60+((p>>20)&15)*10+((p>>16)&15))*75+((p>>12)&15)*10+((p>>8)&15);}
void credits(const std::vector<uint8_t>& resource){
 CreditsTimeline t;CreditsInput in;std::vector<CreditsEvent> events;std::string error;
 check(t.start(resource,false,error),"actual credits resource");in.ready=true;
 check(t.tick(in,events,error)&&events.size()==1&&events[0].kind==kCreditsStartTrack,"request actual track");
 check(t.tick(in,events,error)&&events.empty()&&t.index()==-1,"no fabricated playing completion");
 in.cdPlaying=true;check(t.tick(in,events,error)&&t.index()==0,"playing admits relative origin");in.clockKnown=true;
 unsigned scenes=0,palettes=0;
 for(unsigned row=0;row<17;++row){
  const uint32_t deadline=stamp(resource,0x10fe+row*6);check(t.index()==int(row),"original authored row order");
  if(deadline){in.relativeClock=packed(frames(deadline)-1);check(t.tick(in,events,error)&&events.empty()&&t.index()==int(row),"before actual authored deadline waits");}
  in.relativeClock=deadline;in.ready=false;check(t.tick(in,events,error)&&events.empty()&&t.index()==int(row),"data readiness owns admission");in.ready=true;
  in.paused=true;check(t.tick(in,events,error)&&events.empty()&&t.index()==int(row),"pause preserves deadline");in.paused=false;
  check(t.tick(in,events,error)&&t.index()==int(row+1)&&events.size()==1,"exact source deadline publishes one event");
  if(events[0].kind==kCreditsInstallScene)check(events[0].value==scenes++,"all source scenes in order");else{check(events[0].kind==kCreditsPalette,"source palette event");++palettes;}
 }
 check(scenes==9&&palettes==8,"nine timed scenes and eight palette events");
 check(t.tick(in,events,error)&&t.index()==17,"actual playback drain required");
 in.cdPlaying=false;in.cdIdleOrPaused=true;in.relativeClock=0;
 check(t.tick(in,events,error)&&t.index()==18&&t.countdown()==300,"nonplaying visual clock zero admits source hold");
 for(unsigned n=0;n<300;++n)check(t.tick(in,events,error)&&t.index()==(n==299?19:18),"300 authored visits");
 for(unsigned n=0;n<64;++n)check(t.tick(in,events,error)&&t.index()==(n==63?20:19),"64 authored fade visits");
 check(t.tick(in,events,error)&&events.size()==1&&events[0].kind==kCreditsStartTrack&&events[0].value==2,"postcredits track2");
 check(t.tick(in,events,error)&&t.index()==21,"track2 cannot be assumed playing");in.cdPlaying=true;in.cdIdleOrPaused=false;
 check(t.tick(in,events,error)&&t.index()==22&&t.countdown()==24,"actual second playing acknowledgement");
 for(unsigned n=0;n<24;++n)check(t.tick(in,events,error)&&t.index()==(n==23?23:22),"24 authored visits");
 check(t.tick(in,events,error)&&events.size()==1&&events[0].kind==kCreditsInstallScene&&events[0].value==9,"final source scene9");
 for(unsigned n=0;n<300;++n)check(t.tick(in,events,error),"final source hold");check(t.waitingStart(),"original final Start gate");
 for(unsigned n=0;n<3;++n)check(t.tick(in,events,error)&&t.waitingStart()&&events.empty(),"no forced final input");in.startPressed=true;
 check(t.tick(in,events,error)&&t.index()==26&&t.countdown()==64,"fresh Start starts final fade");in.startPressed=false;
 for(unsigned n=0;n<64;++n)check(t.tick(in,events,error)&&t.active(),"final authored fade");
 check(t.tick(in,events,error)&&t.finished()&&events.size()==1&&events[0].kind==kCreditsApplicationComplete,"single title handoff");
 check(t.tick(in,events,error)&&events.empty(),"no duplicate completion");
 std::cout<<"CREDITS 28 rows 10 scenes exact deadlines and authored holds PASS\n";
}
struct Result {unsigned ticks,waits,opens,closes;size_t glyphs;std::vector<uint8_t> canvas;};
Result dialogue(const std::vector<uint8_t>& sub,const std::vector<uint8_t>& source,size_t offset,uint16_t speaker,unsigned variant,unsigned substitution,bool fast){
 DialogueWindow window;DialogueLayout layout=freshModuleDialogueLayout();layout.variant=uint8_t(variant);layout.substitutionKnown=true;layout.substitution=uint8_t(substitution);std::string error;
 check(window.loadAssets(sub.data(),sub.size(),error)&&window.beginTask(source.data(),source.size(),offset,speaker,layout,error),"source task entry");
 Result result={0,0,0,0,0,{}};unsigned held=0;bool wasWaiting=false;
 for(;result.ticks<20000&&!window.finished();++result.ticks){
  if(window.waiting()){
   if(!wasWaiting){++result.waits;held=0;}
   if(++held==46&&(window.mode()==0||uint8_t(speaker)==1))check(window.acknowledge(error)&&window.waiting(),"fresh acknowledgement posts, never skips task stage");
  }
  wasWaiting=window.waiting();check(window.coordinateTask(error),"caption coordinator");
  for(auto action=window.takeViewAction();action!=kDialogueNoViewAction;action=window.takeViewAction()){
   if(action==kDialogueCloseView)++result.closes;else if(action==kDialogueDefaultView||action==kDialogueSpecialView)++result.opens;
  }
  check(window.tickGlyphs(false,error,fast),"authored font consumer");
 }
 check(window.finished()&&!window.active()&&window.textEnded(),"complete task lifecycle terminates");
 size_t end=offset;while(source.at(end)!=255)++end;check(window.cursor()==end+1,"all original source bytes consumed");
 check(result.waits>0&&result.closes==result.waits&&result.opens>=result.waits,"one close per page, reopen before continuation");
 result.glyphs=window.revealedGlyphs();result.canvas=window.patterns();return result;
}
}
int main(){try{
 std::string tag,hex;check(bool(std::cin>>tag>>hex)&&tag=="SUB","source assets input");const auto sub=unpack(hex);
 check(bool(std::cin>>tag>>hex)&&tag=="CREDITS","credits assets input");credits(unpack(hex));
 unsigned cases=0;size_t offset;unsigned speaker,substitutions;
 while(std::cin>>tag>>offset>>speaker>>substitutions>>hex){const auto source=unpack(hex);check(substitutions==1||substitutions==6,"finite substitution domain");
  for(unsigned variant=0;variant<3;++variant)for(unsigned s=0;s<substitutions;++s){
   const Result normal=dialogue(sub,source,offset,uint16_t(speaker),variant,s,false),fast=dialogue(sub,source,offset,uint16_t(speaker),variant,s,true);
   check(normal.glyphs==fast.glyphs&&normal.canvas==fast.canvas&&normal.waits==fast.waits&&normal.closes==fast.closes,"acceleration preserves every page and final canvas");
   check(fast.ticks<=normal.ticks,"fast reveal does not add artificial stalls");
   std::cout<<"DIALOGUE "<<tag<<" variant "<<variant<<" substitution "<<s<<" ordinary "<<normal.ticks<<" fast "<<fast.ticks<<" pages "<<normal.waits<<" glyphs "<<normal.glyphs<<" PASS\n";
  }++cases;
 }
 check(cases>0,"declared original witnesses supplied");std::cout<<"PASS "<<checks<<" checks "<<cases<<" source witnesses; behavior only, not original pixel equality\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
