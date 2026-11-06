/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INN_SERVICE_VALIDATION_H
#define NOX_NATIVE_INN_SERVICE_VALIDATION_H
#include "nox/native_inn_service.h"
#include "nox/native_world_fixture.h"
namespace Nox {
inline bool validateNativeInnService(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','I','N','N') || input.readUint16LE()!=1 || input.readUint32LE()!=120) return false;
    Native::NewGame base(archive);if (!base.create("Inn",0,0)) return false;
    for (uint c=0;c<120;++c) {
        const byte map=input.readByte(),slot=input.readByte(),count=input.readByte();
        const uint16 money=input.readUint16LE(),balance=input.readUint16LE();const bool paid=input.readByte()!=0;
        byte before[768],expected[768];if (!count || count>6 || input.read(before,768)!=768 || input.read(expected,768)!=768) return false;
        Native::NewGame game(base);
        // Canonical initial families of the ten authored offers; independently
        // validated by the resource loader, not a gameplay destination guess.
        const byte type=map==48?5:map==58?7:map==76?4:(map==72 || map==24)?8:6;
        if (!Native::WorldCheckpointFixture::relocate(archive,game,type,map,24,24)) return false;
        Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES);packet.writeUint32BE(MKTAG('N','P','A','R'));packet.writeUint16LE(1);packet.write(before,768);
        for (uint i=0;i<6;++i) packet.write(base.party.equipment(i)->bytes,16);
        Common::MemoryReadStream members(packet.getData(),packet.size());if (!game.party.load(members) || game.party.memberCount()!=count) return false;
        game.gold=money;Native::InnService service;
        if (!service.begin(archive,game,slot) || !service.welcome(true) || !service.requestRest()) return false;
        Common::MemoryWriteStreamDynamic checkpoint(DisposeAfterUse::YES);if (!service.save(checkpoint)) return false;
        Common::MemoryReadStream restore(checkpoint.getData(),checkpoint.size());Native::InnService continued;
        if (!continued.load(restore) || !continued.confirm(true,game) || game.gold!=balance || (continued.result()==Native::kInnRestPaid)!=paid) return false;
        for (uint i=0;i<6;++i) if (memcmp(game.party.member(i)->bytes,expected+i*128,128)) {error=Common::String::format("Inn restoration differs case%u member%u",c,i);return false;}
    }
    if (input.readUint32LE()!=72) return false;
    for (uint c=0;c<72;++c) {
        Native::TurnClock clock;clock.hour=input.readByte();clock.minute=input.readByte();const byte hour=input.readByte(),minute=input.readByte(),expected=input.readByte();byte hours;
        if (!Native::InnService::sleep(clock,hours) || clock.hour!=hour || clock.minute!=minute || hours!=expected) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+="native-inn\trest_cases=120\tsleep_cases=72\tfull768_records=1\tpending_roundtrips=120\tcpu_execution=0\n";return true;
}
}
#endif
