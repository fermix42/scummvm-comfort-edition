/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_PURSUIT_H
#define NOX_NATIVE_ACTOR_PURSUIT_H
#include "nox/native_actor_motion.h"
#include "common/util.h"
namespace Nox { namespace Native {
enum ActorPursuitPolicy { kPursuitSingleAxisThenWander, kPursuitPreferredThenOtherAxes };
struct ActorPursuitChoice {
    byte direction,preferences[4];
    ActorPursuitChoice() : direction(4) { for (uint i = 0; i < 4; ++i) preferences[i] = byte(i); }
};
class ActorPursuitRules {
public:
    // Direction policy over resolved collision inputs. Ordinary caller policy,
    // contact/battle and NPC approach callbacks are separate integration work.
    static bool choose(byte relativeX, byte relativeY, ActorPursuitPolicy policy, const byte *blocked,
            GameplayRandom &random, ActorPursuitChoice &output) {
        if (!blocked || (policy != kPursuitSingleAxisThenWander && policy != kPursuitPreferredThenOtherAxes)) return false;
        ActorPursuitChoice next; byte *p = next.preferences;
        if (relativeX != 128 && relativeY != 128) {
            p[0] = relativeX < 128 ? 2 : 3; p[1] = relativeY < 128 ? 1 : 0;
            p[2] = relativeY < 128 ? 0 : 1; p[3] = relativeX < 128 ? 3 : 2;
            if (relativeY == 123 || relativeY == 133) p[0] = p[1];
            else if (relativeX == 120 || relativeX == 136) p[1] = p[0];
        } else if (relativeX != 128) {
            p[0] = p[1] = relativeX < 128 ? 2 : 3; p[2] = 0; p[3] = 1;
        } else {
            p[0] = p[1] = relativeY > 128 ? 0 : 1; p[2] = 2; p[3] = 3;
        }
        GameplayRandom after = random; byte roll; if (!after.next(1,255,roll)) return false;
        if (policy == kPursuitSingleAxisThenWander) {
            next.direction = p[roll < 128 ? 0 : 1];
            if (blocked[next.direction]) {
                ActorIntentPlan plan; plan.intent = kActorWander; for (uint i = 0; i < 4; ++i) plan.state.preferences[i] = p[i];
                ActorMotionChoice motion; if (!ActorMotionRules::choose(plan,0,0,0,0,blocked,after,motion)) return false; next.direction = motion.direction;
            }
        } else {
            if (roll < 128) { SWAP(p[0],p[1]); SWAP(p[2],p[3]); }
            if (!blocked[p[0]]) next.direction = p[0];
            else if (!blocked[p[1]]) next.direction = p[1];
            else {
                if (!after.next(1,255,roll)) return false;
                if (roll < 128) SWAP(p[2],p[3]);
                next.direction = !blocked[p[2]] ? p[2] : !blocked[p[3]] ? p[3] : 4;
            }
        }
        random = after; output = next; return true;
    }
};
} }
#endif
