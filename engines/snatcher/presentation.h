/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_PRESENTATION_H
#define SNATCHER_PRESENTATION_H
#include "graphics.h"
#include "animation.h"
namespace Snatcher {
enum ScrollUpdateMode { kScrollRelative, kScrollAbsolute };
struct ScrollPoint {int32_t x16,y16;ScrollPoint():x16(0),y16(0){} };
struct ScrollSnapshot {
 ScrollPoint positions[2],velocities[2],pending[2];ScrollUpdateMode nextMode;bool paused;
 ScrollSnapshot():nextMode(kScrollRelative),paused(false){}
};
// Complete closed arithmetic, with global mode chosen explicitly by the owner.
// This does not infer all original display-controller mode/flag transitions.
class ScrollState {
public:
 bool queuePosition(unsigned plane,ScrollAxis axis,int16_t integer,std::string& error);
 bool setVelocity(unsigned plane,ScrollAxis axis,int16_t rate8_8,std::string& error);
 void nextUpdate(ScrollUpdateMode mode){_state.nextMode=mode;}
 void pause(bool paused){_state.paused=paused;}
 bool tick(std::string& error);
 void clear(){_state=ScrollSnapshot();}
 const ScrollSnapshot& state()const{return _state;}
private:
 friend class SceneStateCodec;
 ScrollSnapshot _state;
};
// Semantic actor control fields recovered from F708/F78E/FDD2. No CPU memory.
struct ActorControlState {
 bool active,reserved;uint8_t flags,completionFlags,phase;uint16_t recordIndex,remaining;
 unsigned parent,head,next;
 ActorControlState():active(false),reserved(false),flags(0),completionFlags(0),phase(0),recordIndex(0xffff),remaining(0xffff),parent(0),head(0),next(0){}
};
enum ActorActionResult {kActorActionApplied,kActorActionWaiting,kActorActionError};
class ActorController {
public:
 ActorController():_actors(64){}
 void clear(){_actors.assign(64,ActorControlState());}
 bool initialize(unsigned actor,uint8_t flags,std::string& error);
 bool link(unsigned parent,unsigned child,std::string& error);
 ActorActionResult applyAction(unsigned actor,unsigned subtype,std::string& error);
 bool markCompletion(unsigned actor,std::string& error);
 bool markFlags(unsigned actor,uint8_t mask,std::string& error);
 bool retire(unsigned actor,std::string& error);
 bool setFlags(unsigned actor,uint8_t flags,std::string& error);
 bool reserve(unsigned actor,bool value,std::string& error);
 const ActorControlState* state(unsigned actor)const{return actor<64?&_actors[actor]:0;}
private:
 friend class SceneStateCodec;
 friend class SceneRuntime;
 std::vector<ActorControlState> _actors;
};
// Raw 0BGR words retain the original three-bit channel encoding (bits 1..3).
// Twelve effect slots, source frame ownership and update delays are game-level.
// Fixed USA asset locators for eight game text colors embedded in SUBCODE.
// This reads color words only; no instruction is interpreted or executed.
bool readGameTextPalette(const uint8_t *subcode,size_t size,std::vector<uint16_t>& colors,std::string& error);
struct PaletteEffect {
 uint8_t type,delay,byteOffset,countMinusOne;bool replaceCycling;
 std::vector<std::vector<uint16_t> > frames;
 PaletteEffect():type(0),delay(0),byteOffset(0),countMinusOne(0),replaceCycling(false){}
};
bool readPaletteEffect(const uint8_t* data,size_t size,size_t begin,size_t end,PaletteEffect& out,std::string& error,uint32_t resourceBase=0x28000);
class PaletteController {
public:
 PaletteController();
 void clear();
 void cancelEffects();
 bool setColors(const std::vector<uint16_t>& colors,std::string& error);
 bool enqueue(const PaletteEffect& effect,std::string& error);
 bool tick(std::string& error,uint8_t* publicationMask=nullptr,std::vector<bool>* knownColors=nullptr);
 void pause(bool value){_paused=value;}
 const std::vector<uint16_t>& colors()const{return _colors;}
 size_t activeEffects()const;
private:
 friend class SceneStateCodec;
 struct Slot {PaletteEffect effect;uint8_t wait,updates;size_t frame;Slot():wait(0),updates(0),frame(0){}};
 std::vector<uint16_t> _colors;std::vector<Slot> _slots;bool _paused;
};
struct PlacedPiece {
 int x,y;unsigned width,height;uint16_t attributes;uint8_t ownerActor;
 PlacedPiece(int px=0,int py=0,unsigned w=0,unsigned h=0,uint16_t a=0):x(px),y(py),width(w),height(h),attributes(a),ownerActor(64){}
};
struct FramePlacement {
 int x,y;bool flipX,flipY;uint16_t attributeOrMask;bool overridePalette;unsigned palette;
 FramePlacement():x(0),y(0),flipX(false),flipY(false),attributeOrMask(0),overridePalette(false),palette(0){}
};
// Native draw-list construction, independent of any VDP. Final raw attributes
// are retained until tile/palette allocation is established at scene level.
bool placeFrame(const std::vector<FramePiece>& frame,const FramePlacement& placement,size_t maxPieces,std::vector<PlacedPiece>& out,std::string& error);
struct ActorInitializer {unsigned actorIndex;uint8_t flags;int16_t x,y;size_t frameTableOffset,sequenceOffset;};
bool readActorInitializers(const uint8_t *data,size_t size,size_t begin,size_t end,std::vector<ActorInitializer>& out,std::string& error,uint32_t resourceBase=0x28000);
// Resource pattern cache, not a hardware memory/register model. Allocation
// indices come from independently decoded graphics-transfer destinations/32.
class PatternBank {
public:
 PatternBank():_patterns(2048*32,0),_present(2048,false){}
 void invalidate(unsigned first,unsigned count);
 bool assign(unsigned firstPattern,const std::vector<uint8_t>& packed,std::string& error);
 bool sample(uint16_t attributes,unsigned x,unsigned y,uint8_t& color,std::string& error)const;
 bool draw(const PlacedPiece& piece,std::vector<uint8_t>& canvas,size_t width,size_t height,std::string& error,const std::vector<uint8_t>* admittedRowWidths=nullptr)const;
 // Row-major original pattern-name words. Source coordinates explicitly name
 // an asset window; callers establish game camera/layer/priority semantics.
 bool drawPlane(const std::vector<uint16_t>& names,size_t columns,size_t rows,int sourceX,int sourceY,std::vector<uint8_t>& canvas,size_t width,size_t height,std::string& error)const;
 void clear();
private:
 friend class SceneStateCodec;
 std::vector<uint8_t> _patterns;std::vector<bool> _present;
};
// Shared source13D4E allocation: the final tile is solid color7, while its
// terminal name cell refers to tile0 rather than that reserved tile.
void initializeOriginalFontCanvas(std::vector<uint8_t>& patterns,std::vector<uint8_t>& names);
struct FontGlyph {uint8_t rows[8],advance;};
bool rasterOriginalFontGlyph(const FontGlyph& glyph,uint8_t code,uint8_t x,uint8_t y,uint8_t color,bool fixedWidth,std::vector<uint8_t>& patterns,std::string& error);
struct FontProgramState {
 size_t cursor,glyphs;uint8_t delay,remaining,color,x,y;bool fixedWidth,active,modeChanged;
 FontProgramState():cursor(0),glyphs(0),delay(0),remaining(0),color(1),x(0),y(0),fixedWidth(false),active(false),modeChanged(false){}
};
class OriginalFont {
public:
 OriginalFont():_loaded(false){}
 // The verified USA SUBCODE embeds 102 8x8 one-bit glyphs and 102 advances.
 // Data offsets are format locators; original code is never executed.
 bool load(const uint8_t *subcode,size_t size,std::string& error);
 bool glyph(uint8_t originalCode,FontGlyph& out,std::string& error)const;
 // One eligible game font worker. The producer owns transactional state and
 // the canvas binding; this consumer does not publish a displayed frame.
 bool stepProgram(const std::vector<uint8_t>& program,FontProgramState& state,std::vector<uint8_t>& patterns,uint8_t& dirtyRows,std::string& error)const;
 bool loaded()const{return _loaded;}
private:
 friend class EngineStateCodec;
 friend class SceneStateCodec;
 std::vector<uint8_t> _rows,_advances;bool _loaded;
};
// Native indexed rasterization: one-bits paint color, zero-bits preserve the
// destination. RGB palette association and page/window placement are separate.
bool drawGlyph(const OriginalFont& font,uint8_t code,int x,int y,uint8_t color,std::vector<uint8_t>& canvas,size_t width,size_t height,std::string& error);
enum TextTokenKind {kTextGlyph,kTextStyle,kTextMode,kTextPause,kTextEnd,kTextLineBreak};
struct TextToken {TextTokenKind kind;uint8_t value;size_t resourceOffset;};
// Original glyph IDs are not Unicode. Dynamic EC/EE/F0 substitutions require
// an independently specified context and therefore stop explicitly here.
class TextReader {
public:
 TextReader():_base(0),_cursor(0),_ended(false){}
 bool start(const uint8_t *data,size_t size,size_t fileOffset,std::string& error);
 bool read(size_t glyphBudget,std::vector<TextToken>& out,std::string& error);
 size_t cursor()const{return _base+_cursor;}
 bool ended()const{return _ended;}
private:
 friend class SceneStateCodec;
 std::vector<uint8_t> _text;size_t _base,_cursor;bool _ended;
};
}
#endif
