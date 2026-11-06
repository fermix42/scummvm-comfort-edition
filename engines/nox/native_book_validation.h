/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BOOK_VALIDATION_H
#define NOX_NATIVE_BOOK_VALIDATION_H
#include "nox/native_book.h"
#include "nox/native_cast.h"
#include "common/memstream.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeBook(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','B','O','K') || input.readUint32LE() != 1 || input.readUint32LE() != 2800) return false;
    Native::ItemCatalog catalog; Common::Array<byte> data;
    if (!catalog.load(archive) || !archive.read("NA/DATA.SAVE.GAMEN", data) || data.size() != 8045) return false;
    uint outcomes[5] = {};
    for (uint i = 0; i < 2800; ++i) {
        const byte id = input.readByte(), profile = input.readByte(), slot = input.readByte();
        byte members[256], before[20], expected[20], actual[20];
        if (input.read(members, 256) != 256 || input.read(before, 20) != 20 || input.read(expected, 20) != 20) return false;
        const byte expectedResult = input.readByte();
        Common::MemoryWriteStreamDynamic partyBytes(DisposeAfterUse::YES);
        partyBytes.writeUint32BE(MKTAG('N','P','A','R')); partyBytes.writeUint16LE(1); partyBytes.write(members, 256);
        partyBytes.write(data.data() + 0xd00, 512); partyBytes.write(data.data() + 0x1e00, 96);
        Common::MemoryReadStream partyInput(partyBytes.getData(), partyBytes.size()); Native::Party party; Native::Inventory inventory; Native::SpellBook book;
        if (!party.load(partyInput) || party.memberCount() != 2 || inventory.grantOne(catalog, 2, id) != Native::kInventoryApplied || !book.importState(before, 20)) return false;
        const Native::BookResult result = book.select(catalog, inventory, party, id, slot, profile == 6, profile == 5);
        if (uint(result) != expectedResult || !book.exportState(actual, 20) || memcmp(actual, expected, 20)) {
            error = Common::String::format("Native book selection differs at case%u result%u", i, uint(result)); return false;
        }
        ++outcomes[expectedResult];
    }
    if (input.err() || input.pos() != input.size() || outcomes[0] != 800 || outcomes[1] != 400 || outcomes[2] != 800 || outcomes[3] != 400 || outcomes[4] != 400) return false;
    report += "native-book\tcases=2800\treadied=800\tunreadied=400\tintelligence_refusal=800\tcombat_refusal=400\tcancelled=400\tcpu_execution=0\n"; return true;
}
inline bool validateNativeBoundBook(HdvArchive &archive, const Native::Inventory &boundInventory, Common::SeekableReadStream &caster,
        Common::String &report, Common::String &error, Common::SaveFileManager &files, bool cold, Native::SpellBook *retainedBook = nullptr) {
    Native::ItemCatalog catalog; Native::Party starter, controlledCaster; Native::SpellBook book;
    byte empty[20] = {}; memset(empty, 255, 10);
    const int owned = boundInventory.find(2, 12);
    if (owned < 0 || boundInventory.item(owned)->quantity() != 1 || !catalog.load(archive) || !starter.create(archive, "TRACE", 0, 0) ||
            !controlledCaster.load(caster) || !controlledCaster.member(0) || controlledCaster.member(0)->bytes[18] != 110) return false;
    byte before[1536], after[1536]; if (!boundInventory.exportRecords(before, 1536)) return false;
    if (cold) {
        Common::InSaveFile *input = files.openForLoading("nox-native-book-state.bin"); const bool loaded = input && book.load(*input); delete input;
        if (!loaded || book.spell(1) != 12 || book.requirement(1) != 110) { error = "Cold native book assignment did not restore"; return false; }
    } else {
        if (!book.importState(empty, 20) || book.select(catalog, boundInventory, starter, 12, 1, false, false) != Native::kBookIntelligenceRefusal || book.spell(1) != 255 ||
            book.select(catalog, boundInventory, controlledCaster, 12, 1, false, false) != Native::kBookReadied || book.spell(1) != 12 || book.requirement(1) != 110 ||
            !boundInventory.exportRecords(after, 1536) || memcmp(before, after, 1536)) {
            error = "Native bound Vortex intelligence refusal/readiness differs"; return false;
        }
        Common::OutSaveFile *output = files.openForSaving("nox-native-book-state.bin", false); bool saved = output && book.save(*output);
        if (output) { output->finalize(); saved = saved && !output->err(); } delete output;
        if (!saved) { error = "Native actual Ready assignment did not save"; return false; }
    }
    Native::CastRules castRules; Native::CastPreparation intent;
    const CharacterRecord *casterRecord = controlledCaster.member(0);
    if (!castRules.load(archive) || castRules.prepare(book, *casterRecord, 1, false, intent) != Native::kCastBattleOnly ||
            castRules.prepare(book, *casterRecord, 1, true, intent) != Native::kCastPrepared || intent.spell != 12 || intent.manaCost != 87 ||
            intent.basePower != 152 || intent.levelThreshold != 10 || casterRecord->bytes[4] != 110 || book.spell(1) != 12) {
        error = "Native retained Vortex cast preparation/refusal differs"; return false;
    }
    if (retainedBook) *retainedBook = book;
    report += "native-bound-cast\tactual_native_ready_slot=1\tworld_refusal=1\tbattle_preparation=1\tMP=110\tprepared_cost=87\tactual_effect=0\tcpu_execution=0\n";
    report += cold ? "native-bound-book\tphase=cold\tactual_native_binding_inventory=1\tactual_ready_slot1=12\trequirement=110\tactual_assignment_restored=1\tcontrolled_caster=1\tcasting=0\tcpu_execution=0\n" :
        "native-bound-book\tphase=warm\tactual_native_binding_inventory=1\tstarter_intelligence_refusal=1\tactual_ready_slot1=12\trequirement=110\tcontrolled_caster=1\tcasting=0\tcpu_execution=0\n"; return true;
}
}
#endif
