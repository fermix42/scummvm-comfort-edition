/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_INTERACTION_H
#define SNATCHER_INTERACTION_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace Snatcher {
// Source-owned game hit regions, not a general address or screen-pixel reader.
bool readInteractionHitMap(const uint8_t* sub,size_t size,std::vector<uint8_t>& map,std::string& error);
struct PointerInteractionSnapshot {
 bool active,selectedKnown;int32_t x16,y16;uint8_t selectedRegion;uint64_t mapStamp;
 PointerInteractionSnapshot():active(false),selectedKnown(false),x16(0),y16(0),selectedRegion(0),mapStamp(0){}
};
struct PointerInteractionResult {
 bool moved,published,adjusting;
 PointerInteractionResult():moved(false),published(false),adjusting(false){}
};
struct PointerInteractionPosition {int16_t x,y;PointerInteractionPosition():x(0),y(0){} };
// PC repeat policy, not an estimate of the original CPU polling throughput.
// ScummVM VirtualMouse uses a12ms visit and10px at normal speed; its slow
// modifier is1/10. At640px this gives one source pixel (128 source operations)
// per admitted visit at kbdmouse_speed3. Late visits never accumulate debt.
// Physical holds/repeat timing are host input, deliberately not save state.
class NativePointerRepeat {
public:
 void reset(){_held=0;_last=0;}
 uint32_t poll(uint32_t now,bool eligible,uint8_t held,int speed){
  held=uint8_t(held&31);
  if(!eligible||!(held&16)||!(held&15)){reset();return 0;}
  if(held==_held&&uint32_t(now-_last)<12)return 0;
  _held=held;_last=now;
  if(speed<0||speed>7)speed=3;
  return uint32_t(32*(speed+1));
 }
private:uint8_t _held=0;uint32_t _last=0;
};
// CE direct flashlight view. Coordinates are already native game pixels, not
// desktop coordinates. The caller establishes aperture and source-point anchor from
// the owned investigation view; geometry is never inferred from a screenshot.
struct PointerDirectView {
 unsigned width,upperHeight;int apertureX,apertureY;int32_t anchorX16,anchorY16;uint64_t mapStamp;
 PointerDirectView():width(0),upperHeight(0),apertureX(0),apertureY(0),anchorX16(0),anchorY16(0),mapStamp(0){}
};
struct PointerDirectProjection {
 bool valid;int mouseX,mouseY;int32_t sourceX16,sourceY16;int translateX,translateY;uint16_t region;uint64_t mapStamp;
 PointerDirectProjection():valid(false),mouseX(0),mouseY(0),sourceX16(0),sourceY16(0),translateX(0),translateY(0),region(0),mapStamp(0){}
};
// SUB10846/175A0: contextual pointer with an independently owned region cache.
// One poll is the original menu polling operation, NOT one 60Hz update. The
// native caller supplies a declared PC repeat policy, never a fitted CPU count.
class PointerInteraction {
public:
 PointerInteraction();
 bool loadMap(const std::vector<uint8_t>& map,std::string& error);
 // Explicit DE86 fresh-game initialization; never inferred for a legacy save.
 void sourceFreshReset();
 // Pre-owner archive provenance: only inactive mode0 proves initial cache0.
 // Nonzero old modes retain unknown cache/position ownership and stay guarded.
 bool migrateLegacy(uint16_t interaction,std::string& error);
 bool enter(std::string& error);
 void leave(){_state.active=false;}
 bool sample(bool nestedCommandPath,uint8_t held,uint32_t polls,PointerInteractionResult& result,std::string& error);
 // Optional CE pan-pad input, not original held-button polling or a literal
 // body-pixel hit test. It changes position only; a real root poll publishes
 // the corresponding original region. Engine input owns opt-in policy.
 bool mousePan(int mouseX,int mouseY,unsigned viewWidth,unsigned upperViewHeight,bool nestedCommandPath,std::string& error);
 bool makeDirectView(unsigned width,unsigned upperHeight,int apertureX,int apertureY,int32_t anchorX16,int32_t anchorY16,PointerDirectView& view,std::string& error)const;
 // Out-of-view/domain/nested points succeed with valid=false, never edge-clamp.
 bool projectDirect(const PointerDirectView& view,int mouseX,int mouseY,bool nestedCommandPath,PointerDirectProjection& point,std::string& error)const;
 // Only a genuine valid point changes the owned source positions. Clicks use
 // the same released-root sample owner to publish the original region cache.
 bool applyDirect(const PointerDirectView& view,int mouseX,int mouseY,bool nestedCommandPath,bool publish,PointerDirectProjection& point,std::string& error);
 // Source10990's exact signed-highword plus independent RNG rounding. Caller
 // latches presentation only after this queued position is actually published.
 bool matchesPublishedPan(const PointerDirectProjection& point,int16_t planeX,int16_t planeY)const;
 // Restore only CE presentation from the actual saved source owner. Fractional
 // source coordinates/cache stay exact; display uses their signed high words.
 bool directProjectionForState(const PointerDirectView& view,PointerDirectProjection& point,std::string& error)const;
 bool queryRegion(uint16_t& region,std::string& error)const;
 bool snapshot(PointerInteractionSnapshot& state,std::string& error)const;
 bool restore(const PointerInteractionSnapshot& state,std::string& error);
 // Source10990 consumes two actual RNG words even if the pointer did not move.
 // The caller owns those draws and visual-mode publication. No RNG is invented.
 bool position(uint16_t firstRandom,uint16_t secondRandom,PointerInteractionPosition& out,std::string& error)const;
 bool active()const{return _state.active;}
private:
 std::vector<uint8_t> _map;PointerInteractionSnapshot _state;
};
}
#endif
