/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_CANNON_VALIDATION_H
#define NOX_NATIVE_BATTLE_CANNON_VALIDATION_H
#include "nox/native_battle_cannon.h"
#include "common/memstream.h"
namespace Nox {
inline bool cannonRecords(Common::SeekableReadStream &in,Native::BattleRecords &records) {
    return in.read(records.stats,2048)==2048 && in.read(records.compact,256)==256 && in.read(records.extended,256)==256 &&
        in.read(records.indices,187)==187 && in.read(records.kinds,187)==187;
}
inline bool validateNativeCannon(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    if (in.readUint32BE()!=MKTAG('N','C','N','R')) return false;
    const uint16 version=in.readUint16LE();if (version!=1 && version!=2) return false;
    const uint32 count=in.readUint32LE();if (!count || count>10000) return false;
    Native::BattleCannon cannon;if (!cannon.load(archive)) return false;
    uint hits=0,misses=0,cancels=0,reloads=0,refusals=0;
    for (uint c=0;c<count;++c) {
        const byte caster=in.readByte(),initial=in.readByte(),target=in.readByte(),category=in.readByte(),displace=in.readByte(),cancel=in.readByte(),
            expectedStep=in.readByte(),expectedClass=in.readByte(),retired=in.readByte(),impact=in.readByte();
        const byte protection=version==2?in.readByte():0;if (protection>1) return false;
        Native::BattleCannonState state;Native::BattleRecords records,expected;
        if (in.read(state.stations,15)!=15 || initial>2) return false;
        const uint32 seed=in.readUint32LE();const byte tail=in.readByte(),last=in.readByte();const uint16 attempts=in.readUint16LE();
        Native::GameplayRandom random;if (!random.importState(seed,tail,last,attempts)) return false;
        records.credit=in.readUint16LE();if (!cannonRecords(in,records)) return false;
        byte expectedStations[15],expectedRandom[8];
        if (in.read(expectedStations,15)!=15 || in.read(expectedRandom,8)!=8) return false;
        expected.credit=in.readUint16LE();if (!cannonRecords(in,expected)) return false;
        Native::CannonStep step;
        if (!cannon.begin(caster,state,step) || uint(step)!=expectedStep) {error=Common::String::format("Cannon station mismatch case%u",c);return false;}
        // Suspend after the source station advance, before target input.
        Common::MemoryWriteStreamDynamic checkpoint(DisposeAfterUse::YES);if (!state.save(checkpoint)) return false;
        Common::MemoryReadStream restored(checkpoint.getData(),checkpoint.size());Native::BattleCannonState resumed;
        if (!resumed.load(restored)) return false;state=resumed;
        if (step==Native::kCannonTarget) {
            if (cancel) {Native::BattleCannon::cancel(state);++cancels;if (retired) return false;}
            else {
                Native::CannonOutcome result;
                if (!cannon.fire(caster,target,category,displace!=0,state,random,records,result,protection!=0) ||
                    result.classification!=expectedClass || result.impactY*17+result.impactX!=impact || !retired) {
                    error=Common::String::format("Cannon impact mismatch case%u",c);return false;
                }
                if (result.classification==2) ++hits;else ++misses;
            }
        } else if (step==Native::kCannonNotHere) {++refusals;if (retired) return false;}
        else {++reloads;if (!retired) return false;}
        byte actualRandom[8];WRITE_LE_UINT32(actualRandom,random.seed());actualRandom[4]=random.tail();actualRandom[5]=random.last();WRITE_LE_UINT16(actualRandom+6,random.generationAttempts());
        if (memcmp(expectedStations,state.stations,15) || memcmp(expectedRandom,actualRandom,8) || records.credit!=expected.credit ||
            memcmp(records.stats,expected.stats,2048) || memcmp(records.compact,expected.compact,256) || memcmp(records.extended,expected.extended,256) ||
            memcmp(records.indices,expected.indices,187) || memcmp(records.kinds,expected.kinds,187)) {
            error=Common::String::format("Cannon records/RNG/reload differ case%u",c);return false;
        }
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report+=Common::String::format("native-cannon\tcases=%u\thits=%u\tmisses=%u\tcancels=%u\treloads=%u\trefusals=%u\tpending_roundtrips=%u\tcpu_execution=0\twhole_battle=0\n",count,hits,misses,cancels,reloads,refusals,count);
    return true;
}
}
#endif
