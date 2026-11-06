/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PARTY_SELECTION_H
#define NOX_NATIVE_PARTY_SELECTION_H
#include "nox/native_party.h"
#include "nox/native_graphics.h"
#include "common/events.h"
namespace Nox { namespace Native {
enum PartySelectionResult { kPartySelectionPending,kPartySelectionAccepted,kPartySelectionCancelled,kPartySelectionInvalid };
// GAME8600-8728: first living default; arrows can include dead in the requested
// mode, while original shifted-number aliases always require a living member.
class PartySelection {
public:
    PartySelection() : _active(false),_dead(false),_count(0),_selected(0) {}
    bool active() const {return _active;}
    byte selected() const {return _selected;}
    bool begin(const Party &party,bool allowDead) {
        if (_active || !party.ready() || !party.memberCount() || party.memberCount()>6) return false;
        uint first=0;while (first<party.memberCount() && party.member(first)->bytes[0]==255) ++first;
        if (first==party.memberCount()) return false;
        _active=true;_dead=allowDead;_count=byte(party.memberCount());_selected=byte(first);return true;
    }
    PartySelectionResult handle(const Common::Event &event,const Party &party) {
        if (!_active || !party.ready() || party.memberCount()!=_count) return kPartySelectionInvalid;
        if (event.type!=Common::EVENT_KEYDOWN) return kPartySelectionPending;
        const Common::KeyCode key=event.kbd.keycode;
        if (key==Common::KEYCODE_ESCAPE) {_active=false;return kPartySelectionCancelled;}
        if (key==Common::KEYCODE_RETURN) {_active=false;return kPartySelectionAccepted;}
        if (key==Common::KEYCODE_UP || key==Common::KEYCODE_DOWN || key==Common::KEYCODE_SPACE) {
            const int direction=key==Common::KEYCODE_UP?-1:1;
            for (uint i=0;i<_count;++i) {
                _selected=byte((int(_selected)+direction+_count)%_count);
                if (_dead || party.member(_selected)->bytes[0]!=255) break;
            }
        } else {
            const char *aliases="!@#$%^";for (uint i=0;i<6;++i)
                if (event.kbd.ascii==uint(aliases[i]) && i<_count && party.member(i)->bytes[0]!=255) _selected=byte(i);
        }
        return kPartySelectionPending;
    }
    bool paint(HdvArchive &archive,const Party &party,HgrFrame &frame) const {
        if (!_active || party.memberCount()!=_count) return false;GlyphFont font;if (!font.load(archive,1)) return false;
        for (uint row=1;row<13;++row) if (!frame.clearTextSpan(25,row,13)) return false;
        for (uint member=0;member<_count;++member) {
            const byte *name=party.member(member)->bytes+75;
            for (uint i=0;i<13 && name[i];++i) if (!frame.drawGlyph(font,name[i],25+i,1+member*2,member==_selected?127:0)) return false;
        }
        return true;
    }
    bool save(Common::WriteStream &out) const {
        if (!_count || _count>6 || _selected>=_count) return false;
        out.writeUint32BE(MKTAG('N','P','S','L'));out.writeUint16LE(1);out.writeByte(_active);out.writeByte(_dead);out.writeByte(_count);out.writeByte(_selected);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=10 || in.readUint32BE()!=MKTAG('N','P','S','L') || in.readUint16LE()!=1) return false;
        const byte active=in.readByte(),dead=in.readByte(),count=in.readByte(),selected=in.readByte();
        if (active>1 || dead>1 || !count || count>6 || selected>=count || in.err()) return false;
        _active=active!=0;_dead=dead!=0;_count=count;_selected=selected;return true;
    }
private:
    bool _active,_dead;byte _count,_selected;
};
} }
#endif
