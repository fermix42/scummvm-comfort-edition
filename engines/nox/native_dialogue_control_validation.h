/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_CONTROL_VALIDATION_H
#define NOX_NATIVE_DIALOGUE_CONTROL_VALIDATION_H
#include "nox/native_conversation.h"
#include "nox/native_item_shortcuts_validation.h"
namespace Nox {
inline bool validateNativeDialogueControl478(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    using namespace Native;
    report="original dialogue control/RNG transitions";
    if (fixture.readUint32BE()!=MKTAG('N','D','4','8') || fixture.readUint32LE()!=1) return false;
    const uint cases=fixture.readUint32LE();if (cases!=768) return false;
    for (uint i=0;i<cases;++i) {
        const byte control=fixture.readByte();const uint32 seed=fixture.readUint32LE();const bool terminal=fixture.readByte()!=0;
        const uint32 expectedSeed=fixture.readUint32LE();const byte tail=fixture.readByte(),last=fixture.readByte();const uint16 attempts=fixture.readUint16LE();
        GameplayRandom random;if (!random.importState(seed,2,73,5)) return false;bool ended=false;
        if (!DialogueControl::resolve(control,&random,ended) || ended!=terminal || random.seed()!=expectedSeed || random.tail()!=tail || random.last()!=last || random.generationAttempts()!=attempts) return false;
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    report="actual authored dwarf packet response and retained control";
    const uint32 seeds[]={0x12341234,0x98765432,0x11223344};uint resolved=0;
    for (uint operation=0;operation<2;++operation) for (uint i=0;i<3;++i) {
        byte fields[256]={};ConversationContext context;context.map=26;context.mapType=6;context.adultMode=true;context.hour=9;
        Common::ScopedPtr<Conversation> owner(new Conversation);Conversation &conversation=*owner;
        GameplayRandom random;if (!random.importState(seeds[i],2)) return false;GameplayRandom expected=random;bool terminal=false;
        if (!DialogueControl::resolve(127,&expected,terminal) || !conversation.begin(archive,"NA/DATA.TLK.L01A",24,fields,256,context)) return false;
        const ConversationResult result=operation?conversation.submit("JOB",&random):conversation.opening(&random);
        if (result!=kConversationReply || conversation.terminalReply()!=terminal || conversation.reply().empty() || random.seed()!=expected.seed() || random.tail()!=expected.tail() || random.last()!=expected.last() || random.generationAttempts()!=expected.generationAttempts()) return false;
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),rng(DisposeAfterUse::YES);if (!conversation.save(before) || !random.save(rng)) return false;
        Common::ScopedPtr<Conversation> restoredOwner(new Conversation);Conversation &restored=*restoredOwner;Common::MemoryReadStream saved(before.getData(),before.size()),savedRandom(rng.getData(),rng.size());GameplayRandom restoredRandom;
        if (!restored.load(saved,archive) || !restoredRandom.load(savedRandom) || restored.terminalReply()!=terminal || restored.reply()!=conversation.reply()) return false;
        Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES),rngAfter(DisposeAfterUse::YES);if (!restored.save(after) || !restoredRandom.save(rngAfter) || before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size()) || rng.size()!=rngAfter.size() || memcmp(rng.getData(),rngAfter.getData(),rng.size())) return false;
        ++resolved;
    }
    // Town 1 actor 8 has an original adult-only farewell and a normal fallback.
    report="authored mature dialogue selection";
    DialoguePacket packet;
    Common::String error;
    if (!loadDialoguePacket(archive,"NA/DATA.TLK.L001",8,packet,error)) return false;
    byte fields[256]={};
    DialogueResponse mature,ordinary;
    const QuestFields quest(fields,sizeof(fields));
    if (!selectDialogueResponse(packet.text,0,"BYE",0,quest,0,true,9,0,mature) ||
        !selectDialogueResponse(packet.text,0,"BYE",0,quest,0,false,9,0,ordinary) ||
        !mature.adultOnly || ordinary.adultOnly || mature.bodyOffset==ordinary.bodyOffset) return false;
    if (!validateNativeItemShortcuts478(archive,report)) return false;
    report=Common::String::format("dialogue\tsource_control_rng_cases=%u\tauthored_packet_responses=%u\tresolved_codec_roundtrips=%u\tready_key_cast_shortcuts=10\tcampaign=0\taudio_suite=0\n",cases,resolved,resolved);return true;
}
}
#endif
