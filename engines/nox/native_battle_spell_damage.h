/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_SPELL_DAMAGE_H
#define NOX_NATIVE_BATTLE_SPELL_DAMAGE_H
#include "nox/native_battle_roster.h"
#include "nox/native_battle_rewards.h"
#include "nox/native_spell_power.h"
namespace Nox { namespace Native {
struct SpellDamageIntent {
    byte casterCell,targetCell,category,basePower,threshold,classification;
    bool instant,protectedTargets,quick;
    byte quickPartyLevel;
    SpellDamageIntent() : casterCell(255),targetCell(255),category(0),basePower(0),threshold(0),classification(1),instant(false),protectedTargets(false),quick(false),quickPartyLevel(0) {}
};
struct SpellDamageOutcome {
    byte classification,powerRoll,armorRoll;uint16 damage,credit;bool killed,suppressed;
    SpellDamageOutcome() : classification(1),powerRoll(0),armorRoll(0),damage(0),credit(0),killed(false),suppressed(false) {}
};
// Shared admitted-target spell impact in ordinary raw tactical records.
// Family target order, mana, animation and post-effect training are callers.
// Quick's temporary metadata policy is deliberately not inferred here.
class BattleSpellDamage {
public:
    BattleSpellDamage() : _ready(false) {}
    bool load(HdvArchive &archive) { _ready=_rewards.load(archive);return _ready; }
    bool resolve(const SpellDamageIntent &intent,GameplayRandom &random,BattleRecords &records,SpellDamageOutcome &output) const {
        VortexVictim caster,target;
        if (!_ready || intent.classification>3 || !records.actor(intent.casterCell,caster) || !records.actor(intent.targetCell,target) ||
            !READ_LE_UINT16(caster.stats+2) || !READ_LE_UINT16(target.stats+2)) return false;
        GameplayRandom rng=random;BattleRecords next=records;SpellDamageOutcome result;result.classification=intent.classification;
        const bool playerSide=caster.kind!=1,protectedHit=intent.protectedTargets && playerSide;
        if (intent.instant) {
            // Preserve the source's explicit zero-skill fallback to the
            // alternate stat field even for a party caster.
            const byte power=caster.kind==3 && caster.stats[34]?caster.stats[18]:caster.stats[6]&127;
            const byte guard=target.kind==3?target.stats[18]:target.stats[6]&127;
            if (power<guard || protectedHit) {
                result.classification=3;result.suppressed=protectedHit && power>=guard;
                output=result;return true;
            }
            result.classification=0;result.damage=3000;result.killed=true;
        } else {
            byte power=intent.basePower;
            if (caster.kind==3 && !effectiveSpellPower(power,target.stats[intent.quick && target.kind!=3?0:1]&127,intent.threshold,power)) return false;
            const SpellPowerBounds bounds=spellPowerBounds(power),armor=spellPowerBounds(target.stats[7]);
            if (!rng.next(bounds.lower,bounds.upper,result.powerRoll) || !rng.next(armor.lower,armor.upper,result.armorRoll)) return false;
            const uint16 gross=uint16(result.powerRoll*2);
            uint16 mitigation=result.armorRoll/4;
            const byte resistance=target.stats[11];
            if (resistance==255) mitigation=uint16(mitigation+0x26ff);
            else if (resistance>=10) mitigation=uint16(mitigation+uint16(uint32(gross&32767)*(resistance/4))/100);
            result.damage=gross>mitigation?uint16(gross-mitigation):0;
            result.suppressed=protectedHit;
            result.killed=!protectedHit && result.damage>=READ_LE_UINT16(target.stats+2);
        }
        if (result.killed) {
            if (playerSide && target.kind<3) {
                const byte casterLevel=intent.quick?(caster.kind==3?intent.quickPartyLevel:caster.stats[0]&127):caster.kind==3 && caster.stats[34]?caster.stats[1]:caster.stats[1]&127;
                const uint16 base=target.stats[5]|uint16(target.stats[8])<<8;
                result.credit=_rewards.credit(base,casterLevel,target.stats[intent.quick && target.kind!=3?0:1]&127,intent.category==5 && target.stats[16]==188);
                next.credit=uint16(next.credit+result.credit);
            }
            if (!BattleRoster::remove(next,target)) return false;
        } else if (!result.suppressed) {
            WRITE_LE_UINT16(target.stats+2,uint16(READ_LE_UINT16(target.stats+2)-result.damage));
            if (!next.store(target)) return false;
        }
        records=next;random=rng;output=result;return true;
    }
private:
    bool _ready;BattleRewardRules _rewards;
};
} }
#endif
