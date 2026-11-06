/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_MOTION_H
#define NOX_NATIVE_BATTLE_MOTION_H
#include "nox/native_encounter_state.h"
namespace Nox { namespace Native {
// Distant open-floor movement turn for the admitted initial Rat encounter.
// Adjacent attacks, obstructed terrain, edges/escape and other actors are
// explicit outstanding branches. Never retire only the player's half-turn.
class BattleMotion {
public:
    static bool advance(byte direction,const byte *terrain,uint count,GameplayRandom &random,BattleRecords &records,bool *moved=nullptr) {
        if (moved) *moved=false;
        if (direction>3 || !terrain || count!=187 || records.compact[2]!=246 || records.extended[2]!=164 ||
            records.stats[0]!=0 || !READ_LE_UINT16(records.stats+2) || !READ_LE_UINT16(records.stats+770)) return false;
        for (uint i=0;i<count;++i) if (terrain[i]!=86) return false;
        const int dx[]={0,0,1,-1},dy[]={-1,1,0,0};
        const int x=int(records.compact[0])+dx[direction],y=int(records.compact[1])+dy[direction];
        if (x<1 || x>9 || y<1 || y>9) return false;
        int enemyX=records.extended[0],enemyY=records.extended[1];
        // A directional attempt into the occupied enemy cell is refused by
        // the original command handler without retiring an enemy turn.
        if (x==enemyX && y==enemyY) return true;
        if (enemyX<1 || enemyX>9 || enemyY<1 || enemyY>9 || MAX(ABS(x-enemyX),ABS(y-enemyY))<2) return false;
        GameplayRandom nextRandom=random;byte roll;if (!nextRandom.next(1,255,roll)) return false;
        // Original shared actor chase orders horizontal/vertical candidates,
        // swaps that order for a low roll, and takes the first open direction.
        const bool horizontal=(roll&128)?x!=enemyX:y==enemyY;
        if (horizontal) enemyX+=x>enemyX?1:-1;
        else enemyY+=y>enemyY?1:-1;
        if (enemyX<1 || enemyX>9 || enemyY<1 || enemyY>9 || (x==enemyX && y==enemyY)) return false;
        BattleRecords next=records;next.compact[0]=byte(x);next.compact[1]=byte(y);next.extended[0]=byte(enemyX);next.extended[1]=byte(enemyY);
        if (!EncounterState::rebuildLookup(next)) return false;
        records=next;random=nextRandom;if (moved) *moved=true;return true;
    }
};
} }
#endif
