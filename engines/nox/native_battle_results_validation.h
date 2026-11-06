/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_RESULTS_VALIDATION_H
#define NOX_NATIVE_BATTLE_RESULTS_VALIDATION_H
#include "nox/native_battle_loot.h"
#include "nox/native_battle_rewards.h"
#include "nox/native_battle_advancement.h"
namespace Nox {
inline bool validateNativeBattleAdvancement(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','A','D') || input.readUint32LE()!=1 || input.readUint32LE()!=135 || input.size()!=12+135*257) return false;
    Native::BattleAdvancement rules;if (!rules.load(archive)) return false;
    for (uint i=0;i<135;++i) {
        const byte chosen=input.readByte();Native::BattleRecords battle;byte expected[128];
        if (chosen>2 || input.read(battle.stats,128)!=128 || input.read(expected,128)!=128) return false;
        byte original[128];memcpy(original,battle.stats,128);Native::BattleAdvancementState state;
        if (!rules.begin(battle,1,state) || state.phase!=1 || state.finished) return false;
        const Native::BattleAdvancementState before=state;
        if (!rules.allocate(99,state) || memcmp(state.proposal,before.proposal,128) || state.remaining!=before.remaining || state.ability!=before.ability) return false;
        for (uint attempt=0;attempt<2;++attempt) {
            while (state.phase==1) if (!rules.allocate(state.ability==chosen?state.remaining:0,state)) return false;
            Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);
            if (state.phase!=2 || !state.save(saved) || saved.size()!=144) return false;
            Common::MemoryReadStream restored(saved.getData(),saved.size());Native::BattleAdvancementState pending;
            if (!pending.load(restored)) return false;state=pending;
            if (!rules.confirm(attempt!=0,battle,1,state)) return false;
            if (!attempt && memcmp(battle.stats,original,128)) return false;
        }
        if (!state.finished || state.phase || memcmp(battle.stats,expected,128)) {
            uint field=0;while (field<128 && battle.stats[field]==expected[field]) ++field;
            report=Common::String::format("Native advancement differs case%u field%u native%u original%u",i,field,field<128?battle.stats[field]:0,field<128?expected[field]:0);return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battle-advancement\tcases=135\trecord_bytes=17280\tover_budget_refusal=1\tnegative_restart=1\tconfirmed_growth=1\tproposal_roundtrip=1\tcpu_execution=0\n";return true;
}
inline bool validateNativeBattleRewards(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','R','W') || input.readUint32LE()!=1 || input.readUint32LE()!=192 || input.size()!=12+192*4625) return false;
    Native::BattleRewardRules rules;if (!rules.load(archive)) return false;
    for (uint i=0;i<192;++i) {
        const byte party=input.readByte(),category=input.readByte(),fled=input.readByte();uint16 gold=input.readUint16LE();Native::BattleRecords battle;
        battle.credit=input.readUint16LE();byte backup[6];
        if (input.read(backup,6)!=6 || input.read(battle.stats,2048)!=2048 || input.read(battle.compact,256)!=256 || input.read(battle.extended,256)!=256) return false;
        const Native::BattleRecords before=battle;const uint16 expectedGold=input.readUint16LE(),expectedPool=input.readUint16LE();byte expected[2048];
        if (input.read(expected,2048)!=2048) return false;Native::BattleRewardOutcome outcome;
        if (!rules.distribute(battle,party,category,fled!=0,gold,backup,6,outcome) || memcmp(battle.stats,expected,2048) ||
            gold!=expectedGold || battle.credit!=expectedPool || memcmp(battle.compact,before.compact,256) || memcmp(battle.extended,before.extended,256)) {
            report=Common::String::format("Native battle reward differs case%u gold=%u/%u pool=%u/%u",i,gold,expectedGold,battle.credit,expectedPool);return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battle-rewards\tcases=192\trecord_bytes=393216\tparty_sizes=6\tflee_half_gold=1\tceil_minimum_XP=1\tcpu_execution=0\n";return true;
}
inline bool validateNativeBattleSkills(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','S','K') || input.readUint32LE()!=1 || input.readUint32LE()!=64 || input.size()!=12+64*256) return false;
    Native::BattleAdvancement rules;if (!rules.load(archive)) return false;
    for (uint i=0;i<64;++i) {
        Native::BattleRecords battle;byte expected[128];
        if (input.read(battle.stats,128)!=128 || input.read(expected,128)!=128 || !rules.settleSkills(battle,1) || memcmp(battle.stats,expected,128)) {
            report=Common::String::format("Native postbattle skill settlement differs case%u",i);return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battle-skills\tcases=64\trecord_bytes=8192\tgroups=5\tdead_skip=1\tcpu_execution=0\tlevel_allocation=0\n";return true;
}
inline bool validateNativeBattleLoot(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','L','T') || input.readUint32LE()!=1 || input.readUint32LE()!=96 || input.size()!=12+96*5148) return false;
    Native::BattleLoot rules;if (!rules.load(archive)) return false;
    uint generated=0;
    for (uint i=0;i<96;++i) {
        const byte party=input.readByte(),enemies=input.readByte(),all=input.readByte();
        const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
        uint16 food=input.readUint16LE(),gold=input.readUint16LE();Native::BattleRecords battle;byte inventory[1536];
        if (input.read(battle.stats,2048)!=2048 || input.read(inventory,1536)!=1536) return false;
        const uint16 expectedFood=input.readUint16LE(),expectedGold=input.readUint16LE();const uint32 expectedSeed=input.readUint32LE();
        const byte expectedTail=input.readByte(),expectedLast=input.readByte();const uint16 expectedAttempts=input.readUint16LE();byte expectedInventory[1536];
        if (input.read(expectedInventory,1536)!=1536) return false;const byte expectedGenerated=input.readByte();
        Native::Inventory items;Native::GameplayRandom random;Native::BattleLootState state;
        if (!items.importRecords(inventory,1536) || !random.importState(seed,tail,last,attempts) || !rules.begin(enemies,party,food,gold,state) || !rules.advance(battle,items,random,state)) return false;
        while (state.pending) if (!rules.choose(all!=0,all!=0,battle,items,random,state)) return false;
        if (!state.finished || !rules.commit(food,gold,state) || !items.exportRecords(inventory,1536)) return false;
        if (food!=expectedFood || gold!=expectedGold || memcmp(inventory,expectedInventory,1536) || state.generated!=expectedGenerated ||
            random.seed()!=expectedSeed || random.tail()!=expectedTail || random.last()!=expectedLast || random.generationAttempts()!=expectedAttempts) {
            uint offset=0;while (offset<1536 && inventory[offset]==expectedInventory[offset]) ++offset;
            report=Common::String::format("Native battle loot differs case%u food=%u/%u gold=%u/%u inventory=%u generated=%u/%u rng=%08x/%08x last=%u/%u",i,food,expectedFood,gold,expectedGold,offset,uint(state.generated),uint(expectedGenerated),random.seed(),expectedSeed,random.last(),expectedLast);return false;
        }
        generated+=state.generated;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-loot\tcases=96\tinventory_bytes=147456\titems=%u\tleave_and_take_all=1\tcurrency_and_rng=1\tcpu_execution=0\tcontrolled_setups=96\n",generated);return true;
}
}
#endif
