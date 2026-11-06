/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_CONTACT_H
#define NOX_NATIVE_NPC_CONTACT_H
#include "nox/native_graphics.h"
#include "nox/native_actors.h"
namespace Nox { namespace Native {
struct NpcContactState {
    // Zero queues a conversation; positive values allow automatic requests;
    // the high bit disables them. The command/encounter owners set availability.
    byte availability, slot, cell;
    NpcContactState() : availability(1),slot(255),cell(255) {}
    bool valid() const { return (slot==255 || !(slot&7)) && (cell==255 || cell<187); }
    bool save(Common::WriteStream &out) const {
        if (!valid()) return false;
        out.writeUint32BE(MKTAG('N','N','C','T')); out.writeUint16LE(1); out.writeByte(availability); out.writeByte(slot); out.writeByte(cell); return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=9 || in.readUint32BE()!=MKTAG('N','N','C','T') || in.readUint16LE()!=1) return false;
        NpcContactState next; next.availability=in.readByte(); next.slot=in.readByte(); next.cell=in.readByte();
        if (in.err() || !next.valid()) return false; *this=next; return true;
    }
};
// Automatic NPC request only. Opening the queued conversation and player UI
// belong to the command owner; no script/CPU entry point is retained here.
class NpcContactRules {
public:
    NpcContactRules() : _ready(false),_avoidA(0),_avoidB(0) {}
    bool load(HdvArchive &archive,byte type) {
        Common::Array<byte> game,footer; MapAssetRange range;
        if (!archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) || !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        _avoidA=footer[58]; _avoidB=footer[59]; _ready=true; return true;
    }
    bool consider(const WorldActor &actor,byte slot,byte cell,byte terrain,NpcContactState &state) const {
        if (!_ready || actor.kind!=4 || actor.state==255 || (slot&7) || cell>=187 || !state.valid()) return false;
        if ((actor.state&8) && !(state.availability&128) && actor.flags!=255 && terrain!=_avoidA && terrain!=_avoidB) {
            state.availability=0; state.slot=slot; state.cell=cell;
        }
        return true;
    }
private:
    bool _ready; byte _avoidA,_avoidB;
};
} }
#endif
