/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PAGE_SEARCH_VALIDATION_H
#define NOX_NATIVE_PAGE_SEARCH_VALIDATION_H
#include "nox/native_world.h"
#include "nox/native_quest_checkpoint.h"
#include "common/memstream.h"
#include "common/savefile.h"
namespace Nox {
inline bool nativePartyBytes(const Native::Party &party, const byte *expected) {
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);
    return party.save(saved) && saved.size() == 870 && !memcmp(saved.getData() + 6, expected, 864);
}
inline bool validateNativePageSearch(HdvArchive &archive, Common::SeekableReadStream &input,
		byte *acquiredFields, Native::Inventory &acquiredInventory, Common::String &report, Common::String &error, Common::SaveFileManager *checkpointFiles = nullptr) {
    if (input.readUint32BE() != MKTAG('N','P','S','R') || input.readUint32LE() != 1 || input.readUint32LE() != 7) return false;
    byte fields[256], records[1536], initialParty[864];
    if (input.read(fields, 256) != 256 || input.read(records, 1536) != 1536 || input.read(initialParty, 864) != 864 || fields[62]) return false;
    Native::Party party; Native::Inventory inventory; Native::ItemCatalog catalog; Native::SearchRules rules; Native::SearchState search;
    if (!party.create(archive, "TRACE", 0, 0) || !nativePartyBytes(party, initialParty) || !inventory.importRecords(records, 1536) ||
            !catalog.load(archive) || !rules.load(archive, 7)) return false;
    for (uint stage = 0; stage < 7; ++stage) {
        const byte map = input.readByte(), x = input.readByte(), y = input.readByte(), slot = input.readByte(), attempts = input.readByte(), prior = input.readByte();
        byte expectedTables[1792], expectedFields[256], expectedInventory[1536], expectedParty[864];
        if (input.read(expectedTables, 1792) != 1792 || input.read(expectedFields, 256) != 256 || input.read(expectedInventory, 1536) != 1536 || input.read(expectedParty, 864) != 864) return false;
        const uint32 expectedSeed = input.readUint32LE(); const byte expectedTail = input.readByte(), expectedLast = input.readByte(); const uint16 expectedGenerations = input.readUint16LE();
        // Only travel/adjacency, derived selection lookup and valid RNG state
        // are controlled at this boundary. Retain all inventory/quest/party
        // results from the preceding Search, with no award/count/flag writes.
        fields[174] = slot; Native::World world(archive); Native::GameplayRandom random;
        if (!world.initialize(7, map, byte(x - 1), y, fields, 256) || !random.importState(0x12345678, 2)) return false;
        if (!stage) {
            byte noTools[256]; memcpy(noTools, fields, 256); noTools[33] = 0;
            Native::World refusal(archive); Native::Party unchangedParty = party; Native::Inventory unchangedInventory = inventory; Native::GameplayRandom unchangedRandom = random; Native::SearchState unchangedSearch = search;
            bool promoted = false;
            if (!refusal.initialize(7, map, byte(x - 1), y, noTools, 256) ||
                    refusal.searchQuestItem(x, y, slot, 1, false, rules, catalog, unchangedSearch, unchangedRandom, unchangedParty, unchangedInventory, promoted) != Native::kSearchNoTools ||
                    memcmp(refusal.questFields(), noTools, 256) || !nativePartyBytes(unchangedParty, initialParty)) return false;
            byte unchanged[1536]; if (!unchangedInventory.exportRecords(unchanged, 1536) || memcmp(unchanged, records, 1536) || unchangedRandom.seed() != random.seed()) return false;
        }
        Native::SearchResult result = Native::kSearchRetry; bool promoted = false;
        uint count = 0;
        while (result == Native::kSearchRetry && count < 100) { result = world.searchQuestItem(x, y, slot, 1, false, rules, catalog, search, random, party, inventory, promoted); ++count; }
        const Common::Array<byte> *tables = world.currentTables();
        if (result != Native::kSearchAwarded || count != attempts || search.priorLockAttempts != prior || !tables || tables->size() != 1792 ||
                memcmp(tables->data(), expectedTables, 1792) || memcmp(world.questFields(), expectedFields, 256) ||
                !inventory.exportRecords(records, 1536) || memcmp(records, expectedInventory, 1536) || !nativePartyBytes(party, expectedParty) ||
                random.seed() != expectedSeed || random.tail() != expectedTail || random.last() != expectedLast || random.generationAttempts() != expectedGenerations) {
            error = Common::String::format("Native retained Search differs at stage%u result%u", stage, uint(result)); return false;
        }
        if (world.searchQuestItem(x, y, slot, 1, false, rules, catalog, search, random, party, inventory, promoted) != Native::kSearchNothing ||
                memcmp(world.questFields(), expectedFields, 256) || memcmp(world.currentTables()->data(), expectedTables, 1792) ||
                !inventory.exportRecords(records, 1536) || memcmp(records, expectedInventory, 1536) || !nativePartyBytes(party, expectedParty) || random.seed() != expectedSeed) return false;
        memcpy(fields, world.questFields(), 256);
        if (stage == 6 && checkpointFiles) {
            Common::OutSaveFile *output = checkpointFiles->openForSaving("nox-native-quest-checkpoint.bin", false);
            bool saved = output && Native::saveQuestCheckpoint(world, party, inventory, random, search, *output);
            if (output) { output->finalize(); saved = saved && !output->err(); } delete output;
            if (!saved) { error = "Native linked Search checkpoint did not save"; return false; }
            report += "native-quest-checkpoint\tphase=warm\tactual_Search_pages=7\towners=world,party,inventory,rng,search\tcomplete_game_save=0\n";
        }
    }
    const int pages = inventory.find(2, 97);
    if (input.err() || input.pos() != input.size() || fields[62] != 253 || pages < 0 || inventory.item(pages)->quantity() != 7) return false;
    memcpy(acquiredFields, fields, 256); acquiredInventory = inventory;
    report += "native-page-search\tawards=7\trepeats=7\tno_tools_refusal=1\tretained_party_inventory_fields=1\tcontrolled_travel_rng=1\tcpu_execution=0\n"; return true;
}
inline bool validateNativeQuestCheckpoint(HdvArchive &archive, Common::SaveFileManager &files, Common::SeekableReadStream &expected,
        byte *acquiredFields, Native::Inventory &acquiredInventory, Common::String &report, Common::String &error) {
    if (expected.readUint32BE() != MKTAG('N','P','S','R') || expected.readUint32LE() != 1 || expected.readUint32LE() != 7 ||
            expected.size() != 12 + 2656 + 7 * 4462 || !expected.seek(12 + 2656 + 6 * 4462)) return false;
    const byte map = expected.readByte(), x = expected.readByte(), y = expected.readByte(); expected.skip(2); const byte prior = expected.readByte();
    byte tables[1792], fields[256], records[1536], partyBytes[864], actualRecords[1536];
    if (expected.read(tables, 1792) != 1792 || expected.read(fields, 256) != 256 || expected.read(records, 1536) != 1536 || expected.read(partyBytes, 864) != 864) return false;
    const uint32 seed = expected.readUint32LE(); const byte tail = expected.readByte(), last = expected.readByte(); const uint16 generations = expected.readUint16LE();
    Native::World world(archive); Native::Party party; Native::GameplayRandom random; Native::SearchState search;
    Common::InSaveFile *input = files.openForLoading("nox-native-quest-checkpoint.bin");
    const bool loaded = input && Native::loadQuestCheckpoint(world, party, acquiredInventory, random, search, *input); delete input;
    const Common::Array<byte> *actualTables = world.currentTables();
    if (!loaded || world.mapId() != map || world.mapType() != 7 || world.x() != byte(x - 1) || world.y() != y ||
            !actualTables || actualTables->size() != 1792 || memcmp(actualTables->data(), tables, 1792) || memcmp(world.questFields(), fields, 256) ||
            !acquiredInventory.exportRecords(actualRecords, 1536) || memcmp(actualRecords, records, 1536) || !nativePartyBytes(party, partyBytes) ||
            search.priorLockAttempts != prior || random.seed() != seed || random.tail() != tail || random.last() != last || random.generationAttempts() != generations) {
        error = "Cold linked native Search checkpoint differs from original component results"; return false;
    }
    memcpy(acquiredFields, world.questFields(), 256);
    report += "native-quest-checkpoint\tphase=cold\tactual_Search_pages=7\towners=world,party,inventory,rng,search\tcomplete_game_save=0\n"; return true;
}
}
#endif
