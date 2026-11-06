/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ACTOR_ACTIONS_H
#define NOX_NATIVE_NPC_ACTOR_ACTIONS_H
#include "nox/native_actor_motion.h"
#include "nox/native_npc_schedule.h"
namespace Nox { namespace Native {
enum NpcActorActionResult { kNpcActorAdvanced,kNpcActorNeedsNavigation,kNpcActorAbsent,kNpcActorInvalid };
// Logical NPC action from resolved collision candidates. The ordinary turn
// owner must run interaction and ordered repaint phases separately. A route or
// chase request is an explicit unimplemented boundary, never a silent no-op.
class NpcActorActions {
public:
    static NpcActorActionResult advance(World &world, const NpcScheduleState &schedule, uint slot,
            const byte *blocked, ActorIntentState &shared, GameplayRandom &random, byte &direction) {
        if (!world.ready() || !schedule.ready() || !blocked || slot >= 256 || (slot & 7)) return kNpcActorInvalid;
        World nextWorld(world); WorldActor actor;
        byte *owned = nextWorld._states[nextWorld._current].tables.data();
        const ActorAdmission admission = admitWorldActor(owned,1792,nextWorld.questFields(),256,1,slot,false,actor);
        if (admission == kActorAbsent) return kNpcActorAbsent;
        if (admission != kActorPresent) return kNpcActorInvalid;
        GameplayRandom after = random; ActorIntentPlan plan;
        if (!ActorIntentRules::plan(actor,shared,after,plan)) return kNpcActorInvalid;
        if (plan.intent == kActorNeedsChase || plan.intent == kActorNeedsRoute) return kNpcActorNeedsNavigation;
        const byte route = owned[1280 + slot + 4];
        if (plan.intent == kActorPattern && route >= 160) return kNpcActorInvalid;
        const byte goalX = route < 160 ? schedule.goalX()[route] : 0,goalY = route < 160 ? schedule.goalY()[route] : 0;
        ActorMotionChoice motion;
        if (!ActorMotionRules::choose(plan,actor.x,actor.y,goalX,goalY,blocked,after,motion)) return kNpcActorInvalid;
        byte *record = owned + 1280 + slot;
        record[0] = byte(actor.x + (motion.direction == 2 ? 1 : motion.direction == 3 ? -1 : 0));
        record[1] = byte(actor.y + (motion.direction == 1 ? 1 : motion.direction == 0 ? -1 : 0));
        record[3] = plan.actor.state; record[6] = plan.actor.capability;
        world.adoptState(nextWorld); shared = plan.state; random = after; direction = motion.direction; return kNpcActorAdvanced;
    }
};
} }
#endif
