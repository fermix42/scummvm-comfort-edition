/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_INTENT_VALIDATION_H
#define NOX_NATIVE_ACTOR_INTENT_VALIDATION_H
#include "nox/native_actor_intent.h"
namespace Nox {
inline bool validateNativeActorIntent(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','A','I','N') || input.readUint32LE() != 1 || input.readUint32LE() != 2) return false;
    uint total = 0,draws = 0;
    for (uint group = 0; group < 2; ++group) {
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(),last = input.readByte(); const uint16 attempts = input.readUint16LE();
        Native::GameplayRandom random; if (!random.importState(seed,tail,last,attempts)) return false;
        const uint count = input.readUint32LE(); if (count != 21504) return false;
        for (uint i = 0; i < count; ++i) {
            Native::WorldActor actor; actor.kind = input.readByte(); actor.state = input.readByte(); actor.capability = input.readByte(); actor.flags = input.readByte(); actor.tile = 246;
            Native::ActorIntentState state; state.movementPhase = input.readByte() != 0; state.hidden = input.readByte() != 0; state.appearanceChanged = input.readByte() != 0; state.pattern = input.readByte();
            if (input.read(state.preferences,4) != 4) return false;
            const byte intent = input.readByte(),capability = input.readByte(),hidden = input.readByte(),changed = input.readByte(),pattern = input.readByte(); byte preferences[4];
            if (input.read(preferences,4) != 4) return false;
            const uint32 afterSeed = input.readUint32LE(); const byte afterTail = input.readByte(),afterLast = input.readByte(); const uint16 afterAttempts = input.readUint16LE();
            Native::ActorIntentPlan plan;
            if (!Native::ActorIntentRules::plan(actor,state,random,plan) || uint(plan.intent) != intent || plan.actor.capability != capability ||
                plan.actor.state != actor.state || plan.actor.flags != actor.flags || plan.actor.kind != actor.kind || plan.state.movementPhase != state.movementPhase ||
                byte(plan.state.hidden) != hidden || byte(plan.state.appearanceChanged) != changed || plan.state.pattern != pattern || memcmp(plan.state.preferences,preferences,4) ||
                random.seed() != afterSeed || random.tail() != afterTail || random.last() != afterLast || random.generationAttempts() != afterAttempts) {
                error = Common::String::format("Native actor intent/RNG differs at group%u case%u",group,i); return false;
            }
            draws += plan.consumedRandom; ++total;
        }
    }
    if (total != 43008 || draws != 8960 || input.err() || input.pos() != input.size()) return false;
    report += "native-actor-intent\tcases=43008\tretained_RNG_groups=2\tactual_RNG_draws=8960\tdefault_inclusive_bounds=1,255\tcapability_shared_state_preferences=1\troute_navigation_commit=0\tcomplete_AI_turn=0\tcpu_execution=0\n"; return true;
}
}
#endif
