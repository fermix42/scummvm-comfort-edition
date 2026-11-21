/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_DIALOGUE_WINDOW_H
#define SNATCHER_DIALOGUE_WINDOW_H
#include "scene_assets.h"
#include <deque>
namespace Snatcher {
// Source dialogue ownership: a 256x64 proportional-font pattern canvas, not an
// overlay or a hardware register model. The scene owner establishes its view.
struct DialogueLayout {
 uint8_t top,variant;bool established,fixedWidth,substitutionKnown;uint8_t substitution;
 DialogueLayout():top(0),variant(0),established(false),fixedWidth(false),substitutionKnown(false),substitution(0){}
};
// SUB-SP6066 clears7800..9DBF: includes7893,971D and9728.
DialogueLayout freshModuleDialogueLayout();
enum DialogueViewAction {kDialogueNoViewAction,kDialogueCloseView,kDialogueReopenView,kDialogueDefaultView,kDialogueSpecialView,kDialogueMenuView};
class DialogueWindow {
public:
 DialogueWindow();
 bool loadAssets(const uint8_t *subcode,size_t size,std::string& error);
 bool begin(const uint8_t *scenario,size_t size,size_t textOffset,uint16_t speakerWord,const DialogueLayout& layout,std::string& error);
 bool beginTask(const uint8_t *scenario,size_t size,size_t textOffset,uint16_t speakerWord,const DialogueLayout& layout,std::string& error);
 bool coordinateTask(std::string& error);
 bool tickGlyphs(bool paused,std::string& error,bool fastReveal=false);
 bool taskOwned()const{return _taskOwned;}
 uint8_t defaultViewCommand()const{return _layout.variant==0?0x0f:_layout.variant==1?0x14:0x16;}
 bool beginMenu(const uint8_t *scenario,size_t size,const std::vector<uint16_t>& identities,unsigned selected,const DialogueLayout& layout,std::string& error);
 bool beginMenuTask(const uint8_t *scenario,size_t size,const std::vector<uint16_t>& identities,unsigned selected,const DialogueLayout& layout,bool opening,std::string& error);
 bool coordinateMenu(std::string& error);
 bool menuTaskOwned()const{return _menuTask;}
 bool menuPainting()const{return _menuTask&&!_waiting;}
 bool menuPieces(std::vector<PlacedPiece>& pieces,std::string& error)const;
 bool tick(bool paused,std::string& error,bool fastReveal=false);
 bool tickPrompt(std::string& error);
 bool acknowledge(std::string& error);
 void cancel();
 bool active()const{return _active;}bool waiting()const{return _waiting;}bool finished()const{return _finished;}
 bool revealing()const{return _active&&!_menu&&!_finished&&!_waiting&&!_transition;}
 bool textEnded()const{return _textEnd;}uint8_t mode()const{return _mode;}
 uint16_t speakerWord()const{return _speakerWord;}
 DialogueViewAction takeViewAction(){if(_views.empty())return kDialogueNoViewAction;DialogueViewAction action=_views.front();_views.pop_front();return action;}
 size_t cursor()const{return _sourceBase+_sourceCursor;}size_t revealedGlyphs()const{return _revealed;}
 // Full owned state is intentional: callers may replay it transactionally into
 // their allocation owner. Empty/inactive windows never invent scene content.
 bool transfers(std::vector<GraphicsTransfer>& out,std::string& error)const;
 const std::vector<uint8_t>& patterns()const{return _patterns;}
private:
 friend class SceneStateCodec;friend class SceneRuntime;
 bool _canvasFresh=true;
 uint8_t _dirtyRows=0;bool _namesDirty=false,_clearCanvasPending=false,_promptNamesDirty=false;
 enum TaskPhase {kTaskReset,kTaskPolicy,kTaskAtlas,kTaskCanvas,kTaskSpeaker,kTaskBody,kTaskReady,kTaskInput,kTaskFinish,kTaskNextCanvas,kTaskReopen};
 bool _taskOwned=false,_ackPending=false,_captionResetPending=false,_policyPending=false,_atlasPending=false;
 uint8_t _taskPhase=kTaskReset;
 // Old payloads keep their literal consumer mode until a real F7 writer binds
 // the shared source9728 owner. Pending writes survive producer restoration.
 bool _fontModePending=false,_fontModeBound=false;
 bool _menuTask=false,_menuOpening=false,_menuCursorPending=false;
 uint8_t _menuPhase=0;
 std::vector<uint8_t> _menuPrograms;size_t _menuProgramCursor=0,_menuSelectionProgram=0;
 OriginalFont _font;std::vector<uint8_t> _subcode,_source,_patterns,_names,_promptPatterns,_cursorPatterns,_stream;
 DialogueLayout _layout;size_t _sourceBase,_sourceCursor,_streamCursor,_revealed;
 std::vector<FramePiece> _cursorGeometry;bool _menu;unsigned _menuSelection;uint8_t _menuCount;
 uint16_t _speakerWord;uint8_t _speaker,_bodyY,_textColor,_color,_x,_y,_delay,_remaining,_promptOffset,_promptRemaining,_mode;
 bool _loaded,_active,_waiting,_finished,_textEnd,_streamActive,_nameStage,_lineBreak,_pause,_promptVisible;
 uint8_t _transition;std::deque<DialogueViewAction> _views;
 bool beginOwned(const uint8_t *scenario,size_t size,size_t textOffset,uint16_t speakerWord,const DialogueLayout& layout,std::string& error);
 bool queueSpeaker(std::string& error);bool queueBody(std::string& error);bool raster(uint8_t code,std::string& error);bool consume(std::string& error);bool advancePrompt(bool& automatic,std::string& error);
 void continuePage();
 void resetCanvas();void promptNames(unsigned offset);
};
}
#endif
