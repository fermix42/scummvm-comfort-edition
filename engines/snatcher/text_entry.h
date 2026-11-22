/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_TEXT_ENTRY_H
#define SNATCHER_TEXT_ENTRY_H
#include "scene_assets.h"
#include "mouse_input.h"
#include <array>
#include <deque>
namespace Snatcher {
// Source1086E/1089C pointer-grid query96DC; owner supplies original coordinates.
bool textEntryMenuContext(const uint8_t* subcode,size_t size,uint16_t x,uint16_t y,uint16_t& context,std::string& error);
struct TextEntrySettings { uint16_t session,limit,phonePolicy; uint8_t mode; TextEntrySettings():session(0),limit(0),phonePolicy(0),mode(0){} };
enum TextEntryMouseTarget { kTextEntryMouseSubmit=0x10000 };
struct TextEntryChoice {uint16_t reference;std::vector<uint8_t> label;};
struct TextEntryEffects {
 int view;bool click,changed,completed,cursorMoved,waitKeyCompletion,cursorHidden,restoreKeyPosition,inputDeferred,bodyRedraw,underlineRedraw;int keyActor,keyRecord,keyX,keyY;uint8_t cursorX,cursorY;
 TextEntryEffects():view(-1),click(false),changed(false),completed(false),cursorMoved(false),waitKeyCompletion(false),cursorHidden(false),restoreKeyPosition(false),inputDeferred(false),bodyRedraw(false),underlineRedraw(false),keyActor(-1),keyRecord(-1),keyX(-1),keyY(-1),cursorX(0),cursorY(0){}
};
// Source 1130A..11DCE. Labels are original scenario bytes excluding FF.
// Completion is a semantic choice, applied by the caller through normal runtime.
class TextEntry {
public:
 TextEntry();
 bool loadAssets(const uint8_t* subcode,size_t size,std::string& error);
 bool begin(const std::vector<TextEntryChoice>& choices,const TextEntrySettings& settings,TextEntryEffects& effects,std::string& error);
 // Global sourceDF86 owner: advance every source update, including inactive input.
 // Pass repeatAlreadyAdvanced=true to step after this call.
 void advanceHeldRepeat(uint8_t pressed,uint8_t held);
 bool step(uint8_t pressed,uint8_t held,uint8_t tick,TextEntryEffects& effects,std::string& error,bool repeatAlreadyAdvanced=false);
 // Only actual actor62 flags==3 may acknowledge this source barrier.
 bool resumeKey(bool actorComplete,TextEntryEffects& effects,std::string& error);
 std::vector<MouseTarget> mouseTargets()const;
 uint8_t mouseActionMask(unsigned target)const;
 bool mouseSelect(int x,int y,TextEntryEffects& effects,std::string& error);
 bool awaitingKey()const{return _pendingKey!=0;}
 bool active()const{return _active;}bool completed()const{return _completed;}
 bool submittedKonami()const;
 uint16_t selectedReference()const{return _choices.empty()?0:_choices[_selected].reference;}
 const TextEntrySettings& settings()const{return _settings;}
 const std::array<uint8_t,18>& buffer()const{return _buffer;}
 uint8_t cursor()const{return _cursor;}uint8_t gridIndex()const{return _gridIndex;}
 uint8_t mode()const{return _mode;}
 bool transfers(std::vector<GraphicsTransfer>& out,std::string& error)const;
 bool beginGraphics(bool fixedWidth,std::string& error);
 bool queueGraphics(const TextEntryEffects& effects,std::string& error);
 bool coordinateGraphics(bool visualBusy,TextEntryEffects& effects,std::string& error);
 bool consumeGraphics(uint8_t counter,std::string& error);
 bool graphicsReady()const{return _graphicsReady;}
 bool beginCloseGraphics(int view,std::string& error);
 bool graphicsBusy()const{return _graphicsBusy;}
 const std::vector<uint8_t>& fontCanvas()const{return _graphicsCanvas;}
 const std::vector<uint8_t>& fontNames()const{return _graphicsNames;}
private:
 friend class EngineStateCodec;
 friend class SceneRuntime;
 struct FontRequest {std::vector<uint8_t> bytes;bool blink;FontRequest(const std::vector<uint8_t>& data,bool isBlink=false):bytes(data),blink(isBlink){}};
 std::vector<uint8_t> _graphicsSource,_graphicsCanvas,_graphicsNames,_graphicsProgram;
 std::deque<FontRequest> _graphicsQueue;FontProgramState _graphicsFont;
 bool _graphicsBusy=false,_graphicsReady=false,_graphicsInitial=false,_graphicsBlink=false,_graphicsUnderline=true;
 bool _graphicsFresh=true,_graphicsResetCaption=false,_graphicsPolicy=false,_graphicsPalette=false,_graphicsNamesPending=false,_graphicsClearCanvas=false,_graphicsClearBuffer=false;
 uint8_t _graphicsPhase=0,_graphicsDirtyRows=0;int _graphicsView=-1;
 void queueUnderline();void startFontRequest(const FontRequest& request);
 OriginalFont _font;bool _loaded,_active,_completed;TextEntrySettings _settings;
 std::vector<uint8_t> _numeric,_alpha,_frames,_phoneLengths,_numericText;std::vector<TextEntryChoice> _choices;
 std::array<uint8_t,18> _buffer;std::array<uint8_t,8> _repeat;
 size_t _selected;uint8_t _heldRepeat,_pendingKey,_cursor,_gridIndex,_mode,_columns,_originX,_originY,_textX,_phase,_anchor,_blinkDeadline;bool _underline,_blinkStarted;
 const std::vector<uint8_t>& grid()const{return _mode==2?_numeric:_alpha;}
 void finish(TextEntryEffects& effects);void erase();bool move(int delta,std::string& error);void coordinates(TextEntryEffects& effects)const;
};
}
#endif
