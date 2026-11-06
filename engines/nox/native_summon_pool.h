/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SUMMON_POOL_H
#define NOX_NATIVE_SUMMON_POOL_H
#include "nox/records.h"
namespace Nox { namespace Native {
enum SummonAllocation { kSummonAllocated, kSummonPoolFull, kSummonUnsupported };
// Own the16 companion stat slots and their original-health snapshot. Actor
// placement, animation and source spell/template selection are separate.
class SummonPool {
public:
    SummonPool() : _ready(false) {}
    bool importRecords(const byte *records, uint size, const byte *health, uint healthSize) {
        if (_ready || !records || size != 512 || !health || healthSize != 32) return false;
        memcpy(_records,records,512); memcpy(_health,health,32); _ready = true; return true;
    }
    bool exportRecords(byte *records, uint size, byte *health, uint healthSize) const {
        if (!_ready || !records || size != 512 || !health || healthSize != 32) return false;
        memcpy(records,_records,512); memcpy(health,_health,32); return true;
    }
    SummonAllocation allocate(const byte *record, uint size, byte &actorIndex) {
        if (!_ready || !record || size != 32) return kSummonUnsupported;
        for (uint i = 0; i < 16; ++i) {
            byte *slot = _records + i * 32;
            if (slot[0] != 255 && READ_LE_UINT16(slot + 2)) continue;
            memcpy(slot,record,32); memcpy(_health + i * 2,record + 2,2);
            actorIndex = 128 + i * 8; return kSummonAllocated;
        }
        return kSummonPoolFull;
    }
private:
    bool _ready; byte _records[512], _health[32];
};
} }
#endif
