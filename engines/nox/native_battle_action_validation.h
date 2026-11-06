/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_ACTION_VALIDATION_H
#define NOX_NATIVE_BATTLE_ACTION_VALIDATION_H
#include "nox/native_battle_attack.h"
#include "nox/native_battle_physical.h"
#include "nox/native_battle_reticle.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeBattleReticle(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','R','T') || input.readUint32LE()!=1 || input.readUint32LE()!=20 || input.size()!=12+20*16386) return false;
    Native::BattleReticle rules;if (!rules.load(archive)) return false;
    for (uint i=0;i<20;++i) {
        const byte cell=input.readByte(),phase=input.readByte();byte before[8192],expected[8192];Native::HgrFrame image;
        if (input.read(before,8192)!=8192 || input.read(expected,8192)!=8192 || !image.importImage(before,8192) || !rules.paint(cell,phase,image)) return false;
        bool visible[8192]={false};
        for (uint y=0;y<192;++y) for (uint x=0;x<40;++x) {
            const uint offset=Native::HgrFrame::rowOffset(y)+x;visible[offset]=true;
            if (image.data()[offset]!=expected[offset]) { report=Common::String::format("Native target reticle differs case%u cell%u phase%u x%u y%u",i,cell,phase,x,y);return false; }
        }
        for (uint offset=0;offset<8192;++offset) if (!visible[offset] && image.data()[offset]!=before[offset]) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battle-reticle\tcases=20\tvisible_HGR_bytes=153600\tnative_padding_bytes_unchanged=10240\tclassified_source_scratch=1\tauthored_frames=4\tcorners=1\tcpu_execution=0\n";return true;
}
inline bool validateNativeBattleTargets(HdvArchive &,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','T','G') || input.readUint32LE()!=1 || input.readUint32LE()!=320 || input.size()!=12+320*2952) return false;
    for (uint i=0;i<320;++i) {
        const byte caster=input.readByte(),range=input.readByte(),pvp=input.readByte();byte memoryKind=input.readByte(),memoryIndex=input.readByte();
        const byte count=input.readByte();byte keys[8];Native::BattleRecords battle;
        if (pvp>1 || count>8 || input.read(keys,8)!=8 || input.read(battle.stats,2048)!=2048 || input.read(battle.compact,256)!=256 ||
            input.read(battle.extended,256)!=256 || input.read(battle.indices,187)!=187 || input.read(battle.kinds,187)!=187) return false;
        const byte expectedResult=input.readByte(),expectedTarget=input.readByte(),expectedKind=input.readByte(),expectedIndex=input.readByte();
        byte target=255,result=0;if (!Native::BattleTargets::initial(battle,caster,range,pvp!=0,memoryKind,memoryIndex,target)) return false;
        if (target==255) result=1;
        else for (uint key=0;key<count;++key) {
            if (keys[key]==0x9b) { result=2;target=255;break; }
            if (keys[key]==0x89) { result=3;target=255;break; }
            if (keys[key]==0x8d) { memoryKind=battle.kinds[target];memoryIndex=battle.indices[target];break; }
            const byte direction=keys[key]==0x8b?0:keys[key]==0x8a?1:keys[key]==0x95?2:keys[key]==0x88?3:255;
            if (!Native::BattleTargets::cycle(battle,caster,range,pvp!=0,direction,target)) return false;
        }
        if (result!=expectedResult || target!=expectedTarget || memoryKind!=expectedKind || memoryIndex!=expectedIndex) {
            report=Common::String::format("Native target UI differs case%u result%u/%u target%u/%u memory%u,%u/%u,%u",i,result,expectedResult,target,expectedTarget,memoryKind,memoryIndex,expectedKind,expectedIndex);return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battle-targets\tcases=320\troster_bytes=938880\tlarge_footprints=1\trectangular_ranges=1\tarrow_cycles=1\tremembered_targets=1\tPvP_fallback=1\tcpu_execution=0\n";return true;
}
inline bool validateNativeBattleAction(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','A','C') || input.readUint32LE()!=1 || input.readUint32LE()!=88 || input.size()!=12+88*5893) return false;
    Native::BattlePhysical rules;if (!rules.load(archive)) return false;
    uint resolved=0,cancelled=0;
    for (uint i=0;i<88;++i) {
        const byte caster=input.readByte(),count=input.readByte();byte keys[8];if (count>8 || input.read(keys,8)!=8) return false;
        const uint32 seed=input.readUint32LE();Native::BattleRecords battle;
        if (input.read(battle.stats,2048)!=2048 || input.read(battle.compact,256)!=256 || input.read(battle.extended,256)!=256 || input.read(battle.indices,187)!=187 || input.read(battle.kinds,187)!=187) return false;
        const uint32 expectedSeed=input.readUint32LE();const byte expectedTail=input.readByte(),expectedLast=input.readByte();const uint16 expectedAttempts=input.readUint16LE(),credit=input.readUint16LE();
        byte expected[2934];if (input.read(expected,2934)!=2934) return false;const byte retired=input.readByte();
        Native::GameplayRandom random;Native::BattleAttackState state;byte target=255;
        if (!random.importState(seed,2,0,0) || !Native::BattleAttack::begin(battle,caster,false,state,target)) return false;
        for (uint key=0;state.phase==1 && key<count;++key) {
            if (keys[key]==0x9b) Native::BattleAttack::cancel(state);
            else if (keys[key]==0x89) {
                if (!Native::BattleAttack::switchHand(battle,caster,false,state,target)) return false;
            } else if (keys[key]==0x8d) {
                Native::PhysicalIntent intent;intent.casterCell=caster;intent.targetCell=target;intent.category=8;intent.offHand=state.current!=0;
                Native::PhysicalOutcome outcome;
                if (!Native::BattleAttack::remember(battle,caster,target,state) || !rules.resolve(intent,random,battle,outcome) ||
                    !Native::BattleAttack::used(battle,caster,false,state,target)) return false;
                ++resolved;
            } else return false;
            Common::MemoryWriteStreamDynamic memory(DisposeAfterUse::YES);if (!state.save(memory) || memory.size()!=24) return false;
            Common::MemoryReadStream saved(memory.getData(),memory.size());Native::BattleAttackState restored;
            if (!restored.load(saved)) return false;state=restored;
        }
        byte actual[2934];memcpy(actual,battle.stats,2048);memcpy(actual+2048,battle.compact,256);memcpy(actual+2304,battle.extended,256);
        memcpy(actual+2560,battle.indices,187);memcpy(actual+2747,battle.kinds,187);
        if (state.phase==1 || (state.phase==2)!=bool(retired) || memcmp(actual,expected,2934) || battle.credit!=credit ||
            random.seed()!=expectedSeed || random.tail()!=expectedTail || random.last()!=expectedLast || random.generationAttempts()!=expectedAttempts) {
            uint field=0;while (field<2934 && actual[field]==expected[field]) ++field;
            report=Common::String::format("Native Attack protocol differs case%u field%u native%u original%u phase%u retire%u rng%08x/%08x",i,field,field<2934?actual[field]:0,field<2934?expected[field]:0,state.phase,retired,random.seed(),expectedSeed);return false;
        }
        cancelled+=!retired;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-action\tcases=88\trecord_bytes=258192\tresolved_hands=%u\tfree_cancels=%u\tdual_hand_and_Tab=1\tpost_hit_cancel=1\tpending_roundtrip=1\tcpu_execution=0\n",resolved,cancelled);return true;
}
}
#endif
