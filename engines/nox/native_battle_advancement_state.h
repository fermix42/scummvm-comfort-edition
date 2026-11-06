/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_ADVANCEMENT_STATE_H
#define NOX_NATIVE_BATTLE_ADVANCEMENT_STATE_H
#include "common/memstream.h"
namespace Nox { namespace Native {
// An uncommitted character proposal. Earned XP remains in the battle records;
// rejecting a proposal restores its original ability/level/HP fields.
struct BattleAdvancementState {
    bool started,finished;byte phase,cursor,ability,remaining,digits,value,points[3],proposal[128];
    BattleAdvancementState() : started(false),finished(false),phase(0),cursor(0),ability(0),remaining(0),digits(0),value(0) {
        memset(points,0,3);memset(proposal,0,128);
    }
    bool save(Common::WriteStream &out) const {
        if (!valid()) return false;
        out.writeUint32BE(MKTAG('N','A','D','V'));out.writeUint16LE(1);
        out.writeByte((started?1:0)|(finished?2:0));out.writeByte(phase);out.writeByte(cursor);out.writeByte(ability);
        out.writeByte(remaining);out.writeByte(digits);out.writeByte(value);out.write(points,3);out.write(proposal,128);
        return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=144 || in.readUint32BE()!=MKTAG('N','A','D','V') || in.readUint16LE()!=1) return false;
        BattleAdvancementState next;const byte flags=in.readByte();if (flags>3 || flags==2) return false;
        next.started=flags&1;next.finished=flags&2;next.phase=in.readByte();next.cursor=in.readByte();next.ability=in.readByte();
        next.remaining=in.readByte();next.digits=in.readByte();next.value=in.readByte();
        if (in.read(next.points,3)!=3 || in.read(next.proposal,128)!=128 || !next.valid() || in.err()) return false;
        *this=next;return true;
    }
    bool valid() const {
        if (phase>3 || cursor>6 || ability>3 || remaining>99 || digits>2 || value>99 || (!started && (finished || phase)) || (finished && phase)) return false;
        for (uint i=0;i<3;++i) if (points[i]>99) return false;
        return true;
    }
};
} }
#endif
