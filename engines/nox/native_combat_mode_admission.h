/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_COMBAT_MODE_ADMISSION_H
#define NOX_NATIVE_COMBAT_MODE_ADMISSION_H
#include "nox/native_encounter_state.h"
#include "nox/native_combat_preferences.h"
namespace Nox { namespace Native {
class CombatModeAdmission {
public:
    static bool begin(HdvArchive &archive,const PreparedEncounter &field,CombatPreference preference,EncounterState &battle) {
        if (!battle.formed || !battle.presented || preference>kPreferAsk) return false;
        if (battle.round.phase==kBattleVictory || battle.round.phase==kBattleDefeat) return true;
        if (battle.mode.selection || battle.quick.partyCount || battle.command || battle.resultPhase || battle.round.actions || battle.inventory.active()) return true;
        Common::ScopedPtr<EncounterState> next(new EncounterState(battle));bool forbidFlee=false;
        const bool restricted=CombatModeRules::forcedTactical(next->round.category,field.contactSprite,field.contextFlags,forbidFlee);
        byte levels[6];if (!next->round.partyCount || next->round.partyCount>6) return false;
        for (uint i=0;i<next->round.partyCount;++i) levels[i]=next->records.stats[i*128+1];
        CombatAdvice advice;
        if (!CombatModeRules::advice(levels,next->round.partyCount,next->records.stats[769],next->round.mapId,next->records.stats[784],next->round.enemyCount,advice)) return false;
        next->mode.average=advice.average;next->mode.difference=advice.difference;next->mode.warning=advice.warning;
        next->mode.selection=restricted || preference==kPreferTactical?2:preference==kPreferAsk?1:3;
        if (forbidFlee) next->exit.control=128;
        // Deliberate CE policy: both preference paths use the source level-based
        // advice, never the original forced-mode framebuffer scratch lifetime.
        if (next->mode.selection==3 && !prepare(archive,field,*next)) return false;
        battle=*next;return true;
    }
    static bool select(HdvArchive &archive,const PreparedEncounter &field,bool quick,EncounterState &battle) {
        if (battle.mode.selection!=1 || battle.quick.partyCount || battle.command || battle.resultPhase || battle.round.actions || battle.inventory.active()) return false;
        Common::ScopedPtr<EncounterState> next(new EncounterState(battle));next->mode.selection=quick?3:2;
        if (quick && !prepare(archive,field,*next)) return false;
        battle=*next;return true;
    }
    static bool automaticTactical(const EncounterState &battle) {
        return !battle.inventory.active() && !battle.command && !battle.resultPhase && !battle.quick.partyCount &&
            (battle.mode.selection==0 || battle.mode.selection==2) && battle.round.phase==kBattleAutomaticTurn;
    }
private:
    static bool prepare(HdvArchive &archive,const PreparedEncounter &field,EncounterState &battle) {
        Common::Array<byte> group;
        return !battle.round.allyCount && archive.readRange("NA/SRTN.COMBAT.S",0x113c+uint(field.admission.group)*42,1,group) &&
            QuickBattle::prepare(battle.round.partyCount,battle.round.enemyCount,battle.round.category,(group[0]&128)!=0,(battle.mode.difference&128)!=0,battle.records,battle.quick);
    }
};
} }
#endif
