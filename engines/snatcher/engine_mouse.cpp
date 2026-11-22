/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "engine.h"
#include "native_visual_jobs.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "graphics/surface.h"
namespace Snatcher {
bool SnatcherEngine::mouseEnabled()const {
 return !ConfMan.hasKey("snatcher_original_controls_only")||!ConfMan.getBool("snatcher_original_controls_only");
}
void SnatcherEngine::releaseInput(){
 _pointerRepeat.reset();
 _startup.cancelMouseSkip();_mouseDialoguePending=false;
 _startupHeld=_startupPressed=_shootingHeld=_shootingPressed=_entryGlobalPressed=_entryFramePressed=0;
 _confirmHeld=_startPressed=_workerStartPressed=false;_entryMousePending=false;_entryMousePressed=0;_mouseCommandPending=_mouseStartupPending=false;
 std::string ignored;const CommandButton buttons[]={kCommandUp,kCommandDown,kCommandLeft,kCommandRight,kCommandCancel,kCommandConfirm};
 for(CommandButton b:buttons)_commandInput.setButton(b,false,ignored);
 _commandInput.discardPressed();
}
bool SnatcherEngine::processMouseEvent(const Common::Event& event,std::string& error){
 error.clear();MouseEvent e;
 switch(event.type){
 case Common::EVENT_MOUSEMOVE:e.kind=kMouseMove;break;
 case Common::EVENT_LBUTTONDOWN:e.kind=kMouseLeftDown;break;
 case Common::EVENT_LBUTTONUP:e.kind=kMouseLeftUp;break;
 case Common::EVENT_RBUTTONDOWN:e.kind=kMouseRightDown;break;
 case Common::EVENT_RBUTTONUP:e.kind=kMouseRightUp;break;
 case Common::EVENT_WHEELUP:case Common::EVENT_WHEELDOWN:e.kind=kMouseWheel;e.wheel=event.type==Common::EVENT_WHEELUP?-1:1;break;
 case Common::EVENT_FOCUS_LOST:e.kind=kMouseFocusLost;releaseInput();_pauseKeyHeld=false;break;
 case Common::EVENT_FOCUS_GAINED:e.kind=kMouseFocusGained;releaseInput();_pauseKeyHeld=false;break;
 default:return true;
 }
 // EventManager already performs scaling, aspect, DPI and letterbox conversion.
 // Retain signed game coordinates and reject outside points; never clamp an
 // out-of-view click onto an active edge control.
 e.x=event.mouse.x;e.y=event.mouse.y;e.valid=!event.mouseOutsideActiveArea&&e.x>=0&&e.y>=0&&e.x<int(_canvasWidth)&&e.y<int(_canvasHeight);
 bool moved=e.valid&&(!_mousePositionKnown||e.x!=_mouseX||e.y!=_mouseY);
 if(e.valid&&e.kind<=kMouseWheel){_mouseX=e.x;_mouseY=e.y;_mousePositionKnown=true;}
 MouseContext c=mouseContext(error);if(!error.empty())return false;uint64_t identity=c.generation;
 if(identity!=_mouseContextIdentity){_mouseDialoguePending=false;_mouseContextIdentity=identity;++_mouseContextGeneration;_entryMousePending=false;_entryMousePressed=0;_mouseCommandPending=_mouseStartupPending=false;}
 c.generation=_mouseContextGeneration;
 MouseAction a=_mouseInput.handle(c,e);
 if(c.kind==kMouseShooting&&c.ready&&e.valid&&e.x<256&&e.y<224&&((e.kind==kMouseMove&&moved)||a.kind==kMouseActivate)){
  _mouseAimSelected=true;_pointerX=uint16_t(e.x);_pointerY=uint16_t(e.y);
 }
 if(c.kind==kMouseInvestigation&&c.ready&&!_mouseCommandPending&&e.valid&&e.kind==kMouseMove&&e.y<143)return moveMouseExamination(e.x,e.y,false,error);
 if(a.kind==kMouseNoAction)return true;
 bool activate=a.kind==kMouseActivate,hover=a.kind==kMouseHover;
 if(c.kind==kMousePaused){if(activate&&_inputPause.isActive()){_inputPause.clear();releaseInput();}}
 else if(c.kind==kMouseTitle||c.kind==kMouseOptions||c.kind==kMouseSaveSlots){
  if(_mouseStartupPending)return true;
  if(a.kind==kMouseCancel){_mouseStartupPending=true;_mouseStartupSelect=false;_mouseStartupContext=identity;_mouseStartupMask=c.kind==kMouseOptions?0x80:0x10;return true;}
  if(activate||hover){if(!_startup.mouseSelect(a.target,_scene,error))return false;if(activate){_mouseStartupPending=_mouseStartupSelect=true;_mouseStartupTarget=a.target;_mouseStartupContext=identity;_mouseStartupMask=(c.kind==kMouseOptions&&a.target<3)?8:0x80;}}
 }else if(c.kind==kMouseStartupSkip){if(activate)_startup.queueMouseSkip();}
 else if(c.kind==kMouseShooting){if(activate)_shootingPressed|=0x20;}
 else if(c.kind==kMouseCredits||c.kind==kMouseContinue){if(activate)_startPressed=true;}
 else if(c.kind==kMouseDialogue){if(activate){if(_dialogueReady){if(!_mouseDialoguePending){_mouseDialoguePending=true;_mouseDialogueContext=identity;_mouseDialogueCursor=_dialogueWindow.cursor();}}else _entryGlobalPressed|=0x20;}}
 else if(c.kind==kMouseEntry){
  if(a.kind==kMouseCancel){if(!_entryMousePressed)_entryMousePressed=0x10;}
  else if((activate||hover)&&!_entryMousePressed){uint8_t mask=_textEntry.mouseActionMask(a.target);if(!mask)return true;_entryMousePending=a.target!=kTextEntryMouseSubmit;if(_entryMousePending){_entryMouseX=e.x;_entryMouseY=e.y;}if(activate)_entryMousePressed=mask;}
 }else if(c.kind==kMouseCommands||c.kind==kMouseInvestigation){
  if(_mouseCommandPending)return true;
  if(a.kind==kMouseCancel){_mouseCommandPending=true;_mouseCommandButton=kCommandCancel;_mouseCommandContext=identity;}
  else if(c.kind==kMouseInvestigation&&a.kind==kMouseScroll){
   // Wheel selection is independent of the hit target, including the upper
   // examination view. Never use a wheel event's coordinates to move the beam.
   if(!c.ready)return true;
   const unsigned n=unsigned(_commandInput.references().size());if(!n)return true;
   const unsigned index=(_commandInput.selectedIndex()+n+(a.delta>0?1:-1))%n;
   if(!_commandInput.select(index,error))return false;_selection=index;
   if(!showMenu(false,error))return false;
  }
  else if(a.target==0xffff){
   if(activate){
    // Confirm the highlighted command at the already positioned flashlight.
    // The normal coordinator publishes its hit region before consuming this
    // intent. It also defers confirmation until a wheel repaint is published.
    _mouseCommandPending=true;_mouseCommandButton=kCommandConfirm;_mouseCommandContext=identity;
    _mouseCommandIndex=_commandInput.selectedIndex();_mouseCommandReference=_commandInput.selectedReference();
   }else if(c.ready&&hover&&!moveMouseExamination(e.x,e.y,false,error))return false;
  }else if(activate||hover||a.kind==kMouseScroll){
   unsigned n=unsigned(_commandInput.references().size()),index=a.target;
   if(!c.ready&&a.target>=n)return true;
   if(index>=n&&hover)return true;
   if(index>=n||a.kind==kMouseScroll){DialogueLayout layout;if(!_scene.dialogueLayout(layout,error))return false;unsigned capacity=layout.variant?6:7,first=capacity-1;if(a.kind==kMouseScroll&&n<=capacity)index=(_commandInput.selectedIndex()+n+(a.delta>0?1:-1))%n;else index=(a.kind==kMouseScroll?(a.delta>0):index==n)?first:0;}
   // A click during this owner's repaint records intent, without retargeting
   // the in-flight presentation. The coordinator applies it once ready.
   if(activate&&a.target<n&&!c.ready){_mouseCommandPending=true;_mouseCommandButton=kCommandConfirm;_mouseCommandContext=identity;_mouseCommandIndex=index;_mouseCommandReference=_commandInput.references()[index];return true;}
   bool changed=index!=_commandInput.selectedIndex();if(!_commandInput.select(index,error))return false;_selection=index;if(changed&&!showMenu(false,error))return false;
   if(activate&&a.target<n){_mouseCommandPending=true;_mouseCommandButton=kCommandConfirm;_mouseCommandContext=identity;_mouseCommandIndex=index;_mouseCommandReference=_commandInput.selectedReference();}
  }
 }
 return error.empty();
}
bool SnatcherEngine::moveMouseExamination(int x,int y,bool publish,std::string& error){
 if(!_pointerInteraction.makeDirectView(256,144,128,72,0,0,_directView,error))return false;
 PointerDirectProjection point;return _pointerInteraction.applyDirect(_directView,x,y,false,publish,point,error);
}
bool SnatcherEngine::consumeMouseCommand(std::string& error){
 if(!_mouseCommandPending)return true;
 MouseContext context=mouseContext(error);if(!error.empty())return false;
 if(context.generation!=_mouseCommandContext||(context.kind!=kMouseCommands&&context.kind!=kMouseInvestigation)){_mouseCommandPending=false;return true;}
 // Selection may have queued a cursor/menu repaint. Keep the accepted click
 // attached to this owner until publication finishes; never replay into another.
 if(!context.ready)return true;
 _mouseCommandPending=false;
 if(_mouseCommandButton==kCommandConfirm){const auto& refs=_commandInput.references();if(_mouseCommandIndex>=refs.size()||refs[_mouseCommandIndex]!=_mouseCommandReference)return true;if(!_commandInput.select(_mouseCommandIndex,error))return false;}
 else if(!context.canCancel)return true;
 return _commandInput.submit(_mouseCommandButton,error);
}
bool SnatcherEngine::consumeMouseDialogue(std::string& error){
 if(!_mouseDialoguePending)return true;MouseContext context=mouseContext(error);if(!error.empty())return false;
 if(context.kind!=kMouseDialogue||context.generation!=_mouseDialogueContext||!_dialogueWindow.waiting()||_dialogueWindow.cursor()!=_mouseDialogueCursor){_mouseDialoguePending=false;return true;}
 // Consume after the prior frame's font/prompt publication, before the next
 // prompt producer. The same page owns the click throughout that transaction.
 if(!context.ready)return true;_mouseDialoguePending=false;return acknowledgeText(error);
}
bool SnatcherEngine::consumeMouseStartup(std::string& error){
 if(!_mouseStartupPending)return true;
 MouseContext context=mouseContext(error);if(!error.empty())return false;
 if(context.generation!=_mouseStartupContext||(context.kind!=kMouseTitle&&context.kind!=kMouseOptions&&context.kind!=kMouseSaveSlots)){_mouseStartupPending=false;return true;}
 if(!context.ready||!_startup.mouseActionReady(_mouseStartupMask))return true;
 _mouseStartupPending=false;
 if(_mouseStartupSelect&&!_startup.mouseSelect(_mouseStartupTarget,_scene,error))return false;
 _startupPressed|=_mouseStartupMask;return true;
}
MouseContext SnatcherEngine::mouseContext(std::string& error){
 error.clear();
 MouseContext c;
 c.ready=mouseEnabled()&&!isPaused()&&_ready&&_blocker.empty()&&_mouseInput.focused();
 uint64_t identity=uint64_t(_events.generation())<<32;
 if(_inputPause.isActive()){c.kind=kMousePaused;c.ready=_ready&&_mouseInput.focused();if(mouseEnabled())c.targets.push_back(MouseTarget(MouseRect((int(_canvasWidth)-96)/2,(int(_canvasHeight)-24)/2,96,24),0));c.generation=identity^(uint64_t(kMousePaused)<<56);return c;}
 if(_startupActive){
  identity^=uint64_t(_startup.phase())<<16;
  if(_startup.phase()==kStartupNativeTitleReady)c.kind=kMouseTitle;
  else if(_startup.phase()==kStartupNativeOptionsPoll){c.kind=kMouseOptions;c.canCancel=true;}
  else if(_startup.phase()==kStartupNativeLoadPoll){c.kind=kMouseSaveSlots;c.canCancel=true;}
  else if(_startup.phase()==kStartupNativeLicense||_startup.phase()==kStartupNativeCinematicFirst||_startup.phase()==kStartupNativeCinematicSecond){c.kind=kMouseStartupSkip;c.targets.push_back(MouseTarget(MouseRect(0,0,int(_canvasWidth),int(_canvasHeight)),0));}
  if(c.kind!=kMouseNone&&c.kind!=kMouseStartupSkip){c.targets=_startup.mouseTargets(_scene,error);c.deferActivation=c.ready;c.ready=c.ready&&nativeStartupInputReady(_scene.graphicsServiceBusy(),_scene.resourceChangePending());}
  if(!error.empty())return c;
 }else if(_runtime){
  identity^=(uint64_t(_runtime->cursor().resource)<<16)|_runtime->cursor().node;
  if(_shooting.active){c.kind=kMouseShooting;c.targets.push_back(MouseTarget(MouseRect(0,0,256,224),0));}
  else if(_creditsPhase){if(_creditsPhase==3&&_credits.waitingStart()){c.kind=kMouseCredits;c.targets.push_back(MouseTarget(MouseRect(0,0,int(_canvasWidth),int(_canvasHeight)),0));}}
  else if(_entryOpen){c.kind=kMouseEntry;c.deferActivation=c.ready&&_textEntry.graphicsReady()&&!_textEntry.awaitingKey()&&!_entryPending;c.ready=c.deferActivation&&!_scene.visualBusy()&&!_textEntry.graphicsBusy()&&!_scene.fontPublicationPending();c.canCancel=true;c.targets=_textEntry.mouseTargets();identity^=uint64_t(_textEntry.mode())<<48;}
  else if(_runtime->status()==kRuntimeChoices&&_commandInput.active()){
   c.kind=kMouseCommands;c.deferActivation=c.ready&&_coordinatorMode==2;c.ready=c.ready&&nativeMenuInputReady(_scene.visualBusy(),_dialogueWindow.menuPainting(),_scene.fontPublicationPending())&&_coordinatorMode==2;c.canCancel=!_runtime->control().selectedPath().empty();
   DialogueLayout layout;if(!_scene.dialogueLayout(layout,error))return c;
   c.targets=commandMouseTargets(unsigned(_commandInput.references().size()),_commandInput.selectedIndex(),layout.variant);
   for(uint16_t ref:_commandInput.references())identity=(identity^ref)*UINT64_C(1099511628211);
   for(uint16_t ref:_runtime->control().selectedPath())identity=(identity^ref)*UINT64_C(1099511628211);
   identity^=uint64_t(c.canCancel)<<55;
   if(_pointerInteraction.active()&&!c.canCancel){c.kind=kMouseInvestigation;c.targets.push_back(MouseTarget(MouseRect(0,0,256,143),0xffff));}
  }else if(_dialogueWindow.active()&&(_dialogueReady||_dialogueWindow.revealing())){
   c.kind=kMouseDialogue;c.deferActivation=c.ready&&_dialogueReady;c.ready=c.ready&&(_dialogueWindow.revealing()||nativeDialogueInputReady(_dialogueReady,_scene.visualBusy(),_scene.fontPublicationPending()));c.targets.push_back(MouseTarget(MouseRect(0,0,int(_canvasWidth),int(_canvasHeight)),0));identity^=_dialogueToken.id;
  }
  if(c.kind==kMouseNone&&!_shooting.active&&!_creditsPhase&&_runtime->status()==kRuntimeWaiting){
   // Only the first unresolved source consumer owns input. Publish the normal
   // Start edge; dispatchNativeCommand remains responsible for completion.
   for(const RuntimeRequest& request:_runtime->requests())if(_events.awaiting(request.token)){
    if(request.kind==kRequestCommand&&request.queue==33&&request.operands.empty()){
     c.kind=kMouseContinue;c.ready=c.ready&&!_scene.visualBusy()&&!_scene.transferBusy()&&!_scene.resourceChangePending()&&!_scene.pendingGraphicsTasks();
     c.targets.push_back(MouseTarget(MouseRect(0,0,int(_canvasWidth),int(_canvasHeight)),0));identity^=request.token.id;
    }
    break;
   }
  }
 }
 identity^=uint64_t(c.kind)<<56;
 c.generation=identity;return c;
}
void SnatcherEngine::drawMouse(Graphics::Surface& screen){
 if((!mouseEnabled()||isPaused())&&!_inputPause.isActive())return;
 std::string error;MouseContext context=mouseContext(error);if(!error.empty()||context.kind==kMouseNone||(!context.ready&&!context.deferActivation))return;
 auto pixel=[&](int px,int py,bool white){if(px>=0&&py>=0&&px<screen.w&&py<screen.h)screen.setPixel(px,py,screen.format.RGBToColor(white?255:0,white?255:0,white?255:0));};
 auto text=[&](const char* label,int center,int y){int width=0;for(const char* p=label;*p;++p){FontGlyph glyph;if(!_font.glyph(uint8_t(*p),glyph,error))return;width+=glyph.advance;}int x=center-width/2;for(const char* p=label;*p;++p){FontGlyph glyph;if(!_font.glyph(uint8_t(*p),glyph,error))return;for(int row=0;row<8;++row)for(int col=0;col<8;++col)if(glyph.rows[row]&(128>>col))pixel(x+col,y+row,true);x+=glyph.advance;}};
 // The original numeric pad completes with Start, and has no END cell. Expose
 // that same action as a visible PC control; phone calls still auto-complete.
 if(context.kind==kMousePaused){text("PAUSED",int(_canvasWidth)/2,int(_canvasHeight)/2-28);if(!mouseEnabled())text("P TO RESUME",int(_canvasWidth)/2,int(_canvasHeight)/2);}
 for(const MouseTarget& target:context.targets)if((context.kind==kMouseEntry&&target.id==kTextEntryMouseSubmit)||context.kind==kMousePaused){
  const MouseRect& r=target.rect;bool hover=_mousePositionKnown&&r.contains(_mouseX,_mouseY);
  for(int y=0;y<r.h;++y)for(int x=0;x<r.w;++x)if(r.x+x>=0&&r.y+y>=0&&r.x+x<screen.w&&r.y+y<screen.h)screen.setPixel(r.x+x,r.y+y,screen.format.RGBToColor(0,hover?64:32,hover?112:64));
  for(int x=0;x<r.w;++x){pixel(r.x+x,r.y,true);pixel(r.x+x,r.y+r.h-1,true);}for(int y=0;y<r.h;++y){pixel(r.x,r.y+y,true);pixel(r.x+r.w-1,r.y+y,true);}
  text(context.kind==kMousePaused?"RESUME":"OK",r.x+r.w/2,r.y+(r.h-8)/2);
 }
 if(!mouseEnabled()||!_mousePositionKnown)return;
 int x=_mouseX,y=_mouseY;
 bool aim=context.kind==kMouseShooting&&_shooting.active&&_mouseAimSelected;
 if(aim){x=_shooting.aimX;y=_shooting.aimY;}
 if(aim){for(int d=-5;d<=5;++d)if(d<-1||d>1){pixel(x+d,y+1,false);pixel(x+1,y+d,false);pixel(x+d,y,true);pixel(x,y+d,true);}}
 else{for(int row=0;row<9;++row)for(int col=0;col<=row/2;++col)pixel(x+col,y+row,col>0&&col<row/2&&row<8);}
}
}
