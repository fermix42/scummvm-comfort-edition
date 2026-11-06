/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_ADMISSION_VALIDATION_H
#define NOX_NATIVE_ENCOUNTER_ADMISSION_VALIDATION_H
#include "nox/native_encounter_admission.h"
#include "nox/native_maps.h"
namespace Nox {
inline bool validateNativeEncounterAdmission(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','A','D','M') || input.readUint32LE()!=2) return false;
    const uint32 cases=input.readUint32LE(),maps=input.readUint32LE();if (!cases || cases>10000 || maps!=76) return false;
    for (uint32 i=0;i<maps;++i) {
        const uint size=input.readUint16LE();if (size<23 || size>256) return false;
        Native::MapDefinition map;map.metadata.resize(size);
        if (input.read(map.metadata.data(),size)!=size) return false;
        const byte expected=input.readByte();byte context;
        if (!map.dialogueContext(context) || context!=expected) return false;
    }
    Native::EncounterAdmissionRules rules;if (!rules.load(archive)) return false;
    for (uint32 i=0;i<cases;++i) {
        Native::EncounterContext context;
        context.mapType=input.readByte();context.mapId=input.readByte();context.location=input.readByte();context.definition=input.readByte();
        context.tile=input.readByte();context.specialLower=input.readByte();context.specialUpper=input.readByte();
        const uint16 descriptor=input.readUint16LE();byte data[4];if (input.read(data,4)!=4) return false;
        const byte monster=input.readByte(),sprite=input.readByte(),spell=input.readByte(),fallback=input.readByte();
        Native::EncounterAdmission result;
        if (!rules.select(context,result) || result.descriptor!=descriptor || result.monster!=monster || result.sprite!=sprite ||
            result.spell!=spell || result.fallback!=(fallback!=0) || result.group!=data[2] ||
            result.raw[0]!=data[0] || result.raw[1]!=data[1] || result.raw[3]!=data[3]) {
            report=Common::String::format("admission selection case%u mismatch",i);return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-encounter-admission\tcases=%u\tmap_contexts=%u\tdescriptor_overrides_fallback=1\tcpu_execution=0\twhole_battle=0\n",cases,maps);return true;
}
}
#endif
