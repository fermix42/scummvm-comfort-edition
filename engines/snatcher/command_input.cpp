/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "command_input.h"
namespace Snatcher {
CommandInput::CommandInput():_active(false),_canCancel(false),_selected(0),_held(0),_pressed(0),_repeatPhase(0),_repeatAnchor(0){}
bool CommandInput::open(const std::vector<uint16_t>& refs,bool canCancel,unsigned remembered,std::string& error){
 error.clear();if(refs.empty()||refs.size()>8){error="standard command input requires1..8 entries; larger/pointer context unsupported";return false;}
 _references=refs;_canCancel=canCancel;_selected=canCancel?0:remembered<refs.size()?remembered:0;
 _pressed=0;_repeatPhase=0;_active=true;return true;
}
bool CommandInput::select(unsigned index,std::string& error){error.clear();if(!_active||index>=_references.size()){error="mouse command selection outside active choices";return false;}_selected=index;return true;}
void CommandInput::close(){_active=false;_references.clear();_selected=0;_pressed=0;_repeatPhase=0;}
bool CommandInput::submit(CommandButton button,std::string& error){error.clear();if(!_active||(button!=kCommandConfirm&&button!=kCommandCancel)){error="Semantic command activation requires an active confirm/cancel owner";return false;}_pressed=uint8_t(button);return true;}
bool CommandInput::setButton(CommandButton button,bool down,std::string& error){
 error.clear();uint8_t bit=uint8_t(button);if(bit!=1&&bit!=2&&bit!=4&&bit!=8&&bit!=16&&bit!=32){error="unknown semantic command button";return false;}
 if(down){if(!(_held&bit))_pressed|=bit;_held|=bit;}else _held&=uint8_t(~bit);return true;
}
uint8_t CommandInput::directionalInput(uint8_t counter,uint8_t pressed){
 // Source110B0: any fresh button edge resets the hold cadence and is emitted
 // unchanged. Without an edge, phase1 waits for low5 anchor bits, then phase2
 // emits at low4 equality. The anchor is decremented once at hold discovery.
 if(pressed){_repeatPhase=0;return pressed;}
 if(!_repeatPhase){_repeatAnchor=counter;if(!_held)return 0;_repeatPhase=1;--_repeatAnchor;}
 uint8_t mask=_repeatPhase==1?31:15;
 if(((counter^_repeatAnchor)&mask)!=0)return 0;
 _repeatPhase=2;return _held;
}
CommandResponse CommandInput::update(uint8_t counter){
 CommandResponse response;if(!_active){_pressed=0;return response;}
 uint8_t pressed=_pressed;_pressed=0;uint8_t input=directionalInput(counter,pressed);
 unsigned before=_selected;
 if(_references.size()>1){if(input==kCommandUp||input==kCommandLeft)_selected=_selected?_selected-1:unsigned(_references.size()-1);
  else if(input==kCommandDown||input==kCommandRight)_selected=(_selected+1)%unsigned(_references.size());}
 response.index=_selected;response.reference=_references[_selected];
 // Confirmation/cancellation consume source pressed edges only. Holding a
 // button never re-enters a command even when directional repeat becomes due.
 if(pressed==kCommandCancel&&_canCancel){response.kind=kCommandCancelled;_active=false;}
 else if(pressed==kCommandConfirm){response.kind=kCommandConfirmed;_active=false;}
 else if(before!=_selected)response.kind=kCommandSelectionChanged;
 return response;
}
}
