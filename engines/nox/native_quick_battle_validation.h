/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_QUICK_BATTLE_VALIDATION_H
#define NOX_NATIVE_QUICK_BATTLE_VALIDATION_H
#include "nox/native_quick_battle.h"
#include "nox/native_quick_display.h"
#include "nox/native_battle_records_validation.h"
namespace Nox {
inline bool validateNativeQuickBattle(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','Q','B','T')) return false;
    const uint32 version=input.readUint32LE();if (version<1 || version>2) return false;
    const uint32 count=input.readUint32LE();if (!count || count>1000) return false;
    Native::QuickBattle rules;if (!rules.load(archive)) return false;uint32 actions=0,shown=0,magic=0,offHand=0;
    for (uint32 i=0;i<count;++i) {
        const byte party=input.readByte(),enemies=input.readByte(),category=input.readByte();
        const byte expectedPhase=version>=2?input.readByte():1;
        const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
        const uint32 expectedActions=input.readUint32LE();
        Native::BattleRecords battle,prepared,resolved,restored;byte metadata[26];
        if (!readBattlePayload(input,battle) || !readBattlePayload(input,prepared) || !readBattlePayload(input,resolved) ||
            !readBattlePayload(input,restored) || input.read(metadata,26)!=26) return false;
        const uint32 afterSeed=input.readUint32LE();const byte afterTail=input.readByte(),afterLast=input.readByte();
        const uint16 afterAttempts=input.readUint16LE(),credit=input.readUint16LE();
        resolved.credit=restored.credit=credit;
        if (i==0) {
            // Controlled pre-operation two-hand variant: both admitted fists,
            // enough target HP to reach the second hand. Not original oracle.
            Native::BattleRecords two= battle;two.stats[21]=0;two.stats[10]=two.stats[9];two.stats[64]=two.stats[65]=0;
            WRITE_LE_UINT16(two.stats+770,3000);Native::QuickBattleState both;Native::GameplayRandom rolls;
            if (!rolls.importState(seed,tail,last,attempts) || !Native::QuickBattle::prepare(party,enemies,category,false,false,two,both)) return false;
            Common::Array<Native::QuickBattleAction> hands;if (!rules.step(rolls,two,both,false,&hands) || hands.size()!=2 || hands[0].magic || hands[0].offHand || !hands[1].offHand) {report="Quick two-hand display capture failed";return false;}
            Native::QuickBattleDisplay between;between.paused=true;if (between.ready(100,true)) return false;
            const uint32 expectedCycles[]={18,41131,82219,123307};
            for (byte rate=0;rate<4;++rate) {
                Native::QuickBattleDisplay display;display.speed=rate;display.queue(hands);display.show(100);display.lines[3]="1234567890";display.beginText();
                if (display.characters!=12 || display.characterCycles(rate)!=expectedCycles[rate]) return false;
                const uint duration=uint((uint64(12)*expectedCycles[rate]+1022)/1023);
                if (display.ready(100+duration-1,true) || !display.pending() || !display.ready(100+duration,true)) return false;
                display.show(2000);display.lines[3]="1234567890";display.beginText();display.control('8',2000,true);
                if (display.ready(20000,true) || display.remaining!=12) return false;
                display.control('8',20000,true);if (!display.ready(20000+duration,true)) return false;
                display.control('+',21000,true);if (display.speed!=(rate?rate-1:0)) return false;
                display.control('-',21000,true);if (display.speed!=(rate?rate:1)) return false;
            }
        }
        Native::QuickBattleState state;Native::GameplayRandom rng;
        if (!rng.importState(seed,tail,last,attempts) || !Native::QuickBattle::prepare(party,enemies,category,false,false,battle,state) || !sameBattleRecords(battle,prepared)) {
            report=Common::String::format("Quick case%u preparation differs",i);return false;
        }
        if (memcmp(state.enemyMetadata,metadata,enemies*2) || memcmp(state.partyLevels,metadata+20,party)) return false;
        uint steps=0;while (!state.phase) {
            Native::BattleRecords plain=battle;Native::QuickBattleState plainState=state;Native::GameplayRandom plainRandom=rng;Common::Array<Native::QuickBattleAction> events;
            if (!rules.step(plainRandom,plain,plainState) || !rules.step(rng,battle,state,false,&events) || !sameBattleRecords(plain,battle) || plain.credit!=battle.credit ||
                plainRandom.seed()!=rng.seed() || plainRandom.tail()!=rng.tail() || plainRandom.last()!=rng.last() || plainRandom.generationAttempts()!=rng.generationAttempts() ||
                plainState.actions!=state.actions || plainState.phase!=state.phase || events.size()>2 || ++steps>10000) { report=Common::String::format("Quick case%u step%u rejected",i,steps);return false; }
            for (uint event=0;event<events.size();++event) {
                if (events[event].actor.empty() || events[event].target.empty() || (events[event].suppressed && events[event].damage)) return false;
                ++shown;magic+=events[event].magic?1:0;offHand+=events[event].offHand?1:0;
            }
            Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES),again(DisposeAfterUse::YES);
            if (!state.save(saved)) return false;Native::QuickBattleState loaded;Common::MemoryReadStream read(saved.getData(),saved.size());
            if (!loaded.load(read) || !loaded.save(again) || saved.size()!=again.size() || memcmp(saved.getData(),again.getData(),saved.size())) return false;
            state=loaded;
        }
        if (state.phase!=expectedPhase || state.actions!=expectedActions || !sameBattleRecords(battle,resolved) || battle.credit!=credit ||
                rng.seed()!=afterSeed || rng.tail()!=afterTail || rng.last()!=afterLast || rng.generationAttempts()!=afterAttempts) {
            uint stat=0;while (stat<2048 && battle.stats[stat]==resolved.stats[stat]) ++stat;
            report=Common::String::format("Quick case%u resolved differs stat%u actions%u/%u credit%u/%u RNG%08x/%08x",i,stat,state.actions,expectedActions,uint(battle.credit),uint(credit),rng.seed(),afterSeed);return false;
        }
        if ((state.phase==1 && !Native::QuickBattle::restore(battle,state)) || !sameBattleRecords(battle,restored)) {
            report=Common::String::format("Quick case%u restoration differs",i);return false;
        }
        actions+=state.actions;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-quick-battle\tcases=%u\tactions=%u\tfull_prepare_battle_restore_records_RNG=1\tevents=%u\tmagic=%u\toff_hand=%u\tobserver_records_RNG_equal=1\ttwo_hand_queue_source_rates_pause_bounds=1\tordinary_mode_UI=0\tresults_world=0\tcpu_execution=0\n",count,actions,shown,magic,offHand);return true;
}
}
#endif
