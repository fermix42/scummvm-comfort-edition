/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../startup_presentation.h"
#include "../runtime.h"
#include "../shooting.h"
#include <fstream>
#include <iterator>
#include <cstdio>
using namespace Snatcher;
namespace {
std::vector<uint8_t> load(const char* path){std::ifstream in(path,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());}
uint32_t packedClock(uint64_t frames){unsigned m=unsigned(frames/4500),s=unsigned(frames/75%60),f=unsigned(frames%75);return uint32_t(m/10*16+m%10)<<24|uint32_t(s/10*16+s%10)<<16|uint32_t(f/10*16+f%10)<<8;}
struct Random: NarrativeRandomSource {RandomWords state;Random():state(0,0){}bool next(uint16_t& value,std::string&)override{value=drawRandom(state);return true;}};
bool writeImage(const char* directory,unsigned command,unsigned update,const SceneImage& image){char suffix[96];std::snprintf(suffix,sizeof(suffix),"/shutter-%u-%u.rgb",command,update);std::ofstream out(std::string(directory)+suffix,std::ios::binary);out.write(reinterpret_cast<const char*>(image.rgb.data()),image.rgb.size());return bool(out);}
}
int main(int argc,char** argv){std::string error;SceneRuntime scene;SceneImage image;
 if(argc==7&&std::string(argv[1])=="--full"){
  auto main=load(argv[2]),title=load(argv[3]),first=load(argv[4]),second=load(argv[5]),sub=load(argv[6]);StartupPresenter presenter;Random random;scene.setRandomSource(&random);scene.establishFreshAnimationClock();if(!scene.loadSharedGraphicsEffects(sub.data(),sub.size(),error)||!presenter.start(main,title,first,second,sub,false,error))return 2;
  StartupFrame frame;uint64_t cdPhase=0;std::vector<unsigned> tracks;unsigned tick=0,rendered=0,oldPhase=0;
  for(;tick<30000&&!presenter.finished();++tick){uint16_t background;random.next(background,error);frame.sourceFrame=tick;frame.soundDriverReady=true;frame.pressed=(presenter.phase()==kStartupNativeTitleReady||presenter.phase()==kStartupNativeOptionsPoll)?128:0;
   if(frame.cdKnown){cdPhase+=UINT64_C(75)*10000000;frame.cdClock=packedClock(cdPhase/599227434);}
   if(!presenter.tick(frame,scene,error)){std::fprintf(stderr,"cinematic failure update%u phase%u track%u clock%08x: %s\n",tick,unsigned(presenter.phase()),frame.cdTrack,frame.cdClock,error.c_str());return 3;}
   auto requests=presenter.takeTracks();for(unsigned track:requests){tracks.push_back(track);frame.cdKnown=true;frame.cdTrack=track;cdPhase=0;frame.cdClock=0;std::printf("track\t%u\t%u\n",tick,track);}presenter.takeFm();presenter.takePcm();scene.takeEffects();
   if(unsigned(presenter.phase())!=oldPhase){oldPhase=unsigned(presenter.phase());std::printf("phase\t%u\t%u\n",tick,oldPhase);}
   if(tick%30==0){if(!presenter.render(scene,kSceneReferenceRGB565,image,error)){std::fprintf(stderr,"cinematic render failure update%u: %s\n",tick,error.c_str());return 4;}++rendered;}
  }
  const std::vector<unsigned> expected={2,3,5,4};if(!presenter.finished()||tracks!=expected||presenter.soundDriverSuspended()){std::fprintf(stderr,"unskipped cinematic did not reach source terminal or track order\n");return 5;}
  std::printf("full cinematic contract: PASS updates=%u rendered=%u source_tracks=2,3,5,4 no_intro_skip=1 normal_gameplay_claim=0\n",tick,rendered);return 0;
 }
 if(argc==6&&std::string(argv[1])=="--shutter"){
  auto main=load(argv[2]),first=load(argv[3]),sub=load(argv[4]);SceneLayout layout;scene.establishFreshAnimationClock();if(!readSceneLayoutPreset(main.data(),main.size(),kSceneTallLayout,layout,error)||!scene.configure(layout,error)||!scene.establishSceneReset(kSceneTallLayout,error)||!scene.establishFreshPalette(error)||!scene.setTransferChannel(0,error)||!scene.loadSharedGraphicsEffects(sub.data(),sub.size(),error))return 2;
  scene.establishFreshModuleDialogueContext();scene.establishSceneFlags(0);scene.setSoundContext(false,0);if(!scene.install(3,first,2,error)||!scene.commitTransfers(error)||!scene.requestVisualCommand(1,error)||!scene.queueScrollPosition(0,kScrollY,0x110,error))return 3;for(unsigned i=0;i<200;++i)if(!scene.tick(false,error))return 4;
  for(unsigned command=8;command<=12;++command){if(!scene.requestVisualCommand(uint16_t(command),error))return 5;for(unsigned update=0;update<=24;++update){if(!scene.render(kSceneReferenceRGB565,image,error))return 6;if(update==0||update==6||update==23||update==24)if(!writeImage(argv[5],command,update,image))return 7;if(!scene.beginFrame(false,error))return 8;}}
  std::printf("cinematic shutter fixture: PASS variants=5 frames=20 normal_gameplay_claim=0\n");return 0;
 }
 return 1;
}
