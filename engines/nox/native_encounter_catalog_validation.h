/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_CATALOG_VALIDATION_H
#define NOX_NATIVE_ENCOUNTER_CATALOG_VALIDATION_H
#include "nox/native_encounter_catalog.h"
namespace Nox {
inline bool validateNativeEncounterCatalog(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','E','C','S') || input.readUint32LE()!=2 || input.readUint32LE()!=222 || input.size()!=12+222*41) return false;
    Native::EncounterCatalog catalog;if (!catalog.load(archive)) return false;
    uint fixed=0;
    for (uint i=0;i<222;++i) {
        const byte type=input.readByte(),sprite=input.readByte();const uint16 index=input.readUint16LE();
        const byte fallback=input.readByte();byte descriptor[4],stats[32];Native::EncounterSelection result;
        if (fallback || input.read(descriptor,4)!=4 || input.read(stats,32)!=32 || !catalog.select(type,sprite,result) ||
            result.descriptor!=index || memcmp(result.data,descriptor,4) || memcmp(result.stats,stats,32)) return false;
        if (result.fixedMonster()) ++fixed;
    }
    Native::EncounterSelection sentinel;memset(&sentinel,91,sizeof(sentinel));Native::EncounterSelection prior=sentinel;
    if (catalog.select(1,164,sentinel) || catalog.select(255,164,sentinel) || catalog.select(8,0,sentinel) || memcmp(&sentinel,&prior,sizeof(sentinel)) || input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-encounter-catalog\tcases=222\tfixed_monster_candidates=%u\tmonster_bytes=%u\tdescriptor_bytes=888\tinvalid_selection_atomic=1\tcpu_execution=0\tencounter_construction=0\n",fixed,fixed*32);return true;
}
}
#endif
