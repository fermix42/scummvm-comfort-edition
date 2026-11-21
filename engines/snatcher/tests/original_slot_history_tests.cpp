/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../startup_presentation.h"
#include "../presentation_bookmark.h"
#include "../interaction.h"
#include "../runtime.h"
#include "../shooting.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
using namespace Snatcher;
namespace {
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::vector<uint8_t> load(const char* path){std::ifstream in(path,std::ios::binary);check(bool(in),"required private original asset unavailable");return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),{});}
struct Random: NarrativeRandomSource {RandomWords state;unsigned draws;Random():state(0,0),draws(0){}bool next(uint16_t& value,std::string&)override{++draws;value=drawRandom(state);return true;}};
uint32_t packedClock(uint64_t frames){unsigned m=unsigned(frames/4500),s=unsigned(frames/75%60),f=unsigned(frames%75);return uint32_t(m/10*16+m%10)<<24|uint32_t(s/10*16+s%10)<<16|uint32_t(f/10*16+f%10)<<8;}
unsigned known(const EnvironmentVisualState& state){unsigned n=0;for(const auto& slot:state.previous)n+=slot.known;return n;}
bool equal(const EnvironmentVisualState& a,const EnvironmentVisualState& b){for(unsigned i=0;i<64;++i){const auto&x=a.previous[i];const auto&y=b.previous[i];if(x.known!=y.known)return false;if(x.known&&(x.piece.x!=y.piece.x||x.piece.y!=y.piece.y||x.piece.width!=y.piece.width||x.piece.height!=y.piece.height||x.piece.attributes!=y.piece.attributes))return false;}return true;}
void history(std::ostream& out,const EnvironmentVisualState& state){out<<'[';for(unsigned i=0;i<64;++i){if(i)out<<',';const auto&s=state.previous[i];out<<"{\"known\":"<<(s.known?"true":"false")<<",\"x\":"<<s.piece.x<<",\"y\":"<<s.piece.y<<",\"width\":"<<s.piece.width<<",\"height\":"<<s.piece.height<<",\"attributes\":"<<s.piece.attributes<<'}';}out<<']';}
}
// Explicit original-resource component fixture. Synthetic slot metadata models
// one backend response, not an earned story checkpoint or persistence receipt.
// No GUI, emulator, game route, image capture or manual gameplay is involved.
int main(int argc,char**argv){try{
 check(argc==7,"usage: original-slot-history-tests MAIN_IP D2 D1 D0 SUBCODE PRIVATE_JSON");
 auto main=load(argv[1]),title=load(argv[2]),first=load(argv[3]),second=load(argv[4]),sub=load(argv[5]);
 std::string error;SceneRuntime scene;Random random;StartupPresenter presenter;
 // This explicit one-time allocation now has an independently recovered boot
 // owner, not a zero default inferred from unsubmitted startup pieces. See
 // phase18-cold-allocation-contract.json; generic unknown fixtures stay unknown.
 check(scene.establishBootSubmissionHistory(error),error.c_str());
 for(const auto& slot:scene.submissionHistory().previous)check(slot.known&&slot.piece.x==-128&&slot.piece.y==-128&&slot.piece.width==8&&slot.piece.height==8&&slot.piece.attributes==0,"explicit source boot allocation decodes exact raw-zero entries");
 scene.establishFreshAnimationClock();scene.establishFreshMusicContext();scene.setRandomSource(&random);
 check(scene.loadGameLayoutPresets(main.data(),main.size(),error)&&scene.loadEnvironmentTables(sub.data(),sub.size(),error)&&scene.loadSharedGraphicsEffects(sub.data(),sub.size(),error),error.c_str());
 StartupStorage storage;storage.known=storage.available=true;for(auto&slot:storage.slots)slot.known=true;
 storage.slots[0].present=storage.slots[0].metadataKnown=true;storage.slots[0].counter=1;storage.slots[0].hours=1;storage.slots[0].minutes=30;storage.slots[0].progression=2;
 StartupSettings savedSettings;check(presenter.start(main,title,first,second,sub,storage,false,nullptr,error),error.c_str());
 StartupFrame frame;uint64_t cdPhase=0;unsigned tick=0,transitions=0,trackPosts=0;bool sawTitle=false,sawRead=false,sawOptions=false;unsigned oldPhase=unsigned(presenter.phase());
 std::printf("phase %u update0 known%u\n",oldPhase,known(scene.submissionHistory()));
 for(;tick<4000&&!presenter.loaded();++tick){
  uint16_t background;check(random.next(background,error),error.c_str());frame.sourceFrame=tick+1;frame.soundDriverReady=true;frame.pressed=0;
  if(frame.cdKnown){cdPhase+=UINT64_C(75)*10000000;frame.cdClock=packedClock(cdPhase/599227434);}
  if(presenter.phase()==kStartupNativeTitleReady){check(presenter.continueSelected(),"actual saved title selects Continue");sawTitle=true;frame.pressed=0x80;}
  if(presenter.phase()==kStartupNativeLoadPoll){check(presenter.selectedSlot()==1,"source selector chooses sole occupied slot1");frame.pressed=0x80;}
  if(presenter.pendingLoadSlot()){check(presenter.pendingLoadSlot()==1&&presenter.acknowledgeLoad(1,savedSettings,error),error.c_str());sawRead=true;}
  if(presenter.phase()==kStartupNativeOptionsPoll){sawOptions=true;frame.pressed=0x80;}
  scene.setSoundContext(false,0);check(presenter.tick(frame,scene,error),error.c_str());
  check(scene.commitEnvironmentFrame(uint16_t(tick+1),error),error.c_str());
  for(unsigned track:presenter.takeTracks()){check(track==2,"Continue fixture never starts cinematic track");++trackPosts;frame.cdKnown=true;frame.cdTrack=track;frame.cdClock=0;cdPhase=0;}
  presenter.takeFm();presenter.takePcm();scene.takeMusicCommands();scene.takeEffects();
  if(unsigned(presenter.phase())!=oldPhase){oldPhase=unsigned(presenter.phase());++transitions;std::printf("phase %u update%u known%u\n",oldPhase,tick+1,known(scene.submissionHistory()));}
 }
 check(sawTitle&&sawRead&&sawOptions&&presenter.loaded()&&presenter.loadedSlot()==1&&trackPosts==1,"actual startup Continue/read/Options/fade reaches loaded boundary");
 const EnvironmentVisualState actual=scene.submissionHistory();
 std::printf("loaded boundary updates%u known%u RNGdraws%u\n",tick,known(actual),random.draws);
 scene.resetForModule();check(equal(actual,scene.submissionHistory()),"module reset preserves exact genuine startup history and unknown mask");
 SceneLayout layout;check(readSceneLayoutPreset(main.data(),main.size(),kSceneTallLayout,layout,error)&&scene.loadGameLayoutPresets(main.data(),main.size(),error)&&scene.configure(layout,error)&&scene.establishSceneReset(kSceneTallLayout,error)&&scene.loadEnvironmentTables(sub.data(),sub.size(),error),error.c_str());
 PointerInteraction pointer;std::vector<uint8_t> map;check(readInteractionHitMap(sub.data(),sub.size(),map,error)&&pointer.loadMap(map,error),error.c_str());pointer.sourceFreshReset();
 NativePresentationBookmark bookmark;bookmark.valid=bookmark.pcmKnown=true;bookmark.ambient=1;
 check(restorePresentationBookmarkPolicies(bookmark,scene,pointer,error),error.c_str());
 unsigned consumed=33;bool consumedKnown=true;for(unsigned i=1;i<consumed;++i)consumedKnown=consumedKnown&&actual.previous[i].known; // slot0 is the source frame's explicit clear.
 const bool composed=scene.commitEnvironmentFrame(uint16_t(tick+1),error);const std::string compositionError=error;
 bookmark.ambient=0;bookmark.interaction=1;check(restorePresentationBookmarkPolicies(bookmark,scene,pointer,error),error.c_str());
 PointerInteractionSnapshot cold;check(pointer.snapshot(cold,error)&&cold.active&&cold.selectedKnown&&cold.selectedRegion==0&&cold.x16==0&&cold.y16==0,"cold slot interaction1 restores active policy over actual fresh source pointer state");
 std::ofstream report(argv[6],std::ios::binary);check(bool(report),"private report cannot be created");
 report<<"{\"scope\":\"Original-resource startup component fixture; synthetic slot metadata; no normal-story or persistence acceptance\",\"source_boot_allocation_contract\":\"phase18-cold-allocation-contract.json\",\"explicit_boot_initialization_once\":true,\"initial_boot_known_slots\":64,\"startup_updates\":"<<tick<<",\"phase_transitions\":"<<transitions<<",\"rng_draws\":"<<random.draws<<",\"title_cd_track\":2,\"title_cd_source_frames\":"<<(cdPhase/599227434)<<",\"startup_known_slots\":"<<known(actual)<<",\"original_environment_required_slots\":"<<consumed<<",\"all_consumed_history_known\":"<<(consumedKnown?"true":"false")<<",\"first_environment_submission_succeeded\":"<<(composed?"true":"false")<<",\"cold_interaction_active\":true,\"cold_position_x\":0,\"cold_position_y\":0,\"cold_selected_region\":0,\"cold_selected_region_known\":true,\"loaded_boundary_history\":";history(report,actual);report<<",\"first_environment_history\":";history(report,scene.submissionHistory());report<<"}\n";report.close();
 if(!composed||!consumedKnown){std::fprintf(stderr,"Original startup history does not satisfy first mode1 submission: %s\n",compositionError.c_str());return 2;}
 std::printf("original slot startup history: %u checks,0 failures; required history known; no seeded slots\n",checks);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"original slot history FAILED: %s\n",e.what());return 1;}}
