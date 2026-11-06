/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_PROJECTILE_H
#define NOX_NATIVE_WORLD_PROJECTILE_H
#include "nox/native_random.h"
namespace Nox { namespace Native {
struct WorldProjectileOutcome {
    bool hit;byte endpoint,impact;
    WorldProjectileOutcome() : hit(false),endpoint(255),impact(255) {}
};
// Complete hostile queued-shot ray and conditional gameplay RNG. Rendering
// consumes the result; it must not generate another impact or random draw.
class WorldProjectile {
public:
    static bool hostile(byte x,byte y,byte direction,bool retainedMode,GameplayRandom &random,WorldProjectileOutcome &output) {
        return trace(x,y,direction,retainedMode && (direction==138 || direction==139),4,128,nullptr,random,output);
    }
    static bool player(byte appearance,byte direction,bool retainedMode,const byte *lookup,const byte *compact,GameplayRandom &random,WorldProjectileOutcome &output) {
        if ((appearance!=119 && appearance!=123) || !lookup || !compact) return false;
        byte eligible[187];for (uint i=0;i<187;++i) {
            if (lookup[i]!=255 && (lookup[i]&3)) return false;
            eligible[i]=lookup[i]!=255 && compact[lookup[i]+2]!=225;
        }
        const bool vertical=direction==138 || direction==139,frigate=appearance==119;
        const byte range=frigate?byte(4+(direction==139 || direction==149)):3;
        return trace(8,frigate?6:5,direction,vertical && (frigate || retainedMode),range,frigate?192:128,eligible,random,output);
    }
private:
    static bool trace(byte x,byte y,byte direction,bool mode,byte range,byte threshold,const byte *eligible,GameplayRandom &random,WorldProjectileOutcome &output) {
        if (x>=17 || y>=11 || (direction!=136 && direction!=138 && direction!=139 && direction!=149)) return false;
        int column=x,row=y;byte orientation=direction==149?0:direction==136?128:direction==138?64:192;
        GameplayRandom next=random;WorldProjectileOutcome result;
        for (uint remaining=range;remaining;--remaining) {
            const int step=(orientation&128)?-1:1;
            if (orientation&64) row+=step;else column+=step;
            if (column<0 || column>=17 || row<0 || row>=11) return false;
            result.endpoint=byte(row*17+column);
            if (eligible?eligible[result.endpoint]!=0:result.endpoint==93) {
                byte roll;if (!next.next(1,255,roll)) return false;
                if (roll<threshold) {result.hit=true;break;}
                // Source1F5E branches past the orientation reload on a miss.
                // Its random byte becomes the next remaining ray step's
                // direction bits, so a missed shot can turn before it ends.
                orientation=roll;
            }
        }
        result.impact=result.endpoint;
        // Horizontal directions clear the retained mode before the ray.
        // Vertical directions retain it. The source always enables it on return.
        if (!result.hit && mode) {
            if (column && column!=10) {
                byte jitter;if (!next.next(1,255,jitter)) return false;
                result.impact=byte(result.endpoint+(jitter>=128?-1:1));
            } else result.impact=byte(result.endpoint+(column==10?-1:1));
        }
        random=next;output=result;return true;
    }
};
} }
#endif
