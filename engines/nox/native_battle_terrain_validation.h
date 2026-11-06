/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_TERRAIN_VALIDATION_H
#define NOX_NATIVE_BATTLE_TERRAIN_VALIDATION_H
#include "nox/native_actor_step.h"
namespace Nox {
inline bool validateNativeBattleTerrain(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','T','R') || input.readUint32LE()!=1) return false;
    const uint32 cases=input.readUint32LE();if (!cases || cases>100000 || input.size()!=12+cases*11) return false;
    Native::ActorStepRules rules[9];
    for (uint type=0;type<9;++type) if (type!=1 && !rules[type].load(archive,byte(type))) return false;
    for (uint32 i=0;i<cases;++i) {
        const byte type=input.readByte();Native::ActorStepProbe probe;
        probe.actorTile=input.readByte();probe.terrain=input.readByte();const byte occupied=input.readByte(),restricted=input.readByte();probe.previousTerrain=input.readByte();
        probe.occupied=occupied!=0;probe.restrictedTerrain=restricted!=0;probe.objectPresent=probe.large=probe.unrestricted=false;probe.objectTile=probe.objectState=0;
        byte blocked[4];if (input.read(blocked,4)!=4) return false;const byte terrain=input.readByte();Native::ActorStepDecision result;
        if (type>8 || type==1 || occupied>1 || restricted>1 || !rules[type].probe(probe,result,true) || result.effectiveTerrain!=terrain) return false;
        for (uint d=0;d<4;++d) if (blocked[d]!=byte(result.blocked)) {
            report=Common::String::format("tactical terrain case%u parent%u tile%u actor%u restricted%u occupancy%u native%u original%u",i,type,probe.terrain,probe.actorTile,restricted,occupied,result.blocked?1:0,blocked[d]);return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-tactical-terrain\tcases=%u\tparent_families=8\tterrain_values=256\toccupancy_restrictions=1\tcpu_execution=0\twhole_battle=0\n",cases);return true;
}
}
#endif
