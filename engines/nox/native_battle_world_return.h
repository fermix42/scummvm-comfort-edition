/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_WORLD_RETURN_H
#define NOX_NATIVE_BATTLE_WORLD_RETURN_H
#include "common/scummsys.h"
namespace Nox { namespace Native {
struct BattleTreasureAward {
    byte marker,kind,id;
    BattleTreasureAward() : marker(0),kind(0),id(0) {}
};
// R commits authored contact effects before growth/loot; the world caller
// removes the retained parent only after results return. Keep both boundaries.
class BattleWorldReturn {
public:
    static bool effects(byte *tables,uint tableSize,byte *fields,uint fieldSize,byte bank,uint slot,bool fled,BattleTreasureAward *award=nullptr) {
        if (!valid(tables,tableSize,fields,fieldSize,bank,slot)) return false;
        if (fled) return true;
        // Read the actual eight-byte owned record, including slot248. Native
        // ownership replaces the original wrapped9-byte transport/stale scratch.
        const byte *definition=tables+768+slot;
        if (!bank) {
            if (slot<244 && (definition[3]&4)) fields[definition[5]]|=definition[7];
        } else {
            const byte *actor=tables+1280+slot;
            if (actor[5]==126) {
                // R98D0 passes A2 to AUXMOVE, which preserves A. BIT does not
                // change A; without bit128 the queued marker therefore stays2.
                if ((definition[0]&64) && (!award || definition[5]>2)) return false;
                if (definition[0]&128) fields[definition[1]]|=definition[3];
                if (definition[0]&64) { award->marker=(definition[0]&128)?fields[definition[1]]:2;award->kind=definition[5];award->id=definition[7]; }
            }
        }
        return true;
    }
    static bool retire(byte *tables,uint tableSize,byte *fields,uint fieldSize,byte bank,uint slot,bool fled) {
        if (!valid(tables,tableSize,fields,fieldSize,bank,slot)) return false;
        if (fled) return true;
        byte *actor=tables+(bank?1280:768)+slot;
        actor[3]=255;if (bank) actor[7]=255;return true;
    }
    static bool complete(byte *tables,uint tableSize,byte *fields,uint fieldSize,byte bank,uint slot,bool fled) {
        return effects(tables,tableSize,fields,fieldSize,bank,slot,fled) && retire(tables,tableSize,fields,fieldSize,bank,slot,fled);
    }
private:
    static bool valid(const byte *tables,uint tableSize,const byte *fields,uint fieldSize,byte bank,uint slot) {
        return tables && tableSize==1792 && fields && fieldSize==256 && bank<=1 && slot<=252 && !(slot&3) && (!bank || !(slot&7));
    }
};
} }
#endif
