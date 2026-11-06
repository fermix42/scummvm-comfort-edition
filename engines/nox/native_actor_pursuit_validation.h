/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_PURSUIT_VALIDATION_H
#define NOX_NATIVE_ACTOR_PURSUIT_VALIDATION_H
#include "nox/native_actor_pursuit.h"
#include "common/memstream.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeActorPursuit(Common::SeekableReadStream &input, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','P','U','R') || input.readUint32LE() != 1 || input.readUint32LE() != 2) return false;
    uint total = 0;
    for (uint stream = 0; stream < (cold ? 1U : 2U); ++stream) {
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(),last = input.readByte(); const uint16 attempts = input.readUint16LE();
        if (input.readUint32LE() != 13122) return false; Native::GameplayRandom random;
        if (cold) {
            Common::InSaveFile *in = files.openForLoading("nox-native-pursuit-rng.bin"); const bool ok = in && random.load(*in); delete in; if (!ok) return false;
        } else if (!random.importState(seed,tail,last,attempts)) return false;
        for (uint operation = 0; operation < 13122; ++operation) {
            const byte x = input.readByte(),y = input.readByte(),policy = input.readByte(); byte blocked[4],geometry[4],preferences[4];
            if (input.read(blocked,4) != 4 || input.read(geometry,4) != 4 || input.read(preferences,4) != 4 || policy > 1) return false;
            if (bool(geometry[0]) != (x > 128) || bool(geometry[1]) != (x < 128) || bool(geometry[2]) != (y > 128) || bool(geometry[3]) != (y < 128)) return false;
            const byte direction = input.readByte(); const uint32 afterSeed = input.readUint32LE(); const byte afterTail = input.readByte(),afterLast = input.readByte(); const uint16 afterAttempts = input.readUint16LE();
            Native::ActorPursuitChoice choice;
            const bool execute = !cold || operation != 0;
            if (execute && !Native::ActorPursuitRules::choose(x,y,Native::ActorPursuitPolicy(policy),blocked,random,choice)) return false;
            if ((execute && (choice.direction != direction || memcmp(choice.preferences,preferences,4))) || random.seed() != afterSeed || random.tail() != afterTail || random.last() != afterLast || random.generationAttempts() != afterAttempts) {
                error = Common::String::format("Native pursuit differs stream%u operation%u relative%u/%u policy%u",stream,operation,uint(x),uint(y),uint(policy)); return false;
            }
            Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES); if (!random.save(state)) return false;
            Common::MemoryReadStream saved(state.getData(),state.size()); if (!random.load(saved)) return false;
            if (!cold && !stream && !operation) {
                Common::OutSaveFile *out = files.openForSaving("nox-native-pursuit-rng.bin",false); if (!out) return false;
                bool ok = random.save(*out); out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false;
            }
            if (execute) ++total;
        }
    }
    if (input.err() || (!cold && (input.pos() != input.size() || total != 26244))) return false;
    report += cold ? "native-actor-pursuit\tphase=cold\tactual_RNG_result_restored=1\tcontinued_choices=13121\tfull_actor_turn=0\tcpu_execution=0\n" :
        "native-actor-pursuit\tphase=warm\tchoices=26244\tgeometry_contexts=81\tcollision_vectors=81\tpolicies=2\tretained_RNG_streams=2\tRNG_roundtrip_between_choices=1\tcontact_battle_approach=0\tfull_actor_turn=0\tcpu_execution=0\n";
    return true;
}
}
#endif
