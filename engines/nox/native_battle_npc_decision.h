/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_NPC_DECISION_H
#define NOX_NATIVE_BATTLE_NPC_DECISION_H
#include "nox/native_battle_targets.h"
#include "nox/native_random.h"
namespace Nox { namespace Native {
enum NpcBattleAction { kNpcPursuit,kNpcPhysical,kNpcCast };
struct NpcBattleDecision {
    NpcBattleAction action;byte target;
    NpcBattleDecision() : action(kNpcPursuit),target(255) {}
};
// One full decision transaction, before invoking movement/physical/spell owners.
// Nearest ordering, the source alternate scratch and engager sentinel are shared
// with party target selection. MP and ranged choices consume separate draws.
class BattleNpcDecision {
public:
    static bool choose(byte cell,GameplayRandom &random,BattleRecords &records,NpcBattleDecision &output) {
        VortexVictim caster;if (!records.actor(cell,caster) || caster.kind==3) return false;
        GameplayRandom rng=random;BattleRecords next=records;NpcBattleDecision result;byte alternate=255;
        if (!BattleTargets::nearest(next,cell,255,false,false,result.target,&alternate)) return false;
        if (result.target!=255) {
            if (caster.stats[4]) {
                byte choice;if (!rng.next(1,255,choice)) return false;
                if (choice>=128) {
                    if (choice<224 && !retarget(next,cell,alternate,result.target)) return false;
                    result.action=kNpcCast;random=rng;output=result;return true;
                }
            }
            bool adjacent=false;const uint origin=uint(caster.actor[1])*17+caster.actor[0],offsets[]={0,1,18,17};
            for (uint foot=0;foot<((caster.actor[3]&8)?4U:1U);++foot) {
                const uint position=origin+offsets[foot];
                adjacent|=ABS(int(position%17)-int(result.target%17))<=1 && ABS(int(position/17)-int(result.target/17))<=1;
            }
            if (caster.stats[15]>>4) {
                byte choice;if (!rng.next(1,255,choice)) return false;
                if (adjacent || choice>=128) {
                    if (choice<224 && !retarget(next,cell,alternate,result.target)) return false;
                    result.action=kNpcPhysical;
                }
            } else if (adjacent) result.action=kNpcPhysical;
        }
        if (result.action==kNpcPursuit) { caster.stats[12]=255;if (!next.store(caster)) return false; }
        records=next;random=rng;output=result;return true;
    }
private:
    static bool retarget(const BattleRecords &records,byte cell,byte alternate,byte &target) {
        byte engager;if (!BattleTargets::engager(records,cell,engager)) return false;
        if (!engager && alternate!=255) target=alternate;return true;
    }
};
} }
#endif
