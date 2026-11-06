/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_MOTION_H
#define NOX_NATIVE_ACTOR_MOTION_H
#include "nox/native_actor_intent.h"
namespace Nox { namespace Native {
struct ActorMotionChoice { byte direction,blocked[4]; bool consumedRandom; };
// Choose a direction from resolved collision candidates. Source ordering and
// thresholds deliberately preserve the original inclusive RNG behavior.
// Navigation/occupancy derivation and movement commit have separate owners.
class ActorMotionRules {
public:
    static bool choose(const ActorIntentPlan &plan, byte x, byte y, byte goalX, byte goalY,
            const byte *blocked, GameplayRandom &random, ActorMotionChoice &output) {
        if (!blocked || (plan.intent != kActorStay && plan.intent != kActorWander && plan.intent != kActorPattern)) return false;
        ActorMotionChoice next; next.direction = 4; next.consumedRandom = false; memcpy(next.blocked,blocked,4);
        if (plan.intent == kActorStay) { output = next; return true; }
        if (plan.intent == kActorPattern) {
            const byte limit = plan.state.pattern;
            const uint distances[4] = {goalY >= y ? uint(goalY - y) : 0, y >= goalY ? uint(y - goalY) : 0,
                x >= goalX ? uint(x - goalX) : 0, goalX >= x ? uint(goalX - x) : 0};
            for (uint i = 0; i < 4; ++i) if (distances[i] >= limit) next.blocked[i] = 1;
        }
        byte candidates[5]; uint count = 0;
        for (uint i = 0; i < 4; ++i) {
            const byte direction = plan.state.preferences[i]; if (direction >= 4) return false;
            if (!next.blocked[direction]) candidates[count++] = direction;
        }
        candidates[count++] = 4; uint selected = 0; GameplayRandom after = random;
        if (count > 1) {
            byte roll; if (!after.next(1,255,roll)) return false; next.consumedRandom = true;
            if (count == 2) selected = roll >= 128 ? 1 : 0;
            else if (count == 3) selected = roll < 85 ? 0 : roll >= 170 ? 1 : 2;
            else if (count == 4) selected = roll < 63 ? 0 : roll >= 191 ? 3 : roll < 116 ? 1 : 2;
            else selected = roll < 51 ? 0 : roll >= 204 ? 3 : roll >= 153 ? 2 : roll < 102 ? 1 : 4;
        }
        next.direction = candidates[selected]; random = after; output = next; return true;
    }
};
} }
#endif
