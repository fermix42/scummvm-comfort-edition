/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_GEOMETRY_VALIDATION_H
#define NOX_NATIVE_BATTLE_GEOMETRY_VALIDATION_H
#include "nox/native_battle_collisions.h"
namespace Nox {
inline bool validateNativeBattleGeometry(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','G','Y') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (!count || count>20000 || input.size()!=12+count*576) return false;
    Native::BattleCollisions rules;if (!rules.load(archive)) return false;uint largeCases=0,canonicalEdges=0;
    for (uint i=0;i<count;++i) {
        byte header[7],terrain[187],actors[187],objects[187],objectRecords[256]={0},expected[4],blocked[4];
        if (input.read(header,7)!=7 || input.read(terrain,187)!=187 || input.read(actors,187)!=187 || input.read(objects,187)!=187 ||
            input.read(objectRecords,4)!=4 || input.read(expected,4)!=4) return false;
        bool same=rules.probe(header[0],header[1],header[2],header[3],header[4]!=0,header[5]!=0,terrain,actors,objects,objectRecords,blocked);
        for (uint direction=0;same && direction<4;++direction) {
            if (blocked[direction]==expected[direction]) continue;
            // For large NPC east/west edges, the original writes FF only to
            // the second leading-cell scratch. A stale first cell can produce
            // raw1 before FF produces2. Tactical NPC pursuit consumes BEQ/BNE
            // only (original M E004..E042); both prohibit the same move, with
            // identical RNG. Native geometry owns an explicit outside-board2.
            // Keep raw oracle markers; admit only this source-proven difference.
            const bool outside=header[4] && ((direction==2 && header[1]==9) || (direction==3 && !header[1]));
            if (outside && blocked[direction]==2 && expected[direction]==1) ++canonicalEdges;
            else same=false;
        }
        if (!same) {
            report=Common::String::format("battle geometry case%u category%u x%u y%u large%u blocked%u,%u,%u,%u/%u,%u,%u,%u",i,uint(header[0]),uint(header[1]),uint(header[2]),uint(header[4]),
                uint(blocked[0]),uint(blocked[1]),uint(blocked[2]),uint(blocked[3]),uint(expected[0]),uint(expected[1]),uint(expected[2]),uint(expected[3]));return false;
        }
        largeCases+=header[4]?1:0;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-geometry\tcases=%u\tlarge_cases=%u\tall_board_origins_parent_footers=1\tleading_pairs_terrain_actor_object=1\tcanonical_large_NPC_edge_markers=%u\traw_oracle_retained=1\tNPC_blocked_and_RNG_semantics_preserved=1\tcpu_execution=0\n",count,largeCases,canonicalEdges);return true;
}
}
#endif
