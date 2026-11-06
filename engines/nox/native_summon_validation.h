/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SUMMON_VALIDATION_H
#define NOX_NATIVE_SUMMON_VALIDATION_H
#include "nox/native_summon_pool.h"
namespace Nox {
inline bool validateNativeSummonPool(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','S','A','L') || input.readUint32LE() != 1 || input.readUint32LE() != 68) return false;
    Native::SummonPool retained; byte retainedIndex = 0; uint allocated = 0, full = 0;
    for (uint i = 0; i < 68; ++i) {
        byte before[512], health[32], record[32], expected[512], expectedHealth[32], actual[512], actualHealth[32];
        if (input.read(before,512) != 512 || input.read(health,32) != 32) return false;
        const byte priorIndex = input.readByte(); if (input.read(record,32) != 32) return false;
        const byte refused = input.readByte(), expectedIndex = input.readByte();
        if (input.read(expected,512) != 512 || input.read(expectedHealth,32) != 32) return false;
        if (i <= 51) { Native::SummonPool initial; if (!initial.importRecords(before,512,health,32)) return false; retained = initial; retainedIndex = priorIndex; }
        else if (!retained.exportRecords(actual,512,actualHealth,32) || memcmp(actual,before,512) || memcmp(actualHealth,health,32) || retainedIndex != priorIndex) return false;
        const Native::SummonAllocation result = retained.allocate(record,32,retainedIndex);
        if ((result == Native::kSummonPoolFull ? 1 : 0) != refused || result == Native::kSummonUnsupported || retainedIndex != expectedIndex ||
            !retained.exportRecords(actual,512,actualHealth,32) || memcmp(actual,expected,512) || memcmp(actualHealth,expectedHealth,32)) {
            error = Common::String::format("Native summon allocation differs at case%u",i); return false;
        }
        allocated += result == Native::kSummonAllocated; full += result == Native::kSummonPoolFull;
    }
    if (input.err() || input.pos() != input.size() || allocated != 64 || full != 4) return false;
    report += "native-summon-pool\tcases=68\tallocated=64\tfull_refusals=4\tretained_fill=16\toriginal_health_snapshot=1\tactor_placement=0\tcpu_execution=0\n"; return true;
}
}
#endif
