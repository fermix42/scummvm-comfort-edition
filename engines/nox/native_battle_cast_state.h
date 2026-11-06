/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_CAST_STATE_H
#define NOX_NATIVE_BATTLE_CAST_STATE_H
#include "nox/native_cast.h"
#include "common/memstream.h"
namespace Nox { namespace Native {
enum BattleCastPhase { kBattleCastIdle,kBattleCastSlot,kBattleCastTarget,kBattleCastDirection,kBattleCastParty };
struct BattleCastState {
    byte phase,slot,member;CastPreparation prepared;
    BattleCastState() : phase(0),slot(255),member(0) {}
    bool valid() const { return phase<=4 && member<6 && (phase<2 || (slot<10 && prepared.spell<64)); }
    bool save(Common::WriteStream &out) const {
        if (!valid()) return false;out.writeUint32BE(MKTAG('N','B','C','A'));out.writeUint16LE(1);
        out.writeByte(phase);out.writeByte(slot);out.writeByte(prepared.spell);out.writeByte(prepared.manaCost);
        out.writeByte(prepared.basePower);out.writeByte(prepared.levelThreshold);out.writeByte(member);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=13 || in.readUint32BE()!=MKTAG('N','B','C','A') || in.readUint16LE()!=1) return false;
        BattleCastState next;next.phase=in.readByte();next.slot=in.readByte();next.prepared.spell=in.readByte();next.prepared.manaCost=in.readByte();
        next.prepared.basePower=in.readByte();next.prepared.levelThreshold=in.readByte();next.member=in.readByte();
        if (!next.valid() || in.err()) return false;*this=next;return true;
    }
};
} }
#endif
