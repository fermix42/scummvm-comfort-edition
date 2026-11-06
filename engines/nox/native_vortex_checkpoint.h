/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VORTEX_CHECKPOINT_H
#define NOX_NATIVE_VORTEX_CHECKPOINT_H
#include "nox/native_vortex_cast.h"
namespace Nox { namespace Native {
// Partial development checkpoint for a logical Vortex transaction, not a
// complete battle/player save. AI, initiative, turn/presentation/audio/session
// state and world handoff must be owned before general save acceptance.
struct VortexCheckpoint {
    BattleRecords battle; SpellBook book;
    byte casterCell, category, protection, direction;
    VortexCheckpoint() : casterCell(0), category(0), protection(0), direction(0) {}
    bool valid() const {
        VortexVictim caster;
        byte bytes[20];
        return direction < 4 && protection < 2 && book.exportState(bytes,20) &&
            battle.actor(casterCell,caster) && caster.kind == 3;
    }
    bool save(Common::WriteStream &out) const {
        byte bytes[20]; if (!valid() || !book.exportState(bytes,20)) return false;
        out.writeUint32BE(MKTAG('N','V','C','B')); out.writeUint16LE(1);
        out.write(bytes,20); out.write(battle.stats,2048); out.write(battle.compact,256);
        out.write(battle.extended,256); out.write(battle.indices,187); out.write(battle.kinds,187);
        out.writeUint16LE(battle.credit); out.writeByte(casterCell); out.writeByte(category);
        out.writeByte(protection); out.writeByte(direction); return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size() - in.pos() != 2966 || in.readUint32BE() != MKTAG('N','V','C','B') || in.readUint16LE() != 1) return false;
        VortexCheckpoint next; byte bytes[20];
        if (in.read(bytes,20) != 20 || !next.book.importState(bytes,20) || in.read(next.battle.stats,2048) != 2048 ||
            in.read(next.battle.compact,256) != 256 || in.read(next.battle.extended,256) != 256 ||
            in.read(next.battle.indices,187) != 187 || in.read(next.battle.kinds,187) != 187) return false;
        next.battle.credit = in.readUint16LE(); next.casterCell = in.readByte(); next.category = in.readByte();
        next.protection = in.readByte(); next.direction = in.readByte();
        if (in.err() || !next.valid()) return false;
        *this = next; return true;
    }
};
} }
#endif
