/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../scene_assets.h"
#include <cstdio>
namespace {
int checks=0,failures=0;
void check(bool v,const char *s){++checks;if(!v){++failures;std::fprintf(stderr,"FAIL scene assets: %s\n",s);}}
void word(std::vector<uint8_t>& d,size_t off,unsigned value){d[off]=uint8_t(value>>8);d[off+1]=uint8_t(value);}
void pointer(std::vector<uint8_t>& d,size_t off,unsigned value){word(d,off,value>>16);word(d,off+2,value);}
std::vector<uint8_t> fixture(){
 std::vector<uint8_t> d(128,0);pointer(d,0,0x28008);pointer(d,4,0x28010);word(d,16,2);d[18]=1;d[19]=10;pointer(d,20,0x28040);word(d,24,2);word(d,26,0x2000);word(d,28,0xffff);
 d[30]=2;d[31]=12;d[32]=7;d[33]=3;word(d,34,0xfffe);word(d,36,10);pointer(d,38,0x28008);word(d,42,0);d[44]=0x80;
 const uint8_t stream[]={0,3,8,'A','B','C',0x1f};for(unsigned i=0;i<sizeof(stream);++i)d[64+i]=stream[i];return d;
}
}
int runSceneAssetsTests(){
 using namespace Snatcher;checks=failures=0;std::string error;std::vector<uint8_t> d=fixture();SceneDirectory directory;SceneAssets assets;
 check(readSceneDirectory(&d[0],d.size(),directory,error)&&directory.tableOffset==16&&directory.tableBytes==2&&directory.gapBytes==0&&directory.relativeEntries==std::vector<uint16_t>(1,2),"bounded directory prefix");
 check(parseSceneAssets(&d[0],d.size(),0,1024,assets,error)==kSceneAssetsReady&&assets.listBegin==18&&assets.listEnd==45&&assets.terminated&&assets.terminator==0x80,"complete specified asset list");
 check(assets.commands.size()==2&&assets.commands[0].payloadBegin==20&&assets.commands[0].payloadEnd==30&&assets.commands[1].family==2,"physical command boundaries");
 check(assets.transfers.size()==1&&assets.transfers[0].sourceOffset==64&&assets.transfers[0].encodedEnd==71&&assets.transfers[0].destination==0x2000&&assets.transfers[0].delay==2&&assets.transfers[0].decoded==std::vector<uint8_t>({'A','B','C'}),"decoded transfer identity and bytes");
 check(assets.actors.size()==1&&assets.actors[0].actorIndex==7&&assets.actors[0].flags==3&&assets.actors[0].x==-2&&assets.actors[0].y==10&&assets.actors[0].frameTableOffset==8&&assets.actors[0].sequenceOffset==44,"all actor fields");
 check(parseSceneAssets(&d[0],d.size(),0,3,assets,error)==kSceneAssetsReady,"exact decoded budget");check(parseSceneAssets(&d[0],d.size(),0,2,assets,error)==kSceneAssetsInvalid&&assets.transfers.empty()&&!assets.terminated&&!error.empty(),"decoded budget transactional failure");
 d[30]=3;check(parseSceneAssets(&d[0],d.size(),0,1024,assets,error)==kSceneAssetsInvalid&&assets.transfers.empty(),"invalid actor-link packet rejects transactionally");
 d[30]=10;check(parseSceneAssets(&d[0],d.size(),0,1024,assets,error)==kSceneAssetsInvalid&&assets.commands.empty(),"unestablished physical family rejects");
 d=fixture();d[19]=255;check(parseSceneAssets(&d[0],d.size(),0,1024,assets,error)==kSceneAssetsInvalid,"truncated display payload");
 d=fixture();check(parseSceneAssets(&d[0],d.size(),1,1024,assets,error)==kSceneAssetsInvalid,"index outside candidate prefix");check(parseSceneAssets(&d[0],d.size(),256,1024,assets,error)==kSceneAssetsInvalid,"index encoding bound");
 d[32]=64;check(parseSceneAssets(&d[0],d.size(),0,1024,assets,error)==kSceneAssetsInvalid&&assets.transfers.empty(),"actor failure rolls back transfers");
 d=fixture();std::vector<GraphicsTransfer> transfers;check(!readGraphicsTransfers(&d[0],d.size(),20,31,1024,transfers,error)&&transfers.empty(),"exact terminator forbids trailing payload");
 check(!readGraphicsTransfers(&d[0],d.size(),20,28,1024,transfers,error),"missing transfer terminator");d[20]=0;d[21]=0;d[22]=0;d[23]=1;check(!readGraphicsTransfers(&d[0],d.size(),20,30,1024,transfers,error),"invalid transfer pointer");
 d=fixture();d[66]=0xff;check(!readGraphicsTransfers(&d[0],d.size(),20,30,1024,transfers,error),"corrupt compressed stream");
 d=fixture();check(!readGraphicsTransfers(&d[0],66,20,30,1024,transfers,error),"short compressed source");d[18]=1;d[19]=2;d[20]=d[21]=255;d[22]=255;check(parseSceneAssets(&d[0],d.size(),0,1,assets,error)==kSceneAssetsReady&&assets.transfers.empty()&&assets.terminator==255,"empty transfer queue and high-bit list terminator");
 d=fixture();word(d,16,8);word(d,18,0);check(readSceneDirectory(&d[0],d.size(),directory,error)&&directory.tableBytes==2&&directory.gapBytes==6,"unresolved directory gap preserved");
 d=fixture();pointer(d,0,0x2800c);pointer(d,8,0x28008);check(!readSceneDirectory(&d[0],d.size(),directory,error)&&directory.relativeEntries.empty(),"invalid third section pointer");
 for(size_t n=0;n<8;++n)check(!readSceneDirectory(&d[0],n,directory,error),"short container header");
 d=fixture();d.resize(40000);word(d,16,0x8000);check(!readSceneDirectory(&d[0],d.size(),directory,error),"negative displacement is not an unsigned forward candidate");
 d=fixture();d.resize(18+4096*2+1);for(size_t i=18;i+1<d.size();i+=2){d[i]=0;d[i+1]=0;}d.back()=128;check(parseSceneAssets(&d[0],d.size(),0,1024,assets,error)==kSceneAssetsInvalid,"bounded physical command work");
 ActorController ownedActors;PaletteController ownedPalettes;d=fixture();check(parseSceneAssets(&d[0],d.size(),0,1024,assets,error)==kSceneAssetsReady&&applySceneActorsAndPalettes(assets,ownedActors,ownedPalettes,error)&&ownedActors.state(7)->active,"ordered scene actor ownership installation");
 check(applySceneActorsAndPalettes(assets,ownedActors,ownedPalettes,error)&&ownedActors.state(7)->active,"active low-word actor reinitializes from ordered scene packet");check(ownedActors.reserve(7,true,error)&&!applySceneActorsAndPalettes(assets,ownedActors,ownedPalettes,error)&&ownedActors.state(7)->reserved,"reserved high-byte actor stops without partial commit");
 SceneAssets order;SceneAssetCommand link={3,0,2,4},init={2,4,6,18};order.commands.push_back(link);order.commands.push_back(init);order.links.push_back(ActorLink{1,2});order.actors.push_back(ActorInitializer{2,0,0,0,0,0});ownedActors.clear();check(applySceneActorsAndPalettes(order,ownedActors,ownedPalettes,error)&&ownedActors.state(1)->head==2&&ownedActors.state(2)->parent==0,"link before initialization preserves original clear ordering");
 order.commands.clear();order.commands.push_back(init);order.commands.push_back(link);ownedActors.clear();check(applySceneActorsAndPalettes(order,ownedActors,ownedPalettes,error)&&ownedActors.state(2)->parent==1,"initialization before link differs as original");
 std::vector<uint8_t> taskBytes(128,0);taskBytes[0]=0x82;pointer(taskBytes,2,0x28040);word(taskBytes,6,0xe400);word(taskBytes,8,0x700);word(taskBytes,10,32);word(taskBytes,12,0xffff);for(unsigned i=0;i<32;++i)taskBytes[64+i]=uint8_t(i);SceneGraphicsTask task;
 check(readSceneGraphicsTask(taskBytes.data(),taskBytes.size(),0,0x28000,task,error)&&task.transfers.size()==1&&task.transfers[0].destination==0xe400&&task.transfers[0].decoded[31]==31&&task.stagingOffsets[0]==0x700,"source-owned raw graphics task preserves allocation and bytes");
 SceneGraphicsQueue queue;for(unsigned i=0;i<7;++i)check(queue.enqueue(task,error),"seven original graphics task slots");check(!queue.enqueue(task,error)&&queue.pending()==7,"full graphics task queue rejects without replacement");std::vector<GraphicsTransfer> ready;check(queue.tick(true,ready,error)&&ready.empty()&&queue.pending()==7,"resource busy preserves task ownership");check(queue.tick(false,ready,error)&&ready.size()==1&&queue.pending()==6,"ordinary task stops source slot scan");queue.clear();task.flags=0x42;for(unsigned i=0;i<7;++i)queue.enqueue(task,error);check(queue.tick(false,ready,error)&&ready.size()==7&&queue.pending()==0,"parallel tasks permit later original slots");
 taskBytes[0]=3;check(!readSceneGraphicsTask(taskBytes.data(),taskBytes.size(),0,0x28000,task,error)&&task.transfers.empty(),"dependent task semantics remain unsupported");taskBytes[0]=0x82;word(taskBytes,10,9000);check(!readSceneGraphicsTask(taskBytes.data(),taskBytes.size(),0,0x28000,task,error),"task staging extent bounded before copy");word(taskBytes,10,32);pointer(taskBytes,2,0x27fff);check(!readSceneGraphicsTask(taskBytes.data(),taskBytes.size(),0,0x28000,task,error),"task source cannot escape its resource owner");
 taskBytes[0]=0x81;pointer(taskBytes,2,0);word(taskBytes,8,0);check(readSceneGraphicsTask(taskBytes.data(),taskBytes.size(),0,0x28000,task,error)&&task.transfers[0].decoded.empty(),"compressed reuse ignores unowned source address and retains staging dependency");queue.clear();check(queue.enqueue(task,error)&&!queue.tick(false,ready,error)&&queue.pending()==1&&ready.empty(),"unknown inherited staging cannot become guessed bytes");check(queue.setStaging(0,std::vector<uint8_t>(32,0x25),error)&&queue.tick(false,ready,error)&&ready[0].decoded==std::vector<uint8_t>(32,0x25),"source-owned staging resolves reuse transactionally");
 check(queue.enqueue(task,error)&&queue.pending()==1,"queue task before resource cleanup");queue.cancelPending();check(queue.pending()==0&&queue.enqueue(task,error)&&queue.tick(false,ready,error)&&ready[0].decoded==std::vector<uint8_t>(32,0x25),"resource cleanup cancels tasks but preserves decoded-source cache ownership");
 std::vector<uint8_t> shared(128,0);shared[0]=3;pointer(shared,2,0x28040);word(shared,6,0x2000);word(shared,8,0);word(shared,10,4);word(shared,12,0x2040);word(shared,14,4);word(shared,16,4);word(shared,18,0xffff);word(shared,64,8);shared[66]=0;for(unsigned i=0;i<8;++i)shared[67+i]=uint8_t(i+1);shared[75]=1;shared[76]=0x1f;check(readSceneGraphicsTask(shared.data(),shared.size(),0,0x28000,task,error)&&task.transfers.size()==2,"shared-source task parses one pointer and six-byte copies");queue.clear();check(queue.enqueue(task,error)&&queue.tick(false,ready,error)&&ready.size()==2&&ready[0].decoded==std::vector<uint8_t>({1,2,3,4})&&ready[1].decoded==std::vector<uint8_t>({5,6,7,8}),"source shared compressed cache fans out to both original allocations");
 SceneGraphicsTask dissolve;dissolve.flags=0x44;GraphicsTransfer allocation;allocation.destination=0;allocation.delay=0;dissolve.transfers.push_back(allocation);dissolve.stagingOffsets.push_back(0);dissolve.copyLengths.push_back(64);dissolve.stagingSources.push_back(std::vector<uint8_t>(32,255));queue.clear();check(queue.enqueue(dissolve,error)&&!queue.tick(false,ready,error)&&queue.pending()==1&&ready.empty(),"pixel dissolve without source pattern rejects transactionally");uint8_t pattern[16];for(unsigned i=0;i<8;++i){pattern[i*2]=uint8_t(i/2);pattern[i*2+1]=uint8_t(i&1?0xf0:0x0f);}check(queue.setDissolvePattern(pattern,16,error),"synthetic complete nibble permutation establishes effect owner");for(unsigned i=0;i<65;++i){check(queue.tick(false,ready,error)&&ready.size()==1&&ready[0].decoded.size()==64,"source pixel dissolve produces bounded copy every update");if(i==0)check(!ready[0].knownBytes[32]&&ready[0].knownBytes[0],"uninitialized cache tail remains explicitly unknown during partial clear");if(i==63)check(ready[0].decoded==std::vector<uint8_t>(64,0)&&ready[0].knownBytes==std::vector<bool>(64,true)&&queue.pending()==1,"all pixels known zero after sixty-four passes but owner still active");}check(queue.pending()==0,"source sixty-fifth dissolve update retires task");pattern[0]=4;check(!queue.setDissolvePattern(pattern,16,error),"invalid original effect mask cannot replace owned pattern");
 uint8_t wipeHeader[]={0x25,0};check(readSceneGraphicsTask(wipeHeader,2,0,0xd400,task,error)&&task.flags==0x25&&task.copyLengths[0]==32,"source tile-fill task has header-only semantic format");queue.clear();pattern[0]=0;check(queue.setDissolvePattern(pattern,16,error)&&queue.enqueue(task,error),"owned tile fill sequence");for(unsigned i=0;i<65;++i)check(queue.tick(false,ready,error)&&ready.size()==1,"tile fill publishes every source effect pass");check(ready[0].decoded==std::vector<uint8_t>(32,0x77)&&queue.pending()==0,"source tile fill reaches color7 after65passes");std::vector<uint8_t> tileClear(12,0);tileClear[0]=0xa4;word(tileClear,10,32);check(readSceneGraphicsTask(tileClear.data(),tileClear.size(),0,0xd400,task,error)&&queue.enqueue(task,error),"source tile clear single allocation does not require nonexistent terminator");for(unsigned i=0;i<65;++i)check(queue.tick(false,ready,error),"tile clear consumes known prior fill");check(ready[0].decoded==std::vector<uint8_t>(32,0)&&queue.pending()==0,"source tile clear retiresknowntransparent after65passes");
 std::printf("scene assets: %d checks, %d failures\n",checks,failures);return failures;
}
#ifdef SNATCHER_SCENE_ASSETS_TEST_MAIN
#include <fstream>
#include <map>
#include <sstream>
namespace {
std::vector<std::string> columns(const std::string& line){std::vector<std::string> values;std::istringstream in(line);std::string v;while(std::getline(in,v,'\t'))values.push_back(v);return values;}
bool load(const std::string& path,std::vector<uint8_t>& data){std::ifstream f(path.c_str(),std::ios::binary);if(!f)return false;f.seekg(0,std::ios::end);std::streamoff length=f.tellg();if(length<=0||length>16*1024*1024)return false;f.seekg(0);data.resize(size_t(length));return bool(f.read(reinterpret_cast<char*>(&data[0]),length));}
bool emitTransfers(const std::string& identity,const std::vector<Snatcher::GraphicsTransfer>& transfers,std::ofstream& table,std::ofstream& pixels,size_t& pixelOffset){
 for(size_t i=0;i<transfers.size();++i){const Snatcher::GraphicsTransfer& t=transfers[i];table<<identity<<'\t'<<i<<'\t'<<t.recordOffset<<'\t'<<t.sourceOffset<<'\t'<<t.encodedEnd<<'\t'<<t.destination<<'\t'<<t.delay<<'\t'<<t.decoded.size()<<'\t'<<pixelOffset<<'\n';if(!t.decoded.empty())pixels.write(reinterpret_cast<const char*>(&t.decoded[0]),t.decoded.size());pixelOffset+=t.decoded.size();}
 return bool(table)&&bool(pixels);
}
}
int main(int argc,char **argv){
 if(argc==1)return runSceneAssetsTests()?1:0;
 if(argc!=4||std::string(argv[1])!="--batch"){std::fprintf(stderr,"Usage: scene-assets-tests [--batch MANIFEST PRIVATE_OUTPUT_DIRECTORY]\n");return 2;}
 std::ifstream manifest(argv[2]);std::string root=argv[3];std::ofstream directories((root+"/directories.tsv").c_str()),lists((root+"/lists.tsv").c_str()),effects((root+"/effects.tsv").c_str()),paletteUpdates((root+"/palette-updates.tsv").c_str()),commands((root+"/commands.tsv").c_str()),actors((root+"/actors.tsv").c_str()),transfers((root+"/transfers.tsv").c_str()),pixels((root+"/decoded.bin").c_str(),std::ios::binary);
 if(!manifest||!directories||!lists||!effects||!paletteUpdates||!commands||!actors||!transfers||!pixels)return 2;
 std::map<std::string,std::vector<uint8_t> > cache;size_t pixelOffset=0,requests=0;std::string line;
 while(std::getline(manifest,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();std::vector<std::string> c=columns(line);if(c.size()!=5)return 2;
  if(!cache.count(c[2])&&!load(c[2],cache[c[2]]))return 2;const std::vector<uint8_t>& data=cache[c[2]];std::string error;
  if(c[0]=="D"){
   Snatcher::SceneDirectory d;if(!Snatcher::readSceneDirectory(&data[0],data.size(),d,error))return 3;
   directories<<c[1]<<'\t'<<d.tableOffset<<'\t'<<d.tableBytes<<'\t'<<d.gapBytes<<'\t';for(size_t i=0;i<d.relativeEntries.size();++i)directories<<d.relativeEntries[i]<<',';directories<<'\n';
  }else if(c[0]=="T"){
   std::vector<Snatcher::GraphicsTransfer> staged;if(!Snatcher::readGraphicsTransfers(&data[0],data.size(),std::stoul(c[3]),std::stoul(c[4]),4*1024*1024,staged,error)||!emitTransfers(c[1],staged,transfers,pixels,pixelOffset))return 3;
  }else if(c[0]=="P"){
   Snatcher::PaletteEffect effect;Snatcher::PaletteController palette;
   if(!Snatcher::readPaletteEffect(&data[0],data.size(),std::stoul(c[3]),std::stoul(c[4]),effect,error)||!palette.enqueue(effect,error))return 3;
   for(unsigned tick=0;tick<64;++tick){if(!palette.tick(error))return 3;paletteUpdates<<c[1]<<'\t'<<tick<<'\t'<<palette.activeEffects();for(size_t i=0;i<palette.colors().size();++i)paletteUpdates<<'\t'<<palette.colors()[i];paletteUpdates<<'\n';}
  }else if(c[0]=="L"){
   Snatcher::SceneAssets s;Snatcher::SceneAssetResult result=Snatcher::parseSceneAssets(&data[0],data.size(),unsigned(std::stoul(c[3])),4*1024*1024,s,error);
   lists<<c[1]<<'\t'<<int(result)<<'\t'<<s.listBegin<<'\t'<<s.listEnd<<'\t'<<s.terminated<<'\t'<<unsigned(s.terminator)<<'\t'<<s.unsupportedOpcode<<'\t'<<s.unsupportedOffset<<'\t'<<s.commands.size()<<'\t'<<s.transfers.size()<<'\t'<<s.actors.size()<<'\n';
   for(size_t i=0;i<s.commands.size();++i){const Snatcher::SceneAssetCommand& q=s.commands[i];commands<<c[1]<<'\t'<<i<<'\t'<<unsigned(q.family)<<'\t'<<q.offset<<'\t'<<q.payloadBegin<<'\t'<<q.payloadEnd<<'\n';}
   for(size_t i=0;i<s.links.size();++i)effects<<c[1]<<"\tlink\t"<<i<<'\t'<<s.links[i].parent<<'\t'<<s.links[i].child<<'\n';
   for(size_t i=0;i<s.controls.size();++i)effects<<c[1]<<"\tcontrol\t"<<i<<'\t'<<unsigned(s.controls[i].family)<<'\t'<<s.controls[i].value<<'\t'<<s.controls[i].commandOffset<<'\n';
   for(size_t i=0;i<s.palettes.size();++i){const Snatcher::PaletteEffect&p=s.palettes[i];effects<<c[1]<<"\tpalette\t"<<i<<'\t'<<unsigned(p.type)<<'\t'<<unsigned(p.delay)<<'\t'<<unsigned(p.byteOffset)<<'\t'<<unsigned(p.countMinusOne)<<'\t'<<p.replaceCycling;for(size_t j=0;j<p.frames.size();++j){effects<<'\t';for(size_t k=0;k<p.frames[j].size();++k)effects<<p.frames[j][k]<<',';}effects<<'\n';}
   if(!emitTransfers(c[1],s.transfers,transfers,pixels,pixelOffset))return 3;
   for(size_t i=0;i<s.actors.size();++i){const Snatcher::ActorInitializer& a=s.actors[i];actors<<c[1]<<'\t'<<i<<'\t'<<a.actorIndex<<'\t'<<unsigned(a.flags)<<'\t'<<a.x<<'\t'<<a.y<<'\t'<<a.frameTableOffset<<'\t'<<a.sequenceOffset<<'\n';}
  }else return 2;++requests;
 }
 directories.close();lists.close();effects.close();paletteUpdates.close();commands.close();actors.close();transfers.close();pixels.close();
 if(manifest.bad()||!directories||!lists||!effects||!paletteUpdates||!commands||!actors||!transfers||!pixels)return 2;
 std::printf("scene asset corpus: %zu requests, %zu decoded bytes\n",requests,pixelOffset);return 0;
}
#endif
