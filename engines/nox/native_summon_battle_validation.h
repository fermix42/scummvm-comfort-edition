/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SUMMON_BATTLE_VALIDATION_H
#define NOX_NATIVE_SUMMON_BATTLE_VALIDATION_H
#include "nox/native_battle_summon.h"
#include "nox/native_battle_records_validation.h"
#include "nox/native_battle_aftermath.h"
namespace Nox {
inline bool validateNativeSummonBattle(HdvArchive &archive,byte spell,Common::String &report) {
    Native::NewGame game(archive);Native::EncounterState battle;Native::BattleAdvancement advancement;
    Native::BattleRound rounds;Native::BattleAftermath aftermath;Native::CastRules casting;Native::ItemCatalog items;
    Native::BattlefieldCatalog fields;Common::Array<byte> terrain;Native::GameplayRandom random;
    report="area battle initialization rejected";
    if (!game.create("Area",0,0) || !advancement.load(archive) || !rounds.load(archive) || !aftermath.load(archive) ||
        !casting.load(archive) || !items.load(archive) || !fields.load(archive) || !fields.project(2,8,terrain) || !random.importState(0x19231923,2)) return false;
    for (uint i=0;i<6;++i) memcpy(battle.records.stats+i*128,game.party.member(i)->bytes,128);
    WRITE_LE_UINT16(battle.records.stats+5,32000);report="area controlled prerequisite allocation rejected";
    for (uint settlement=0;settlement<9;++settlement) {
        Native::BattleAdvancementState proposal;if (!advancement.begin(battle.records,1,proposal)) return false;
        for (uint n=0;!proposal.finished && n<4;++n) {
            if (proposal.phase==1) { if (!advancement.allocate(proposal.ability==2?proposal.remaining:0,proposal)) return false; }
            else if (proposal.phase==2) { if (!advancement.confirm(true,battle.records,1,proposal)) return false; }
            else return false;
        }
        if (!proposal.finished) return false;
    }
    if (battle.records.stats[1]!=10 || !game.party.applyBattleMembers(battle.records.stats,768,1)) return false;
    byte shortcuts[20];memset(shortcuts,255,10);memset(shortcuts+10,0,10);Native::SpellBook book;
    if (!book.importState(shortcuts,20)) return false;bool owned=false;report="area prerequisite book preparation rejected";
    for (uint id=0;id<70;++id) {
        ItemDefinition definition;if (!items.definition(2,byte(id),definition)) return false;
        if ((definition.bytes[12]&31)!=8 || definition.bytes[6]!=spell) continue;
        if (game.inventory.grantOne(items,2,byte(id))!=Native::kInventoryApplied ||
            book.select(items,game.inventory,game.party,byte(id),0,false,false)!=Native::kBookReadied) return false;
        owned=true;break;
    }
    if (!owned) return false;game.book=book;byte count=0,flags=0;
    if (!Native::EncounterFormation::group(archive,124,126,1,random,battle.records,count,flags) || count!=1 || flags ||
        !Native::BattleRoster::rebuild(battle.records,1,1,0)) return false;
    battle.formed=true;battle.round.category=8;CharacterRecord caster;memcpy(caster.bytes,battle.records.stats,128);
    Native::CastPreparation prepared;if (casting.prepare(game.book,caster,0,true,prepared)!=Native::kCastPrepared || prepared.spell!=spell) return false;
    Native::SummonOutcome result;const byte beforeMana=battle.records.stats[4];
    report="summon actual cast/round rejected";
    if (!rounds.summon(prepared,terrain.data(),187,battle.round,random,battle.records,result) || !result.spentMana || !result.placed ||
        battle.records.stats[4]!=byte(beforeMana-prepared.manaCost)) return false;
    Common::MemoryWriteStreamDynamic checkpoint(DisposeAfterUse::YES),rngPacket(DisposeAfterUse::YES);
    if (!battle.save(checkpoint) || !random.save(rngPacket)) return false;
    Native::EncounterState resumed;Native::GameplayRandom resumedRandom;
    Common::MemoryReadStream savedBattle(checkpoint.getData(),checkpoint.size()),savedRng(rngPacket.getData(),rngPacket.size());
    if (!resumed.load(savedBattle) || !resumedRandom.load(savedRng) || !sameBattleRecords(battle.records,resumed.records) ||
        memcmp(battle.round.summonHealth,resumed.round.summonHealth,32)) return false;
    battle=resumed;random=resumedRandom;
    uint turns=0;
    while (battle.round.phase==Native::kBattlePartyTurn && turns++<128) {
        const byte cell=byte(battle.records.compact[1]*17+battle.records.compact[0]);Native::VortexVictim actor;byte kind,range,selected=255;
        if (!battle.records.actor(cell,actor) || !Native::BattleTargets::hand(actor,false,kind,range) ||
            !Native::BattleTargets::nearest(battle.records,cell,range,true,false,selected)) return false;
        if (selected==255) { if (!rounds.pass(terrain.data(),187,battle.round,random,battle.records)) return false; }
        else { Native::PhysicalOutcome physical;if (!rounds.attack(selected,terrain.data(),187,battle.round,random,battle.records,physical)) return false; }
    }
    report="area victory/loot/world return rejected";
    const bool defeated=READ_LE_UINT16(battle.records.stats+770)==0;
    const bool dismissed=spell==18 && battle.records.extended[3]==255 && READ_LE_UINT16(battle.records.stats+770)==12;
    if (battle.round.phase!=Native::kBattleVictory || turns>=128 || (!defeated && !dismissed)) return false;
    Native::WorldActorTurnEvent contact;contact.battle=true;contact.bank=0;contact.slot=4;contact.direction=0;contact.cell=76;
    if (!aftermath.begin(contact,game,battle,random)) return false;
    for (uint n=0;battle.resultPhase==1 && n<32;++n) if (!aftermath.choose(false,false,game,battle,random)) return false;
    if (!aftermath.returnReady(battle) || !aftermath.retireWorldContact(contact,game,battle) ||
        game.party.member(0)->bytes[4]!=byte(beforeMana-prepared.manaCost)) return false;
    Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES),again(DisposeAfterUse::YES);if (!game.save(packet)) return false;
    Native::NewGame cold(archive);Common::MemoryReadStream saved(packet.getData(),packet.size());
    if (!cold.load(saved) || !cold.save(again) || packet.size()!=again.size() || memcmp(packet.getData(),again.getData(),packet.size())) return false;
    uint npcMagic=0;
    if (spell==17 || spell==19 || spell==20) {
        const byte initial=spell==17?60:spell==19?255:69;
        for (uint i=0;i<battle.round.allyCount;++i) if (battle.records.stats[1284+i*32]<initial) ++npcMagic;
    }
    report=Common::String::format("native-summon-battle\tspell=%u\tcreated=%u\tnpc_magic_slots=%u\tphysical_followup_turns=%u\tnative_snapshot_resume=1\tdeath_loot_world_cold=1\tcontrolled_prerequisite_XP=32000\tordinary_cast_UI=0\tfresh_process_summon_resume=0\tcpu_execution=0\n",uint(spell),result.placed,npcMagic,turns);return true;
}
}
#endif
