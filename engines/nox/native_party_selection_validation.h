/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PARTY_SELECTION_VALIDATION_H
#define NOX_NATIVE_PARTY_SELECTION_VALIDATION_H
#include "nox/native_party_selection.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativePartySelection(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','P','S','R') || input.readUint16LE()!=2 || input.readUint32LE()!=768) return false;
    Native::Party base;if (!base.create(archive,"Selection",0,0)) return false;
    for (uint c=0;c<768;++c) {
        const byte count=input.readByte();byte states[6];if (!count || count>6 || input.read(states,6)!=6) return false;
        const byte dead=input.readByte(),key=input.readByte(),initial=input.readByte(),expected=input.readByte(),kind=input.readByte();
        Common::MemoryWriteStreamDynamic bytes(DisposeAfterUse::YES);bytes.writeUint32BE(MKTAG('N','P','A','R'));bytes.writeUint16LE(1);
        for (uint i=0;i<6;++i) {
            byte member[128];memcpy(member,base.member(0)->bytes,128);member[0]=i<count?states[i]:170;member[69]=i<count?0:1;
            if (member[0]==255) WRITE_LE_UINT16(member+2,0);bytes.write(member,128);
        }
        for (uint i=0;i<6;++i) bytes.write(base.equipment(i)->bytes,16);
        Common::MemoryReadStream records(bytes.getData(),bytes.size());Native::Party party;
        if (!party.load(records)) return false;Native::PartySelection selection;
        if (!selection.begin(party,dead!=0) || selection.selected()!=initial) {error=Common::String::format("Selector initial case%u",c);return false;}
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!selection.save(saved)) return false;
        Common::MemoryReadStream resume(saved.getData(),saved.size());Native::PartySelection continued;if (!continued.load(resume)) return false;
        Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd.ascii=key&127;
        event.kbd.keycode=key==0x8b?Common::KEYCODE_UP:key==0x8a?Common::KEYCODE_DOWN:key==0xa0?Common::KEYCODE_SPACE:
            key==0x8d?Common::KEYCODE_RETURN:key==0x9b?Common::KEYCODE_ESCAPE:Common::KEYCODE_INVALID;
        const Native::PartySelectionResult result=continued.handle(event,party);
        // Source Escape writes9B into its overloaded return byte, not actor27.
        // Typed cancellation retains cursor identity and yields no actor choice.
        const bool identity=kind==2?(expected==0x9b && continued.selected()==initial):continued.selected()+1==expected;
        if (uint(result)!=kind || !identity) {
            error=Common::String::format("Selector input case%u key%u actual%u/%u expected%u/%u",c,key,continued.selected(),result,expected,kind);return false;
        }
    }
    report+="native-party-selector\tcases=768\toriginal_arrows_aliases_returns=1\tcontinuation_roundtrips=768\tcpu_execution=0\n";return !input.err() && input.pos()==input.size();
}
}
#endif
