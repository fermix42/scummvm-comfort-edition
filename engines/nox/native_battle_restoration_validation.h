/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_RESTORATION_VALIDATION_H
#define NOX_NATIVE_BATTLE_RESTORATION_VALIDATION_H
#include "nox/native_battle_restoration.h"
#include "nox/native_battle_records_validation.h"
#include "nox/native_battle_aftermath.h"
namespace Nox {
inline bool validateNativeRestorationBattle(HdvArchive &archive,Common::String &report) {
    report="restoration battle initialization rejected";
    Native::NewGame game(archive);Native::EncounterState battle;Native::BattleAdvancement advancement;
    Native::BattleAdvancementState proposal;Native::BattleRound rounds;Native::BattleAftermath aftermath;
    Native::CastRules casting;Native::ItemCatalog items;Native::BattlefieldCatalog fields;
    Common::Array<byte> terrain;Native::GameplayRandom random;
    if (!game.create("Restore",0,0) || !advancement.load(archive) || !rounds.load(archive) || !aftermath.load(archive) ||
        !casting.load(archive) || !items.load(archive) || !fields.load(archive) || !fields.project(2,8,terrain) || !random.importState(0x19231923,2)) return false;
    for (uint i=0;i<6;++i) memcpy(battle.records.stats+i*128,game.party.member(i)->bytes,128);
    // Controlled prerequisite XP, then the real qualified allocation owner
    // derives consistent level/HP/INT/MP rather than inventing those fields.
    WRITE_LE_UINT16(battle.records.stats+5,32000);
    report="restoration controlled allocation prerequisite rejected";
    // Original results award at most one level per member per settlement.
    // Repeat that actual operation during declared checkpoint preparation.
    for (uint settlement=0;settlement<9;++settlement) {
        proposal=Native::BattleAdvancementState();
        if (!advancement.begin(battle.records,1,proposal)) return false;
        for (uint n=0;!proposal.finished && n<4;++n) {
            if (proposal.phase==1) { if (!advancement.allocate(proposal.ability==2?proposal.remaining:0,proposal)) return false; }
            else if (proposal.phase==2) { if (!advancement.confirm(true,battle.records,1,proposal)) return false; }
            else return false;
        }
        if (!proposal.finished) return false;
    }
    if (!proposal.finished || battle.records.stats[1]!=10 || battle.records.stats[4]<83 || !game.party.applyBattleMembers(battle.records.stats,768,1)) return false;
    report="restoration controlled book preparation rejected";
    const byte spells[]={32,48,49,50,51,52};byte shortcuts[20];memset(shortcuts,255,10);memset(shortcuts+10,0,10);
    Native::SpellBook book;if (!book.importState(shortcuts,20)) return false;
    for (uint slot=0;slot<6;++slot) {
        bool found=false;
        for (uint id=0;id<70;++id) {
            ItemDefinition definition;if (!items.definition(2,byte(id),definition)) return false;
            if ((definition.bytes[12]&31)!=8 || definition.bytes[6]!=spells[slot]) continue;
            if (game.inventory.grantOne(items,2,byte(id))!=Native::kInventoryApplied ||
                book.select(items,game.inventory,game.party,byte(id),byte(slot),false,false)!=Native::kBookReadied) return false;
            found=true;break;
        }
        if (!found) return false;
    }
    report="restoration battle formation rejected";
    game.book=book;byte count=0,flags=0,objects[187];memset(objects,255,187);
    if (!Native::EncounterFormation::group(archive,124,126,1,random,battle.records,count,flags) || count!=1 || flags ||
        !Native::BattleRoster::rebuild(battle.records,1,1,0)) return false;
    battle.formed=true;battle.round.category=8;
    Native::RestorationOutcome outcome;const byte beforeMana=battle.records.stats[4];
    const uint32 beforeSeed=random.seed();const uint16 beforeAttempts=random.generationAttempts();
    if (!rounds.restore(50,2,0,true,terrain.data(),187,objects,187,255,battle.armorBackup,6,battle.round,random,battle.records,outcome) ||
        !outcome.cancelled || battle.records.stats[4]!=beforeMana || battle.round.actions || random.seed()!=beforeSeed || random.generationAttempts()!=beforeAttempts) return false;
    report="restoration cast and NPC retirement rejected";uint costs=0;
    for (uint slot=0;slot<6;++slot) {
        CharacterRecord caster;memcpy(caster.bytes,battle.records.stats,128);Native::CastPreparation prepared;
        if (casting.prepare(game.book,caster,byte(slot),true,prepared)!=Native::kCastPrepared || prepared.spell!=spells[slot] ||
            !rounds.restore(prepared.spell,prepared.manaCost,0,false,terrain.data(),187,objects,187,255,battle.armorBackup,6,
                battle.round,random,battle.records,outcome) || outcome.cancelled || !outcome.spentMana) return false;
        costs+=prepared.manaCost;
        if (prepared.spell==49 && outcome.effect!=Native::kRestorationNotDead) return false;
    }
    if (costs!=83 || battle.records.stats[4]!=byte(beforeMana-costs) || battle.round.phase!=Native::kBattlePartyTurn) return false;
    report="restoration physical continuation rejected";uint turns=0;
    while (battle.round.phase==Native::kBattlePartyTurn && turns++<128) {
        const byte cell=byte(battle.records.compact[1]*17+battle.records.compact[0]);Native::VortexVictim caster;
        byte kind,range,target=255;
        if (!battle.records.actor(cell,caster) || !Native::BattleTargets::hand(caster,false,kind,range) ||
            !Native::BattleTargets::nearest(battle.records,cell,range,true,false,target)) return false;
        if (target==255) { if (!rounds.pass(terrain.data(),187,battle.round,random,battle.records)) return false; }
        else { Native::PhysicalOutcome physical;if (!rounds.attack(target,terrain.data(),187,battle.round,random,battle.records,physical)) return false; }
    }
    if (battle.round.phase!=Native::kBattleVictory || turns>=128 || READ_LE_UINT16(battle.records.stats+770)) return false;
    Native::WorldActorTurnEvent contact;contact.battle=true;contact.bank=0;contact.slot=4;contact.direction=0;contact.cell=76;
    report="restoration aftermath and loot rejected";
    if (!aftermath.begin(contact,game,battle,random)) return false;
    for (uint n=0;battle.resultPhase==1 && n<32;++n) if (!aftermath.choose(false,false,game,battle,random)) return false;
    if (!aftermath.returnReady(battle) || !aftermath.retireWorldContact(contact,game,battle) ||
        game.party.member(0)->bytes[4]!=byte(beforeMana-costs)) return false;
    report="restoration returned game cold serialization rejected";
    Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES),again(DisposeAfterUse::YES);
    if (!game.save(packet)) return false;Native::NewGame cold(archive);Common::MemoryReadStream saved(packet.getData(),packet.size());
    if (!cold.load(saved) || !cold.save(again) || packet.size()!=again.size() || memcmp(packet.getData(),again.getData(),packet.size())) return false;
    report=Common::String::format("native-restoration-battle\tstandard_spell_slots=6\tcancel_free=1\trefusal_spends=1\tmana_cost=83\tactual_physical_followup_turns=%u\tdeath_loot_parent_return_cold=1\tcontrolled_prerequisite_XP=32000\tordinary_cast_UI=0\tcpu_execution=0\n",turns);return true;
}
inline bool validateNativeBattleRestoration(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','R','S') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (!count || count>10000 || input.size()!=12+count*6262) return false;
    Native::BattleRestoration rules;if (!rules.load(archive)) return false;
    uint cancelledCount=0,refused=0,moved=0;
    for (uint32 i=0;i<count;++i) {
        const byte spell=input.readByte(),cost=input.readByte(),caster=input.readByte(),target=input.readByte(),cancelled=input.readByte(),
            party=input.readByte(),category=input.readByte(),engagement=input.readByte();
        byte terrain[187],objects[187],backups[6],expectedBackup[6];Native::BattleRecords battle,expected;
        if (input.read(terrain,187)!=187 || input.read(objects,187)!=187 || input.read(backups,6)!=6 || !readBattlePayload(input,battle) ||
            input.read(expectedBackup,6)!=6 || !readBattlePayload(input,expected)) return false;
        Native::RestorationOutcome result;
        if (cancelled>1 || !rules.resolve(spell,cost,caster,target,cancelled!=0,party,category,terrain,187,objects,187,
                engagement,backups,6,battle,result)) { report=Common::String::format("restoration case%u spell%u rejected",i,spell);return false; }
        if (!sameBattleRecords(battle,expected) || memcmp(backups,expectedBackup,6)) {
            uint stat=0;while (stat<2048 && battle.stats[stat]==expected.stats[stat]) ++stat;
            uint actor=0;while (actor<256 && battle.compact[actor]==expected.compact[actor]) ++actor;
            report=Common::String::format("restoration case%u spell%u differs stat%u actor%u MP%u/%u",i,spell,stat,actor,battle.stats[caster*128+4],expected.stats[caster*128+4]);return false;
        }
        cancelledCount+=result.cancelled?1:0;refused+=result.effect!=Native::kRestorationApplied?1:0;moved+=result.moved;
        if (result.spentMana==(cancelled!=0)) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    Common::String linked;if (!validateNativeRestorationBattle(archive,linked)) { report=linked;return false; }
    report=Common::String::format("native-battle-restoration\tcases=%u\tspell_slots=6\tfamilies=5\tparty_sizes=3\tparent_types=8\tcancelled=%u\trefusals=%u\tmoves=%u\tfull_records_backup_MP_occupancy=1\tcpu_execution=0\tcast_UI_whole_battle=0\n",count,cancelledCount,refused,moved)+linked;return true;
}
}
#endif
