/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_CAST_H
#define NOX_NATIVE_WORLD_CAST_H
#include "nox/native_cast.h"
#include "nox/native_new_game.h"
#include "nox/native_party_selection.h"
#include "nox/native_battle_restoration.h"
namespace Nox { namespace Native {
enum WorldCastResult {kWorldCastHandled,kWorldCastClosed,kWorldCastTravel,kWorldCastUnsupported};
// Whole world C workflow: slot -> living caster -> source refusal/effect or
// party/orb selection -> retained result or map transition. No world turn is
// spent by the original9443/7128 command return,including cancellation.
class WorldCast {
public:
    WorldCast() : _phase(0),_type(0),_map(0),_slot(255),_caster(255),_spell(255),_cost(0) {}
    bool active() const {return _phase!=0;}
    byte phase() const {return _phase;}
    bool matches(const World &world) const {return !active() || (world.ready() && world.mapType()==_type && world.mapId()==_map);}
    bool begin(HdvArchive &archive,const NewGame &game,HgrFrame &frame) {
        if (active() || !game.ready()) return false;
        WorldCast next;next._phase=1;next._type=game.world.mapType();next._map=game.world.mapId();next._underlay=frame;
        if (!next.paint(archive,game,frame)) return false;*this=next;return true;
    }
    WorldCastResult handle(const Common::Event &event,HdvArchive &archive,NewGame &game,bool riding,HgrFrame &frame) {
        if (!active() || !matches(game.world) || event.type!=Common::EVENT_KEYDOWN) return kWorldCastUnsupported;
        const Common::KeyCode key=event.kbd.keycode;
        if (_phase==5 || key==Common::KEYCODE_ESCAPE) {close(frame);return kWorldCastClosed;}
        if (_phase==1) {
            if (event.kbd.ascii<'0' || event.kbd.ascii>'9') return kWorldCastHandled;
            _slot=byte(event.kbd.ascii-'0');_selection=PartySelection();
            if (!_selection.begin(game.party,false)) return kWorldCastUnsupported;_phase=2;
        } else if (_phase==2 || _phase==3) {
            const PartySelectionResult selected=_selection.handle(event,game.party);
            if (selected==kPartySelectionInvalid) return kWorldCastUnsupported;
            if (selected==kPartySelectionCancelled) {close(frame);return kWorldCastClosed;}
            if (selected==kPartySelectionAccepted) {
                if (_phase==3) {
                    if (!restoreParty(game,_selection.selected())) return kWorldCastUnsupported;
                    _phase=5;
                } else {
                    _caster=_selection.selected();CastRules rules;CastPreparation preparation;
                    if (!rules.load(archive)) return kWorldCastUnsupported;
                    const CastResult result=rules.prepare(game.book,*game.party.member(_caster),_slot,false,preparation);
                    if (result!=kCastPrepared) {
                        if (result==kCastUnsupported) return kWorldCastUnsupported;
                        _message=rules.refusal(result);_phase=5;
                    } else {
                        _spell=preparation.spell;_cost=preparation.manaCost;_message=rules.name(_spell);
                        if (_spell>=49 && _spell<=51) {
                            _selection=PartySelection();if (!_selection.begin(game.party,_spell==49)) return kWorldCastUnsupported;_phase=3;
                        } else if (_spell==63) _phase=4;
                        else if (_spell==52) {if (!restoreParty(game,0)) return kWorldCastUnsupported;_phase=5;}
                        else if (_spell==57 || _spell==58) {
                            if (!spendMana(game)) return kWorldCastUnsupported;
                            game.clock.lightTicks=120;game.clock.lightActive=_spell==58?129:1;_phase=5;
                        } else if (_spell==59) {
                            if (!spendMana(game)) return kWorldCastUnsupported;
                            _message=Common::String::format("X-AXIS %u\rY-AXIS %u",game.world.x(),game.world.y());_phase=5;
                        } else if (_spell==60) {
                            if (!spendMana(game)) return kWorldCastUnsupported;
                            // The source origin helper updates the Surface
                            // return before QuickExit reads it on the Surface.
                            _destination=game.world.mapType()==0?NavigationPoint(0,0,game.world.x(),game.world.y()):game.navigation.surfaceReturn();
                            close(frame);return kWorldCastTravel;
                        } else return kWorldCastUnsupported; // Unauthored empty53..56 entries.
                    }
                }
            }
        } else if (_phase==4) {
            if (event.kbd.ascii<'1' || event.kbd.ascii>'9') return kWorldCastHandled;
            const NavigationResult result=game.navigation.choose(byte(event.kbd.ascii-'1'),_destination);
            if (result==kNavigationUnsupported) return kWorldCastUnsupported;
            if (result==kNavigationUnbound) {_message="That orb has not been enchanted.";_phase=5;}
            else if (_destination.map && riding) {_message="You must exit your transport first.";_phase=5;}
            else {const NavigationPoint destination=_destination;close(frame);_destination=destination;return kWorldCastTravel;}
        } else return kWorldCastUnsupported;
        return paint(archive,game,frame)?kWorldCastHandled:kWorldCastUnsupported;
    }
    NavigationPoint destination() const {return _destination;}
    bool save(Common::WriteStream &out) const {
        if (!_phase || _phase>5 || _message.size()>512) return false;
        out.writeUint32BE(MKTAG('N','W','C','S'));out.writeUint16LE(1);out.writeByte(_phase);out.writeByte(_type);out.writeByte(_map);
        out.writeByte(_slot);out.writeByte(_caster);out.writeByte(_spell);out.writeByte(_cost);out.writeUint16LE(uint16(_message.size()));
        out.write(_message.c_str(),_message.size());out.write(_underlay.data(),8192);
        if ((_phase==2 || _phase==3) && !_selection.save(out)) return false;return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.readUint32BE()!=MKTAG('N','W','C','S') || in.readUint16LE()!=1) return false;
        WorldCast next;next._phase=in.readByte();next._type=in.readByte();next._map=in.readByte();next._slot=in.readByte();next._caster=in.readByte();next._spell=in.readByte();next._cost=in.readByte();
        const uint length=in.readUint16LE();const bool selection=next._phase==2 || next._phase==3;
        if (!next._phase || next._phase>5 || length>512 || (next._slot!=255 && next._slot>=10) || (next._caster!=255 && next._caster>=6) ||
            (next._spell!=255 && next._spell>=64) || in.size()-in.pos()!=int64(length)+8192+(selection?10:0)) return false;
        for (uint i=0;i<length;++i) {const byte c=in.readByte();if (c!=13 && (c<32 || c>=127)) return false;next._message+=char(c);}
        byte image[8192];if (in.read(image,8192)!=8192 || !next._underlay.importImage(image,8192)) return false;
        if (selection) {byte bytes[10];if (in.read(bytes,10)!=10) return false;Common::MemoryReadStream part(bytes,10);if (!next._selection.load(part) || !next._selection.active()) return false;}
        if (in.err() || in.pos()!=in.size()) return false;*this=next;return true;
    }
private:
    void close(HgrFrame &frame) {frame=_underlay;_phase=0;}
    bool spendMana(NewGame &game) const {
        if (_caster>=game.party.memberCount() || game.party.member(_caster)->bytes[4]<_cost) return false;
        byte members[768];for (uint i=0;i<6;++i) memcpy(members+i*128,game.party.member(i)->bytes,128);
        members[uint(_caster)*128+4]-=_cost;return game.party.applyBattleMembers(members,768,game.party.memberCount());
    }
    bool restoreParty(NewGame &game,byte target) {
        if (_caster>=game.party.memberCount() || target>=game.party.memberCount() || game.party.member(_caster)->bytes[4]<_cost) return false;
        BattleRecords records;for (uint i=0;i<6;++i) memcpy(records.stats+i*128,game.party.member(i)->bytes,128);
        RestorationOutcome outcome;
        if (_spell==52) {records.stats[uint(_caster)*128+4]-=_cost;BattleRestoration::divine(records,byte(game.party.memberCount()),0,outcome);}
        else {
            if (_spell==49) {if (!BattleRestoration::resurrect(records,target,0,false,outcome)) return false;}
            else if (_spell==50 || _spell==51) {if (records.stats[uint(target)*128]==255) return false;BattleRestoration::heal(records,_caster,target,_spell==51?128:0,outcome);}
            else return false;
            records.stats[uint(_caster)*128+4]-=_cost;
        }
        if (!game.party.applyBattleMembers(records.stats,768,game.party.memberCount())) return false;
        if (outcome.effect==kRestorationNotDead) _message="That character is not dead.";
        return true;
    }
    static bool line(const GlyphFont &font,HgrFrame &frame,uint x,uint y,const Common::String &text) {
        uint column=x,row=y;
        for (uint i=0;i<text.size();++i) {
            const byte c=byte(text[i]);if (c==13 || column>=39) {column=x;++row;if (c==13) continue;}
            if (row>=24 || !frame.drawGlyph(font,c|128,column++,row,0)) return false;
        }
        return true;
    }
    bool paint(HdvArchive &archive,const NewGame &game,HgrFrame &frame) const {
        CastRules rules;GlyphFont font;if (!rules.load(archive) || !font.load(archive,1)) return false;HgrFrame next;
        if (!line(font,next,1,1,"Cast spell")) return false;
        if (_phase==1) {
            for (uint i=0;i<10;++i) if (!line(font,next,1,3+i*2,Common::String::format("%u %s",i,game.book.spell(i)<64?rules.name(game.book.spell(i)).c_str():"--"))) return false;
        } else if (_phase==2 || _phase==3) {
            if (!line(font,next,1,4,_phase==2?"Who casts?":"Who receives the spell?") || !_selection.paint(archive,game.party,next)) return false;
        } else if (_phase==4) {if (!line(font,next,1,4,"Crystal orb 1-9 (Escape cancels)")) return false;}
        else if (_phase==5) {if (!line(font,next,1,4,_message) || !line(font,next,1,22,"Press a key")) return false;}
        frame=next;return true;
    }
    byte _phase,_type,_map,_slot,_caster,_spell,_cost;Common::String _message;HgrFrame _underlay;PartySelection _selection;NavigationPoint _destination;
};
} }
#endif
