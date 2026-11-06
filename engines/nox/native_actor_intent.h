/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_INTENT_H
#define NOX_NATIVE_ACTOR_INTENT_H
#include "nox/native_actors.h"
#include "nox/native_random.h"
namespace Nox { namespace Native {
enum ActorIntent { kActorStay, kActorNeedsChase, kActorNeedsRoute, kActorWander, kActorPattern };
struct ActorIntentState {
    bool movementPhase,hidden,appearanceChanged;
    byte pattern,preferences[4];
};
struct ActorIntentPlan {
    ActorIntent intent;
    WorldActor actor;
    ActorIntentState state;
    bool consumedRandom;
};
// Initial world actor policy before route/target services or navigation.
// Real RNG state is retained even when a random decision ends in staying.
class ActorIntentRules {
public:
    static bool plan(const WorldActor &actor, const ActorIntentState &state, GameplayRandom &random, ActorIntentPlan &output) {
        if (actor.kind != 0 && actor.kind != 4) return false;
        ActorIntentPlan next; next.intent = kActorStay; next.actor = actor; next.state = state; next.consumedRandom = false;
        GameplayRandom after = random; bool wander = false;
        if (!actor.kind) {
            if (!(actor.state & 1) && (actor.state & 2)) { output = next; return true; }
            if (!(actor.state & 16)) next.intent = kActorNeedsChase;
            else {
                byte value; if (!after.next(1,255,value)) return false;
                next.consumedRandom = true;
                wander = actor.state & 1 ? value < 128 : value < 25;
            }
        } else if (!actor.capability || (actor.capability & 128)) {
            if (actor.capability == 255) next.intent = kActorNeedsChase;
            else if (actor.flags && actor.flags != 255) {
                if (actor.state & 4) next.intent = kActorNeedsChase;
                else {
                    byte value; if (!after.next(1,255,value)) return false;
                    next.consumedRandom = true; wander = value >= 128;
                }
            }
        } else if (actor.capability == 3) {
            next.actor.capability = 2;
            if (!state.movementPhase) next.state.hidden = true;
        } else if (actor.capability == 4) {
            next.actor.capability = 0;
            if (actor.flags == 255) { next.state.hidden = false; next.state.appearanceChanged = true; }
        } else next.intent = kActorNeedsRoute;
        if (wander) {
            for (uint i = 0; i < 4; ++i) next.state.preferences[i] = byte(i);
            if (actor.kind == 4 && actor.flags >= 1 && actor.flags <= 4) {
                next.intent = kActorPattern; next.state.pattern = actor.flags;
            } else next.intent = kActorWander;
        }
        random = after; output = next; return true;
    }
};
} }
#endif
