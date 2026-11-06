/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_EQUIPMENT_OWNERSHIP_VALIDATION_H
#define NOX_NATIVE_EQUIPMENT_OWNERSHIP_VALIDATION_H
#include "nox/native_equipment_ownership.h"
namespace Nox {
inline bool validateNativeEquipmentOwnership(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','E','O','W') || input.readUint32LE() != 1 || input.readUint32LE() != 432) return false;
    Native::EquipmentOwnership ownership; if (!ownership.load(archive)) return false;
    for (uint i = 0; i < 432; ++i) {
        const byte member = input.readByte(), category = input.readByte(); byte bytes[1536], after[1536];
        if (input.read(bytes,1536) != 1536) return false;
        const int first = input.readSint16LE(); const byte firstCopies = input.readByte();
        const int second = input.readSint16LE(); const byte secondCopies = input.readByte(), hands = input.readByte(), copies = input.readByte();
        Native::Inventory inventory; Native::EquipmentOwners owners;
        uint occupied = 0; while (occupied < 256 && bytes[occupied * 6] == 254) ++occupied;
        byte actualCopies = 0;
        if (occupied >= 256 || !inventory.importRecords(bytes,1536) || !ownership.resolve(inventory,member,category,owners) ||
            !ownership.readyCopies(*inventory.item(occupied),actualCopies) || actualCopies != copies || owners.first != first || owners.second != second ||
            owners.firstCopies != firstCopies || owners.secondCopies != secondCopies || owners.hands != hands ||
            !inventory.exportRecords(after,1536) || memcmp(bytes,after,1536)) {
            error = Common::String::format("Native equipment ownership differs at case%u",i); return false;
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-equipment-ownership\tcases=432\tparty_members=6\tcategory_and_weapon_ownership=1\tall_party_ready_count=1\tinventory_unchanged=1\tactual_Ready=0\tcpu_execution=0\n"; return true;
}
}
#endif
