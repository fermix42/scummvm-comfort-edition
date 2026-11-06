/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ACTION_DISPATCH_H
#define NOX_NATIVE_NPC_ACTION_DISPATCH_H
#include "nox/native_npc_route_recovery.h"
#include "nox/native_actor_pursuit.h"
namespace Nox { namespace Native {
struct NpcActionState {
    NpcRouteRecoveryState navigation; byte pattern;
    NpcActionState() : pattern(0) {}
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','N','A','S')); out.writeUint16LE(1);
        if (!navigation.save(out)) return false; out.writeByte(pattern); return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size() - in.pos() != 29 || in.readUint32BE() != MKTAG('N','N','A','S') || in.readUint16LE() != 1) return false;
        byte data[22]; if (in.read(data,22) != 22) return false; Common::MemoryReadStream nav(data,22); NpcActionState next;
        if (!next.navigation.load(nav)) return false; next.pattern = in.readByte(); if (next.pattern > 4 || in.err()) return false; *this = next; return true;
    }
};
enum NpcActionResult { kNpcActionAdvanced,kNpcActionWaitingForRoute,kNpcActionAbsent,kNpcActionInvalid };
// Complete NPC logical action dispatch from resolved spatial phase inputs.
// Contact/battle, actor-loop preparation and ordered repaint remain turn phases.
class NpcActionDispatcher {
public:
    static NpcActionResult advance(World &world, NpcScheduleState &schedule, NpcRouteStore &store, uint slot,
            const byte *blocked, const NpcRouteRecoveryInputs &inputs, NpcActionState &state, GameplayRandom &random,
            const FixedMessageCatalog &catalog, TextGlyphSurface &surface, byte &direction, bool &message,ActorMovePhase *movement=nullptr) {
        if (!world.ready() || !schedule.ready() || !blocked || slot >= 256 || (slot & 7)) return kNpcActionInvalid;
        World nextWorld(world); WorldActor actor;
        byte *owned = nextWorld._states[nextWorld._current].tables.data();
        const ActorAdmission admission = admitWorldActor(owned,1792,nextWorld.questFields(),256,1,slot,false,actor);
        if (admission == kActorAbsent) { direction = 4; message = false; return kNpcActionAbsent; }
        if (admission != kActorPresent) return kNpcActionInvalid;
        NpcActionState next = state; NpcScheduleState nextSchedule = schedule; NpcRouteStore nextStore = store;
        GameplayRandom after = random; TextGlyphSurface nextSurface = surface;
        ActorIntentState shared; shared.movementPhase = next.navigation.motion.movementPhase; shared.hidden = next.navigation.motion.hidden;
        shared.appearanceChanged = next.navigation.appearanceChanged; shared.pattern = next.pattern; memcpy(shared.preferences,next.navigation.preferences,4);
        ActorIntentPlan plan; if (!ActorIntentRules::plan(actor,shared,after,plan)) return kNpcActionInvalid;
        next.navigation.motion.movementPhase = plan.state.movementPhase; next.navigation.motion.hidden = plan.state.hidden;
        next.navigation.appearanceChanged = plan.state.appearanceChanged; next.pattern = plan.state.pattern; memcpy(next.navigation.preferences,plan.state.preferences,4);
        byte selected = 4; bool emitted = false; NpcActionResult result = kNpcActionAdvanced;
        if (plan.intent == kActorNeedsRoute) {
            const NpcRouteRecoveryResult routed = NpcRouteRecovery::advance(nextWorld,nextSchedule,nextStore,slot,blocked,inputs,next.navigation,after,catalog,nextSurface,selected,emitted,movement);
            if (routed == kNpcRouteRecoveryInvalid) return kNpcActionInvalid;
            if (routed == kNpcRouteRecoveryMissing) result = kNpcActionWaitingForRoute;
        } else {
            if (plan.intent == kActorNeedsChase) {
                // NPC bit flags are cleared by the original actor preparation;
                // compact actors derive this policy from state bit0 instead.
                ActorPursuitChoice pursuit;
                if (!ActorPursuitRules::choose(next.navigation.motion.relativeX,next.navigation.motion.relativeY,kPursuitSingleAxisThenWander,blocked,after,pursuit)) return kNpcActionInvalid;
                selected = pursuit.direction; memcpy(next.navigation.preferences,pursuit.preferences,4);
            } else {
                const byte route = owned[1280 + slot + 4];
                if (plan.intent == kActorPattern && route >= 160) return kNpcActionInvalid;
                const byte goalX = route < 160 ? schedule.goalX()[route] : 0,goalY = route < 160 ? schedule.goalY()[route] : 0;
                ActorMotionChoice motion;
                if (!ActorMotionRules::choose(plan,actor.x,actor.y,goalX,goalY,blocked,after,motion)) return kNpcActionInvalid;
                selected = motion.direction;
            }
            byte *record = owned + 1280 + slot;
            record[3] = plan.actor.state; record[6] = plan.actor.capability;
            if (movement && !movement->apply(nextWorld,slot,selected)) return kNpcActionInvalid;
            record[0] = byte(actor.x + (selected == 2 ? 1 : selected == 3 ? -1 : 0)); record[1] = byte(actor.y + (selected == 1 ? 1 : selected == 0 ? -1 : 0));
            record[3] = plan.actor.state; record[6] = plan.actor.capability;
            NpcRouteFollower::updateMotion(next.navigation.motion,selected); next.navigation.advanceScreenCell(selected);
        }
        world.adoptState(nextWorld); schedule = nextSchedule; store = nextStore; state = next; random = after; surface = nextSurface; direction = selected; message = emitted; return result;
    }
};
} }
#endif
