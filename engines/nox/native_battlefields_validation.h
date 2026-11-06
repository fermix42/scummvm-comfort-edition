/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLEFIELDS_VALIDATION_H
#define NOX_NATIVE_BATTLEFIELDS_VALIDATION_H
#include "nox/native_battlefields.h"
namespace Nox {
inline bool validateNativeBattlefieldChoice(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','S','C') || input.readUint32LE()!=1 || input.readUint32LE()!=110 || input.size()!=12+110*7) return false;
    Native::BattlefieldCatalog catalog;if (!catalog.load(archive)) return false;
    for (uint i=0;i<110;++i) {
        const byte type=input.readByte(),terrain=input.readByte(),contact=input.readByte(),flags=input.readByte();
        const byte field=input.readByte(),style=input.readByte(),water=input.readByte();Native::BattlefieldChoice result;
        if (!catalog.choose(type,terrain,contact,flags,result) || result.field!=field || result.layoutFlags!=style || result.deepWater!=water) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battlefield-choice\tcases=110\tcpu_execution=0\tencounter_construction=0\n";return true;
}
inline bool validateNativeBattlefields(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','F','L') || input.readUint32LE()!=2 || input.readUint32LE()!=160 || input.size()!=12+160*189) return false;
    Native::BattlefieldCatalog catalog;if (!catalog.load(archive)) return false;
    uint bytes=0;
    const byte types[]={0,2,3,4,5,6,7,8};
    for (uint field=0;field<20;++field) for (uint i=0;i<8;++i) {
        const byte type=types[i];if (input.readByte()!=field || input.readByte()!=type) return false;
        byte expected[187];Common::Array<byte> actual;
        if (input.read(expected,187)!=187 || !catalog.project(byte(field),type,actual) || actual.size()!=187 || memcmp(expected,actual.data(),187)) return false;
        bytes+=187;
    }
    Common::Array<byte> sentinel;sentinel.push_back(91);
    if (catalog.project(20,8,sentinel) || catalog.project(0,1,sentinel) || catalog.project(0,9,sentinel) || sentinel.size()!=1 || sentinel[0]!=91 || input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battlefields\tfields=20\tcases=160\tbytes=%u\tinvalid_selection_atomic=1\tcpu_execution=0\tencounter_construction=0\n",bytes);return true;
}
}
#endif
