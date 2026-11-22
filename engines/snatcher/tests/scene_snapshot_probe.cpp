/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../scene_state.h"
#include <fstream>
#include <iterator>
#include <iostream>
using namespace Snatcher;
static bool read(const std::string& path,std::vector<uint8_t>& out){std::ifstream f(path.c_str(),std::ios::binary);if(!f)return false;out.assign(std::istreambuf_iterator<char>(f),{});return !out.empty()&&out.size()<=16*1024*1024;}
struct Source:SceneStateSource{
 std::string root;std::vector<uint8_t> sub;
 bool loadSystemResource(std::vector<uint8_t>& b,std::string& e)override{b=sub;return !b.empty();}
 bool loadSceneResource(uint16_t id,std::vector<uint8_t>& b,std::string& e)override{if(id>=97||sub.size()<0x432+97*16){e="resource catalog unavailable";return false;}std::string name(reinterpret_cast<const char*>(sub.data()+0x432+id*16),16);name=name.substr(0,name.find(char(0)));while(!name.empty()&&name.back()==' ')name.pop_back();if(!read(root+"/"+name,b)){e="cannot read "+name;return false;}return true;}
};
int main(int argc,char** argv){
 if(argc!=4){std::cerr<<"scene-snapshot-probe SECTION EXTRACTED_FILES OUTPUT_DIRECTORY\n";return 2;}
 Source source;source.root=argv[2];NativeSaveSection section(kSaveScene);std::string error;SceneRuntime scene;
 if(!read(source.root+"/SUBCODE.BIN",source.sub)||!read(argv[1],section.bytes)||!SceneStateCodec::decode(section,source,scene,error)){std::cerr<<error;return 1;}
 for(unsigned i=0;i<scene.paletteColors().size();++i)std::cout<<"color "<<i<<" "<<scene.paletteColors()[i]<<"\n";
 for(const auto& t:scene.assets().transfers){std::cout<<"transfer "<<t.recordOffset<<" source "<<t.sourceOffset<<" dest "<<t.destination<<" bytes "<<t.decoded.size()<<" delay "<<t.delay<<"\n";std::ofstream f((std::string(argv[3])+"/transfer-"+std::to_string(t.destination)+".bin").c_str(),std::ios::binary);f.write(reinterpret_cast<const char*>(t.decoded.data()),t.decoded.size());}
 for(unsigned i=0;i<64;++i){SceneActorView actor;if(!scene.inspectActor(i,actor,error))return 1;if(!actor.owned)continue;std::cout<<"actor "<<i<<" active "<<actor.active<<" flags "<<unsigned(actor.flags)<<" phase "<<unsigned(actor.phase)<<" sequence "<<actor.sequence<<" record "<<actor.record<<" x "<<actor.x<<" y "<<actor.y<<" resource "<<actor.resource<<"\n";if(actor.active&&!(actor.flags&2)){std::vector<PlacedPiece> pieces;if(!scene.actorPieces(i,pieces,error)){std::cerr<<error;return 1;}for(const auto& p:pieces)std::cout<<"piece "<<i<<" "<<p.x<<" "<<p.y<<" "<<p.width<<" "<<p.height<<" "<<p.attributes<<"\n";}}
 uint16_t resource;unsigned sceneIndex;scene.resourceIdentity(resource,sceneIndex);for(unsigned mode=0;mode<(resource==29?8u:7u);++mode){SceneRuntime copy=scene;SceneLayout layout=copy.presentationLayout();if(mode==1||mode==3)layout.planeEnabled[0]=false;if(mode==2||mode==3)layout.planeEnabled[1]=false;if(mode==6&&resource!=29){bool repaired=false;if(!copy.recoverCreditsPalette(repaired,error)||!repaired){std::cerr<<error;return 1;}}if(mode>=6&&resource==29){bool repaired=false;if(!copy.recoverHospitalCombatMap(5,source.sub,repaired,error)||!repaired){std::cerr<<"recovery failed: "<<error;return 1;}bool repeated=true;if(!copy.recoverHospitalCombatMap(5,source.sub,repeated,error)||repeated){std::cerr<<"recovery is not idempotent";return 1;}}if(mode==5){for(const auto& transfer:scene.assets().transfers)if(!copy.applyTransfer(transfer,error)){std::cerr<<error;return 1;}}SceneImage image;if(!copy.configure(layout,error)||!copy.render(kSceneRGB888,image,error,mode==4||mode==7?33:64)){std::cerr<<error;return 1;}std::string path=std::string(argv[3])+"/layer-"+char('0'+mode)+".rgb";std::ofstream f(path.c_str(),std::ios::binary);f.write(reinterpret_cast<const char*>(image.rgb.data()),image.rgb.size());if(!f)return 1;std::cout<<"layer "<<mode<<" "<<image.width<<" "<<image.height<<" pieces "<<image.submittedPieces<<" limited "<<image.spriteLimitReached<<"\n";}
 return 0;
}
