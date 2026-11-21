/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../presentation_bookmark.h"
#include "../scene_runtime.h"
#include "../interaction.h"
#include "../game_service.h"
#include <cstdio>
#include <stdexcept>
using namespace Snatcher;
namespace {
unsigned checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool equal(const EnvironmentVisualState& a,const EnvironmentVisualState& b){
 for(unsigned i=0;i<64;++i){const auto& x=a.previous[i];const auto& y=b.previous[i];
  if(x.known!=y.known)return false;if(!x.known)continue;
  if(x.piece.x!=y.piece.x||x.piece.y!=y.piece.y||x.piece.width!=y.piece.width||x.piece.height!=y.piece.height||x.piece.attributes!=y.piece.attributes)return false;
 }return true;
}
void word(std::vector<uint8_t>& data,size_t offset,unsigned value){data[offset]=uint8_t(value>>8);data[offset+1]=uint8_t(value);}
// Explicit synthetic directory matching the source-owned environmental format.
// Fixture values establish no original startup history or pixel acceptance.
std::vector<uint8_t> tables(){
 const size_t base=0xd400;std::vector<uint8_t> out(0xf322-base,0);
 word(out,0xeede-base,0xfff0);word(out,0xeee0-base,0x1e);word(out,0xeee2-base,0x1e);
 for(unsigned i=3;i<=9;++i)word(out,0xeede-base+i*2,0x1fc);
 out[0xeef9-base]=32;out[0xeefa-base]=16;
 for(unsigned i=0;i<3;++i){out[0xf086-base+i]=0;out[0xf08a-base+i]=255;out[0xf08e-base+i]=255;}
 for(unsigned i=0;i<32;++i)out[0xf0ba-base+i]=uint8_t(70+i);
 for(unsigned i=0;i<8;++i)word(out,0xf0a2-base+i*2,100+i);
 return out;
}
void configure(SceneRuntime& scene,const std::vector<uint8_t>& sub,std::string& error){
 SceneLayout layout;layout.width=256;layout.height=224;layout.columns=32;layout.rows=64;
 check(scene.configure(layout,error)&&scene.setPalette(std::vector<uint16_t>(128,0),0,error),"configure source scene owners");
 scene.establishBlankAssets();scene.establishFreshMusicContext();
 check(scene.loadEnvironmentTables(sub.data(),sub.size(),error),"load typed environmental directory");
}
GameSaveRecord record(unsigned ambient,unsigned interaction){
 GameSaveRecord out;out.disc=std::string(64,'a');out.scenarioStamps.assign(39,123);out.packed.assign(352,0);out.contextKnown=true;out.resource=94;out.resumeEntry=0;out.progression=2;
 out.bookmark.known=true;out.bookmark.presentation=0;out.bookmark.ambient=uint8_t(ambient);out.bookmark.interaction=uint8_t(interaction);return out;
}
}
int main(){try{
 SceneRuntime boot;std::string bootError;
 check(boot.establishBootSubmissionHistory(bootError),"recovered fresh game boot allocation is explicit");
 for(const auto& slot:boot.submissionHistory().previous)check(slot.known&&slot.piece.x==-128&&slot.piece.y==-128&&slot.piece.width==8&&slot.piece.height==8&&slot.piece.attributes==0,"source zero entry decoded exactly into game fields");
 const auto bootHistory=boot.submissionHistory();
 check(!boot.establishBootSubmissionHistory(bootError)&&equal(bootHistory,boot.submissionHistory()),"boot allocation cannot overwrite existing retained history");
 boot.resetForModule();check(equal(bootHistory,boot.submissionHistory()),"boot allocation survives module reset without reseeding");
 std::string error;auto sub=tables();SceneRuntime scene;configure(scene,sub,error);
 std::vector<PlacedPiece> pieces;for(unsigned i=0;i<64;++i)pieces.push_back(PlacedPiece{int(i)+11,int(i)+17,8,8,uint16_t(200+i)});
 // Menu cursor publication follows ED28: its first commit arms the next
 // submission. This fixture must actually emit the newly owned pieces.
 check(scene.setMenuCursorPieces(pieces,error)&&scene.commitEnvironmentFrame(17,error)&&scene.commitEnvironmentFrame(18,error),"real submission establishes history after deferred cursor publication");
 const auto history=scene.submissionHistory();for(const auto& slot:history.previous)check(slot.known,"all submitted slots are known");
 check(scene.validateModuleEnvironmentHistory(1,error),"actual complete history admits environmental module restore");
 scene.resetForModule();check(equal(history,scene.submissionHistory())&&scene.environmentMode()==0,"module reset retains exact application history, clears policy");
 configure(scene,sub,error);PointerInteraction pointer;std::vector<uint8_t> map(36*20,3);map[0]=0;check(pointer.loadMap(map,error),"cold source hit map");pointer.sourceFreshReset();
 NativePresentationBookmark bookmark;bookmark.valid=bookmark.pcmKnown=true;bookmark.ambient=1;
 check(restorePresentationBookmarkPolicies(bookmark,scene,pointer,error)&&scene.environmentMode()==1&&!pointer.active(),"original environmental policy restored");
 auto expected=history;std::vector<PlacedPiece> output;EnvironmentVisualTables decoded;check(readEnvironmentVisualTables(sub.data(),sub.size(),decoded,error)&&composeEnvironmentVisual(decoded,1,22,{},{},expected,output,error),"independent real submission contract");
 check(scene.commitEnvironmentFrame(22,error)&&equal(expected,scene.submissionHistory()),"first restored environmental submission consumes exact retained history");
 bookmark.ambient=0;bookmark.interaction=1;
 check(restorePresentationBookmarkPolicies(bookmark,scene,pointer,error)&&pointer.active(),"cold original interaction policy restored");
 PointerInteractionSnapshot cold;check(pointer.snapshot(cold,error)&&cold.x16==0&&cold.y16==0&&cold.selectedKnown&&cold.selectedRegion==0,"cold module reset position/cache remain source zero");
 PointerInteractionResult sampled;uint16_t region=0;
 check(pointer.sample(false,0,1,sampled,error)&&pointer.queryRegion(region,error)&&region==3,"ordinary released sample publishes actual source hit-map region");
 check(pointer.mousePan(145,85,256,112,false,error)&&pointer.sample(false,0,1,sampled,error),"live examination publishes its actual position/cache");
 PointerInteractionSnapshot live;check(pointer.snapshot(live,error)&&live.x16!=0&&live.y16!=0,"live pointer state exists");
 bookmark.interaction=0;check(restorePresentationBookmarkPolicies(bookmark,scene,pointer,error)&&!pointer.active(),"live restoration can leave examination policy");
 bookmark.interaction=1;check(restorePresentationBookmarkPolicies(bookmark,scene,pointer,error),"live restoration reactivates without native10 reset");
 PointerInteractionSnapshot restored;check(pointer.snapshot(restored,error)&&restored.x16==live.x16&&restored.y16==live.y16&&restored.selectedRegion==live.selectedRegion&&restored.selectedKnown==live.selectedKnown,"live restoration preserves exact position/cache");
 scene.setMusicSuppressed(true);bookmark.suppressed=false;
 check(restorePresentationBookmarkPolicies(bookmark,scene,pointer,error)&&scene.musicSuppressed(),"false saved suppression bit preserves existing source suppression");
 scene.setMusicSuppressed(false);bookmark.suppressed=true;
 check(restorePresentationBookmarkPolicies(bookmark,scene,pointer,error)&&scene.musicSuppressed(),"true saved suppression bit sets source suppression");
 auto before=scene.submissionHistory();bookmark.interaction=2;
 check(!restorePresentationBookmarkPolicies(bookmark,scene,pointer,error)&&equal(before,scene.submissionHistory())&&pointer.active(),"unrecovered interaction policy rejected transactionally");
 bookmark.interaction=1;bookmark.ambient=10;
 check(!restorePresentationBookmarkPolicies(bookmark,scene,pointer,error)&&scene.environmentMode()==0&&pointer.active(),"unrecovered environmental policy rejected transactionally");
 SceneRuntime partial;configure(partial,sub,error);pieces.resize(4);
 check(partial.setMenuCursorPieces(pieces,error)&&partial.commitEnvironmentFrame(0,error)&&partial.commitEnvironmentFrame(1,error),"partial real submission after deferred cursor publication");
 auto incomplete=partial.submissionHistory();check(incomplete.previous[3].known&&!incomplete.previous[4].known,"unwritten history stays unknown");
 check(!partial.validateModuleEnvironmentHistory(1,error)&&partial.validateModuleEnvironmentHistory(0,error),"unowned history rejected before cold environmental restore; ordinary policy unaffected");
 partial.resetForModule();configure(partial,sub,error);bookmark.ambient=1;bookmark.interaction=0;
 check(restorePresentationBookmarkPolicies(bookmark,partial,pointer,error)&&!partial.commitEnvironmentFrame(22,error)&&equal(incomplete,partial.submissionHistory()),"unknown inherited slot fails without fabricated history or partial commit");
 partial.reset();for(const auto& slot:partial.submissionHistory().previous)check(!slot.known,"full owner destruction resets history knowledge");
 for(unsigned ambient=0;ambient<=1;++ambient)for(unsigned interaction=0;interaction<=1;++interaction){
  auto original=record(ambient,interaction);
  std::vector<uint8_t> bytes;GameSaveRecord decodedRecord;
  check(encodeGameSaveRecord(original,bytes,error)&&decodeGameSaveRecord(bytes,original.disc,original.scenarioStamps,decodedRecord,error),"existing SGR2 context record roundtrip");
  check(decodedRecord.bookmark.ambient==ambient&&decodedRecord.bookmark.interaction==interaction,"SGR2 policies remain exact without serializing draw history or pointer positions");
 }
 std::printf("bookmark context: %u checks,0 failures\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"bookmark context FAILED:%s\n",e.what());return 1;}}
