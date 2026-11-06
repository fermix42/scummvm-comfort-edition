/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_PHYSICAL_H
#define NOX_NATIVE_BATTLE_PHYSICAL_H
#include "nox/native_battle_roster.h"
#include "nox/native_battle_rewards.h"
namespace Nox { namespace Native {
enum PhysicalResult { kPhysicalMiss, kPhysicalBlocked, kPhysicalHit, kPhysicalCritical, kPhysicalResistedCritical, kPhysicalKilled, kPhysicalSuppressed };
struct PhysicalIntent {
    byte casterCell,targetCell,category;
    bool offHand,exceptionalWeapon,protectedTargets,quick,quickRear,projectileMode;
    byte quickPartyLevel;
    PhysicalIntent() : casterCell(255),targetCell(255),category(0),offHand(false),exceptionalWeapon(false),protectedTargets(false),quick(false),quickRear(false),projectileMode(false),quickPartyLevel(0) {}
};
struct PhysicalOutcome {
    PhysicalResult result;byte hitChance,hitRoll,classification,powerRoll,armorRoll;
    uint16 damage,credit;bool killed;
    PhysicalOutcome() : result(kPhysicalMiss),hitChance(0),hitRoll(0),classification(1),powerRoll(0),armorRoll(0),damage(0),credit(0),killed(false) {}
};
// Ordinary selected-target physical attack pipeline for all three combatant kinds.
// Command collection, range/projectile geometry and turn scheduling own their
// separate contracts; this operation commits its records/RNG atomically.
class BattlePhysical {
public:
    BattlePhysical() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> hit,overrides;
        if (!archive.readRange("NA/BS.AUX.RTN.BK2",0x1362,6,hit) || hit.size()!=6 ||
            !_rewards.load(archive) ||
            !archive.readRange("NA/BS.AUX.RTN.BK2",0x1350,18,overrides) || overrides.size()!=18) return false;
        for (uint i=0;i<6;++i) if (!decimal(hit[i],_hit[i])) return false;
        memcpy(_overrides,overrides.data(),18);_ready=true;return true;
    }
    bool resolve(const PhysicalIntent &intent,GameplayRandom &random,BattleRecords &records,PhysicalOutcome &output) const {
        if (!_ready || intent.casterCell==intent.targetCell) return false;
        VortexVictim caster,target;
        if (!records.actor(intent.casterCell,caster) || !records.actor(intent.targetCell,target) ||
            !READ_LE_UINT16(caster.stats+2) || !READ_LE_UINT16(target.stats+2)) return false;
        // Type1 is an unavailable hand. Range/target acquisition and projectile
        // presentation belong to the command owner; this resolves admitted hits.
        const byte weapon=caster.kind==3?byte(intent.offHand?caster.stats[21]&15:caster.stats[21]>>4):byte(caster.stats[15]>>4);
        if (weapon==1) return false;const bool ranged=weapon!=0;
        const byte base=caster.kind==3?caster.stats[intent.offHand?10:9]:caster.stats[9];
        if (!base) return false;
        GameplayRandom rng=random;BattleRecords next=records;PhysicalOutcome result;
        const bool player=caster.kind==3,playerSide=caster.kind!=1;
        const byte level=caster.stats[1]&127,targetLevel=target.stats[1]&127;
        const byte attackerSkill=player && caster.stats[34]?caster.stats[34]:caster.stats[13];
        const byte defenderSkill=target.kind==3?target.stats[34]:target.stats[10];
        const byte defense=target.kind==3?target.stats[31]:((target.stats[14]&128)?target.stats[14]&127:0);
        bool blocked=false,miss=false,interrupted=false;
        byte flags=player?caster.stats[intent.offHand?65:64]:0;
        if (!(flags&96)) {
            byte chance=50;if (player && !decimal(caster.stats[8],chance)) return false;
            const uint delta=MIN<uint>(ABS(int(level)-int(targetLevel)),5);
            int adjusted=int(chance)+(level>=targetLevel?int(_hit[delta]):-int(_hit[delta]));
            chance=byte(CLIP<int>(adjusted,5,95));
            byte overrideChance=0;if (!categoryChance(intent.category,caster.stats[16],overrideChance)) return false;
            if ((!player || !caster.stats[34]) && overrideChance) chance=overrideChance;
            result.hitChance=bcd(chance);byte roll;if (!folded(rng,roll)) return false;result.hitRoll=bcd(roll);miss=roll>chance;
            if (!miss && !(target.kind==3 && target.stats[70]>=9) && defense) {
                if (!folded(rng,roll)) return false;
                blocked=MIN<uint>(defense,95)>=roll;miss=blocked;
            }
            if (!miss && (player || attackerSkill)) {
                const uint numerator=uint(attackerSkill)+(player?caster.stats[17]:attackerSkill);
                const uint divisor=target.stats[12]==caster.cell?8:target.stats[12]==255?4:2;
                const uint critical=MIN<uint>(numerator/divisor,95);
                if (!folded(rng,roll)) return false;
                if (critical>=roll) {
                    if (!folded(rng,roll)) return false;
                    result.classification=(defenderSkill/2>=100 || defenderSkill/2>roll)?3:0;
                }
            }
            if (!miss && ranged && !intent.exceptionalWeapon) {
                byte engager=0;if (!nearbyEngager(records,caster,engager)) return false;
                if (engager) {
                    if (!intent.quick) {
                        byte pressure;if (!rng.next(1,255,pressure)) return false;
                        if (pressure<128) { miss=true;interrupted=true; }
                    }
                }
            }
            if (player) {
                if (result.classification!=1 && !BattleRoster::practice(caster,35)) return false;
                if (!BattleRoster::practice(caster,ranged?29:26)) return false;
            } else if (target.kind==3 && target.stats[70]<9 && (targetLevel>=3 || !miss)) {
                if (!BattleRoster::practice(target,32)) return false;
            }
        }
        // Authored flag16 randomizes the projectile kind before its rendering.
        // Keep its source draw even when drawing is accelerated. The separate
        // world/transport miss-offset mode is not admitted by this operation.
        if (intent.quick && intent.quickRear) {
            // Q's rear physical fallback commits the real hit/practice
            // preflight,then returns before damage,flavor and engagement.
            if (!next.store(caster) || !next.store(target)) return false;
            records=next;random=rng;output=result;return true;
        }
        if (!intent.quick && ranged && (flags&16)) { byte projectile;if (!rng.next(1,255,projectile)) return false; }
        if (!intent.quick && ranged && miss && intent.projectileMode && target.actor[0] && target.actor[0]!=10) {
            byte displacement;if (!rng.next(1,255,displacement)) return false;
        }
        if (miss) {
            // The ordinary melee miss/block message choice consumes a source
            // draw even though its visual choice has no damage effect.
            if (!interrupted && (!ranged || !blocked)) { byte flavor;if (!rng.next(1,255,flavor)) return false; }
            result.result=blocked?kPhysicalBlocked:kPhysicalMiss;
        } else if (player && (target.stats[intent.quick && target.kind!=3?0:1]&128) && !intent.exceptionalWeapon) {
            result.result=kPhysicalHit; // Source physical-immunity branch, no power/armor draws.
        } else {
            if (!spread(rng,base,result.powerRoll) || !spread(rng,target.stats[7],result.armorRoll)) return false;
            uint16 gross=uint16(result.powerRoll*2);
            // Source F0A7/F343 reloads the caster after E0CC commits practice.
            // A promotion in this very attack affects critical DAMAGE,while
            // its hit/critical eligibility used the earlier skill above.
            const byte damageSkill=player && caster.stats[34]?caster.stats[34]:caster.stats[13];
            if (!result.classification) gross=uint16(gross+(ranged?damageSkill:damageSkill*4));
            const uint16 mitigation=intent.exceptionalWeapon && (target.stats[1]&128)?result.armorRoll/4:result.armorRoll;
            result.damage=gross>mitigation?uint16(gross-mitigation):0;
            result.result=!result.classification?kPhysicalCritical:result.classification==3?kPhysicalResistedCritical:kPhysicalHit;
            if (intent.protectedTargets && playerSide) result.result=kPhysicalSuppressed;
            else {
                const uint16 hp=READ_LE_UINT16(target.stats+2);
                if (result.damage>=hp) {
                    result.credit=playerSide && target.kind<3?credit(caster,target,intent):0;
                    next.credit=uint16(next.credit+result.credit);result.killed=true;result.result=kPhysicalKilled;
                    // Store other target training before removal commits death.
                    if (!next.store(target) || !BattleRoster::remove(next,target)) return false;
                } else WRITE_LE_UINT16(target.stats+2,hp-result.damage);
            }
        }
        caster.stats[12]=intent.targetCell;
        if (!next.store(caster) || (!result.killed && !next.store(target))) return false;
        records=next;random=rng;output=result;return true;
    }
private:
    static bool nearbyEngager(const BattleRecords &battle,const VortexVictim &caster,byte &cell) {
        cell=0;
        for (uint i=0;i<187;++i) {
            if (battle.indices[i]==255) continue;
            if (ABS(int(i%17)-int(caster.cell%17))>1 || ABS(int(i/17)-int(caster.cell/17))>1) continue;
            VortexVictim other;if (!battle.actor(byte(i),other)) return false;
            if (other.stats[12]==caster.cell) { cell=byte(i);return true; }
        }
        return true;
    }
    static bool decimal(byte value,byte &out) {
        if ((value&15)>9 || (value>>4)>9) return false;
        out=byte((value>>4)*10+(value&15));return true;
    }
    static byte bcd(uint value) { return byte((value/10)*16+value%10); }
    static bool folded(GameplayRandom &rng,byte &out) {
        if (!rng.next(1,199,out)) return false;if (out>=100) out-=100;return true;
    }
    static bool spread(GameplayRandom &rng,byte power,byte &out) {
        const uint spread=power/10;return rng.next(byte(power-spread),byte(MIN<uint>(255,power+spread)),out);
    }
    // Filled from the authored category overrides, not a CPU dispatch table.
    bool categoryChance(byte category,byte tile,byte &out) const {
        out=0;
        for (int i=15;i>=0;i-=3) if (_overrides[i]==category && _overrides[i+1]==tile) return decimal(_overrides[i+2],out);
        return true;
    }
    uint16 credit(const VortexVictim &caster,const VortexVictim &target,const PhysicalIntent &intent) const {
        const byte casterLevel=intent.quick?(caster.kind==3?intent.quickPartyLevel:caster.stats[0]&127):caster.kind==3 && caster.stats[34]?caster.stats[1]:caster.stats[1]&127;
        const byte targetLevel=target.stats[intent.quick && target.kind!=3?0:1]&127;
        const bool special=caster.kind!=1 && intent.category==5 && target.stats[16]==188;
        const uint16 base=target.stats[5]|uint16(target.stats[8])<<8;
        return _rewards.credit(base,casterLevel,targetLevel,special);
    }
    bool _ready;byte _hit[6],_overrides[18];BattleRewardRules _rewards;
};
} }
#endif
