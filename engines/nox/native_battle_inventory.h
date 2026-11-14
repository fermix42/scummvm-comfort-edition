/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_INVENTORY_H
#define NOX_NATIVE_BATTLE_INVENTORY_H
#include "nox/native_encounter_state.h"
namespace Nox { namespace Native {
// Adapt live battle members to existing inventory transactions. The game keeps
// equipment/owned items; BattleRecords keeps tactical positions and opponents.
class BattleInventory {
public:
    static bool begin(HdvArchive &archive,const NewGame &game,EncounterState &battle,SoundEvents *sounds=nullptr) {
        if (!available(game,battle) || battle.inventory.active()) return false;
        Common::ScopedPtr<NewGame> view(new NewGame(game));
        if (!view->party.applyBattleMembers(battle.records.stats,768,battle.round.partyCount)) return false;
        InventoryWindow inventory;inventory.setSoundEvents(sounds);HgrFrame image=battle.frame;
        if (!inventory.begin(archive,*view,image,true,battle.round.activeParty)) return false;
        battle.inventory=inventory;battle.frame=image;return true;
    }
    static InventoryWindowResult handle(const Common::Event &event,HdvArchive &archive,NewGame &game,EncounterState &battle,SoundEvents *sounds=nullptr) {
        if (!available(game,battle) || !battle.inventory.active() || !battle.inventory.inCombat()) return kInventoryWindowUnsupported;
        Common::ScopedPtr<NewGame> view(new NewGame(game));
        if (!view->party.applyBattleMembers(battle.records.stats,768,battle.round.partyCount)) return kInventoryWindowUnsupported;
        const Party before=view->party;InventoryWindow inventory=battle.inventory;inventory.setSoundEvents(sounds);HgrFrame image=battle.frame;
        const InventoryWindowResult result=inventory.handle(event,archive,*view,image);
        if (result==kInventoryWindowUnsupported) return result;
        bool changed=false;
        for (uint i=0;i<6;++i) changed|=memcmp(before.member(i)->bytes,view->party.member(i)->bytes,128)!=0 || memcmp(before.equipment(i)->bytes,view->party.equipment(i)->bytes,16)!=0;
        // Preserve enemy/summon records and compact facing/status bits. This UI
        // changes equipment-derived member data, not tactical actor ownership.
        for (uint i=0;i<battle.round.partyCount;++i) memcpy(battle.records.stats+i*128,view->party.member(i)->bytes,128);
        // Browsing/cancel must not replace stale world members with a tactical
        // snapshot. A real equipment transaction commits its actual party owner.
        if (!changed) view->party=game.party;
        game.adoptState(*view);battle.inventory=inventory;battle.frame=image;return result;
    }
private:
    static bool available(const NewGame &game,const EncounterState &battle) {
        return game.ready() && battle.formed && battle.presented && battle.round.partyCount==game.party.memberCount() &&
            battle.round.phase==kBattlePartyTurn && !battle.command && !battle.resultPhase && !battle.exit.phase && !battle.quick.partyCount &&
            (battle.mode.selection==0 || battle.mode.selection==2);
    }
};
} }
#endif
