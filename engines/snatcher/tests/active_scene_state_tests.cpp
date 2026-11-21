/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../scene_state.h"
#include <cstdio>
#include <stdexcept>
using namespace Snatcher;
namespace {unsigned checks;void check(bool b,const char* e){++checks;if(!b)throw std::runtime_error(e);}struct Source:SceneStateSource{bool loadSceneResource(uint16_t,std::vector<uint8_t>&,std::string&)override{return false;}};}
int main(){try{std::string error;Source source;SceneRuntime scene;SceneLayout layout;layout.width=256;layout.height=224;layout.columns=32;layout.rows=64;check(scene.configure(layout,error),"layout");scene.establishBlankAssets();scene.establishFreshAnimationClock();scene.establishFreshModuleDialogueContext();scene.establishSceneFlags(0);check(scene.setPalette(std::vector<uint16_t>(128,0),0,error)&&scene.setTransferChannel(0,error),"palette and allocation");
 for(unsigned mode:{0u,1u,2u,9u,0x14u,0x16u}){SceneRuntime original=scene;if(mode)check(original.requestVisualCommand(mode,error),error.c_str());else check(original.postDialogueView(0x0f,error),error.c_str());
  SceneGraphicsTask task;task.flags=2;GraphicsTransfer t{};t.destination=32;t.decoded.assign(8192,0x11);task.transfers.push_back(t);task.stagingOffsets.push_back(0);task.copyLengths.push_back(8192);task.stagingSources.emplace_back();check(original.queueGraphicsTasks({task},error),error.c_str());NativeSaveSection archive;check(SceneStateCodec::encode(original,archive,error,true,true),error.c_str());SceneRuntime restored;check(SceneStateCodec::decode(archive,source,restored,error),error.c_str());
  for(unsigned tick=0;tick<40;++tick){check(original.tick(false,error)&&restored.tick(false,error),error.c_str());NativeSaveSection a,b;check(SceneStateCodec::encode(original,a,error,true,true)&&SceneStateCodec::encode(restored,b,error,true,true)&&a.bytes==b.bytes,"inflight visual/copy/caption continuation must match every frame");SceneImage x,y;check(original.render(kSceneRGB888,x,error)&&restored.render(kSceneRGB888,y,error)&&x.rgb==y.rgb,"no partial restored graphics publication");}
 }
 std::printf("PASS %u active scene continuation checks\n",checks);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL active scene: %s\n",e.what());return 1;}}
