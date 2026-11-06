/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_DIRECTED_SPELL_VALIDATION_H
#define NOX_NATIVE_BATTLE_DIRECTED_SPELL_VALIDATION_H
#include "nox/native_battle_directed_spell.h"
#include "nox/native_battle_records_validation.h"
#include "nox/native_battle_aftermath.h"
namespace Nox {
inline bool validateNativeDirectedBattle(HdvArchive &archive,byte spell,Common::String &report) {
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
    Native::DirectedSpellSelection selection;Native::DirectedSpellOutcome result;const byte target=byte(battle.records.extended[1]*17+battle.records.extended[0]),beforeMana=battle.records.stats[4];
    const uint32 beforeSeed=random.seed();const uint16 beforeAttempts=random.generationAttempts();selection.cell=target;selection.direction=Native::kNorth;selection.cancelled=true;report="directed cancellation rejected";
    if (spell!=12 && spell!=21 && spell!=23 && spell!=25 && (!rounds.directed(prepared,selection,terrain.data(),187,battle.round,random,battle.records,result) || !result.cancelled ||
        battle.round.actions || battle.records.stats[4]!=beforeMana || random.seed()!=beforeSeed || random.generationAttempts()!=beforeAttempts)) return false;
    selection.cancelled=false;report="directed actual cast/round rejected";
    if (!rounds.directed(prepared,selection,terrain.data(),187,battle.round,random,battle.records,result) || result.cancelled ||
        !result.spentMana || result.targets.empty() || battle.records.stats[4]!=byte(beforeMana-prepared.manaCost)) return false;
    uint turns=0;
    while (battle.round.phase==Native::kBattlePartyTurn && turns++<128) {
        const byte cell=byte(battle.records.compact[1]*17+battle.records.compact[0]);Native::VortexVictim actor;byte kind,range,selected=255;
        if (!battle.records.actor(cell,actor) || !Native::BattleTargets::hand(actor,false,kind,range) ||
            !Native::BattleTargets::nearest(battle.records,cell,range,true,false,selected)) return false;
        if (selected==255) { if (!rounds.pass(terrain.data(),187,battle.round,random,battle.records)) return false; }
        else { Native::PhysicalOutcome physical;if (!rounds.attack(selected,terrain.data(),187,battle.round,random,battle.records,physical)) return false; }
    }
    report="area victory/loot/world return rejected";
    if (battle.round.phase!=Native::kBattleVictory || turns>=128 || READ_LE_UINT16(battle.records.stats+770)) return false;
    Native::WorldActorTurnEvent contact;contact.battle=true;contact.bank=0;contact.slot=4;contact.direction=0;contact.cell=76;
    if (!aftermath.begin(contact,game,battle,random)) return false;
    for (uint n=0;battle.resultPhase==1 && n<32;++n) if (!aftermath.choose(false,false,game,battle,random)) return false;
    if (!aftermath.returnReady(battle) || !aftermath.retireWorldContact(contact,game,battle) ||
        game.party.member(0)->bytes[4]!=byte(beforeMana-prepared.manaCost)) return false;
    Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES),again(DisposeAfterUse::YES);if (!game.save(packet)) return false;
    Native::NewGame cold(archive);Common::MemoryReadStream saved(packet.getData(),packet.size());
    if (!cold.load(saved) || !cold.save(again) || packet.size()!=again.size() || memcmp(packet.getData(),again.getData(),packet.size())) return false;
    report=Common::String::format("native-directed-battle\tspell=%u\tordinary_cancel_free_vortex_spent=1\tactual_cast_targets=%u\tcast_kills=%u\tphysical_followup_turns=%u\tdeath_loot_world_cold=1\tcontrolled_prerequisite_XP=32000\tordinary_cast_UI=0\tcpu_execution=0\n",uint(spell),result.targets.size(),result.killed,turns);return true;
}
inline bool validateNativeBattleDirectedSpell(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','D','S') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (!count || count>10000 || input.size()!=12+count*5934) return false;
    Native::BattleDirectedSpell rules;if (!rules.load(archive)) return false;uint kills=0,targets=0,cancels=0,refusals=0;
    for (uint32 i=0;i<count;++i) {
        Native::CastPreparation prepared;prepared.spell=input.readByte();prepared.manaCost=input.readByte();prepared.basePower=input.readByte();prepared.levelThreshold=input.readByte();
        const byte caster=input.readByte();Native::DirectedSpellSelection selection;selection.cell=input.readByte();selection.cancelled=input.readByte()!=0;selection.direction=input.readByte();
        const byte category=input.readByte(),protection=input.readByte();const uint32 seed=input.readUint32LE();
        const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE(),pool=input.readUint16LE();
        Native::BattleRecords battle,expected;if (!readBattlePayload(input,battle)) return false;battle.credit=pool;
        const uint32 afterSeed=input.readUint32LE();const byte afterTail=input.readByte(),afterLast=input.readByte();
        const uint16 afterAttempts=input.readUint16LE();expected.credit=input.readUint16LE();
        const byte targetCount=input.readByte(),killCount=input.readByte(),cancelled=input.readByte(),refused=input.readByte();byte expectedTargets[32];
        if (input.read(expectedTargets,32)!=32 || !readBattlePayload(input,expected) || protection>1) return false;
        Native::GameplayRandom random;Native::DirectedSpellOutcome result;
        if (!random.importState(seed,tail,last,attempts) || !rules.resolve(prepared,caster,selection,category,protection!=0,random,battle,result)) {
            report=Common::String::format("directed case%u spell%u rejected",i,uint(prepared.spell));return false;
        }
        bool sameTargets=result.targets.size()==targetCount;
        for (uint n=0;sameTargets && n<targetCount;++n) sameTargets=result.targets[n]==expectedTargets[n];
        if (!sameBattleRecords(battle,expected) || !sameTargets || result.killed!=killCount || result.cancelled!=(cancelled!=0) ||
            result.directionRefused!=(refused!=0) || random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
            uint stat=0;while (stat<2048 && battle.stats[stat]==expected.stats[stat]) ++stat;
            report=Common::String::format("directed case%u spell%u differs stat%u targets%u/%u kills%u/%u rng%08x/%08x credit%u/%u",i,uint(prepared.spell),stat,
                result.targets.size(),uint(targetCount),result.killed,uint(killCount),random.seed(),afterSeed,battle.credit,expected.credit);return false;
        }
        kills+=result.killed;targets+=result.targets.size();cancels+=result.cancelled?1:0;refusals+=result.directionRefused?1:0;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-directed-spell\tcases=%u\tspell_slots=16\tfull_dispatch_mana_targets_impacts=1\ttargets=%u\tkills=%u\tcancels=%u\tvortex_spent_refusals=%u\tcpu_execution=0\tordinary_cast_UI=0\n",count,targets,kills,cancels,refusals);
    const byte spells[]={0,1,5,6,7,8,9,10,11,12,13,15,21,23,24,25};for (uint i=0;i<16;++i) {
        Common::String linked;if (!validateNativeDirectedBattle(archive,spells[i],linked)) { report=Common::String::format("directed spell%u integration: %s",uint(spells[i]),linked.c_str());return false; }
        report+=linked;
    }
    return true;
}
}
#endif
