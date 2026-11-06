/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CHARACTER_CREATION_H
#define NOX_NATIVE_CHARACTER_CREATION_H
#include "nox/native_new_game.h"
#include "nox/native_graphics.h"
#include "common/events.h"
namespace Nox { namespace Native {
// Keyboard workflow shared by the native host and deterministic event replay.
// Creation commits only on confirmation; Escape restarts the entire selection.
class CharacterCreation {
public:
    enum Stage { kName,kGender,kRace,kConfirm,kCreated };
    CharacterCreation() : _stage(kName),_gender(0),_race(0) {}
    Stage stage() const { return _stage; }
    const Common::String &name() const { return _name; }
    byte gender() const { return _gender; }
    byte race() const { return _race; }
    bool adoptReadyGame(const NewGame &game,HdvArchive &archive) {
        if (!game.ready() || !game.party.member(0)) return false;Common::Array<byte> loader;
        if (!archive.read("NA/LOADER.P",loader) || loader.size()!=10514) return false;
        const byte *record=game.party.member(0)->bytes;CharacterCreation next;
        next._gender=record[loader[0x4be+10]];
        //Equipment derivation owns bits7/6 for the two weapon hands in this
        //same source byte. Decode race without rejecting an equipped player;
        //the full record (including both weapon flags) remains unchanged.
        next._race=record[loader[0x4be+12]]&63;
        if (next._gender>=3 || next._race>=5) return false;
        for (uint i=0;i<15 && record[75+i];++i) {
            const byte c=record[75+i]&127;if (c<32 || c>122 || next._name.size()>=14) return false;next._name+=char(c);
        }
        if (!next.validName()) return false;next._stage=kCreated;*this=next;return true;
    }
    bool handle(const Common::Event &event,NewGame &game) {
        if (event.type!=Common::EVENT_KEYDOWN || _stage==kCreated) return true;
        const Common::KeyCode key=event.kbd.keycode;
        if (key==Common::KEYCODE_ESCAPE) { *this=CharacterCreation(); return true; }
        if (_stage==kName) {
            if (key==Common::KEYCODE_BACKSPACE) { if (!_name.empty()) _name.deleteLastChar(); }
            else if (key==Common::KEYCODE_RETURN) { if (validName()) _stage=kGender; }
            else if (event.kbd.ascii>=32 && event.kbd.ascii<=122 && _name.size()<14) _name+=char(event.kbd.ascii);
        } else if (key==Common::KEYCODE_RETURN) {
            if (_stage==kGender) _stage=kRace;
            else if (_stage==kRace) _stage=kConfirm;
            else { if (!game.create(_name,_gender,_race)) return false; _stage=kCreated; }
        } else if (key==Common::KEYCODE_RIGHT || key==Common::KEYCODE_LEFT) {
            const uint count=_stage==kGender?3:5;
            if (_stage==kGender) _gender=byte((_gender+(key==Common::KEYCODE_RIGHT?1:count-1))%count);
            else if (_stage==kRace) _race=byte((_race+(key==Common::KEYCODE_RIGHT?1:count-1))%count);
        }
        return true;
    }
    bool render(const GlyphFont &font,HgrFrame &frame) const {
        HgrFrame next;
        static const char *genders[]={"MALE","FEMALE","OTHER"};
        static const char *races[]={"HUMAN","HALFLING","ELF","DARK ELF","DWARF"};
        if (!text(next,font,4,3,"NOX ARCHAIST") || !text(next,font,4,5,"CREATE YOUR CHARACTER") ||
            !text(next,font,4,8,Common::String("NAME: ")+_name) ||
            !text(next,font,4,10,Common::String("GENDER: ")+genders[_gender]) ||
            !text(next,font,4,12,Common::String("RACE: ")+races[_race])) return false;
        const char *prompt=_stage==kName?"ENTER A NAME, THEN RETURN":_stage==kConfirm?"RETURN TO BEGIN":_stage==kCreated?"":"ARROWS TO CHOOSE, RETURN TO ACCEPT";
        if (!text(next,font,2,17,prompt) || !text(next,font,4,20,"ESC TO START OVER")) return false;
        frame=next; return true;
    }
private:
    bool validName() const { for (uint i=0;i<_name.size();++i) if (_name[i]!=' ') return true; return false; }
    static bool text(HgrFrame &frame,const GlyphFont &font,uint column,uint row,const Common::String &value) {
        if (column+value.size()>40) return false;
        for (uint i=0;i<value.size();++i) if (!frame.drawGlyph(font,byte(value[i]),column+i,row,0)) return false;
        return true;
    }
    Stage _stage; Common::String _name; byte _gender,_race;
};
} }
#endif
