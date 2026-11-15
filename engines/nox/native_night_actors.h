/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NIGHT_ACTORS_H
#define NOX_NATIVE_NIGHT_ACTORS_H
#include "nox/resources.h"
namespace Nox { namespace Native {
// Resident FAF0: map-entry/daylight producer, not a per-frame actor reset.
class NightActors {
public:
    bool load(HdvArchive &archive) {return archive.readRange("NA/BS.ROUTINES.BK2",0x2992,24,_records);}
    bool apply(byte map,byte mode,byte *tables,uint size,bool &changed) const {
        changed=false;if (_records.size()!=24 || !tables || size!=1792 || mode>3) return false;
        if (map!=46) return true;
        byte records[24]={};if (mode>=2) memcpy(records,_records.data(),24);
        changed=memcmp(tables+768,records,24)!=0;memcpy(tables+768,records,24);return true;
    }
private:
    Common::Array<byte> _records;
};
} }
#endif
