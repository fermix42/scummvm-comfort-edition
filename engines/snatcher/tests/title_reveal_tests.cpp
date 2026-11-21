/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../startup_presentation.h"
#include "../scene_state.h"
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace Snatcher;
namespace {
unsigned checks;std::string error;
void check(bool value,const char* why){++checks;if(!value){std::fprintf(stderr,"Title reveal: %s: %s\n",why,error.c_str());std::exit(1);}}
std::vector<uint8_t> load(const char* path){std::ifstream in(path,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),{});}
struct Source:SceneStateSource{std::vector<uint8_t> title;bool loadSceneResource(uint16_t id,std::vector<uint8_t>& out,std::string&)override{if(id!=4)return false;out=title;return true;}};
uint64_t digest(const std::vector<uint8_t>& bytes){uint64_t h=14695981039346656037ULL;for(uint8_t x:bytes)h=(h^x)*1099511628211ULL;return h;}
}
int main(int argc,char**argv){
 if(argc!=7)return 2;auto main=load(argv[1]),title=load(argv[2]),first=load(argv[3]),second=load(argv[4]),sub=load(argv[5]);Source source;source.title=title;
 StartupPresenter presenter;SceneRuntime scene;scene.establishFreshAnimationClock();StartupStorage storage;storage.known=storage.available=true;for(auto& slot:storage.slots)slot.known=true;StartupSettings settings;
 check(presenter.start(main,title,first,second,sub,storage,true,&settings,error),"original title setup after abbreviated legal owner");
 StartupFrame frame;frame.soundDriverReady=true;unsigned ordinal=0,cdStart=0,readyTick=0,flashVisit=0,delayClock=0,flashClock=0,revealClock=0;bool reachedReady=false;std::set<uint64_t> revealImages;auto priorPhase=presenter.phase();
 for(unsigned tick=0;tick<2000;++tick){
  frame.sourceFrame=tick;
  // Deterministic typed CD observation at the original source rate; no
  // injected movie state, hardware execution, or native clock modification.
  if(frame.cdKnown){unsigned cd=(uint64_t(tick-cdStart)*750000000)/599227434;unsigned m=cd/4500,s=cd/75%60,f=cd%75;frame.cdClock=((m/10*16+m%10)<<24)|((s/10*16+s%10)<<16)|((f/10*16+f%10)<<8);}
  check(presenter.tick(frame,scene,error),"source startup visit");auto tracks=presenter.takeTracks();if(!tracks.empty()){frame.cdKnown=true;frame.cdTrack=tracks.back();frame.cdClock=0;cdStart=tick;}presenter.takeFm();presenter.takePcm();auto phase=presenter.phase();unsigned clock=scene.animationClock().sourceTicks;
  if(phase!=priorPhase){if(phase==kStartupNativeTitleDelay)delayClock=clock;if(phase==kStartupNativeTitleFlashDelay){check(clock-delayClock==24,"original24 eligible title-delay visits");flashClock=clock;}if(phase==kStartupNativeTitleReveal){check(clock-flashClock==4,"original4 eligible flash-delay visits");revealClock=clock;}if(phase==kStartupNativeTitleReady)check(clock-revealClock==86,"original86 eligible reveal visits");priorPhase=phase;}
  if(phase<kStartupNativeTitleDelay||phase>kStartupNativeTitleReady)continue;
  SceneImage image;check(presenter.render(scene,kSceneReferenceRGB565,image,error)&&image.width==256&&image.height==224,"complete original-size title raster");
  uint8_t visual=0;scene.visualCommand(visual);auto layout=scene.framePresentationLayout();
  if(visual==7){
   // First observation is the Sub request. Main then initializes5; its
   // following visits publish4,3,2,1,0 before the distinct cleanup visit.
   if(flashVisit==0)check(layout.sourceY[0]==-256,"Sub post cannot expose cleared Main origin early");
   else if(flashVisit==1)check(!layout.displayEnabled&&layout.sourceY[0]==0,"Main init clears displayed origin but retains black pre-H-blank frame");
   else if(flashVisit<=5){check(layout.displayEnabled&&layout.viewBands.size()>=2,"original flash bands are active");check(layout.sourceY[0]==(flashVisit<5?48:16),"original48/16 scroll values");}
   else check(layout.displayEnabled&&layout.sourceY[0]==0,"counter0 restores ordinary view");
   check(scene.scroll().positions[0].y16==256*65536,"clearing Main publication must preserve Sub position");++flashVisit;
  }
  if(phase==kStartupNativeTitleReveal)revealImages.insert(digest(image.rgb));
  if(phase==kStartupNativeTitleReady&&!readyTick){
   NativeSaveSection packet;SceneRuntime restored;check(SceneStateCodec::encode(scene,packet,error)&&packet.bytes[3]=='T'&&packet.bytes.back()==0,"SNST retains disabled Main full-plane publication");check(SceneStateCodec::decode(packet,source,restored,error),"title scroll publication restores");SceneImage a,b;check(scene.render(kSceneReferenceRGB565,a,error)&&restored.render(kSceneReferenceRGB565,b,error)&&a.rgb==b.rgb,"restored title's complete visible frame is exact");SceneRuntime next=scene;check(next.beginFrame(false,error)&&restored.beginFrame(false,error)&&next.render(kSceneReferenceRGB565,a,error)&&restored.render(kSceneReferenceRGB565,b,error)&&a.rgb==b.rgb,"next Main does not repaint stale Sub scroll after restore");auto invalid=packet;invalid.bytes.back()=2;check(!SceneStateCodec::decode(invalid,source,restored,error),"invalid publication bool rejected");auto legacy=packet;legacy.bytes[3]='S';legacy.bytes.pop_back();check(SceneStateCodec::decode(legacy,source,restored,error),"historical SNSS remains readable without guessed new flag");
  }
  char name[1024];std::snprintf(name,sizeof(name),"%s/%03u.rgb",argv[6],ordinal);std::ofstream out(name,std::ios::binary);out.write((const char*)image.rgb.data(),image.rgb.size());check(bool(out),"private frame export");
  unsigned visible=0;for(size_t i=0;i<image.rgb.size();i+=3)visible+=(image.rgb[i]||image.rgb[i+1]||image.rgb[i+2]);std::printf("frame %u tick%u clock%u phase%u visual%u pixels%u\n",ordinal++,tick,clock,unsigned(phase),unsigned(visual),visible);
  if(phase==kStartupNativeTitleReady){check(scene.scroll().positions[0].y16==256*65536&&layout.sourceY[0]==0,"ready transition cannot inject a late global scroll reset");reachedReady=true;if(!readyTick)readyTick=tick;if(tick>=readyTick+360)break;}
 }
 check(reachedReady&&flashVisit==7&&revealImages.size()>=22,"all flash phases and progressive palette reveal precede Ready");std::printf("Title reveal: %u checks,0 failures; %zu distinct reveal rasters; original24/4/86 waits\n",checks,revealImages.size());return 0;
}
