/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TEMPLE_SERVICE_H
#define NOX_NATIVE_TEMPLE_SERVICE_H
#include "nox/native_new_game.h"
#include "nox/native_battle_restoration.h"
namespace Nox { namespace Native {

enum TempleResult { kTempleNone,kTempleHealed,kTempleResurrected,kTempleNotDead,kTemplePoor,kTempleCancelled };
// Complete source service transaction: authored offer and average-level prices,
// payment BEFORE target selection/effect, living-condition preservation/refusal,
// death recovery and one exit acknowledgement. No machine execution context.
class TempleService {
public:
    TempleService() : _phase(0),_map(0),_slot(0),_heal(0),_revive(0),_offer(0),_result(kTempleNone) {}
    byte phase() const { return _phase; }
    uint16 healingPrice() const { return _heal; }
    uint16 resurrectionPrice() const { return _revive; }
    uint16 offer() const { return _offer; }
    TempleResult result() const { return _result; }
    bool begin(HdvArchive &archive,const NewGame &game,byte slot) {
        if (_phase || !game.ready() || !game.party.memberCount() || (slot&7)) return false;
        Common::Array<byte> raw;if (!archive.read("NA/SRTN.NPC.TALK",raw) || raw.size()!=32831) return false;
        const uint start=0x71d2,size=0x494;const byte *data=raw.data()+start;
        uint cursor=0x379;bool found=false;uint16 multiplier=0;
        while (cursor<size && data[cursor]!=255) {
            const uint length=data[cursor];if (length<7 || length>size-cursor) return false;
            if (data[cursor+1]==game.world.mapId() && data[cursor+2]==slot) {
                multiplier=READ_LE_UINT16(data+cursor+3);found=true;break;
            }
            cursor+=length;
        }
        if (!found) return false;
        const uint count=game.party.memberCount();byte sum=0;
        for (uint i=0;i<count;++i) sum=byte(sum+game.party.member(i)->bytes[1]);
        const byte level=sum/count;if (level>10) return false;
        if (level==1) multiplier=1;
        TempleService next;next._phase=1;next._map=game.world.mapId();next._slot=slot;
        next._heal=uint16(uint16(multiplier*level)*count);next._revive=READ_LE_UINT16(data+0x35a+uint(level)*2);
        next._offer=uint16(start+cursor+7);*this=next;return true;
    }
    bool choose(byte choice,NewGame &game) {
        if (_phase!=1 || !game.ready() || game.world.mapId()!=_map || choice>2) return false;
        if (!choice) {_phase=3;_result=kTempleCancelled;return true;}
        const uint16 price=choice==1?_heal:_revive;
        if (game.gold<price) {_phase=3;_result=kTemplePoor;return true;}
        NewGame next(game);next.gold-=price;
        if (choice==2) {_phase=2;game.adoptState(next);return true;}
        BattleRecords records;for (uint i=0;i<6;++i) memcpy(records.stats+i*128,next.party.member(i)->bytes,128);
        RestorationOutcome outcome;BattleRestoration::divine(records,next.party.memberCount(),0,outcome);
        if (!next.party.applyBattleMembers(records.stats,768,next.party.memberCount())) return false;
        game.adoptState(next);_phase=3;_result=kTempleHealed;return true;
    }
    bool target(byte member,NewGame &game) {
        if (_phase!=2 || !game.ready() || game.world.mapId()!=_map || (member!=255 && member>=game.party.memberCount())) return false;
        if (member==255) {_phase=3;_result=kTempleCancelled;return true;}
        BattleRecords records;for (uint i=0;i<6;++i) memcpy(records.stats+i*128,game.party.member(i)->bytes,128);
        RestorationOutcome outcome;
        if (!BattleRestoration::resurrect(records,member,1,false,outcome) ||
                !game.party.applyBattleMembers(records.stats,768,game.party.memberCount())) return false;
        _phase=3;_result=outcome.effect==kRestorationNotDead?kTempleNotDead:kTempleResurrected;return true;
    }
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','T','H','S'));out.writeUint16LE(1);out.writeByte(_phase);
        out.writeByte(_map);out.writeByte(_slot);out.writeUint16LE(_heal);out.writeUint16LE(_revive);out.writeUint16LE(_offer);out.writeByte(byte(_result));return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=16 || in.readUint32BE()!=MKTAG('N','T','H','S') || in.readUint16LE()!=1) return false;
        TempleService next;next._phase=in.readByte();next._map=in.readByte();next._slot=in.readByte();
        next._heal=in.readUint16LE();next._revive=in.readUint16LE();next._offer=in.readUint16LE();next._result=TempleResult(in.readByte());
        if (next._phase>3 || (next._slot&7) || next._result>kTempleCancelled || in.err()) return false;
        *this=next;return true;
    }
private:
    byte _phase,_map,_slot;uint16 _heal,_revive,_offer;TempleResult _result;
};
} }
#endif
