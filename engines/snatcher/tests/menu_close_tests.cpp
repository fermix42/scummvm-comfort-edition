/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../scene_runtime.h"
#include <cstdio>
#include <cstdlib>
using namespace Snatcher;
namespace {unsigned checks;void check(bool x,const char* label){++checks;if(!x){std::fprintf(stderr,"Menu close: %s\n",label);std::exit(1);}}}
int main(){std::string e;for(bool alternate:{false,true}){
 SceneRuntime scene;SceneLayout layout;layout.width=256;layout.height=224;layout.columns=32;layout.rows=64;layout.planeEnabled[1]=false;
 check(scene.configure(layout,e)&&scene.establishSceneReset(kSceneTallLayout,e),"tall source layout");scene.establishBlankAssets();scene.establishFreshModuleDialogueContext();std::vector<uint16_t> colors(128);colors[1]=0xeee;colors[2]=0x00e;check(scene.setPalette(colors,0,e),"known colors");GraphicsTransfer t;t.destination=32;t.decoded.assign(64,0x11);for(unsigned i=32;i<64;++i)t.decoded[i]=0x22;check(scene.applyTransfer(t,e)&&scene.fillPlaneCells(0,0,0,32,32,1,e)&&scene.fillPlaneCells(0,0,32,32,32,2,e),"distinct lower-map fixture");
 check(scene.requestVisualCommand(alternate?0x12:0x0d,e),"open menu");for(unsigned n=0;scene.visualBusy()&&n<16;++n)check(scene.beginFrame(false,e),e.c_str());check(!scene.visualBusy(),"opened owner settled");
 check(alternate?scene.requestVisualCommand(0x13,e):scene.requestMenuClose(e),"source close request");auto prior=scene.presentationLayout().viewBands;check(scene.beginFrame(true,e)&&scene.visualBusy()&&scene.presentationLayout().viewBands.size()==prior.size(),"paused close stays pending");
 // Constants derive from Main38F8/3916 and3AB2/3ACA. Compare the complete
 // canonical raster to an independently computed plane-map sample at each phase.
 for(unsigned phase=0;phase<8;++phase){check(scene.beginFrame(false,e),e.c_str());int origin=alternate?240:256,threshold=140;if(phase<=6){origin-=int(phase)*19;if(origin<144)origin=144;threshold+=int(phase)*19;}
  SceneImage image;check(scene.render(kSceneReferenceRGB565,image,e),e.c_str());for(unsigned y=0;y<224;++y)for(unsigned x=0;x<256;++x){unsigned sy=y;if(phase<=6&&y>=(alternate?160u:unsigned(threshold+1)))sy=(y+unsigned(origin))%512;uint8_t expected=sy<256?1:2;check(image.indexed[y*256+x]==expected,"source close split full-image equality");}
  check(scene.visualBusy(),"cleanup still awaits earlier worker report");
 }
 check(scene.beginFrame(false,e)&&!scene.visualBusy()&&scene.presentationLayout().viewBands.empty(),"close report releases ordinary continuation");uint8_t command;check(scene.visualCommand(command)&&command==0,"close resets primary mailbox");
 }
 std::printf("Menu close: %u checks,0 failures; both layouts, init/19px steps/cleanup/report and full224-row rasters\n",checks);}
