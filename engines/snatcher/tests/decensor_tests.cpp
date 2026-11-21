/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../decensor.h"
#include "../scene_assets.h"
#include "../scenario.h"
#include "../narrative_state.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <cstdlib>
using namespace Snatcher;
static unsigned checks=0;
void check(bool pass,const char* message){++checks;if(!pass){std::cerr<<message<<"\n";std::exit(1);}}
std::vector<uint8_t> read(const std::string& path){std::ifstream file(path.c_str(),std::ios::binary);check(bool(file),"open fixture");return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),{});}
int main(int argc,char** argv){
 check(argc==3,"usage: decensor-tests pack manifest");auto bytes=read(argv[1]);std::string error;std::vector<DecensorFile> files;
 check(parseDecensor(bytes,files,error)&&files.size()==5,"parse private corpus");
 std::ifstream manifest(argv[2]);std::string line;unsigned count=0;
 while(std::getline(manifest,line)){std::stringstream row(line);std::string name,original,expected;std::getline(row,name,'\t');std::getline(row,original,'\t');std::getline(row,expected);if(!expected.empty()&&expected.back()=='\r')expected.pop_back();auto f=std::find_if(files.begin(),files.end(),[&](const DecensorFile& f){return f.name==name;});check(f!=files.end(),"resource in pack");auto source=read(original),gold=read(expected);std::vector<uint8_t> target;check(applyDecensor(*f,source,target,error)&&target==gold,"exact intended resource bytes");check(source==read(original),"source immutable");source.pop_back();auto before=target;check(!applyDecensor(*f,source,target,error)&&target==before,"wrong source rejected atomically");auto bad=*f;bad.spans[0].offset=bad.targetSize;check(!applyDecensor(bad,read(original),target,error)&&target==before,"bounds failure atomic");++count;}
 check(count==5,"all five resources");
 // Exercise the actual native asset consumer on the restored presentations,
 // including Alice's new animation list and all shower entry/return lists.
 std::ifstream corpus(argv[2]);
 while(std::getline(corpus,line)){
  std::stringstream row(line);std::string name,original,expected;std::getline(row,name,'\t');std::getline(row,original,'\t');std::getline(row,expected);if(!expected.empty()&&expected.back()=='\r')expected.pop_back();auto data=read(expected);
  if(name=="SP25.BIN"){Scenario scenario,old;auto raw=read(original);check(parseScenario(data.data(),data.size(),scenario,error)&&validateLocalTargets(scenario,error)&&parseScenario(raw.data(),raw.size(),old,error),"restored Alice scenario/control flow");
   NarrativeSnapshot n;n.scenarioStamps.assign(39,1);n.scenarioStamps[25]=narrativeScenarioStamp(old);n.cursor=n.entry=Cursor(64,0);auto originalState=n;
   check(rebindNarrativeScenario(n,25,old,scenario,error)&&n.scenarioStamps[25]==narrativeScenarioStamp(scenario),"verified Alice pair migrates unrelated continuation");
   n=originalState;n.scenarioStamps[25]^=1;check(!rebindNarrativeScenario(n,25,old,scenario,error),"unknown scenario stamp rejects");
   for(unsigned owner=0;owner<5;++owner){n=originalState;Cursor changed(82,1159);if(owner==0)n.cursor=changed;if(owner==1)n.entry=changed;if(owner==2)n.work.push_back(NarrativeWork(changed));if(owner==3)n.returns.push_back(changed);if(owner==4){RuntimeRequest r;r.owner=changed;n.requests.push_back(r);}check(!rebindNarrativeScenario(n,25,old,scenario,error)&&n.scenarioStamps==originalState.scenarioStamps,"changed active instruction cannot be reinterpreted");}
   continue;}
  std::vector<unsigned> lists=name=="DATA_F0.BIN"?std::vector<unsigned>{11}:name=="DATA_H09.BIN"?std::vector<unsigned>{13}:name=="DATA_G0.BIN"?std::vector<unsigned>{4,5,6,7,10}:std::vector<unsigned>{6,7};
  for(unsigned index:lists){SceneAssets assets;check(parseSceneAssets(data.data(),data.size(),index,16*1024*1024,assets,error)==kSceneAssetsReady,"native restored graphics staging");check(assets.terminated&&!assets.transfers.empty(),"restored transfers and list boundary");}
 }
 for(size_t n=0;n<bytes.size();++n){std::vector<uint8_t> truncated(bytes.begin(),bytes.begin()+n);std::vector<DecensorFile> rejected;check(!parseDecensor(truncated,rejected,error)&&rejected.empty(),"reject every truncated prefix");}
 auto bad=bytes;bad.push_back(0);check(!parseDecensor(bad,files,error)&&files.empty(),"reject trailing payload");bad=bytes;bad[0]^=1;check(!parseDecensor(bad,files,error),"reject version");bad=bytes;bad[8]=4;check(!parseDecensor(bad,files,error),"reject partial scope");bad=bytes;bad[16]='X';check(!parseDecensor(bad,files,error),"reject unapproved file");
 std::cout<<checks<<" decensor checks passed\n";
}
