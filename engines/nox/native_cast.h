/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CAST_H
#define NOX_NATIVE_CAST_H
#include "nox/native_book.h"
namespace Nox { namespace Native {
enum CastResult { kCastPrepared, kCastEmptySlot, kCastBattleOnly, kCastWorldOnly, kCastIntelligenceRefusal, kCastManaRefusal, kCastUnsupported };
struct CastPreparation {
    byte spell, manaCost, basePower, levelThreshold;
    CastPreparation() : spell(255), manaCost(0), basePower(0), levelThreshold(0) {}
};
struct NpcCastPreparation {
    CastPreparation cast;byte powerColumn,mode;bool replenished;
    NpcCastPreparation() : powerColumn(0),mode(0),replenished(false) {}
};
// Player/NPC preparation policies. Resource bytes are authored cost,
// power/level-threshold descriptors, never a native instruction/handler-address table.
class CastRules {
public:
    CastRules() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;
        if (!archive.readRange("NA/SRTN.COMBAT.S", 0x4c13, 320, data) || data.size() != 320) return false;
        for (uint i = 0; i < 64; ++i) {
            _cost[i] = data[i * 4];_threshold[i] = data[256 + i];
            for (uint column=0;column<3;++column) _power[i][column]=data[i*4+1+column];
        }
        Common::Array<byte> overlay;if (!archive.readRange("NA/SRTN.COMBAT.S",0x44b3,0x89f,overlay)) return false;
        for (uint i=0;i<64;++i) {
            const uint address=READ_LE_UINT16(overlay.data()+0x5d7+i*2);
            if (address<0x9c01 || !readText(overlay,address-0x9c01,_names[i])) return false;
        }
        const uint refusalOffsets[]={0,0x272,0x243,0x232,0x255,0x263};
        for (uint i=1;i<6;++i) if (!readText(overlay,refusalOffsets[i],_refusals[i])) return false;
        _ready = true; return true;
    }
    Common::String name(byte spell) const { return _ready && spell<64?_names[spell]:Common::String(); }
    Common::String refusal(CastResult result) const { return _ready && result>0 && result<6?_refusals[result]:Common::String(); }
    CastResult prepare(const SpellBook &book, const CharacterRecord &caster, byte slot, bool inBattle, CastPreparation &output) const {
        if (!_ready || slot >= 10) return kCastUnsupported;
        const byte spell = book.spell(slot);
        if (spell & 128) return kCastEmptySlot;
        if (spell >= 64) return kCastUnsupported;
        if (spell < 49 && !inBattle) return kCastBattleOnly;
        if (spell >= 57 && inBattle) return kCastWorldOnly;
        if (caster.bytes[18] < book.requirement(slot)) return kCastIntelligenceRefusal;
        if (caster.bytes[4] < _cost[spell]) return kCastManaRefusal;
        CastPreparation next; next.spell = spell; next.manaCost = _cost[spell]; next.basePower = _power[spell][0]; next.levelThreshold = _threshold[spell];
        output = next; return kCastPrepared;
    }
    bool prepareNpc(byte group,byte descriptor,byte recordSpell,byte recordMode,byte mapId,bool specialFlag,
            byte &mana,NpcCastPreparation &output) const {
        if (!_ready || (group!=1 && group!=2)) return false;
        NpcCastPreparation next;next.mode=group==1?(recordMode&128?recordMode:byte(1)):descriptor;
        byte spell=group==1?byte(recordSpell&15):descriptor;
        if ((spell==11 || spell==139) && mapId==32 && specialFlag) { next.mode|=128;spell=3; }
        else if (spell==12 && mapId==8) spell=10;
        spell&=63;next.powerColumn=next.mode&128?2:group==2?1:0;
        next.cast.spell=spell;next.cast.manaCost=_cost[spell];next.cast.basePower=_power[spell][next.powerColumn];next.cast.levelThreshold=_threshold[spell];
        if (mana<next.cast.manaCost) { mana=next.cast.manaCost;next.replenished=true; }
        output=next;return true;
    }
private:
    static bool readText(const Common::Array<byte> &data,uint offset,Common::String &out) {
        if (offset>=data.size()) return false;out.clear();
        for (uint i=offset;i<data.size();++i) { const byte c=data[i]&127;if (!c) return true;out+=char(c); }return false;
    }
    bool _ready; byte _cost[64], _power[64][3], _threshold[64];Common::String _names[64],_refusals[6];
};
} }
#endif
