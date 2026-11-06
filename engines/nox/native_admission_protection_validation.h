/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ADMISSION_PROTECTION_VALIDATION_H
#define NOX_NATIVE_ADMISSION_PROTECTION_VALIDATION_H
#include "nox/native_encounter_admission.h"
namespace Nox {
inline bool validateNativeAdmissionProtection(Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','A','P','R') || input.readUint32LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (!count || count>10000 || input.size()!=12+count*10) return false;
    uint protectedCases=0,crossing=0;
    for (uint i=0;i<count;++i) {
        byte row[10],tables[1792]={},fields[256]={};if (input.read(row,10)!=10) return false;
        if (row[1]!=0 && row[1]!=4) return false;const uint slot=row[2];if (slot&7) return false;
        tables[1280+slot+3]=row[3];tables[1280+slot+5]=row[4];tables[768+slot]=row[5];
        tables[768+slot+9]=55;tables[768+slot+11]=4;fields[55]=row[6];bool protectedTargets=false;
        if (!Native::EncounterAdmissionRules::protection(tables,1792,fields,256,row[1]?1:0,row[2],protectedTargets) ||
            uint(protectedTargets)!=row[7] || Native::EncounterAdmissionRules::initialNoFlee(row[0],0)!=row[8] || row[9]!=row[1]) {
            report=Common::String::format("admission protection case%u differs",i);return false;
        }
        protectedCases+=protectedTargets?1:0;crossing+=slot==248?1:0;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-admission-protection\tcases=%u\tprotected=%u\tdefinition_page_crossing=%u\tparent_no_flee_and_conditional_protection=1\tcpu_execution=0\n",count,protectedCases,crossing);return true;
}
}
#endif
