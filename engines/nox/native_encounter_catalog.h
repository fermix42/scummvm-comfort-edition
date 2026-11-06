/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_CATALOG_H
#define NOX_NATIVE_ENCOUNTER_CATALOG_H
#include "nox/resources.h"
namespace Nox { namespace Native {
struct EncounterSelection {
    uint16 descriptor;
    byte data[4],stats[32];
    bool fixedMonster() const { return (data[0]&64)!=0; }
};
// Immutable authored descriptor matching and its referenced monster record.
// Admission policy, counts/scaling, formation and battle execution are callers.
class EncounterCatalog {
public:
    EncounterCatalog() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> descriptors,monsters;
        if (!archive.readRange("NA/SRTN.COMBAT.S",3714,141*4,descriptors) ||
            !archive.readRange("NA/SRTN.COMBAT.S",9788,142*32,monsters)) return false;
        for (uint i=0;i<141;++i) if (descriptors[i*4+2]>=142) return false;
        _descriptors=descriptors;_monsters=monsters;_ready=true;return true;
    }
    bool select(byte mapType,byte sprite,EncounterSelection &output) const {
        if (!_ready || mapType>8 || mapType==1) return false;
        const byte context=mapType==5?6:mapType;
        for (uint i=0;i<141;++i) {
            const byte *record=_descriptors.data()+i*4;const byte type=record[0]&191;
            if (!(type&128) && type!=context) continue;
            if (record[1]!=sprite) continue;
            EncounterSelection next;next.descriptor=uint16(i);memcpy(next.data,record,4);
            memset(next.stats,0,32);
            if (next.fixedMonster()) memcpy(next.stats,_monsters.data()+uint(record[2])*32,32);
            output=next;return true;
        }
        // No source-admitted descriptor: retain an explicit boundary. Do not
        // reproduce the original unbounded scan into unrelated memory.
        return false;
    }
private:
    bool _ready;Common::Array<byte> _descriptors,_monsters;
};
} }
#endif
