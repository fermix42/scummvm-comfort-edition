/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_ATTACK_H
#define NOX_NATIVE_WORLD_ATTACK_H
#include "nox/native_world_actor_turn.h"
#include "nox/native_world_transport.h"
namespace Nox { namespace Native {
enum WorldAttackResult {kWorldAttackSelected,kWorldAttackNoTarget,kWorldAttackRefused,kWorldAttackInvalid};
struct WorldAttackContext {byte type,map,x,y,appearance,speed;bool ownedShip;};
class WorldAttack {
public:
    static WorldAttackResult choose(const WorldAttackContext &context,byte direction,const WorldScene &scene,
            const byte *tables,WorldActorTurnEvent &event,bool &alarm) {
        if (direction>3 || !tables) return kWorldAttackInvalid;
        const byte near[]={76,110,94,92},around[]={76,77,95,112,128,127,109,92};
        byte cell=near[direction];
        if (context.appearance==119) {
            bool found=false;for (int i=7;i>=0;--i) if (scene.actors.lookup[around[i]]!=255) {cell=around[i];found=true;break;}
            if (!found) return kWorldAttackNoTarget;
        } else if (scene.actors.lookup[cell]==255) return kWorldAttackNoTarget;
        if (context.map==48 && (context.x==16 || context.x==18) && context.y==35) return kWorldAttackNoTarget;
        if (context.speed>=4 || (!context.ownedShip && scene.actors.terrain[93]>=229 && scene.actors.terrain[93]<233)) return kWorldAttackRefused;
        const byte raw=scene.actors.kinds[cell],kind=raw&7,slot=scene.actors.lookup[cell];
        if ((kind!=0 && kind!=4) || (slot&3) || (kind==4 && (slot&7))) return kWorldAttackInvalid;
        const byte *actor=tables+(kind==4?1280:768)+slot;const byte tile=actor[2];
        if (!tile || actor[3]==255) return kWorldAttackInvalid;
        if (tile==225 || (context.type==2 && tile==205) || (context.type==6 && tile==209)) return kWorldAttackNoTarget;
        WorldActorTurnEvent selected;selected.battle=true;selected.bank=kind==4?1:0;selected.slot=slot;selected.direction=direction;selected.cell=cell;
        event=selected;alarm=context.map && context.map!=101 && raw>=2;return kWorldAttackSelected;
    }
    static void alarm(byte *npcs,const byte *footer,WorldTransportState &controls) {
        if (WorldTransportControl::raiseAlarm(npcs,footer)) controls.used=controls.alarm=true;
    }
};
} }
#endif
