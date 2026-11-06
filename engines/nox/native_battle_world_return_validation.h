/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_WORLD_RETURN_VALIDATION_H
#define NOX_NATIVE_BATTLE_WORLD_RETURN_VALIDATION_H
#include "nox/native_battle_world_return.h"
#include "nox/native_world.h"
namespace Nox {
inline bool validateNativeBattleWorldReturn(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','W','R') || input.readUint32LE()!=2) return false;
    const uint32 cases=input.readUint32LE();if (!cases || cases>10000 || input.size()!=12+cases*(6+2048*3)) return false;
    for (uint32 i=0;i<cases;++i) {
        const byte type=input.readByte(),map=input.readByte(),family=input.readByte(),bank=input.readByte(),slot=input.readByte(),fled=input.readByte();
        byte state[2048],mid[2048],expected[2048];
        if (fled>1 || input.read(state,2048)!=2048 || input.read(mid,2048)!=2048 || input.read(expected,2048)!=2048 ||
            !Native::BattleWorldReturn::effects(state+256,1792,state,256,bank,slot,fled!=0) || memcmp(state,mid,2048) ||
            !Native::BattleWorldReturn::retire(state+256,1792,state,256,bank,slot,fled!=0) || memcmp(state,expected,2048)) {
            report=Common::String::format("battle world return case%u effects/parent boundary differs",i);return false;
        }
        Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES);
        packet.writeUint32BE(MKTAG('N','W','L','D'));packet.writeUint16LE(1);
        packet.writeByte(type);packet.writeByte(map);packet.writeByte(20);packet.writeByte(19);packet.writeByte(1);
        packet.write(state,256);packet.writeUint16LE(1);packet.writeByte(family);packet.writeByte(map);packet.write(state+256,1792);
        Common::MemoryReadStream source(packet.getData(),packet.size());Native::World world(archive);Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);
        if (!world.load(source) || !world.save(saved) || saved.size()!=packet.size() || memcmp(saved.getData(),packet.getData(),packet.size())) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report=Common::String::format("native-battle-world-return\tcases=%u\tquest_then_parent_commit=1\tcompact_extended_fled=1\tcold_world_roundtrips=%u\tcpu_execution=0\twhole_battle=0\n",cases,cases);return true;
}
}
#endif
