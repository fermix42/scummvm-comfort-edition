/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../scene_runtime.h"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cstdio>
using namespace Snatcher;
namespace {unsigned checks;void check(bool b,const char* message){++checks;if(!b)throw std::runtime_error(message);}}
int main(int argc,char** argv){try{check(argc==2,"Supply the private original SUBCODE fixture");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> sub((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());std::string error;std::vector<uint16_t> text;check(readGameTextPalette(sub.data(),sub.size(),text,error),"original text palette");
 for(bool resumed:{false,true}){SceneRuntime scene;SceneLayout layout;layout.width=256;layout.height=224;layout.columns=32;layout.rows=64;layout.planeEnabled[1]=false;check(scene.configure(layout,error)&&scene.setPalette(std::vector<uint16_t>(128,0),0,error)&&scene.establishFreshPalette(error),"cold application palette");scene.establishBlankAssets();scene.establishFreshModuleDialogueContext();scene.establishFreshAnimationClock();scene.establishSceneFlags(0);check(scene.setTransferChannel(0,error),"native copy owner");
  if(resumed)check(!scene.queueScenarioGraphicsInitialization(sub.data(),sub.size(),error,true)&&scene.pendingGraphicsTasks()==0,"missing text owner rejects before mutating initialization");
  check(scene.setTextPalette(text,error)&&scene.queueScenarioGraphicsInitialization(sub.data(),sub.size(),error,resumed),"source EC5A packet");
  auto service=[&](){check(scene.beginFrame(false,error)&&scene.updateObjects(false,error),error.c_str());if(scene.graphicsServiceBusy())check(scene.commitTransfers(error),error.c_str());};for(unsigned i=0;i<4;++i)service();
  SceneResourcePreparation job;bool complete=false;for(unsigned i=0;i<8&&!complete;++i){check(scene.advanceResourcePreparation(job,complete,error),error.c_str());service();}check(complete,"bookmark preparation completes without a guessed delay");for(unsigned i=0;i<3;++i)service();
  for(unsigned i=0;i<8;++i)check(scene.paletteColors()[i]==(resumed?text[i]:0),"resume restores exactly the authored eight colors; fresh path stays separate");
  GraphicsTransfer glyph{};glyph.destination=32;glyph.decoded.assign(32,0x11);check(scene.applyTransfer(glyph,error)&&scene.fillPlaneCells(0,0,0,32,64,1,error),"positive glyph bitmap and map");SceneImage image;check(scene.render(kSceneRGB888,image,error),error.c_str());check(image.indexed[0]==1,"same indexed glyph exists in both branches");check(image.rgb[0]==(resumed?238:0)&&image.rgb[1]==(resumed?238:0)&&image.rgb[2]==(resumed?238:0),"old omitted request renders black; resumed request makes the same glyph visible");
 }
 std::printf("PASS %u original resume menu palette checks\n",checks);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL original resume menu: %s\n",e.what());return 1;}}
