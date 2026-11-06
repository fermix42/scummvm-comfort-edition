/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ARMOR_READY_VALIDATION_H
#define NOX_NATIVE_ARMOR_READY_VALIDATION_H
#include "nox/native_armor_ready.h"
namespace Nox {
inline bool validateNativeArmorReady(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','A','R','D') || input.readUint32LE() != 1 || input.readUint32LE() != 742) return false;
    Native::ItemCatalog catalog; Native::EquipmentOwnership ownership;
    if (!catalog.load(archive) || !ownership.load(archive)) return false;
    uint applied = 0, refused = 0;
    for (uint i = 0; i < 742; ++i) {
        const byte id = input.readByte(), profile = input.readByte();
        byte party[768], equipment[96], records[1536], expectedParty[768], expectedEquipment[96], expectedRecords[1536], actual[1536];
        if (input.read(party,768) != 768 || input.read(equipment,96) != 96 || input.read(records,1536) != 1536) return false;
        const byte result = input.readByte();
        if (input.read(expectedParty,768) != 768 || input.read(expectedEquipment,96) != 96 || input.read(expectedRecords,1536) != 1536) return false;
        Native::Inventory inventory; CharacterRecord character; EquipmentRecord equipped;
        memcpy(character.bytes,party,128); memcpy(equipped.bytes,equipment,16);
        if (!inventory.importRecords(records,1536)) return false;
        const Native::ArmorReadyResult outcome = Native::selectWorldArmor(catalog,ownership,0,id,profile == 4,inventory,character,equipped);
        memcpy(party,character.bytes,128); memcpy(equipment,equipped.bytes,16);
        if (uint(outcome) != result || !inventory.exportRecords(actual,1536) || memcmp(party,expectedParty,768) ||
            memcmp(equipment,expectedEquipment,96) || memcmp(actual,expectedRecords,1536)) {
            error = Common::String::format("Native armor transaction differs at case%u item%u profile%u result%u expected%u",i,id,profile,uint(outcome),result); return false;
        }
        applied += outcome == Native::kArmorReadied || outcome == Native::kArmorUnreadied;
        refused += outcome != Native::kArmorReadied && outcome != Native::kArmorUnreadied;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-armor-ready\tcases=742\tapplied=%u\trefused=%u\tfull_party_equipment_inventory=1\tdeferred_recalculation=1\tordinary_UI=0\tcpu_execution=0\n",applied,refused); return true;
}
}
#endif
