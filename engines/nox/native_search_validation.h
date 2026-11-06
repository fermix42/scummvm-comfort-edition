/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SEARCH_VALIDATION_H
#define NOX_NATIVE_SEARCH_VALIDATION_H
#include "nox/native_search.h"
namespace Nox {
inline bool validateNativeSearchReward(HdvArchive &archive, Common::SeekableReadStream &input,
        Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','S','R','W') || input.readUint32LE() != 1 || input.readUint32LE() != 270) return false;
    Native::ItemCatalog catalog; if (!catalog.load(archive)) return false;
    uint awarded = 0, repeated = 0, capacity = 0;
    for (uint i = 0; i < 270; ++i) {
        byte objects[256], fields[256], records[1536], expectedObjects[256], expectedFields[256], expectedRecords[1536];
        if (input.read(objects, 256) != 256 || input.read(fields, 256) != 256 || input.read(records, 1536) != 1536 ||
                input.read(expectedObjects, 256) != 256 || input.read(expectedFields, 256) != 256 || input.read(expectedRecords, 1536) != 1536) return false;
        Native::Inventory inventory; if (!inventory.importRecords(records, sizeof(records))) return false;
        const Native::SearchRewardResult result = Native::searchQuestReward(objects, sizeof(objects), fields, sizeof(fields), catalog, inventory);
        if (result == Native::kSearchRewardUnsupported || !inventory.exportRecords(records, sizeof(records)) ||
                memcmp(objects, expectedObjects, 256) || memcmp(fields, expectedFields, 256) || memcmp(records, expectedRecords, 1536)) {
            error = Common::String::format("Native Search reward differs at case%u result%u", i, uint(result)); return false;
        }
        if (result == Native::kSearchRewardAwarded) ++awarded;
        else if (result == Native::kSearchRewardRepeated) ++repeated;
        else if (result == Native::kSearchRewardCapacity) ++capacity;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-search-reward\tcases=270\tawards=%u\trepeats=%u\tcapacity=%u\tlock_phase=0\tcpu_execution=0\n", awarded, repeated, capacity);
    return true;
}
}
#endif
