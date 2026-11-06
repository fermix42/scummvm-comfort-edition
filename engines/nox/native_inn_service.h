/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INN_SERVICE_H
#define NOX_NATIVE_INN_SERVICE_H
#include "nox/native_temple_service.h"
#include "nox/native_world_travel.h"
namespace Nox { namespace Native {

enum InnResult { kInnNone,kInnRestPaid,kInnPoor,kInnCancelled };
// Full rest transaction from an authored inn offer. Party storage/pickup/leave
// is a separate remaining branch, never silently mapped onto resting.
class InnService {
public:
    InnService() : _phase(0),_map(0),_slot(0),_price(0),_offer(0),_quote(0),_result(kInnNone) {}
    byte phase() const { return _phase; }
    uint16 price() const { return _price; }
    uint16 offer() const { return _offer; }
    uint16 quote() const { return _quote; }
    InnResult result() const { return _result; }
    const WorldTravelPlan &destination() const { return _room; }
    bool begin(HdvArchive &archive,const NewGame &game,byte slot) {
        if (_phase || !game.ready() || (slot&7)) return false;
        Common::Array<byte> raw;if (!archive.read("NA/SRTN.NPC.TALK",raw) || raw.size()!=32831) return false;
        const uint start=0x2b44,size=0x1683;const byte *data=raw.data()+start;
        for (uint at=0x68a;at<size && data[at]!=255;) {
            if (size-at<12) return false;const uint length=READ_LE_UINT16(data+at);
            if (length<12 || length>size-at) return false;
            if (data[at+2]==game.world.mapId() && data[at+3]==slot) {
                const uint relative=READ_LE_UINT16(data+at+length-4);if (relative>=length) return false;
                InnService next;next._phase=1;next._map=game.world.mapId();next._slot=slot;
                next._price=READ_LE_UINT16(data+at+4);next._offer=uint16(start+at+12);next._quote=uint16(start+at+relative);
                next._room.x=data[at+8];next._room.y=data[at+9];next._room.map=data[at+10];next._room.type=data[at+11];
                *this=next;return true;
            }
            at+=length;
        }
        return false;
    }
    bool welcome(bool accepted) {
        if (_phase!=1) return false;_phase=accepted?2:5;if (!accepted) _result=kInnCancelled;return true;
    }
    bool requestRest() { if (_phase!=2) return false;_phase=3;return true; }
    bool resumeWelcome() {if (_phase!=2) return false;_phase=1;return true;}
    bool closeService() {if (_phase!=2) return false;_phase=5;_result=kInnCancelled;return true;}
    bool confirm(bool accepted,NewGame &game) {
        if (_phase!=3 || !game.ready() || game.world.mapId()!=_map) return false;
        if (!accepted) {_phase=5;_result=kInnCancelled;return true;}
        if (game.gold<_price) {_phase=5;_result=kInnPoor;return true;}
        NewGame next(game);next.gold-=_price;BattleRecords records;
        for (uint i=0;i<6;++i) memcpy(records.stats+i*128,next.party.member(i)->bytes,128);
        RestorationOutcome outcome;BattleRestoration::divine(records,next.party.memberCount(),1,outcome);
        if (!next.party.applyBattleMembers(records.stats,768,next.party.memberCount())) return false;
        game.adoptState(next);_phase=4;_result=kInnRestPaid;return true;
    }
    // L1E01-1E30: increment first, stop at06; do not reset minutes, spend
    // ordinary world turns or invoke absent quest/NPC hourly callbacks.
    static bool sleep(TurnClock &clock,byte &hours) {
        if (clock.hour>=0x24 || (clock.hour&15)>9) return false;
        uint hour=(clock.hour>>4)*10+(clock.hour&15);hours=0;
        do {hour=(hour+1)%24;++hours;} while (hour!=6);
        clock.hour=6;return true;
    }
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','I','N','S'));out.writeUint16LE(1);out.writeByte(_phase);out.writeByte(_map);out.writeByte(_slot);
        out.writeUint16LE(_price);out.writeUint16LE(_offer);out.writeUint16LE(_quote);out.writeByte(byte(_result));
        out.writeByte(_room.type);out.writeByte(_room.map);out.writeByte(_room.x);out.writeByte(_room.y);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=20 || in.readUint32BE()!=MKTAG('N','I','N','S') || in.readUint16LE()!=1) return false;
        InnService next;next._phase=in.readByte();next._map=in.readByte();next._slot=in.readByte();next._price=in.readUint16LE();
        next._offer=in.readUint16LE();next._quote=in.readUint16LE();next._result=InnResult(in.readByte());
        next._room.type=in.readByte();next._room.map=in.readByte();next._room.x=in.readByte();next._room.y=in.readByte();
        if (next._phase>5 || (next._slot&7) || next._room.type>8 || next._room.type==1 || next._result>kInnCancelled || in.err()) return false;
        *this=next;return true;
    }
private:
    byte _phase,_map,_slot;uint16 _price,_offer,_quote;InnResult _result;WorldTravelPlan _room;
};
} }
#endif
