/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ROUTE_FOLLOW_H
#define NOX_NATIVE_NPC_ROUTE_FOLLOW_H
#include "nox/native_npc_route_store.h"
#include "nox/native_actor_move_phase.h"
namespace Nox { namespace Native {
enum NpcRouteFollowResult { kNpcRouteFollowAdvanced, kNpcRouteFollowMissing, kNpcRouteFollowNeedsRecovery, kNpcRouteFollowInvalid };
struct NpcRouteMotionState {
    byte relativeX,relativeY; bool movementPhase,hidden;
    NpcRouteMotionState() : relativeX(128),relativeY(128),movementPhase(false),hidden(false) {}
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','R','M','S')); out.writeUint16LE(1);
        out.writeByte(relativeX); out.writeByte(relativeY); out.writeByte(movementPhase); out.writeByte(hidden); return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size() - in.pos() != 10 || in.readUint32BE() != MKTAG('N','R','M','S') || in.readUint16LE() != 1) return false;
        NpcRouteMotionState next; next.relativeX = in.readByte(); next.relativeY = in.readByte(); const byte phase = in.readByte(),hide = in.readByte();
        if (phase > 1 || hide > 1 || in.err()) return false; next.movementPhase = phase != 0; next.hidden = hide != 0; *this = next; return true;
    }
};
class NpcRouteFollower {
public:
    static void updateMotion(NpcRouteMotionState &motion, byte selected) {
        if (selected >= 4) return;
        motion.relativeX = byte(motion.relativeX + (selected == 2 ? 1 : selected == 3 ? -1 : 0));
        motion.relativeY = byte(motion.relativeY + (selected == 1 ? 1 : selected == 0 ? -1 : 0));
        const bool verticalEntry = ((selected == 0 && motion.relativeY == 133) || (selected == 1 && motion.relativeY == 123)) && motion.relativeX >= 120 && motion.relativeX < 137;
        const bool horizontalEntry = ((selected == 2 && motion.relativeX == 120) || (selected == 3 && motion.relativeX == 136)) && motion.relativeY >= 123 && motion.relativeY < 134;
        if (verticalEntry || horizontalEntry) motion.movementPhase = false;
    }
    // Active-route logical movement only. Recovery mode and derived collision
    // inputs are explicit boundaries; the caller owns interactions and repaint.
    static NpcRouteFollowResult advance(World &world, NpcScheduleState &schedule, NpcRouteStore &store,
            uint actorSlot, const byte *blocked, NpcRouteMotionState &motion, byte &direction,ActorMovePhase *movement=nullptr) {
        if (!world.ready() || !schedule.ready() || actorSlot >= 256 || (actorSlot & 7) || !blocked) return kNpcRouteFollowInvalid;
        const byte *actor = world.currentTables()->data() + 1280 + actorSlot;
        if (actor[6] != 1) return kNpcRouteFollowNeedsRecovery;
        int slot = -1;
        for (uint i = 0; i < 14; ++i) {
            const byte *r = schedule._requests + i * 4;
            if (!(r[2] & 128) && r[0] == actor[4] && r[1] == actorSlot) { slot = int(i); break; }
        }
        if (slot < 0) { direction = 4; return kNpcRouteFollowMissing; }
        const uint page = schedule._requests[slot * 4 + 3];
        if (page >= 14 || !(store._readyMask & (1U << page))) return kNpcRouteFollowInvalid;
        const byte *payload = store._data + page * 256; const uint cursor = payload[0];
        if (cursor < 2 || cursor > 254 || (cursor & 1)) return kNpcRouteFollowInvalid;
        const byte x = payload[cursor],y = payload[cursor + 1];
        const bool finished = actor[0] == x && actor[1] == y && cursor == 2;
        byte selected = finished ? 4 : actor[0] < x ? 2 : actor[0] > x ? 3 : actor[1] > y ? 0 : 1;
        if (!finished && blocked[selected] != 0 && blocked[selected] != 2) return kNpcRouteFollowNeedsRecovery;
        World next(world); byte *record = next._states[next._current].tables.data() + 1280 + actorSlot;
        if (finished) record[6]=4;
        if (movement && !movement->apply(next,actorSlot,selected)) return kNpcRouteFollowInvalid;
        if (finished) {
            record[6] = 4; schedule._requests[slot * 4 + 2] = 255;
            if (!motion.movementPhase) motion.hidden = true;
        } else {
            record[0] = byte(record[0] + (selected == 2 ? 1 : selected == 3 ? -1 : 0));
            record[1] = byte(record[1] + (selected == 1 ? 1 : selected == 0 ? -1 : 0));
        }
        if (!finished) updateMotion(motion,selected);
        store._data[page * 256] = finished ? 0 : cursor >= 3 ? byte(cursor - 2) : byte(cursor);
        world.adoptState(next); direction = selected; return kNpcRouteFollowAdvanced;
    }
};
} }
#endif
