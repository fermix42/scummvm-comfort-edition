/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_ACTOR_PHASE_VALIDATION_H
#define NOX_NATIVE_WORLD_ACTOR_PHASE_VALIDATION_H
#include "nox/native_world_actor_phase.h"
namespace Nox {
inline bool validateNativeWorldActorPhase(Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    if (in.readUint32BE()!=MKTAG('N','W','A','P') || in.readUint32LE()!=1) return false;
    const uint32 count=in.readUint32LE();if (count!=15840) return false;
    const byte bounds[]={104,152,104,152};
    for (uint32 i=0;i<count;++i) {
        byte a[9],b[9];if (in.read(a,9)!=9 || in.read(b,9)!=9) return false;
        Native::WorldActor actor;actor.kind=a[2];actor.state=a[3];actor.capability=a[4];actor.tile=a[8];
        actor.large=actor.kind==4 ? (actor.state&16)!=0:(actor.state&8)!=0;
        Native::WorldActorPhase phase;
        if (!Native::WorldActorPhaseRules::plan(a[0],a[1],actor,a[6],a[5]!=0,a[7],bounds,phase) ||
            phase.column!=b[0] || phase.row!=b[1] || phase.oldOutside!=bool(b[2]) ||
            phase.skipStatic!=bool(b[5]) || phase.face!=bool(b[6]) || byte(phase.route)!=b[8]) {
            error=Common::String::format("World actor entry/exit phase differs at case%u",i);return false;
        }
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report+="native-world-actor-phase\tcases=15840\told_new_viewport_scroll_facing_cache_eligibility=1\tcpu_execution=0\n";return true;
}
}
#endif
