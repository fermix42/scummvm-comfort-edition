/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INN_MEMBERSHIP_VALIDATION_H
#define NOX_NATIVE_INN_MEMBERSHIP_VALIDATION_H
#include "nox/native_inn_membership.h"
#include "nox/native_world_fixture.h"
namespace Nox {
inline bool nativeInnOwnedState(Common::SeekableReadStream &in,Native::NewGame &game,bool install,Common::String &error) {
    byte members[768],equipment[96],items[1536],storage[2048];
    if (in.read(members,768)!=768 || in.read(equipment,96)!=96 || in.read(items,1536)!=1536 || in.read(storage,2048)!=2048) return false;
    const uint16 gold=in.readUint16LE(),food=in.readUint16LE();const byte count=in.readByte();
    if (!count || count>6 || in.err()) return false;
    if (install) {
        Common::MemoryWriteStreamDynamic data(DisposeAfterUse::YES);data.writeUint32BE(MKTAG('N','P','A','R'));data.writeUint16LE(1);data.write(members,768);data.write(equipment,96);
        Common::MemoryReadStream input(data.getData(),data.size());
        if (!game.party.load(input) || !game.inventory.importRecords(items,1536) || !game.inn.importState(storage,2048)) return false;
        game.gold=gold;game.food=food;
    } else {
        for (uint i=0;i<6;++i) for (uint field=0;field<128;++field) if (game.party.member(i)->bytes[field]!=members[i*128+field]) {
            error=Common::String::format("Inn member%u field%u actual%u expected%u",i,field,game.party.member(i)->bytes[field],members[i*128+field]);return false;
        }
        for (uint i=0;i<6;++i) if (memcmp(game.party.equipment(i)->bytes,equipment+i*16,16)) {error="Inn equipment differs";return false;}
        byte actual[1536];if (!game.inventory.exportRecords(actual,1536) || memcmp(actual,items,1536)) {error="Inn inventory ownership differs";return false;}
        if (!game.inn.ready() || memcmp(game.inn.data(),storage,2048)) {error="Inn storage differs";return false;}
        if (game.gold!=gold || game.food!=food) {error="Inn gold/food differs";return false;}
    }
    return game.party.memberCount()==count;
}
inline bool validateNativeInnMembership(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','I','M','P') || input.readUint16LE()!=1 || input.readUint32LE()!=240) return false;
    Native::NewGame base(archive);if (!base.create("Membership",0,0) || !Native::WorldCheckpointFixture::relocate(archive,base,6,9,24,24)) return false;
    const Native::NpcSchedulePolicy policy={false,false};
    if (Native::NpcScheduler::apply(base.world,base.clock.hour,base.clock.minute,policy,base.schedule)==Native::kNpcScheduleUnsupported) return false;
    for (uint c=0;c<240;++c) {
        const byte count=input.readByte(),selected=input.readByte(),slot=input.readByte();if (count<2 || count>6 || !selected || selected>count || slot>=12) return false;
        Native::NewGame game(base);if (!nativeInnOwnedState(input,game,true,error)) return false;
        if (Native::InnMembership::leave(archive,game,selected-1)!=Native::kInnMemberApplied || !nativeInnOwnedState(input,game,false,error)) {
            error=Common::String::format("Leave case%u: %s",c,error.c_str());return false;
        }
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES),again(DisposeAfterUse::YES);
        if (!game.save(saved)) return false;Common::MemoryReadStream cold(saved.getData(),saved.size());Native::NewGame continued(archive);
        if (!continued.load(cold) || !continued.save(again) || saved.size()!=again.size() || memcmp(saved.getData(),again.getData(),saved.size())) return false;
        if (Native::InnMembership::pickup(continued,50)!=Native::kInnMemberApplied || !nativeInnOwnedState(input,continued,false,error)) {
            error=Common::String::format("Pickup case%u: %s",c,error.c_str());return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+="native-inn-membership\tchains=240\tall12slots=1\tprimary_and_companions=1\tfull_owned_records=1\tNGST3_roundtrips=240\tcpu_execution=0\n";return true;
}
}
#endif
