/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_CANNON_H
#define NOX_NATIVE_WORLD_CANNON_H
#include "nox/native_world_projectile.h"
namespace Nox { namespace Native {
enum WorldCannonResult {kWorldCannonRefused,kWorldCannonCancelled,kWorldCannonFired,kWorldCannonUnsupported};
// Whole player world-cannon transaction. Caller owns the actual direction
// input, following world turn and frame. No HP, XP or loot is manufactured.
class WorldCannon {
public:
    static WorldCannonResult fire(byte appearance,byte direction,byte map,bool protectedTargets,bool &mode,bool &alarm,
            const byte *footer,byte *compact,byte *npcs,byte *lookup,byte *kinds,GameplayRandom &random,WorldProjectileOutcome &output) {
        if (appearance!=119 && appearance!=123) return kWorldCannonRefused;
        if (direction==155) return kWorldCannonCancelled;
        if (!footer || !compact || !npcs || !lookup || !kinds) return kWorldCannonUnsupported;
        GameplayRandom next=random;WorldProjectileOutcome result;
        if (!WorldProjectile::player(appearance,direction,mode,lookup,compact,next,result)) return kWorldCannonUnsupported;
        byte actors[256],people[256],indices[187],types[187];
        memcpy(actors,compact,256);memcpy(people,npcs,256);memcpy(indices,lookup,187);memcpy(types,kinds,187);
        if (result.hit) {
            const byte slot=indices[result.endpoint],kind=types[result.endpoint]&7;
            if (slot==255 || (slot&3) || (kind!=0 && kind!=4) || (kind==4 && (slot&7))) return kWorldCannonUnsupported;
            byte *record=(kind==4?people:actors)+slot;
            if (!protectedTargets) {
                // Source18B6 uses bit8 for this removal footprint. Do not
                // silently substitute the world NPC's separate large bit16.
                const bool four=(record[3]&8)!=0;
                if (four && (result.endpoint%17==16 || result.endpoint/17==10)) return kWorldCannonUnsupported;
                record[3]=255;if (kind==4) record[7]=255;
                const uint offsets[]={0,1,17,18};
                for (uint i=0;i<(four?4U:1U);++i) indices[result.endpoint+offsets[i]]=types[result.endpoint+offsets[i]]=255;
            }
            // The alarm follows removal even when protection refused it.
            if (map>=46 && map<50) {
                for (uint i=0;i<256;i+=8) if ((people[i+2]>=footer[64] && people[i+2]<footer[65]) || people[i+2]==footer[66] || people[i+2]==123) people[i+6]=255;
                alarm=true;
            }
        }
        memcpy(compact,actors,256);memcpy(npcs,people,256);memcpy(lookup,indices,187);memcpy(kinds,types,187);
        mode=true;random=next;output=result;return kWorldCannonFired;
    }
};
} }
#endif
