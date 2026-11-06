/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TEMPLE_SERVICE_VALIDATION_H
#define NOX_NATIVE_TEMPLE_SERVICE_VALIDATION_H
#include "nox/native_temple_service.h"
#include "nox/native_world_fixture.h"
namespace Nox {
inline bool validateNativeTempleService(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','T','M','P') || input.readUint16LE()!=1) return false;
    const uint32 count=input.readUint32LE();if (count!=960 || input.size()-input.pos()!=int64(count)*17) return false;
    Native::NewGame base(archive);if (!base.create("Service",0,0) || !Native::WorldCheckpointFixture::relocate(archive,base,7,15,24,24)) return false;
    for (uint c=0;c<count;++c) {
        const byte partyCount=input.readByte();byte levels[6];if (input.read(levels,6)!=6 || !partyCount || partyCount>6) return false;
        const uint16 healing=input.readUint16LE(),resurrection=input.readUint16LE();const byte choice=input.readByte();
        const uint16 money=input.readUint16LE(),balance=input.readUint16LE();const bool paid=input.readByte()!=0;
        Native::NewGame game(base);Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES);
        packet.writeUint32BE(MKTAG('N','P','A','R'));packet.writeUint16LE(1);
        for (uint i=0;i<6;++i) {
            byte member[128];memcpy(member,base.party.member(0)->bytes,128);member[0]=i<partyCount?0:170;member[1]=levels[i];member[69]=0;
            packet.write(member,128);
        }
        for (uint i=0;i<6;++i) packet.write(base.party.equipment(i)->bytes,16);
        Common::MemoryReadStream partyInput(packet.getData(),packet.size());if (!game.party.load(partyInput)) return false;
        game.gold=money;Native::TempleService temple;
        if (!temple.begin(archive,game,216) || temple.healingPrice()!=healing || temple.resurrectionPrice()!=resurrection) {
            error=Common::String::format("Temple pricing differs case%u",c);return false;
        }
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!temple.save(saved)) return false;
        Common::MemoryReadStream restore(saved.getData(),saved.size());Native::TempleService continued;
        if (!continued.load(restore) || !continued.choose(choice,game) || game.gold!=balance || (continued.result()!=Native::kTemplePoor)!=paid) {
            error=Common::String::format("Temple payment differs case%u",c);return false;
        }
        Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES);if (!continued.save(after)) return false;
        Common::MemoryReadStream pending(after.getData(),after.size());Native::TempleService cold;
        if (!cold.load(pending) || cold.phase()!=continued.phase() || cold.result()!=continued.result()) return false;
    }
    report+="native-temple\tcases=960\tpricing_payment_original=1\tcontinuation_roundtrips=1920\tcpu_execution=0\n";
    return !input.err();
}
}
#endif
