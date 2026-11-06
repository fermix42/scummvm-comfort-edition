/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_DIRECTED_SPELL_H
#define NOX_NATIVE_BATTLE_DIRECTED_SPELL_H
#include "nox/native_battle_spell_damage.h"
namespace Nox { namespace Native {
struct DirectedSpellSelection {
    byte cell,direction;bool cancelled;
    DirectedSpellSelection() : cell(255),direction(255),cancelled(false) {}
};
struct DirectedSpellOutcome {
    bool cancelled,spentMana,directionRefused;uint killed;
    Common::Array<byte> targets,jitter;Common::Array<SpellDamageOutcome> impacts;
    DirectedSpellOutcome() : cancelled(false),spentMana(false),directionRefused(false),killed(0) {}
};
// Standard missile/blast/chain/blade/Nox families share one confirmed cast
// transaction. Selection UI and visual/audio continuations remain callers.
// Preserve cosmetic jitter draws because they share the gameplay RNG stream.
class BattleDirectedSpell {
public:
    BattleDirectedSpell() : _ready(false) {}
    bool load(HdvArchive &archive) { _ready=_impact.load(archive);return _ready; }
    static bool supports(byte spell) {
        return spell<=1 || (spell>=5 && spell<=13) || spell==15 || spell==21 || spell==23 || spell==24 || spell==25;
    }
    bool resolve(const CastPreparation &prepared,byte casterCell,const DirectedSpellSelection &selection,byte category,bool protectedTargets,
            GameplayRandom &random,BattleRecords &records,DirectedSpellOutcome &output) const {
        VortexVictim caster;if (!_ready || !supports(prepared.spell) || !records.actor(casterCell,caster) ||
            !READ_LE_UINT16(caster.stats+2) || caster.stats[4]<prepared.manaCost) return false;
        const byte spell=prepared.spell;DirectedSpellOutcome result;
        const bool massTarget=spell==21 || spell==23 || spell==25;
        if (spell!=12 && !massTarget && selection.cancelled) { result.cancelled=true;output=result;return true; }
        BattleRecords next=records;GameplayRandom rng=random;
        if (spell==12) {
            const byte direction=caster.kind==3?(selection.cancelled?255:selection.direction):byte(kSouth);
            if (caster.actor[0]>10 || caster.actor[1]>10) return false;
            if (direction>3 || (direction==kNorth && !caster.actor[1]) || (direction==kSouth && caster.actor[1]==10) ||
                (direction==kEast && caster.actor[0]==10) || (direction==kWest && !caster.actor[0])) {
                // Escape/non-direction or an outward boundary direction is a
                // spent Vortex, not the ordinary free target cancellation.
                caster.stats[4]-=prepared.manaCost;if (!next.store(caster)) return false;
                result.directionRefused=result.spentMana=true;records=next;output=result;return true;
            }
            if (!vortex(next,caster,direction,result.targets)) return false;
            caster.stats[12]=255;
        } else {
            if (!massTarget && selection.cell>=187) return false;
            if (spell==9 || spell==10) {
                if (!chain(next,caster,selection.cell,spell==9?2:4,result.targets)) return false;
                caster.stats[12]=255;
            } else if (spell==21 || spell==23 || spell==25) {
                if (!mass(next,caster,result.targets)) return false;
                caster.stats[12]=result.targets.empty()?255:result.targets[0];
            } else {
                if (!append(next,caster.cell,selection.cell,result.targets)) return false;
                caster.stats[12]=result.targets.empty()?255:result.targets[0];
            }
        }
        caster.stats[4]-=prepared.manaCost;if (!next.store(caster)) return false;
        const bool blade=spell==15 || spell==21 || spell==23 || spell==24 || spell==25;
        for (uint i=0;i<result.targets.size();++i) {
            if (blade) {
                byte point,quadrant;if (!rng.next(0,5,point) || !rng.next(1,255,quadrant)) return false;
                result.jitter.push_back(point);result.jitter.push_back(quadrant);
            }
            SpellDamageIntent intent;intent.casterCell=casterCell;intent.targetCell=result.targets[i];intent.category=category;
            intent.basePower=prepared.basePower;intent.threshold=prepared.levelThreshold;intent.protectedTargets=protectedTargets;intent.instant=spell==11 || spell==12;
            SpellDamageOutcome impact;if (!_impact.resolve(intent,rng,next,impact)) return false;
            result.killed+=impact.killed?1:0;result.impacts.push_back(impact);
        }
        result.spentMana=true;records=next;random=rng;output=result;return true;
    }
private:
    static bool append(const BattleRecords &battle,byte caster,byte cell,Common::Array<byte> &targets) {
        if (cell>=187) return false;if (battle.indices[cell]==255 || cell==caster) return true;
        for (uint i=0;i<targets.size();++i) if (targets[i]==cell) return true;
        if (targets.size()>=32) return false;targets.push_back(cell);return true;
    }
    static bool chain(const BattleRecords &battle,const VortexVictim &caster,byte center,byte radius,Common::Array<byte> &targets) {
        if (!append(battle,caster.cell,center,targets)) return false;
        byte candidates[38];uint count=0;
        for (uint cell=0;cell<187 && count<38;++cell) {
            if (battle.indices[cell]==255) continue;VortexVictim actor;if (!battle.actor(byte(cell),actor)) return false;
            const uint x=actor.actor[0],y=actor.actor[1];if (x>=17 || y>=11) return false;
            if (x==uint(center%17) && y==uint(center/17)) continue;
            if (ABS(int(x)-int(center%17))>radius || ABS(int(y)-int(center/17))>radius) continue;
            // The original candidate capacity counts occupied cells before
            // canonical-coordinate deduplication, including large footprints.
            candidates[count++]=byte(y*17+x);
        }
        for (uint i=0;i<count;++i) if (!append(battle,caster.cell,candidates[i],targets)) return false;
        return true;
    }
    static bool mass(const BattleRecords &battle,const VortexVictim &caster,Common::Array<byte> &targets) {
        const byte wanted=caster.kind==3?1:caster.actor[5];
        for (uint cell=0;cell<187;++cell) {
            if (battle.indices[cell]==255 || (battle.kinds[cell]&128)) continue;
            VortexVictim actor;if (!battle.actor(byte(cell),actor)) return false;
            if (actor.kind!=2 && actor.kind!=wanted && wanted!=255) continue;
            if ((actor.actor[3]&8) && !(battle.kinds[cell]&64)) continue;
            if (actor.kind==2 && actor.actor[4]!=wanted) continue;
            if (actor.kind==caster.kind && actor.index==caster.index) continue;
            if ((actor.kind==3?actor.actor[3]:actor.actor[7])==255) continue;
            // The multi-target branch appends before nearest/range ranking.
            if (!append(battle,caster.cell,byte(cell),targets)) return false;
        }
        return true;
    }
    static bool vortex(const BattleRecords &battle,const VortexVictim &caster,byte direction,Common::Array<byte> &targets) {
        for (uint cell=0;cell<187;++cell) {
            if (battle.indices[cell]==255) continue;VortexVictim actor;if (!battle.actor(byte(cell),actor)) return false;
            const byte x=actor.actor[0],y=actor.actor[1];if (x>=17 || y>=11) return false;
            if ((x==caster.actor[0] && y==caster.actor[1]) || ((actor.actor[3]&8) && !(battle.kinds[cell]&64))) continue;
            const uint dx=ABS(int(x)-int(caster.actor[0])),dy=ABS(int(y)-int(caster.actor[1]));bool chosen=false;
            switch (direction) {
            case kNorth:chosen=y<caster.actor[1] && dx<=dy;break;
            case kSouth:chosen=y>=caster.actor[1] && dx<=dy;break;
            case kEast:chosen=x>=caster.actor[0] && dy<dx;break;
            case kWest:chosen=x<caster.actor[0] && dy<dx;break;
            }
            if (chosen && !append(battle,caster.cell,byte(cell),targets)) return false;
        }
        return true;
    }
    bool _ready;BattleSpellDamage _impact;
};
} }
#endif
