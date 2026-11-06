/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_STEP_VALIDATION_H
#define NOX_NATIVE_ACTOR_STEP_VALIDATION_H
#include "nox/native_actor_step.h"
namespace Nox {
inline bool validateNativeActorStep(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','A','S','T') || input.readUint32LE() != 1 || input.readUint32LE() != 224384) return false;
    Native::ActorStepRules rules; byte last = 255,retained = 77; uint linked = 0;
    for (uint i = 0; i < 224384; ++i) {
        const byte start = input.readByte(),type = input.readByte(); Native::ActorStepProbe probe;
        probe.terrain = input.readByte(); probe.actorTile = input.readByte(); const byte flags = input.readByte();
        probe.restrictedTerrain = (flags & 1) != 0; probe.large = (flags & 2) != 0; probe.unrestricted = (flags & 4) != 0;
        probe.occupied = input.readByte() != 0; probe.objectPresent = input.readByte() != 0; probe.objectTile = input.readByte(); probe.objectState = input.readByte();
        const byte prior = input.readByte(),blocked = input.readByte(),effective = input.readByte();
        if (type != last) { if (!rules.load(archive,type)) return false; last = type; }
        if (start) retained = prior;
        else { if (retained != prior) return false; ++linked; }
        probe.previousTerrain = retained; Native::ActorStepDecision result;
        if (!rules.probe(probe,result) || byte(result.blocked) != blocked || result.effectiveTerrain != effective) {
            error = Common::String::format("Native world actor destination differs at case%u type%u",i,uint(type)); return false;
        }
        retained = result.effectiveTerrain;
    }
    if (linked != 96 || input.err() || input.pos() != input.size()) return false;
    report += "native-actor-step\tcases=224384\tstyles=8\tretained_candidate_continuations=96\tfull_destination_decision=1\tsource_footer_bytes=72\tcomplete_AI_turn=0\tbattle=0\tcpu_execution=0\n"; return true;
}
}
#endif
