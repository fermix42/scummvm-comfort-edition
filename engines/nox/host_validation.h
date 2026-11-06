/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_HOST_VALIDATION_H
#define NOX_HOST_VALIDATION_H
#include "common/events.h"
#include "common/config-manager.h"
#include "common/system.h"
#include "engines/engine.h"
#include "nox/runtime.h"
#include "gui/gui-manager.h"
#include "gui/ThemeEval.h"
#include "image/png.h"
#include "common/savefile.h"

namespace Nox {
// Test-only input source: Escape is delivered only after the real global menu
// has paused its engine. No dialog is bypassed and no game state is injected.
class MenuValidationSource : public Common::EventSource {
	Engine &_engine;
	Runtime *&_runtime;
	Common::EventDispatcher *_dispatcher;
	bool _armed = false, _keyup = false, _quit = false, _mouseUp = false;
	Common::Point _click;
	bool _chooser=false, _save=false;
	bool _cancelChooser=false;
	Common::String _cancelCapture;
	uint _chooserStep=0;
	uint32 _nextInput=0;
	int _saveRow=1;
	int _loadRow=0;
	bool _grid=false,_gridSave=false,_gridKeyUp=false;
	uint _gridTile=0,_gridPhase=0,_gridTextPos=0;
	Common::String _gridText,_gridCapture;
	Common::KeyState _gridLastKey;
	int _popupIndex=-1,_popupRemaining=0;
	uint _popupPhase=0;
	bool captureChooser(const Common::String &filename) {
		Graphics::Surface overlay;
		overlay.create(g_system->getOverlayWidth(),g_system->getOverlayHeight(),g_system->getOverlayFormat());
		g_system->grabOverlay(overlay);
		Common::OutSaveFile *capture=g_system->getSavefileManager()->openForSaving(filename,false);
		bool ok=false;
		if(capture){ok=Image::writePNG(*capture,overlay);capture->finalize();ok=ok&&!capture->err();delete capture;}
		overlay.free();return ok;
	}
	uint32 _opened = 0;
	uint64 _cycles = 0;
public:
	bool closed = false, stable = false, chooserCaptured = false, cancelCaptured=false;
	MenuValidationSource(Engine &engine, Runtime *&runtime, Common::EventManager &events, bool enabled)
		: _engine(engine), _runtime(runtime), _dispatcher(enabled ? events.getEventDispatcher() : nullptr) {
		if (_dispatcher) _dispatcher->registerSource(this, false);
	}
	~MenuValidationSource() { if (_dispatcher) _dispatcher->unregisterSource(this); }
	void arm() { _armed = true; }
	void armQuit() { _armed = true; _quit = true; _opened = 0; }
	void chooser(bool save,int saveRow=1,bool cancel=false,const char *capture=nullptr) {
		// Mouse row coordinates are relative to a known viewport, not the
		// legitimate remembered scroll position from an earlier dialog.
		ConfMan.setInt("gui_saveload_last_pos",0);
		_chooser=true;_save=save;_saveRow=saveRow;_cancelChooser=cancel;_chooserStep=0;_nextInput=0;
		_cancelCapture=capture?capture:"";cancelCaptured=false;
		_loadRow=0;
	}
	void loadRow(int row) { _loadRow=row; }
	void selectSession(uint index) { _popupIndex=index;_popupRemaining=index;_popupPhase=0;_nextInput=0; }
	void grid(bool save,uint tile,const char *description,const char *capture) {
		_grid=true;_gridSave=save;_gridTile=tile;_gridPhase=0;_gridTextPos=0;
		_gridText=description?description:"";_gridCapture=capture;_nextInput=0;
	}
	bool allowMapping() const override { return false; }
	bool pollEvent(Common::Event &event) override {
		if (_mouseUp) { _mouseUp=false; event.type=Common::EVENT_LBUTTONUP; event.mouse=_click; return true; }
		if(_gridKeyUp){_gridKeyUp=false;event.type=Common::EVENT_KEYUP;event.kbd=_gridLastKey;event.kbd.flags=0;return true;}
		if(_popupIndex>=0&&_engine.isPaused()){
			const uint32 now=g_system->getMillis();
			if(!_nextInput){_nextInput=now+100;return false;}
			if(now<_nextInput)return false;
			_nextInput=now+100;
			if(_popupPhase==0){
				int16 dx=0,dy=0,dw=0,dh=0,x=0,y=0,w=0,h=0;
				if(!g_gui.xmlEval()->getWidgetData("Dialog.SaveLoadChooser",dx,dy,dw,dh)||
				   !g_gui.xmlEval()->getWidgetData("SaveLoadChooser.List",x,y,w,h))return false;
				_click=Common::Point(dx+x+int(60*g_gui.getScaleFactor()),dy+y+(g_gui.getFontHeight()+2)/2);
				_popupPhase=1;_mouseUp=true;event.type=Common::EVENT_LBUTTONDOWN;event.mouse=_click;return true;
			}
			Common::KeyState key;
			if(_popupPhase==1){key=Common::KeyState(Common::KEYCODE_HOME);_popupPhase=2;}
			else if(_popupRemaining){key=Common::KeyState(Common::KEYCODE_DOWN);--_popupRemaining;}
			else{
				captureChooser("nox-grid-session-popup.png");key=Common::KeyState(Common::KEYCODE_RETURN);
				_popupIndex=-1;_nextInput=now+200;
			}
			event.type=Common::EVENT_KEYDOWN;event.kbd=key;_gridLastKey=key;_gridKeyUp=true;return true;
		}
		if(_grid&&_engine.isPaused()){
			const uint32 now=g_system->getMillis();
			if(!_nextInput){_nextInput=now+100;return false;}
			if(now<_nextInput)return false;
			_nextInput=now+100;
			int16 dx=0,dy=0,dw=0,dh=0,x=0,y=0,w=0,h=0;
			if(_gridPhase==0){
				if(!g_gui.xmlEval()->getWidgetData("Dialog.SaveLoadChooser",dx,dy,dw,dh)||
				   !g_gui.xmlEval()->getWidgetData("SaveLoadChooser.List",x,y,w,h))return false;
				const float scale=g_gui.getScaleFactor();
				const int buttonW=int(166*scale),buttonH=int(126*scale),frameW=int(10*scale);
				const int spacingX=int(4*scale),spacingY=int(8*scale),slotW=buttonW+frameW+spacingX;
				const int availableW=dw-20,columns=MAX(1,availableW/slotW);
				const int extraX=columns>1?(availableW%slotW)/(columns-1):0;
				int filterHeight=0;
				if(!_gridSave){
					const int lineHeight=g_gui.getFontHeight()+2,gap=int(6*scale),right=MAX<int>(x+int(180*scale),dw-int(10*scale));
					const int rowW=MAX<int>(int(120*scale),right-x);
					const int actions=int(128*scale)+int(78*scale)*2+int(86*scale)+int(96*scale)+gap*5;
					filterHeight=rowW>=int(220*scale)+actions?lineHeight+int(8*scale):lineHeight*2+gap+int(8*scale);
				}
				_click=Common::Point(dx+x+spacingX/2+frameW/2+buttonW/2+(_gridTile%columns)*(slotW+extraX),
					dy+y+filterHeight+spacingY/2+buttonH/2+(_gridTile/columns)*(buttonH+g_gui.getFontHeight()+2+spacingY));
				chooserCaptured=captureChooser(_gridCapture+"-chooser.png");
				_gridPhase=1;if(!_gridSave)_grid=false;
			}else if(_gridPhase==1||_gridPhase==5){
				if(!g_gui.xmlEval()->getWidgetData("Dialog.SavenameDialog",dx,dy,dw,dh)||
				   !g_gui.xmlEval()->getWidgetData(_gridPhase==1?"SavenameDialog.Description":"SavenameDialog.Ok",x,y,w,h))return false;
				_click=Common::Point(dx+x+w/2,dy+y+h/2);
				if(_gridPhase==5){captureChooser(_gridCapture+"-name.png");_grid=false;}
				else _gridPhase=2;
			}else{
				Common::KeyState key;
				if(_gridPhase==2){key=Common::KeyState(Common::KEYCODE_HOME);_gridPhase=3;}
				else if(_gridPhase==3){key=Common::KeyState(Common::KEYCODE_END,0,Common::KBD_SHIFT);_gridPhase=4;}
				else if(_gridTextPos<_gridText.size()){
					const byte ch=_gridText[_gridTextPos++];key=Common::KeyState(Common::KeyCode(ch>='A'&&ch<='Z'?ch+32:ch),ch);
				}else{_gridPhase=5;return false;}
				event.type=Common::EVENT_KEYDOWN;event.kbd=key;_gridLastKey=key;_gridKeyUp=true;return true;
			}
			_mouseUp=true;event.type=Common::EVENT_LBUTTONDOWN;event.mouse=_click;return true;
		}
		if (_chooser && _engine.isPaused()) {
			const uint32 now=g_system->getMillis();
			if(!_nextInput){_nextInput=now+100;return false;}
			if(now<_nextInput)return false;
			if(_cancelChooser){
				if(!_cancelCapture.empty())cancelCaptured=captureChooser(_cancelCapture);
				_chooser=false;_keyup=true;event.type=Common::EVENT_KEYDOWN;
				event.kbd=Common::KeyState(Common::KEYCODE_ESCAPE,27);return true;
			}
			int16 dx=0,dy=0,dw=0,dh=0,x=0,y=0,w=0,h=0;
			if(!g_gui.xmlEval()->getWidgetData("Dialog.SaveLoadChooser",dx,dy,dw,dh)||
			   !g_gui.xmlEval()->getWidgetData(_chooserStep?"SaveLoadChooser.Choose":"SaveLoadChooser.List",x,y,w,h))return false;
			if(!_chooserStep){
				if(_save){
					chooserCaptured=captureChooser("nox-save-chooser-ui.png");
				}
				// Native save list contains logical slot 1 at row 1. Compact
				// load list's first row is an existing selected-session save.
				const int lineHeight=g_gui.getFontHeight()+2;
				const int rowHeight=lineHeight+g_gui.xmlEval()->getVar("Globals.ListWidget.itemSpacing",0);
				const int padding=g_gui.xmlEval()->getVar("Globals.ListWidget.Padding.Top",0);
				int filterHeight=0;
				if(!_save){
					// Existing CE load dialog inserts session controls above the
					// themed list. Mirror its layout arithmetic for mouse input.
					const float scale=g_gui.getScaleFactor();
					const int gap=int(6*scale),right=MAX<int>(x+int(180*scale),dw-int(10*scale));
					const int rowWidth=MAX<int>(int(120*scale),right-x);
					const int actions=int(128*scale)+int(78*scale)*2+int(86*scale)+int(96*scale)+gap*5;
					filterHeight=rowWidth>=int(220*scale)+actions?lineHeight+int(8*scale):lineHeight*2+gap+int(8*scale);
				}
				_click=Common::Point(dx+x+10,dy+y+filterHeight+padding+rowHeight*(_save?_saveRow:_loadRow)+rowHeight/2);
			}else {
				captureChooser(_save?"nox-save-chooser-selected-ui.png":"nox-load-chooser-selected-ui.png");
				_click=Common::Point(dx+x+w/2,dy+y+h/2);
			}
			if(++_chooserStep==2)_chooser=false;
			_nextInput=now+100;_mouseUp=true;
			event.type=Common::EVENT_LBUTTONDOWN;event.mouse=_click;return true;
		}
		if (_keyup) {
			_keyup = false; event.type = Common::EVENT_KEYUP;
			event.kbd = Common::KeyState(Common::KEYCODE_ESCAPE, 27); return true;
		}
		if (!_armed || !_engine.isPaused()) return false;
		if (!_opened) { _opened = g_system->getMillis(); _cycles = _runtime?_runtime->cpuClocks():0; }
		if (g_system->getMillis() - _opened < 100) return false;
		if (_quit) {
			// CEPlaythroughExitDialog uses fixed overlay geometry: origin
			// (40,40), Quit button (330,80,80,20). This fixture uses the
			// engine's 560x384 overlay. Leave "mark complete" unchecked.
			_click=Common::Point(410,130);
			_armed=false;_mouseUp=true;event.type=Common::EVENT_LBUTTONDOWN;event.mouse=_click;return true;
		}
		stable = _cycles == (_runtime?_runtime->cpuClocks():0);
		closed = true; _armed = false; _keyup = true;
		event.type = Common::EVENT_KEYDOWN;
		event.kbd = Common::KeyState(Common::KEYCODE_ESCAPE, 27); return true;
	}
};
} // namespace Nox
#endif
