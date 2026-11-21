/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_COMMAND_INPUT_H
#define SNATCHER_COMMAND_INPUT_H
#include <cstdint>
#include <string>
#include <vector>
namespace Snatcher {
enum CommandButton { kCommandUp=1,kCommandDown=2,kCommandLeft=4,kCommandRight=8,kCommandCancel=16,kCommandConfirm=32 };
enum CommandResponseKind { kCommandNoResponse,kCommandSelectionChanged,kCommandConfirmed,kCommandCancelled };
struct CommandResponse {
 CommandResponseKind kind;uint16_t reference;unsigned index;
 CommandResponse():kind(kCommandNoResponse),reference(0),index(0){}
};
// Original digital command identities, including the bounded two-page list.
// DialogueWindow owns visible page/arrow rows; pointer/text entry is separate.
class CommandInput {
public:
 CommandInput();
 bool open(const std::vector<uint16_t>& references,bool canCancel,unsigned rememberedIndex,std::string& error);
 void close();
 bool select(unsigned index,std::string& error);
 bool submit(CommandButton button,std::string& error);
 // The global input owner continues sampling while a visual owner prevents
 // menu polling. Old edges are not replayed after that owner becomes ready.
 void discardPressed(){_pressed=0;}
 // CE keymapper button edges; repeated down notifications are ignored.
 bool setButton(CommandButton button,bool down,std::string& error);
 // Explicit original logical counter low byte, not host milliseconds.
 CommandResponse update(uint8_t counter);
 bool active()const{return _active;}
 unsigned selectedIndex()const{return _selected;}
 uint16_t selectedReference()const{return _references.empty()?0:_references[_selected];}
 const std::vector<uint16_t>& references()const{return _references;}
private:
 friend class EngineStateCodec;
 std::vector<uint16_t> _references;
 bool _active,_canCancel;unsigned _selected;
 uint8_t _held,_pressed,_repeatPhase,_repeatAnchor;
 uint8_t directionalInput(uint8_t counter,uint8_t pressed);
};
}
#endif
