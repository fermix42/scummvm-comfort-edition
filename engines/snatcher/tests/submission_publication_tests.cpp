/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../scene_state.h"
#include <cstdio>
#include <stdexcept>
namespace {
unsigned checks;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
void word(std::vector<uint8_t>& b,size_t p,unsigned v){b[p]=uint8_t(v>>8);b[p+1]=uint8_t(v);}
void pointer(std::vector<uint8_t>& b,size_t p,unsigned v){word(b,p,v>>16);word(b,p+2,v);}
struct Source:Snatcher::SceneStateSource {
 std::vector<uint8_t> data;
 bool loadSceneResource(uint16_t id,std::vector<uint8_t>& out,std::string&)override{if(id!=22)return false;out=data;return true;}
};
}
int main(){using namespace Snatcher;try{
 Source source;auto& b=source.data;b.assign(4096,0);pointer(b,0,0x28008);pointer(b,4,0x28020);word(b,32,2);b[34]=2;b[35]=12;b[36]=29;pointer(b,42,0x28080);word(b,46,512-48);b[48]=0x80;
 for(unsigned i=0;i<128;++i)word(b,128+2*i,272);word(b,130,282);
 word(b,406,1);word(b,416,2);b[515]=1;b[516]=1;b[519]=1;b[520]=0x81;
 SceneRuntime scene;std::string error;check(scene.establishBootSubmissionHistory(error),"cold source allocation");SceneLayout layout;layout.width=layout.height=16;layout.columns=layout.rows=32;layout.planeEnabled[0]=layout.planeEnabled[1]=false;check(scene.configure(layout,error),"fixture layout");scene.establishBlankAssets();std::vector<uint16_t> colors(128,0);colors[1]=0x000e;colors[2]=0x00e0;check(scene.setPalette(colors,0,error)&&scene.setTransferChannel(0,error),"source color ownership");scene.establishFreshAnimationClock();scene.establishFreshModuleDialogueContext();scene.establishSceneFlags(0);
 GraphicsTransfer transfer;transfer.destination=32;transfer.decoded.assign(64,0x11);for(unsigned i=32;i<64;++i)transfer.decoded[i]=0x22;check(scene.applyTransfer(transfer,error),"independent pattern data");check(scene.install(22,b,0,error),"source actor program");
 auto pixel=[&](SceneRuntime& owner,unsigned expected){SceneImage image;check(owner.render(kSceneRGB888,image,error),error.c_str());check(image.indexed.size()==256,"full fixture frame");for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)check(image.indexed[y*16+x]==(x<8&&y<8?expected:0),"source submission pixel ownership");};
 auto step=[&](SceneRuntime& owner,unsigned counter){check(owner.beginFrame(false,error,int(counter))&&owner.updateObjects(false,error)&&owner.commitEnvironmentFrame(uint16_t(counter),error),error.c_str());};
 step(scene,0);pixel(scene,0);check(scene.animation(29)->frameIndex==0,"computed first actor differs from blank display");
 step(scene,1);pixel(scene,1);check(scene.animation(29)->frameIndex==1,"new frame remains pending");SceneImage filtered;check(scene.render(kSceneRGB888,filtered,error,29),"display adaptation filters original actor ownership");for(uint8_t value:filtered.indexed)check(!value,"hidden published actor does not require changing logical controls");pixel(scene,1);
 NativeSaveSection packet;check(SceneStateCodec::encode(scene,packet,error),error.c_str());check(packet.bytes[3]=='T',"separate submission schema");SceneRuntime restored;check(SceneStateCodec::decode(packet,source,restored,error),error.c_str());pixel(restored,1);
 check(restored.render(kSceneRGB888,filtered,error,29),"restored actor ownership filters published geometry");for(uint8_t value:filtered.indexed)check(!value,"restored display exclusion preserves owner identity");
 NativeSaveSection malformed=packet;malformed.bytes[malformed.bytes.size()-14]=65;check(!SceneStateCodec::decode(malformed,source,restored,error),"invalid pending piece owner rejected");pixel(restored,1);malformed=packet;malformed.bytes[malformed.bytes.size()-15]=65;check(!SceneStateCodec::decode(malformed,source,restored,error),"invalid displayed piece owner rejected");pixel(restored,1);malformed=packet;malformed.bytes[malformed.bytes.size()-34]=65;check(!SceneStateCodec::decode(malformed,source,restored,error),"oversized displayed submission rejected transactionally");pixel(restored,1);
 check(restored.beginFrame(true,error)&&restored.updateObjects(true,error),"pause retains publication boundary");pixel(restored,1);
 for(unsigned i=2;i<12;++i){step(scene,i);step(restored,i);pixel(scene,i%2?1:2);pixel(restored,i%2?1:2);NativeSaveSection a,c;check(SceneStateCodec::encode(scene,a,error)&&SceneStateCodec::encode(restored,c,error)&&a.bytes==c.bytes,"restored future pending/displayed owners exact");}
 SceneActorPatch hidden;hidden.fields=SceneActorPatch::kFlags;hidden.flags=3;check(scene.patchActor(29,hidden,error),"source hidden control");pixel(scene,1);step(scene,12);pixel(scene,2);step(scene,13);pixel(scene,0);
 // A module's actor clear cannot retroactively change the uploaded list.
 step(restored,12);pixel(restored,2);restored.resetForModule();check(restored.configure(layout,error)&&restored.setPalette(colors,0,error),"module renderer owner");restored.establishBlankAssets();check(restored.applyTransfer(transfer,error),"module resource patterns");pixel(restored,2);check(restored.beginFrame(false,error),"module publishes prior completed submission");pixel(restored,1);check(restored.commitEnvironmentFrame(13,error)&&restored.beginFrame(false,error),"cleared module produces next empty list");pixel(restored,0);
 std::printf("submission publication: %u checks passed\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"submission publication failure after %u checks: %s\n",checks,e.what());return 1;}}
