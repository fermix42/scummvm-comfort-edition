/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../decensor.h"
#include "../scene_runtime.h"
#include "../frame_publication.h"
#include "../runtime.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace Snatcher;
static unsigned checks=0;
static unsigned animatedFrames=0,guardedFixtures=0;
struct FixtureRandom:public NarrativeRandomSource {uint16_t value=0;bool next(uint16_t& out,std::string&)override{out=value++;return true;}};
static void require(bool ok,const std::string& reason){++checks;if(!ok)throw std::runtime_error(reason);}
static std::vector<uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);require(bool(f),"open fixture");return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});}
static uint64_t digest(const std::vector<uint8_t>& data){uint64_t h=UINT64_C(14695981039346656037);for(auto v:data)h=(h^v)*UINT64_C(1099511628211);return h;}
static uint64_t animate(std::vector<uint8_t> bytes,unsigned resource,unsigned list,const std::vector<uint8_t>& boot,const std::vector<uint8_t>& sub){
 SceneRuntime runtime;SceneLayout layout;std::string e;
 FixtureRandom random;runtime.setRandomSource(&random);
 auto expect=[&](bool ok,const char* operation){require(ok,std::string(operation)+": "+e);};
 require(readSceneLayoutPreset(boot.data()+512,boot.size()-512,kSceneTallLayout,layout,e),e);
 require(runtime.configure(layout,e)&&runtime.establishSceneReset(kSceneTallLayout,e)&&runtime.establishFreshPalette(e)&&runtime.setTransferChannel(0,e),e);
 runtime.establishBlankAssets();runtime.establishFreshModuleDialogueContext();runtime.establishFreshMusicContext();runtime.establishFreshAnimationClock();runtime.establishSceneFlags(0);runtime.setSoundContext(false,0);
 require(runtime.loadSharedGraphicsEffects(sub.data(),sub.size(),e),e);
 expect(runtime.install(uint16_t(resource),bytes,0,e),"inherited list0");
 for(unsigned i=0;i<32&&runtime.transferBusy();++i){expect(runtime.beginFrame(false,e),"initial publication");expect(runtime.commitTransfers(e),"initial transfers");}
 require(!runtime.transferBusy(),"bounded list0 completion");
 expect(runtime.install(uint16_t(resource),bytes,list,e),"install");
 for(unsigned i=0;i<32&&runtime.transferBusy();++i){expect(runtime.beginFrame(false,e),"target publication");expect(runtime.commitTransfers(e),"target transfers");}
 require(!runtime.transferBusy(),"bounded target completion");
 // Installed scene/animation programs must own their input, not borrow the
 // caller's resource vector or a temporary decensor output allocation.
 std::fill(bytes.begin(),bytes.end(),0xcd);std::vector<uint8_t>().swap(bytes);
 uint64_t hash=0;FramePublication publication;
 for(unsigned tick=0;tick<120;++tick){
  expect(runtime.tick(false,e),"tick");
  expect(runtime.commitTransfers(e),"transfer");
  expect(runtime.commitEnvironmentFrame(uint16_t(tick),e),"environment");
  SceneImage image;if(!runtime.render(kSceneRGB888,image,e)){
   // A detached display list may need a previous narrative allocation. Preserve
   // that limitation explicitly, rather than inventing assets or ignoring it.
   require(e=="scene tile references unavailable pattern/pixel"||e=="line sampling requires established original later-row history","unexpected render failure: "+e);
   ++guardedFixtures;std::cout<<"GUARDED resource="<<resource<<" list="<<list<<" tick="<<tick<<" reason="<<e<<"\n";
   return hash^digest(std::vector<uint8_t>(e.begin(),e.end()))^tick;
  }
  ++animatedFrames;
  require(image.width==256&&image.height==224&&image.rgb.size()==image.width*image.height*3,"render dimensions/format");
  require(runtime.pendingGraphicsTasks()<=7&&runtime.pendingGraphicsCopies()<=7,"bounded graphics tasks/copies");
  publication.prepare(unsigned(image.width),unsigned(image.height));
  for(size_t i=0;i<image.width*image.height;++i)publication.pixels()[i]=uint16_t((image.rgb[i*3]>>3)<<11|(image.rgb[i*3+1]>>2)<<5|(image.rgb[i*3+2]>>3));
  publication.publish();require(!publication.publish(),"same final frame cannot upload twice");
  hash=(hash*33)^digest(image.rgb);runtime.takeEffects();runtime.takeMusicCommands();
 }
 require(publication.uploads()<=120&&publication.unchanged()>=120,"bounded presentation uploads");
 return hash;
}
int main(int argc,char** argv){try{
 require(argc==5,"usage: decensor-lifecycle-tests sidecar manifest boot subcode");
 auto sidecar=read(argv[1]),boot=read(argv[3]),sub=read(argv[4]);require(boot.size()>512,"boot fixture");std::string e;std::vector<DecensorFile> files;require(parseDecensor(sidecar,files,e),e);
 // Synthetic bounds and aliasing contracts exercise the same production helper.
 DecensorFile f;f.sourceSize=4;f.targetSize=6;f.spans.push_back({3,{7,8,9}});
 std::vector<uint8_t> source={1,2,3,4},out;require(applyDecensor(f,source,out,e)&&out==std::vector<uint8_t>({1,2,3,7,8,9}),"growth boundary");
 auto expected=out;require(!applyDecensor(f,out,out,e)&&out==expected,"grown output cannot be reapplied as original");
 auto alias=source;require(applyDecensor(f,alias,alias,e)&&alias==expected,"input/output alias safe");
 f.sourceSize=6;require(applyDecensor(f,out,out,e)&&out==expected,"equal-sized repeat idempotent");
 f.spans[0].offset=UINT32_MAX;require(!applyDecensor(f,out,out,e)&&out==expected,"overflow rejected transactionally");
 std::ifstream manifest(argv[2]);std::string line;unsigned rows=0;uint64_t frameHash=0;
 while(std::getline(manifest,line)){
  std::stringstream row(line);std::string name,rawPath,expectedPath;std::getline(row,name,'\t');std::getline(row,rawPath,'\t');std::getline(row,expectedPath);if(!expectedPath.empty()&&expectedPath.back()=='\r')expectedPath.pop_back();
  auto raw=read(rawPath),gold=read(expectedPath);const DecensorFile* patch=nullptr;for(const auto& entry:files)if(entry.name==name)patch=&entry;require(patch!=nullptr,"approved fixture");
  for(unsigned cycle=0;cycle<256;++cycle){std::vector<uint8_t> result;require(applyDecensor(*patch,raw,result,e)&&result==gold,"repeated original-resource load");auto prior=result;bool repeated=applyDecensor(*patch,result,result,e);require((raw.size()==gold.size()?repeated&&! (result!=gold):!repeated&&result==prior),"target repeat idempotent or explicitly rejected");}
  ++rows;if(name=="SP25.BIN")continue;
  std::vector<unsigned> lists=name=="DATA_F0.BIN"?std::vector<unsigned>{11}:name=="DATA_H09.BIN"?std::vector<unsigned>{13}:name=="DATA_G0.BIN"?std::vector<unsigned>{4,5,6,7,10}:std::vector<unsigned>{6,7};
  unsigned resource=97;for(unsigned id=0;id<97;++id){std::string entry(reinterpret_cast<const char*>(sub.data()+0x432+id*16),16);while(!entry.empty()&&(entry.back()==0||entry.back()==' '))entry.pop_back();if(entry==name)resource=id;}require(resource<97,"original resource catalog identity");
  for(unsigned list:lists){
   // Destroy/recreate the real scene owner across off/on/off/on. Both repeated
   // modes must be bitwise deterministic even after caller storage is poisoned.
   uint64_t off=animate(raw,resource,list,boot,sub),on=animate(gold,resource,list,boot,sub);
   require(animate(raw,resource,list,boot,sub)==off,"off after on matches original");
   require(animate(gold,resource,list,boot,sub)==on,"on after off matches patched");
   frameHash^=off^on;std::cout<<name<<" list="<<list<<" off="<<off<<" on="<<on<<"\n";
  }
 }
 require(rows==5,"complete patch corpus");std::cout<<"PASS_WITH_LIMITS checks="<<checks<<" animated_frames="<<animatedFrames<<" guarded_fixtures="<<guardedFixtures<<" digest="<<frameHash<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
