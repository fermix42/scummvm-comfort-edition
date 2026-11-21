/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "interaction.h"
#include <algorithm>
namespace Snatcher {
namespace {
const size_t kMapOffset=0x109e8-0xd400,kMapSize=36*20;
const int32_t kXMinimum=-71*65536,kXMaximum=69*65536,kYMinimum=-39*65536,kYMaximum=37*65536,kStep=512;
uint64_t stamp(const std::vector<uint8_t>& bytes){uint64_t value=UINT64_C(14695981039346656037);for(uint8_t byte:bytes)value=(value^byte)*UINT64_C(1099511628211);return value;}
int integerPart(int32_t fixed){unsigned high=uint32_t(fixed)>>16;return high<32768?int(high):int(high)-65536;}
bool inRange(int32_t x,int32_t y){return x>=kXMinimum&&x<kXMaximum&&y>=kYMinimum&&y<kYMaximum;}
}
bool readInteractionHitMap(const uint8_t* sub,size_t size,std::vector<uint8_t>& map,std::string& error){
 error.clear();if(!sub||size<kMapOffset+kMapSize){error="Original interaction hit-region table is truncated";return false;}
 std::vector<uint8_t> next(sub+kMapOffset,sub+kMapOffset+kMapSize);map.swap(next);return true;
}
PointerInteraction::PointerInteraction(){}
void PointerInteraction::sourceFreshReset(){uint64_t identity=_state.mapStamp;_state=PointerInteractionSnapshot();_state.mapStamp=identity;_state.selectedKnown=true;}
bool PointerInteraction::migrateLegacy(uint16_t interaction,std::string& error){
 error.clear();if(_map.size()!=kMapSize||interaction>3){error="Legacy pointer migration requires its source table and valid interaction policy";return false;}
 if(!interaction)sourceFreshReset();else{uint64_t identity=_state.mapStamp;_state=PointerInteractionSnapshot();_state.mapStamp=identity;}return true;
}
bool PointerInteraction::loadMap(const std::vector<uint8_t>& map,std::string& error){
 error.clear();if(map.size()!=kMapSize){error="Interaction hit-region table requires36 by20 cells";return false;}
 uint64_t identity=stamp(map);if(!_map.empty()&&_state.mapStamp!=identity){error="Interaction hit-region source cannot change within its owner";return false;}
 _map=map;_state.mapStamp=identity;return true;
}
bool PointerInteraction::enter(std::string& error){
 error.clear();if(_map.size()!=kMapSize){error="Pointer interaction has no source hit-region owner";return false;}
 // Native10 resets only the two positions. It does not clear96DC.
 _state.x16=_state.y16=0;_state.active=true;return true;
}
bool PointerInteraction::sample(bool nestedCommandPath,uint8_t held,uint32_t polls,PointerInteractionResult& result,std::string& error){
 error.clear();if(!_state.active||_map.size()!=kMapSize){error="Pointer polling requires its active source owner";return false;}
 if(polls>1000000){error="Pointer polling exceeds finite native dispatch bound";return false;}
 PointerInteractionResult next;PointerInteractionSnapshot state=_state;
 if(!nestedCommandPath&&polls){
  next.adjusting=(held&16)!=0;
  if(next.adjusting){
   for(uint32_t i=0;i<polls;++i){int32_t y=state.y16,x=state.x16;
    // Source ordering is observable with opposite directions at a boundary.
    if((held&1)&&state.y16-kStep>=kYMinimum)state.y16-=kStep;
    if((held&2)&&state.y16+kStep<kYMaximum)state.y16+=kStep;
    if((held&8)&&state.x16+kStep<kXMaximum)state.x16+=kStep;
    if((held&4)&&state.x16-kStep>=kXMinimum)state.x16-=kStep;
    next.moved=next.moved||x!=state.x16||y!=state.y16;
   }
  }else{
   unsigned column=unsigned(integerPart(state.x16)+72)>>2,row=unsigned(integerPart(state.y16)+40)>>2;
   if(column>=36||row>=20){error="Pointer position has no original hit-region cell";return false;}
   state.selectedRegion=_map[row*36+column];state.selectedKnown=true;next.published=true;
  }
 }
 _state=state;result=next;return true;
}
bool PointerInteraction::queryRegion(uint16_t& region,std::string& error)const{
 error.clear();if(!_state.selectedKnown){error="Pointer context has not published an actual region";return false;}region=_state.selectedRegion;return true;
}
bool PointerInteraction::mousePan(int mouseX,int mouseY,unsigned viewWidth,unsigned upperViewHeight,bool nestedCommandPath,std::string& error){
 error.clear();if(!_state.active||_map.size()!=kMapSize){error="Mouse pan requires its active source pointer owner";return false;}
 if(nestedCommandPath)return true;
 if(viewWidth<144||upperViewHeight<80||viewWidth>65535||upperViewHeight>65535){error="Mouse pan geometry cannot contain the qualified source pan pad";return false;}
 int64_t x=int64_t(mouseX)-int64_t(viewWidth/2),y=int64_t(mouseY)-int64_t(upperViewHeight/2);
 x=std::max<int64_t>(-71,std::min<int64_t>(68,x));y=std::max<int64_t>(-39,std::min<int64_t>(36,y));
 _state.x16=int32_t(x*65536);_state.y16=int32_t(y*65536);return true;
}
bool PointerInteraction::makeDirectView(unsigned width,unsigned upperHeight,int apertureX,int apertureY,int32_t anchorX16,int32_t anchorY16,PointerDirectView& view,std::string& error)const{
 error.clear();if(!_state.active||_map.size()!=kMapSize){error="Direct examination requires its active original hit-map owner";return false;}
 if(!width||!upperHeight||width>65535||upperHeight>65535||apertureX<0||apertureY<0||unsigned(apertureX)>=width||unsigned(apertureY)>=upperHeight||!inRange(anchorX16,anchorY16)){error="Direct examination view/anchor is outside its owned geometry";return false;}
 PointerDirectView next;next.width=width;next.upperHeight=upperHeight;next.apertureX=apertureX;next.apertureY=apertureY;next.anchorX16=anchorX16;next.anchorY16=anchorY16;next.mapStamp=_state.mapStamp;view=next;return true;
}
bool PointerInteraction::projectDirect(const PointerDirectView& view,int mouseX,int mouseY,bool nestedCommandPath,PointerDirectProjection& point,std::string& error)const{
 error.clear();PointerDirectView checked;if(!makeDirectView(view.width,view.upperHeight,view.apertureX,view.apertureY,view.anchorX16,view.anchorY16,checked,error))return false;
 if(view.mapStamp!=_state.mapStamp){error="Direct examination view belongs to a different hit-map owner";return false;}
 PointerDirectProjection next;
 if(nestedCommandPath||mouseX<0||mouseY<0||unsigned(mouseX)>=view.width||unsigned(mouseY)>=view.upperHeight){point=next;return true;}
 // Original planeA moves the flashlight itself over a fixed body. Retained
 // paired source positions/frames establish the positive mouse displacement;
 // no whole-viewport translation or inferred camera inversion is applied.
 int64_t x=int64_t(view.anchorX16)+(int64_t(mouseX)-view.apertureX)*65536;
 int64_t y=int64_t(view.anchorY16)+(int64_t(mouseY)-view.apertureY)*65536;
 if(x<kXMinimum||x>=kXMaximum||y<kYMinimum||y>=kYMaximum){point=next;return true;}
 next.valid=true;next.mouseX=mouseX;next.mouseY=mouseY;next.sourceX16=int32_t(x);next.sourceY16=int32_t(y);next.translateX=next.translateY=0;next.mapStamp=_state.mapStamp;
 unsigned column=unsigned(integerPart(next.sourceX16)+72)>>2,row=unsigned(integerPart(next.sourceY16)+40)>>2;next.region=_map[row*36+column];point=next;return true;
}
bool PointerInteraction::applyDirect(const PointerDirectView& view,int mouseX,int mouseY,bool nestedCommandPath,bool publish,PointerDirectProjection& point,std::string& error){
 PointerDirectProjection next;if(!projectDirect(view,mouseX,mouseY,nestedCommandPath,next,error))return false;if(!next.valid){point=next;return true;}
 PointerInteractionSnapshot previous=_state;_state.x16=next.sourceX16;_state.y16=next.sourceY16;
 if(publish){PointerInteractionResult result;if(!sample(false,0,1,result,error)){_state=previous;return false;}}
 point=next;return true;
}
bool PointerInteraction::matchesPublishedPan(const PointerDirectProjection& point,int16_t planeX,int16_t planeY)const{
 if(!_state.active||!point.valid||point.mapStamp!=_state.mapStamp||!inRange(point.sourceX16,point.sourceY16))return false;
 int x=integerPart(point.sourceX16),y=integerPart(point.sourceY16)-40;
 return (planeX==x||planeX==x+1)&&(planeY==y||planeY==y+1);
}
bool PointerInteraction::directProjectionForState(const PointerDirectView& view,PointerDirectProjection& point,std::string& error)const{
 error.clear();PointerDirectView checked;if(!makeDirectView(view.width,view.upperHeight,view.apertureX,view.apertureY,view.anchorX16,view.anchorY16,checked,error))return false;
 if(view.mapStamp!=_state.mapStamp||!inRange(_state.x16,_state.y16)){error="Saved direct presentation has no matching source view";return false;}
 PointerDirectProjection next;int dx=integerPart(_state.x16-view.anchorX16),dy=integerPart(_state.y16-view.anchorY16);
 int64_t mx=int64_t(view.apertureX)+dx,my=int64_t(view.apertureY)+dy;
 if(mx<0||my<0||uint64_t(mx)>=view.width||uint64_t(my)>=view.upperHeight){point=next;return true;}
 next.valid=true;next.mouseX=int(mx);next.mouseY=int(my);next.translateX=next.translateY=0;next.sourceX16=_state.x16;next.sourceY16=_state.y16;next.mapStamp=_state.mapStamp;
 unsigned column=unsigned(integerPart(_state.x16)+72)>>2,row=unsigned(integerPart(_state.y16)+40)>>2;next.region=_map[row*36+column];point=next;return true;
}
bool PointerInteraction::snapshot(PointerInteractionSnapshot& state,std::string& error)const{
 error.clear();if(_map.size()!=kMapSize){error="Pointer context has no source table identity";return false;}state=_state;return true;
}
bool PointerInteraction::restore(const PointerInteractionSnapshot& state,std::string& error){
 error.clear();if(_map.size()!=kMapSize||state.mapStamp!=_state.mapStamp||!inRange(state.x16,state.y16)||(!state.selectedKnown&&state.selectedRegion)||(state.selectedKnown&&std::find(_map.begin(),_map.end(),state.selectedRegion)==_map.end())){error="Pointer snapshot source/position/cache ownership is invalid";return false;}
 // A valid cached region need not match the current position: held adjustment
 // deliberately retains the preceding published region until release.
 _state=state;return true;
}
bool PointerInteraction::position(uint16_t firstRandom,uint16_t secondRandom,PointerInteractionPosition& out,std::string& error)const{
 error.clear();if(!_state.active||_map.size()!=kMapSize){error="Pointer display has no active source owner";return false;}
 PointerInteractionPosition next;next.x=int16_t(integerPart(_state.x16)+((firstRandom&0x8000)?1:0));next.y=int16_t(integerPart(_state.y16)-40+((secondRandom&0x8000)?1:0));out=next;return true;
}
}
