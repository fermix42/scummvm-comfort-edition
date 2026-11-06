/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_FORMATION_VALIDATION_H
#define NOX_NATIVE_ENCOUNTER_FORMATION_VALIDATION_H
#include "nox/native_encounter_formation.h"
#include "nox/native_encounter_admission.h"
namespace Nox {
inline bool validateNativeTransportAdmission(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','T','F','M') || input.readUint32LE()!=1) return false;
    const uint32 cases=input.readUint32LE();if (!cases || cases>10000 || input.size()!=12+cases*5156) return false;
    Native::EncounterAdmissionRules rules;if (!rules.load(archive)) return false;
    for (uint32 i=0;i<cases;++i) {
        Native::EncounterContext context;context.mapType=input.readByte();context.mapId=input.readByte();context.location=input.readByte();context.definition=input.readByte();context.tile=input.readByte();
        const byte appearance=input.readByte(),transport=input.readByte(),waterLower=input.readByte(),waterUpper=input.readByte();
        context.specialLower=input.readByte();context.specialUpper=input.readByte();const byte partyCount=input.readByte();
        Native::GameplayRandom random;const uint32 seed=input.readUint32LE();const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
        Native::BattleRecords records,expected;
        if (transport>1 || !random.importState(seed,tail,last,attempts) || input.read(records.stats,2048)!=2048 || input.read(records.compact,256)!=256 || input.read(records.extended,256)!=256) return false;
        byte outcome[8];if (input.read(outcome,8)!=8 || input.read(expected.stats,2048)!=2048 || input.read(expected.compact,256)!=256 || input.read(expected.extended,256)!=256) return false;
        const uint32 finalSeed=input.readUint32LE();const byte finalTail=input.readByte(),finalLast=input.readByte();const uint16 finalAttempts=input.readUint16LE();
        Native::EncounterAdmission admission;const byte flags=Native::EncounterAdmissionRules::transportFlags(context.mapType,context.tile,appearance,transport!=0,waterLower,waterUpper);
        const byte noFlee=context.mapType>=4 || (flags&192)==192?128:0;byte count=0,groupFlags=0;
        if (!rules.select(context,admission) || admission.group!=outcome[0] || admission.monster!=outcome[1] || admission.sprite!=outcome[2] || admission.spell!=outcome[3] || flags!=outcome[4] || noFlee!=outcome[6] ||
            !Native::EncounterFormation::group(archive,admission.group,admission.monster?admission.monster:255,partyCount,random,records,count,groupFlags,flags,admission.sprite)) {
            report=Common::String::format("transport admission case%u selection/construction refused",i);return false;
        }
        if (count!=outcome[5] || groupFlags!=outcome[7] || memcmp(records.stats,expected.stats,1791) || memcmp(records.stats+1792,expected.stats+1792,256) ||
            memcmp(records.compact,expected.compact,256) || memcmp(records.extended,expected.extended,256) ||
            random.seed()!=finalSeed || random.tail()!=finalTail || random.last()!=finalLast || random.generationAttempts()!=finalAttempts) {
            report=Common::String::format("transport admission case%u records/count/RNG mismatch",i);return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-transport-admission\tcases=%u\tlayouts=5\tselection_count_templates=1\tfull_record_bytes=%u\tcpu_execution=0\twhole_battle=0\n",cases,cases*2559);return true;
}
inline bool validateNativeGeneralFormation(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','G','F','M') || input.readUint32LE()!=1) return false;
    const uint32 cases=input.readUint32LE();
    if (!cases || cases>10000 || input.size()!=12+cases*(20+2*2560)) return false;
    uint64 bytes=0;
    for (uint32 i=0;i<cases;++i) {
        const byte group=input.readByte(),partyCount=input.readByte(),layout=input.readByte(),expectedCount=input.readByte();
        Native::GameplayRandom random;const uint32 seed=input.readUint32LE();
        const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
        Native::BattleRecords records,expected;
        if (layout || !random.importState(seed,tail,last,attempts) ||
            input.read(records.stats,2048)!=2048 || input.read(records.compact,256)!=256 || input.read(records.extended,256)!=256 ||
            input.read(expected.stats,2048)!=2048 || input.read(expected.compact,256)!=256 || input.read(expected.extended,256)!=256) return false;
        const uint32 finalSeed=input.readUint32LE();const byte finalTail=input.readByte(),finalLast=input.readByte();const uint16 finalAttempts=input.readUint16LE();
        byte count=0,flags=0;
        if (!Native::EncounterFormation::group(archive,group,group==124?126:255,partyCount,random,records,count,flags)) {
            report=Common::String::format("general formation case%u group%u construction refused",i,group);return false;
        }
        // Same original inactive-friendly ROM/scratch alias as the retained
        // fixed-formation oracle; native preserves that opaque input byte.
        const bool exact=!memcmp(records.stats,expected.stats,1791) && !memcmp(records.stats+1792,expected.stats+1792,256) &&
            !memcmp(records.compact,expected.compact,256) && !memcmp(records.extended,expected.extended,256);
        if (!exact || count!=expectedCount || random.seed()!=finalSeed || random.tail()!=finalTail ||
            random.last()!=finalLast || random.generationAttempts()!=finalAttempts) {
            report=Common::String::format("general formation case%u group%u party%u count%u expected%u records%u RNG%u",i,group,partyCount,count,expectedCount,exact?1:0,random.seed()==finalSeed?1:0);return false;
        }
        bytes+=2559;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-general-formation\tcases=%u\trecord_bytes=%llu\tparty_sizes=1,3,6\tcount_placement_templates=1\tunused_scratch_alias_bytes_excluded=%u\tcpu_execution=0\twhole_battle=0\n",cases,(unsigned long long)bytes,cases);return true;
}
inline bool validateNativeEncounterFormation(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','F','C','S') || input.readUint32LE()!=2 || input.readUint32LE()!=8 || input.size()!=12+8*(16+2*2560)) return false;
    Native::EncounterCatalog catalog;Native::EncounterSelection enemy;
    if (!catalog.load(archive) || !catalog.select(8,164,enemy)) return false;
    for (uint i=0;i<8;++i) {
        Native::GameplayRandom random;const uint32 seed=input.readUint32LE();
        const byte tail=input.readByte(),last=input.readByte();const uint16 attempts=input.readUint16LE();
        Native::BattleRecords records,expected;
        if (!random.importState(seed,tail,last,attempts) || input.read(records.stats,2048)!=2048 || input.read(records.compact,256)!=256 || input.read(records.extended,256)!=256 ||
            input.read(expected.stats,2048)!=2048 || input.read(expected.compact,256)!=256 || input.read(expected.extended,256)!=256 ||
            !Native::EncounterFormation::fixedSingle(archive,enemy,1,random,records)) return false;
        const uint32 finalSeed=input.readUint32LE();const byte finalTail=input.readByte(),finalLast=input.readByte();const uint16 finalAttempts=input.readUint16LE();
        // Original clear fills 511 bytes but its subsequent ROM transfer also
        // copies scratch byte 0x97ff into unused friendly slot15 byte31. That
        // CPU alias has no active actor and is not a native record operation.
        // All other bytes, including unchanged surrounding records, are exact.
        if (memcmp(records.stats,expected.stats,1791) || memcmp(records.stats+1792,expected.stats+1792,256) || memcmp(records.compact,expected.compact,256) || memcmp(records.extended,expected.extended,256) ||
            random.seed()!=finalSeed || random.tail()!=finalTail || random.last()!=finalLast || random.generationAttempts()!=finalAttempts) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-encounter-formation\tcases=8\trecord_bytes=20472\trng_states=8\tunused_scratch_alias_bytes_excluded=8\tcpu_execution=0\tcontrolled_setup=1\ttactical_execution=0\n";return true;
}
}
#endif
