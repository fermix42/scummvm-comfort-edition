/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SCRIPTED_TREASURE_VALIDATION_H
#define NOX_NATIVE_SCRIPTED_TREASURE_VALIDATION_H
#include "nox/native_battle_world_return.h"
#include "nox/native_battle_loot.h"
#include "nox/native_battle_aftermath.h"
namespace Nox {
// Whole connected owner integration, separate from the 24 original-code
// comparisons below. Setup ends before formation/turns; no result is injected.
inline bool validateNativeScriptedAftermath(HdvArchive &archive,Common::String &report) {
    Native::NewGame game(archive);Native::EncounterState battle;Native::BattleRound rounds;Native::BattleAftermath aftermath;
    Native::BattlefieldCatalog fields;Common::Array<byte> terrain;Native::GameplayRandom random;
    if (!game.create("Award",0,0) || !rounds.load(archive) || !aftermath.load(archive) || !fields.load(archive) ||
        !fields.project(2,8,terrain) || !random.importState(0x19231923,2)) return false;
    for (uint i=0;i<6;++i) memcpy(battle.records.stats+i*128,game.party.member(i)->bytes,128);
    WRITE_LE_UINT16(battle.records.stats+5,99); // Controlled prerequisite, before actual fight.
    if (!game.party.applyBattleMembers(battle.records.stats,768,1)) return false;
    Common::MemoryWriteStreamDynamic world(DisposeAfterUse::YES);if (!game.world.save(world)) return false;
    Common::Array<byte> parent;parent.resize(world.size());memcpy(parent.data(),world.getData(),world.size());
    parent[11+12]=0;bool found=false;
    for (uint at=269;at+1794<=parent.size();at+=1794) if (parent[at]==6 && parent[at+1]==7) {
        const byte definition[]={192,12,0,4,0,2,0,68},actor[]={25,34,157,1,1,126,0,0};
        memcpy(parent.data()+at+2+776,definition,8);memcpy(parent.data()+at+2+1288,actor,8);found=true;
    }
    Common::MemoryReadStream setup(parent.data(),parent.size());
    if (!found || !game.world.load(setup)) return false;
    byte count=0,flags=0;
    if (!Native::EncounterFormation::group(archive,124,126,1,random,battle.records,count,flags) || count!=1 || flags ||
        !Native::BattleRoster::rebuild(battle.records,1,1,0)) return false;
    battle.formed=true;battle.round.category=8;
    uint turns=0;
    while (battle.round.phase==Native::kBattlePartyTurn && turns++<128) {
        const byte cell=byte(battle.records.compact[1]*17+battle.records.compact[0]);
        Native::VortexVictim caster;byte kind,range,target=255;
        if (!battle.records.actor(cell,caster) || !Native::BattleTargets::hand(caster,false,kind,range) ||
            !Native::BattleTargets::nearest(battle.records,cell,range,true,false,target)) return false;
        if (target==255) { if (!rounds.pass(terrain.data(),187,battle.round,random,battle.records)) return false; }
        else { Native::PhysicalOutcome outcome;if (!rounds.attack(target,terrain.data(),187,battle.round,random,battle.records,outcome)) return false; }
    }
    if (battle.round.phase!=Native::kBattleVictory || turns>=128 || READ_LE_UINT16(battle.records.stats+770)) {
        report="scripted aftermath fixture failed to earn its actual Rat defeat";return false;
    }
    Native::WorldActorTurnEvent contact;contact.battle=true;contact.bank=1;contact.slot=8;contact.direction=0;contact.cell=76;
    if (!aftermath.begin(contact,game,battle,random) || battle.resultPhase!=3 || !battle.loot.scripted || battle.loot.started) {
        report="scripted award was not retained across earned advancement boundary";return false;
    }
    Common::MemoryWriteStreamDynamic savedGame(DisposeAfterUse::YES),savedBattle(DisposeAfterUse::YES);
    if (!game.save(savedGame) || !battle.save(savedBattle)) return false;
    Native::NewGame coldGame(archive);Native::EncounterState coldBattle;
    Common::MemoryReadStream gs(savedGame.getData(),savedGame.size()),bs(savedBattle.getData(),savedBattle.size());
    if (!coldGame.load(gs) || !coldBattle.load(bs) || !coldBattle.loot.scripted || coldBattle.loot.kind!=2 || coldBattle.loot.id!=68) return false;
    for (uint steps=0;coldBattle.resultPhase==3 && steps<32;++steps) {
        const Native::BattleAdvancementState &proposal=coldBattle.advancement;
        Common::KeyCode key=Common::KEYCODE_RETURN;
        if (proposal.phase==2) key=Common::KEYCODE_y;
        else if (proposal.phase==1 && proposal.ability==0 && !proposal.digits) key=Common::KEYCODE_1;
        else if (proposal.phase==1 && proposal.ability==0 && proposal.digits==1) key=Common::KEYCODE_2;
        else if (proposal.phase==1 && proposal.ability>0 && !proposal.digits) key=Common::KEYCODE_0;
        if (!aftermath.abilityInput(key,coldGame,coldBattle,random)) return false;
    }
    if (!aftermath.returnReady(coldBattle) || !coldBattle.loot.scripted || !coldBattle.loot.committed ||
        coldGame.inventory.find(2,68)<0 || coldGame.party.member(0)->bytes[1]!=2 || !aftermath.retireWorldContact(contact,coldGame,coldBattle)) return false;
    Common::MemoryWriteStreamDynamic returned(DisposeAfterUse::YES);if (!coldGame.world.save(returned)) return false;
    if (!(returned.getData()[11+12]&4)) return false;
    bool retired=false;
    for (uint at=269;at+1794<=returned.size();at+=1794) if (returned.getData()[at]==6 && returned.getData()[at+1]==7)
        retired=returned.getData()[at+2+1288+3]==255 && returned.getData()[at+2+1288+7]==255;
    if (!retired || aftermath.begin(contact,coldGame,coldBattle,random)) return false;
    report=Common::String::format("native-scripted-aftermath\tactual_battle_turns=%u\tearned_enemy_death=1\tadvancement_award_cold=1\tlevel2_inventory_world_return=1\tparent_retired_after_results=1\tcpu_execution=0\tordinary_admission_UI=0\n",turns);return true;
}
inline bool validateNativeScriptedTreasure(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','S','T') || input.readUint32LE()!=1) return false;
    const uint32 cases=input.readUint32LE();if (!cases || cases>10000 || input.size()!=12+cases*(4+2*1792)) return false;
    Native::BattleLoot loot;if (!loot.load(archive)) return false;
    for (uint32 i=0;i<cases;++i) {
        const byte kind=input.readByte(),id=input.readByte();input.readByte();input.readByte();
        byte before[1792],expected[1792],tables[1792]={0};
        if (input.read(before,1792)!=1792 || input.read(expected,1792)!=1792) return false;
        const byte definition[]={192,12,0,4,0,kind,0,id};memcpy(tables+776,definition,8);tables[1293]=126;
        byte fields[256];memcpy(fields,before,256);fields[12]=0;Native::BattleTreasureAward award;
        if (!Native::BattleWorldReturn::effects(tables,1792,fields,256,1,8,false,&award) || award.marker!=4 ||
            award.kind!=kind || award.id!=id || memcmp(fields,before,256)) return false;
        Native::BattleLootState state;state.scripted=true;state.kind=award.kind;state.id=award.id;
        // The retained scripted award survives the same twenty-byte result
        // continuation while advancement is still pending and loot not begun.
        Common::MemoryWriteStreamDynamic pending(DisposeAfterUse::YES);if (!state.save(pending)) return false;
        Common::MemoryReadStream saved(pending.getData(),pending.size());Native::BattleLootState restored;
        if (!restored.load(saved) || !restored.scripted || restored.started || restored.kind!=kind || restored.id!=id) return false;
        Native::Inventory inventory;if (!inventory.importRecords(before+256,1536)) return false;
        Native::BattleRecords battle;Native::GameplayRandom random;if (!random.importState(0x19231923,2)) return false;
        const Native::GameplayRandom initial=random;
        if (!loot.begin(1,1,50,75,restored) || !loot.advance(battle,inventory,random,restored,fields,256)) return false;
        uint16 food=50,gold=75;byte actual[1792];memcpy(actual,fields,256);
        if (!loot.commit(food,gold,restored) || food!=50 || gold!=75 || !inventory.exportRecords(actual+256,1536) ||
            memcmp(actual,expected,1792) || !restored.finished || restored.pending || restored.generated!=1) {
            report=Common::String::format("scripted treasure case%u fields/inventory/result differs",i);return false;
        }
        Common::MemoryWriteStreamDynamic rngBefore(DisposeAfterUse::YES),rngAfter(DisposeAfterUse::YES),settled(DisposeAfterUse::YES);
        if (!initial.save(rngBefore) || !random.save(rngAfter) || rngBefore.size()!=rngAfter.size() || memcmp(rngBefore.getData(),rngAfter.getData(),rngBefore.size()) || !restored.save(settled)) return false;
        Common::MemoryReadStream cold(settled.getData(),settled.size());Native::BattleLootState done;
        if (!done.load(cold) || !done.scripted || !done.committed || loot.commit(food,gold,done) || loot.advance(battle,inventory,random,done,fields,256)) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    Common::String linked;if (!validateNativeScriptedAftermath(archive,linked)) { report=linked;return false; }
    report=Common::String::format("native-scripted-treasure\tcases=%u\tquest_loot_link=1\tpending_and_settled_cold=%u\tcapacity_quantity_fields=1\trng_unchanged=1\tcpu_execution=0\twhole_battle=0\n",cases,cases*2)+linked;return true;
}
}
#endif
