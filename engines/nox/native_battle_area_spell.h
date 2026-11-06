/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_AREA_SPELL_H
#define NOX_NATIVE_BATTLE_AREA_SPELL_H
#include "nox/native_battle_spell_damage.h"
namespace Nox { namespace Native {
// The authored visible tactical field is eleven by eleven within the retained
// seventeen-column record grid. Keep the source's clipped ring visitation order,
// outer-corner policy and draws on empty/already selected cells.
class SpellAreaTargets {
public:
    static bool collect(bool fireball,byte radius,byte center,byte caster,const BattleRecords &battle,
            GameplayRandom &random,Common::Array<byte> &targets) {
        if (radius<1 || radius>3 || center>=187 || center%17>10 || caster>=187) return false;
        GameplayRandom rng=random;Common::Array<byte> selected;byte threshold=255;
        int x=center%17,y=center/17;const int centerX=x,centerY=y;
        if (!visit(x,y,255,caster,battle,rng,selected)) return false;
        for (uint distance=1;distance<=radius;++distance) {
            const byte left=byte(centerX-distance),right=byte(centerX+distance),top=byte(centerY-distance),bottom=byte(centerY+distance);
            const bool rounded=!fireball && distance==radius && radius!=1;
            if (y) --y;
            if (!visit(x,y,threshold,caster,battle,rng,selected)) return false;
            while (x) {
                --x;if ((!rounded || left<x) && !visit(x,y,threshold,caster,battle,rng,selected)) return false;
                if (x==left) break;
            }
            while (y!=10) {
                ++y;if ((!rounded || y<bottom) && !visit(x,y,threshold,caster,battle,rng,selected)) return false;
                if (y==bottom) break;
            }
            while (x!=10) {
                ++x;if ((!rounded || x<right) && !visit(x,y,threshold,caster,battle,rng,selected)) return false;
                if (x==right) break;
            }
            while (y) {
                --y;if ((!rounded || top<y) && !visit(x,y,threshold,caster,battle,rng,selected)) return false;
                if (y==top) break;
            }
            while (x!=centerX) {
                --x;if (!visit(x,y,threshold,caster,battle,rng,selected)) return false;
            }
            const uint decay=fireball?64:0;threshold=threshold>=decay?byte(threshold-decay):129;
        }
        random=rng;targets=selected;return true;
    }
private:
    static bool visit(int x,int y,byte threshold,byte caster,const BattleRecords &battle,GameplayRandom &random,Common::Array<byte> &targets) {
        if (x<0 || x>10 || y<0 || y>10) return false;
        if (threshold!=255) { byte roll;if (!random.next(1,255,roll)) return false;if (roll>=threshold) return true; }
        const byte cell=byte(y*17+x);if (battle.indices[cell]==255) return true;
        VortexVictim actor;if (!battle.actor(cell,actor)) return false;
        if ((actor.actor[3]&8) && !(battle.kinds[cell]&64)) return true;
        if (cell==caster) return true;
        for (uint i=0;i<targets.size();++i) if (targets[i]==cell) return true;
        if (targets.size()>=32) return false;targets.push_back(cell);return true;
    }
};
struct AreaSpellOutcome {
    bool cancelled,spentMana;uint killed;Common::Array<byte> targets;Common::Array<SpellDamageOutcome> impacts;
    AreaSpellOutcome() : cancelled(false),spentMana(false),killed(0) {}
};
// Complete standard Fireball/Storm semantic cast: confirmed ground selection,
// mana, ordered stochastic/deterministic area, shared impacts and engagement.
// Book selection, pending target UI, pixel effects/audio and turn retirement
// remain explicit callers rather than implicit CPU continuations.
class BattleAreaSpell {
public:
    BattleAreaSpell() : _ready(false) {}
    bool load(HdvArchive &archive) { _ready=_impact.load(archive);return _ready; }
    bool resolve(const CastPreparation &prepared,byte casterCell,byte center,bool cancelled,byte category,bool protectedTargets,
            GameplayRandom &random,BattleRecords &records,AreaSpellOutcome &output) const {
        const bool fireball=prepared.spell>=2 && prepared.spell<=4,storm=prepared.spell>=27 && prepared.spell<=29;
        VortexVictim caster;if (!_ready || (!fireball && !storm) || !records.actor(casterCell,caster) ||
            !READ_LE_UINT16(caster.stats+2) || caster.stats[4]<prepared.manaCost) return false;
        AreaSpellOutcome result;if (cancelled) { result.cancelled=true;output=result;return true; }
        BattleRecords next=records;GameplayRandom rng=random;
        caster.stats[4]-=prepared.manaCost;if (!next.store(caster)) return false;
        if (!SpellAreaTargets::collect(fireball,byte(prepared.spell-(fireball?1:26)),center,casterCell,next,rng,result.targets)) return false;
        caster.stats[12]=255;if (!next.store(caster)) return false;
        for (uint i=0;i<result.targets.size();++i) {
            SpellDamageIntent intent;intent.casterCell=casterCell;intent.targetCell=result.targets[i];intent.category=category;
            intent.basePower=prepared.basePower;intent.threshold=prepared.levelThreshold;intent.protectedTargets=protectedTargets;
            SpellDamageOutcome impact;if (!_impact.resolve(intent,rng,next,impact)) return false;
            result.killed+=impact.killed?1:0;result.impacts.push_back(impact);
        }
        result.spentMana=true;records=next;random=rng;output=result;return true;
    }
private:
    bool _ready;BattleSpellDamage _impact;
};
} }
#endif
