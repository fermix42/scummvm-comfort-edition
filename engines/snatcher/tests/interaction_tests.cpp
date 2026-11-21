/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../interaction.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <limits>
#include <algorithm>
namespace {
unsigned checks;
void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAILED:%s\n",message);throw std::runtime_error(message);}}
Snatcher::PointerInteractionSnapshot stateOf(const Snatcher::PointerInteraction& owner){Snatcher::PointerInteractionSnapshot state;std::string error;check(owner.snapshot(state,error),"snapshot real owner");return state;}
}
int main(int argc,char** argv){using namespace Snatcher;std::string error;PointerInteraction owner;PointerInteractionResult result;uint16_t region=777;
 check(!owner.enter(error)&&!owner.queryRegion(region,error)&&region==777,"unknown owner cannot invent initial region0");
 PointerInteraction fresh;fresh.sourceFreshReset();check(fresh.queryRegion(region,error)&&region==0,"actual DE86 fresh reset clears96DC, independently of pointer entry");region=777;
 std::vector<uint8_t> map(720);for(unsigned i=0;i<map.size();++i)map[i]=uint8_t((i*17+i/36)%11);
 check(!owner.loadMap(std::vector<uint8_t>(719),error)&&owner.loadMap(map,error)&&owner.enter(error),"exact source extent required before entry");
 auto initial=stateOf(owner);check(initial.active&&initial.x16==0&&initial.y16==0&&!initial.selectedKnown,"entry only clears source positions");
 check(owner.sample(true,0,10,result,error)&&!result.published&&!owner.queryRegion(region,error),"nested command path cannot publish pointer context");
 check(owner.sample(false,0,0,result,error)&&!result.published&&!owner.queryRegion(region,error),"zero polls produce no fabricated source transition");
 check(owner.sample(false,0,1,result,error)&&result.published&&!result.moved&&owner.queryRegion(region,error)&&region==map[10*36+18],"actual release poll publishes source center cell");
 check(owner.sample(false,16|8,128,result,error)&&result.adjusting&&result.moved&&!result.published,"held adjustment advances exact source512 quanta");
 auto moved=stateOf(owner);check(moved.x16==65536&&moved.y16==0&&moved.selectedKnown&&moved.selectedRegion==region,"movement preserves prior cached query until release");
 check(owner.sample(false,16|8,384,result,error)&&owner.sample(false,0,1,result,error)&&owner.queryRegion(region,error)&&region==map[10*36+19],"four source pixels cross one hit-region column");
 check(owner.enter(error)&&owner.queryRegion(region,error)&&region==map[10*36+19]&&stateOf(owner).x16==0,"reentry preserves published cache while resetting both accumulators");
 auto stable=stateOf(owner);check(!owner.sample(false,16|8,1000001,result,error)&&stateOf(owner).x16==stable.x16,"oversized native dispatch rejects transactionally");
 check(owner.sample(false,16|8,100000,result,error)&&stateOf(owner).x16==69*65536-512,"right source exclusive boundary rejects excess polls");
 check(owner.sample(false,16|12,1,result,error)&&stateOf(owner).x16==69*65536-1024,"right then left ordering remains observable at exclusive edge");
 check(owner.sample(false,16|4,100000,result,error)&&stateOf(owner).x16==-71*65536,"left inclusive boundary");
 check(owner.sample(false,16|3,100000,result,error)&&stateOf(owner).y16==0,"opposite directions cancel away from boundaries");
 check(owner.sample(false,16|1,100000,result,error)&&stateOf(owner).y16==-39*65536,"up inclusive boundary");
 check(owner.sample(false,16|3,1,result,error)&&stateOf(owner).y16==-39*65536+512,"up then down ordering remains observable at inclusive edge");
 check(owner.sample(false,16|2,100000,result,error)&&stateOf(owner).y16==37*65536-512,"down exclusive boundary");
 // Every reachable 1/128-pixel quantum has a defined source index. In
 // particular, negative fractional positions use the signed high word.
 for(int axis=0;axis<2;++axis){int lo=axis?-39:-71,hi=axis?37:69;for(int32_t q=lo*65536;q<hi*65536;q+=512){auto s=initial;s.x16=axis?0:q;s.y16=axis?q:0;check(owner.restore(s,error),"restore explicit controlled boundary fixture");check(owner.sample(false,0,1,result,error)&&owner.queryRegion(region,error),"actual cache publication at every fixed position");int part=q>=0?q/65536:(q-65535)/65536;unsigned col=unsigned((axis?0:part)+72)>>2,row=unsigned((axis?part:0)+40)>>2;check(region==map[row*36+col],"source signed integer lookup agrees for all valid quanta");}}
 check(owner.restore(initial,error)&&owner.sample(false,16|4,1,result,error),"negative fractional display fixture");PointerInteractionPosition position;
 check(owner.position(0,0,position,error)&&position.x==-1&&position.y==-40,"display uses signed highword without false truncation");
 check(owner.position(0x8000,0x8000,position,error)&&position.x==0&&position.y==-39,"two actual random high bits independently round source view coordinates");
 auto saved=stateOf(owner);saved.selectedKnown=true;saved.selectedRegion=10;check(owner.restore(saved,error)&&owner.queryRegion(region,error)&&region==10,"stale source cache remains valid independently from its current position");
 auto corrupt=saved;corrupt.selectedRegion=255;check(!owner.restore(corrupt,error)&&owner.queryRegion(region,error)&&region==10,"cached region absent from actual source map rejects transactionally");
 auto invalid=saved;invalid.x16=69*65536;check(!owner.restore(invalid,error)&&stateOf(owner).x16==saved.x16,"out-of-domain position cannot mutate owner");
 invalid=saved;invalid.mapStamp^=1;check(!owner.restore(invalid,error)&&owner.queryRegion(region,error)&&region==10,"foreign source snapshot rejects transactionally");
 invalid=saved;invalid.selectedKnown=false;check(!owner.restore(invalid,error),"unknown cache has canonical unowned payload");
 std::vector<uint8_t> other=map;other[0]^=1;check(!owner.loadMap(other,error)&&owner.queryRegion(region,error)&&region==10,"hit-region assets cannot silently replace live source identity");
 owner.leave();check(!owner.sample(false,0,1,result,error)&&!owner.position(0,0,position,error)&&owner.queryRegion(region,error)&&region==10,"leaving pointer mode retains actual query cache but disables active owners");
 PointerInteraction legacy;check(!legacy.migrateLegacy(0,error)&&legacy.loadMap(map,error)&&!legacy.migrateLegacy(4,error),"legacy migration requires actual table and supported source policy");
 check(legacy.migrateLegacy(0,error)&&!legacy.active()&&legacy.queryRegion(region,error)&&region==0,"old inactive source provenance permits known initial zero");
 for(uint16_t mode=1;mode<=3;++mode){check(legacy.migrateLegacy(mode,error)&&!legacy.active()&&!legacy.queryRegion(region,error)&&!stateOf(legacy).selectedKnown,"old nonzero interaction remains inactive and unknown without a guessed pointer reset");check(legacy.enter(error)&&!legacy.queryRegion(region,error),"entry alone cannot invent a legacy missing cache");check(legacy.sample(false,0,1,result,error)&&legacy.queryRegion(region,error),"real new source publication resolves the unknown cache");}
 check(legacy.enter(error)&&legacy.queryRegion(region,error),"fresh reentry preserves a genuinely produced cache");
 PointerInteraction mouse;check(!mouse.mousePan(128,72,256,144,false,error)&&mouse.loadMap(map,error)&&mouse.enter(error),"CE mouse adaptation requires real active source ownership");
 check(mouse.sample(false,0,1,result,error)&&mouse.queryRegion(region,error),"mouse fixture has an actual previous published source region");uint16_t oldRegion=region;
 check(mouse.mousePan(0,0,256,144,false,error)&&stateOf(mouse).x16==-71*65536&&stateOf(mouse).y16==-39*65536&&mouse.queryRegion(region,error)&&region==oldRegion,"upper-left pan changes only position and preserves stale query cache");
 check(mouse.sample(false,0,1,result,error)&&mouse.queryRegion(region,error)&&region==map[0],"actual root poll publishes the clamped source cell after mouse input");
 check(mouse.mousePan(255,143,256,144,false,error)&&stateOf(mouse).x16==68*65536&&stateOf(mouse).y16==36*65536,"lower-right pan stays within whole-pixel source bounds");
 check(mouse.mousePan(128,72,256,144,false,error)&&stateOf(mouse).x16==0&&stateOf(mouse).y16==0,"qualified256 by144 center maps to source zero");
 auto mouseBefore=stateOf(mouse);check(mouse.mousePan(0,0,256,144,true,error)&&stateOf(mouse).x16==mouseBefore.x16&&stateOf(mouse).y16==mouseBefore.y16&&stateOf(mouse).selectedRegion==mouseBefore.selectedRegion,"nested command paths ignore mouse pan without query side effects");
 check(!mouse.mousePan(0,0,0,144,false,error)&&!mouse.mousePan(0,0,256,0,false,error)&&!mouse.mousePan(0,0,143,80,false,error)&&!mouse.mousePan(0,0,144,79,false,error)&&!mouse.mousePan(0,0,65536,144,false,error)&&stateOf(mouse).x16==mouseBefore.x16&&stateOf(mouse).y16==mouseBefore.y16,"invalid geometry rejects transactionally");
 check(mouse.mousePan(std::numeric_limits<int>::min(),std::numeric_limits<int>::max(),256,144,false,error)&&stateOf(mouse).x16==-71*65536&&stateOf(mouse).y16==36*65536,"extreme ordinary-event coordinates clamp without signed subtraction overflow");
 for(int y=0;y<144;++y)for(int x=0;x<256;++x){check(mouse.mousePan(x,y,256,144,false,error),"all qualified viewport pixels have a bounded CE pan mapping");auto s=stateOf(mouse);check(s.x16>=-71*65536&&s.x16<=68*65536&&s.y16>=-39*65536&&s.y16<=36*65536,"mouse output stays in the exact source position domain");check(mouse.sample(false,0,1,result,error)&&mouse.queryRegion(region,error),"mouse input still uses original source hit-region publication");int px=std::max(-71,std::min(68,x-128)),py=std::max(-39,std::min(36,y-72));check(region==map[((py+40)>>2)*36+((px+72)>>2)],"published mouse region equals the actual source table cell");}
 mouse.leave();mouseBefore=stateOf(mouse);check(!mouse.mousePan(128,72,256,144,false,error)&&stateOf(mouse).x16==mouseBefore.x16&&stateOf(mouse).y16==mouseBefore.y16,"inactive CE pointer events cannot mutate retained source position");
 // Direct CE pointing follows the observed flashlight displacement, with no edge clamping.
 PointerInteraction direct;PointerDirectView view;PointerDirectProjection point;
 check(!direct.makeDirectView(256,144,128,72,0,0,view,error)&&direct.loadMap(map,error)&&direct.enter(error),"direct view requires active original source ownership");
 check(direct.makeDirectView(256,144,128,72,0,0,view,error)&&direct.sample(false,0,1,result,error)&&direct.queryRegion(region,error),"direct source view and initial released cache are owned");oldRegion=region;
 check(direct.applyDirect(view,138,38,false,false,point,error)&&point.valid&&point.sourceX16==10*65536&&point.sourceY16==-34*65536&&point.translateX==0&&point.translateY==0,"rightward mouse moves the original flashlight right without translating the viewport");
 check(direct.queryRegion(region,error)&&region==oldRegion,"hover changes position but cannot directly assign cached region");
 check(direct.applyDirect(view,138,38,false,true,point,error)&&direct.queryRegion(region,error)&&region==point.region&&region==map[((-34+40)>>2)*36+((10+72)>>2)],"direct click publishes only the actual released-root hit-map sample");
 auto directBefore=stateOf(direct);
 for(int y=-39;y<=36;++y)for(int x=-71;x<=68;++x){
  check(direct.projectDirect(view,128+x,72+y,false,point,error)&&point.valid&&point.sourceX16==x*65536&&point.sourceY16==y*65536,"every original source pixel projects to one actual CE mouse point");
  check(point.region==map[((y+40)>>2)*36+((x+72)>>2)]&&point.translateX==0&&point.translateY==0,"source hit-map and unchanged original flashlight displacement agree across whole domain");
  for(int ry=0;ry<2;++ry)for(int rx=0;rx<2;++rx)check(direct.matchesPublishedPan(point,int16_t(x+rx),int16_t(y-40+ry)),"all four actual RNG-rounded publication variants match");
  check(!direct.matchesPublishedPan(point,int16_t(x+2),int16_t(y-40)),"unrelated pending pan cannot be treated as the rendered projection");
 }
 check(stateOf(direct).x16==directBefore.x16&&stateOf(direct).y16==directBefore.y16&&stateOf(direct).selectedRegion==directBefore.selectedRegion,"pure hit projection never mutates source position/cache");
 const int outside[][2]={{-1,72},{256,72},{128,-1},{128,144},{56,72},{197,72},{128,32},{128,109},{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}};
 for(const auto& xy:outside)check(direct.applyDirect(view,xy[0],xy[1],false,true,point,error)&&!point.valid&&stateOf(direct).x16==directBefore.x16&&stateOf(direct).selectedRegion==directBefore.selectedRegion,"out-of-view/map points do not clamp or select an edge hotspot");
 check(direct.applyDirect(view,128,72,true,true,point,error)&&!point.valid&&stateOf(direct).x16==directBefore.x16,"nested menus cannot select the investigation behind them");
 PointerDirectView foreign=view;foreign.mapStamp^=1;point.valid=true;
 check(!direct.applyDirect(foreign,128,72,false,true,point,error)&&point.valid&&stateOf(direct).x16==directBefore.x16,"foreign scene/map rejects transactionally including output");
 PointerDirectView fraction;check(direct.makeDirectView(256,144,128,72,-512,-512,fraction,error)&&direct.projectDirect(fraction,128,72,false,point,error)&&point.sourceX16==-512&&point.region==map[((-1+40)>>2)*36+((-1+72)>>2)],"signed fractional anchor uses actual source highword lookup");
 auto keepView=view;check(!direct.makeDirectView(0,144,128,72,0,0,view,error)&&view.mapStamp==keepView.mapStamp&&!direct.makeDirectView(256,144,256,72,0,0,view,error),"invalid geometry does not invent a view");
 auto savedDirect=directBefore;savedDirect.x16=-512;savedDirect.y16=65536+512;savedDirect.selectedKnown=true;savedDirect.selectedRegion=10;
 check(direct.restore(savedDirect,error)&&direct.directProjectionForState(keepView,point,error)&&point.valid&&point.sourceX16==-512&&point.sourceY16==65536+512&&point.translateX==0&&point.translateY==0&&point.mouseX==127&&point.mouseY==73,"cold direct display uses signed source highwords without rounding saved positions");
 check(direct.queryRegion(region,error)&&region==10&&stateOf(direct).x16==savedDirect.x16&&stateOf(direct).y16==savedDirect.y16,"presentation reconstruction cannot overwrite a stale saved query cache or source fraction");
 check(!direct.directProjectionForState(foreign,point,error)&&direct.queryRegion(region,error)&&region==10,"cold display rejects a foreign map/view transactionally");
 direct.leave();check(!direct.projectDirect(keepView,128,72,false,point,error),"inactive scene cannot retain clickable examination targets");
 // Independently retained original frames pair positive source X/Y with
 // positive flashlight displacement about(128,72), including signed fractions.
 struct Observed {int32_t x,y;int left,top,right,bottom;};
 const Observed original[]={{0,0,106,50,152,96},{1273856,0,124,51,170,96},{1273856,-1321984,124,32,170,78},{970752,970240,119,63,165,109}};
 for(const auto& frame:original){auto state=directBefore;state.x16=frame.x;state.y16=frame.y;check(direct.enter(error)&&direct.restore(state,error)&&direct.directProjectionForState(keepView,point,error),"retained original source point has an owned direct projection");
  check(point.mouseX>=frame.left&&point.mouseX<=frame.right&&point.mouseY>=frame.top&&point.mouseY<=frame.bottom&&point.translateX==0&&point.translateY==0,"literal pointer remains inside the actual moving original flashlight without a fitted transform");}
 std::vector<uint8_t> source(0x10cb8-0xd400,0),loaded(1,99);for(unsigned i=0;i<map.size();++i)source[0x109e8-0xd400+i]=map[i];
 check(!readInteractionHitMap(nullptr,source.size(),loaded,error)&&loaded.size()==1&&loaded[0]==99,"null source table rejection preserves output");
 check(!readInteractionHitMap(source.data(),source.size()-1,loaded,error)&&loaded[0]==99,"truncated source table rejection preserves output");
 check(readInteractionHitMap(source.data(),source.size(),loaded,error)&&loaded==map,"exact source table bytes retained");
 if(argc==3&&std::string(argv[1])=="--original"){
  std::ifstream input(argv[2],std::ios::binary);source.assign(std::istreambuf_iterator<char>(input),{});check(input.eof()||bool(input),"original source is read-only");check(readInteractionHitMap(source.data(),source.size(),loaded,error),"original table complete");unsigned count[256]={};for(uint8_t value:loaded)++count[value];unsigned distinct=0;for(unsigned n:count)if(n)++distinct;check(distinct==11&&count[10]==1,"original table has all11 observed semantic regions");PointerInteraction actual;check(actual.loadMap(loaded,error)&&actual.enter(error)&&actual.sample(false,0,1,result,error)&&actual.queryRegion(region,error),"original source center query owned");std::printf("original interaction map:720 cells,%u distinct regions,center=%u\n",distinct,unsigned(region));
 }
 std::printf("interaction:%u checks,0 failures\n",checks);return 0;
}
