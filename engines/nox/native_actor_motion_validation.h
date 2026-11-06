/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_MOTION_VALIDATION_H
#define NOX_NATIVE_ACTOR_MOTION_VALIDATION_H
#include "nox/native_actor_motion.h"
namespace Nox {
inline bool validateNativeActorMotion(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','A','M','O') || input.readUint32LE() != 1 || input.readUint32LE() != 2) return false;
    uint total = 0,draws = 0,linked = 0;
    for (uint group = 0; group < 2; ++group) {
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(),last = input.readByte(); const uint16 attempts = input.readUint16LE();
        Native::GameplayRandom random; if (!random.importState(seed,tail,last,attempts)) return false;
        if (input.readUint32LE() != 8505) return false;
        for (uint i = 0; i < 8505; ++i) {
            const byte mode = input.readByte(),flags = input.readByte(),x = input.readByte(),y = input.readByte(),goalX = input.readByte(),goalY = input.readByte();
            Native::ActorIntentState state; state.movementPhase = state.hidden = state.appearanceChanged = false; state.pattern = flags;
            byte blocked[4],expectedBlocked[4]; if (input.read(state.preferences,4) != 4 || input.read(blocked,4) != 4) return false;
            const byte intent = input.readByte(),direction = input.readByte(); if (input.read(expectedBlocked,4) != 4) return false;
            const uint32 afterSeed = input.readUint32LE(); const byte afterTail = input.readByte(),afterLast = input.readByte(); const uint16 afterAttempts = input.readUint16LE();
            Native::ActorIntentPlan plan; plan.intent = mode ? Native::kActorPattern : Native::kActorWander; plan.state = state; plan.consumedRandom = false;
            if (mode == 2) {
                Native::WorldActor actor; actor.kind = 4; actor.state = actor.capability = 0; actor.flags = flags;
                if (!Native::ActorIntentRules::plan(actor,state,random,plan)) return false;
                ++linked; draws += plan.consumedRandom;
            }
            Native::ActorMotionChoice result;
            if (uint(plan.intent) != intent || !Native::ActorMotionRules::choose(plan,x,y,goalX,goalY,blocked,random,result) ||
                result.direction != direction || memcmp(result.blocked,expectedBlocked,4) || random.seed() != afterSeed || random.tail() != afterTail ||
                random.last() != afterLast || random.generationAttempts() != afterAttempts) {
                error = Common::String::format("Native actor motion/RNG differs at group%u case%u",group,i); return false;
            }
            draws += result.consumedRandom; ++total;
        }
    }
    if (total != 17010 || draws != 16969 || linked != 7290 || input.err() || input.pos() != input.size()) return false;
    report += "native-actor-motion\tcases=17010\tlinked_intent_to_choice=7290\tactual_RNG_calls=16969\tretained_RNG_groups=2\tpattern_goal_bounds=1\tnavigation_commit=0\tcomplete_AI_turn=0\tcpu_execution=0\n"; return true;
}
}
#endif
