/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ROUTE_RECOVERY_H
#define NOX_NATIVE_NPC_ROUTE_RECOVERY_H
#include "nox/native_npc_route_follow.h"
#include "nox/native_message_strip.h"
#include "nox/native_random.h"
#include "common/memstream.h"
#include "common/util.h"
namespace Nox { namespace Native {
struct NpcRouteRecoveryState {
    NpcRouteMotionState motion; bool appearanceChanged; byte preferences[4],screenCell;
    NpcRouteRecoveryState() : appearanceChanged(false),screenCell(93) { for (uint i = 0; i < 4; ++i) preferences[i] = byte(i); }
    void advanceScreenCell(byte direction) {
        if (direction == 0 && motion.relativeY >= 123) screenCell = byte(screenCell - 17);
        else if (direction == 1 && motion.relativeY < 134) screenCell = byte(screenCell + 17);
        else if (direction == 2 && motion.relativeX < 137) ++screenCell;
        else if (direction == 3 && motion.relativeX >= 120) --screenCell;
    }
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','R','R','S')); out.writeUint16LE(1); if (!motion.save(out)) return false;
        out.writeByte(appearanceChanged); out.write(preferences,4); out.writeByte(screenCell); return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size() - in.pos() != 22 || in.readUint32BE() != MKTAG('N','R','R','S') || in.readUint16LE() != 1) return false;
        byte data[10]; if (in.read(data,10) != 10) return false; Common::MemoryReadStream motionInput(data,10); NpcRouteRecoveryState next;
        if (!next.motion.load(motionInput)) return false; const byte changed = in.readByte(); if (changed > 1 || in.read(next.preferences,4) != 4) return false;
        next.appearanceChanged = changed != 0; next.screenCell = in.readByte();
        for (uint i = 0; i < 4; ++i) if (next.preferences[i] > 3) return false;
        if (in.err()) return false; *this = next; return true;
    }
};
struct NpcRouteRecoveryInputs { byte partyX,partyY; bool previewEmpty,suppressApproach; };
enum NpcRouteRecoveryResult { kNpcRouteRecoveryAdvanced,kNpcRouteRecoveryMissing,kNpcRouteRecoveryInvalid };
class NpcRouteRecovery {
public:
    // Resolved collision/preview inputs are supplied by the turn's spatial
    // owner. This owner retains route/counter/RNG and real message rendering.
    static NpcRouteRecoveryResult advance(World &world, NpcScheduleState &schedule, NpcRouteStore &store, uint actorSlot,
            const byte *blocked, const NpcRouteRecoveryInputs &inputs, NpcRouteRecoveryState &state, GameplayRandom &random,
            const FixedMessageCatalog &catalog, TextGlyphSurface &surface, byte &direction, bool &message,ActorMovePhase *movement=nullptr) {
        if (!blocked) return kNpcRouteRecoveryInvalid;
        const NpcRouteFollowResult simple = NpcRouteFollower::advance(world,schedule,store,actorSlot,blocked,state.motion,direction,movement);
        if (simple == kNpcRouteFollowAdvanced) { state.advanceScreenCell(direction); message = false; return kNpcRouteRecoveryAdvanced; }
        if (simple == kNpcRouteFollowMissing) {
            if (movement && !movement->apply(world,actorSlot,direction)) return kNpcRouteRecoveryInvalid;
            message = false; return kNpcRouteRecoveryMissing;
        }
        if (simple != kNpcRouteFollowNeedsRecovery || !world.ready() || !schedule.ready() || actorSlot >= 256 || (actorSlot & 7)) return kNpcRouteRecoveryInvalid;
        const byte *original = world.currentTables()->data() + 1280 + actorSlot;
        if (original[6] != 1 && original[6] != 2) return kNpcRouteRecoveryInvalid;
        int requestSlot = -1;
        for (uint i = 0; i < 14; ++i) {
            const byte *r = schedule._requests + i * 4;
            if (!(r[2] & 128) && r[0] == original[4] && r[1] == actorSlot) { requestSlot = int(i); break; }
        }
        if (requestSlot < 0) { direction = 4; message = false; return kNpcRouteRecoveryMissing; }
        const uint page = schedule._requests[requestSlot * 4 + 3];
        if (page >= 14 || !(store._readyMask & (1U << page))) return kNpcRouteRecoveryInvalid;
        World nextWorld(world); NpcScheduleState nextSchedule = schedule; NpcRouteStore nextStore = store; NpcRouteRecoveryState next = state;
        GameplayRandom after = random; TextGlyphSurface nextSurface = surface; bool emitted = false;
        byte *actor = nextWorld._states[nextWorld._current].tables.data() + 1280 + actorSlot;
        byte *payload = nextStore._data + page * 256; uint cursor = payload[0];
        if (cursor < 2 || cursor > 254 || (cursor & 1)) return kNpcRouteRecoveryInvalid;
        byte targetX = payload[cursor],targetY = payload[cursor + 1],selected = 4; bool recover = false,finished = false,arrived = false;
        const auto atTarget = [&]() { return actor[0] == targetX && actor[1] == targetY; };
        const auto toward = [&]() -> byte { return actor[0] < targetX ? 2 : actor[0] > targetX ? 3 : actor[1] > targetY ? 0 : 1; };
        const auto skipPoint = [&]() { if (cursor >= 3) cursor -= 2; targetX = payload[cursor]; targetY = payload[cursor + 1]; actor[6] = 2; };
        if (actor[6] == 2 && atTarget()) {
            if (cursor >= 3) cursor -= 2; targetX = payload[cursor]; targetY = payload[cursor + 1]; actor[6] = 1;
            if (atTarget()) { finished = true; arrived = true; }
        }
        if (!finished && actor[6] == 1) {
            if (atTarget() && cursor == 2) finished = true;
            else {
                selected = toward();
                if (blocked[selected] == 0 || blocked[selected] == 2) { if (cursor >= 3) cursor -= 2; }
                else { skipPoint(); recover = true; }
            }
        } else if (!finished) {
            const byte desired = toward();
            const bool edge = (desired == 0 && next.motion.relativeY < 124) || (desired == 1 && next.motion.relativeY >= 133) ||
                (desired == 2 && next.motion.relativeX >= 136) || (desired == 3 && next.motion.relativeX < 121);
            const bool partyAtPoint = inputs.partyX == targetX && inputs.partyY == targetY;
            if ((partyAtPoint || (!edge && !inputs.previewEmpty)) && cursor != 2) skipPoint();
            recover = true;
        }
        if (finished) {
            cursor = 0; selected = 4; actor[6] = 4; nextSchedule._requests[requestSlot * 4 + 2] = 255;
            if (!next.motion.movementPhase) next.motion.hidden = true;
            if (arrived) { actor[6] = 0; if (actor[7] == 255) { next.motion.hidden = false; next.appearanceChanged = true; } }
        } else if (recover) {
            const int dx = int(targetX) - actor[0],dy = int(targetY) - actor[1]; byte preferred[2],other[2];
            if (!dx) { preferred[0] = preferred[1] = dy >= 0 ? 1 : 0; other[0] = 2; other[1] = 3; }
            else if (!dy) { preferred[0] = preferred[1] = dx > 0 ? 2 : 3; other[0] = 0; other[1] = 1; }
            else {
                preferred[0] = dx > 0 ? 2 : 3; preferred[1] = dy > 0 ? 1 : 0; uint count = 0;
                for (uint i = 0; i < 4; ++i) if (i != preferred[0] && i != preferred[1]) other[count++] = byte(i);
            }
            byte roll; if (!after.next(1,255,roll)) return kNpcRouteRecoveryInvalid;
            if (roll < 128) { SWAP(preferred[0],preferred[1]); SWAP(other[0],other[1]); }
            bool presentationFailed = false;
            const auto approach = [&](byte candidate) -> bool {
                static const byte neighbors[4] = {110,76,92,94};
                if (!inputs.suppressApproach && next.screenCell == neighbors[candidate]) {
                    ++nextSchedule._approachCount;
                    if (nextSchedule._approachCount >= 3) {
                        nextSchedule._approachCount = 0;
                        if (!showFixedMessageStrip(catalog,36,nextSurface)) presentationFailed = true;
                        emitted = true; return true;
                    }
                }
                return false;
            };
            if (!blocked[preferred[0]]) selected = preferred[0];
            else if (approach(preferred[0])) selected = 4;
            else if (!blocked[preferred[1]]) selected = preferred[1];
            else if (preferred[1] != preferred[0] && approach(preferred[1])) selected = 4;
            else {
                if (!after.next(1,255,roll)) return kNpcRouteRecoveryInvalid;
                if (roll < 128) SWAP(other[0],other[1]);
                selected = !blocked[other[0]] ? other[0] : !blocked[other[1]] ? other[1] : 4;
            }
            if (presentationFailed) return kNpcRouteRecoveryInvalid;
            next.preferences[0] = preferred[0]; next.preferences[1] = preferred[1]; next.preferences[2] = other[0]; next.preferences[3] = other[1];
        }
        payload[0] = byte(cursor);
        if (movement && !movement->apply(nextWorld,actorSlot,selected)) return kNpcRouteRecoveryInvalid;
        actor[0] = byte(actor[0] + (selected == 2 ? 1 : selected == 3 ? -1 : 0)); actor[1] = byte(actor[1] + (selected == 1 ? 1 : selected == 0 ? -1 : 0));
        NpcRouteFollower::updateMotion(next.motion,selected); next.advanceScreenCell(selected);
        world.adoptState(nextWorld); schedule = nextSchedule; store = nextStore; state = next; random = after; surface = nextSurface; direction = selected; message = emitted;
        return kNpcRouteRecoveryAdvanced;
    }
};
} }
#endif
