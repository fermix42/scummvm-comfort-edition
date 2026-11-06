/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WEAPON_READY_VALIDATION_H
#define NOX_NATIVE_WEAPON_READY_VALIDATION_H
#include "nox/native_weapon_ready.h"
namespace Nox {
inline bool validateNativeWeaponReady(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','W','R','D') || input.readUint32LE() != 1 || input.readUint32LE() != 1111) return false;
    Native::ItemCatalog catalog; Native::EquipmentOwnership ownership;
    if (!catalog.load(archive) || !ownership.load(archive)) return false;
    uint committed = 0;
    for (uint i = 0; i < 1111; ++i) {
        const byte id = input.readByte(), profile = input.readByte();
        byte party[768], equipment[96], records[1536], expected[2400], actual[1536];
        if (input.read(party,768) != 768 || input.read(equipment,96) != 96 || input.read(records,1536) != 1536) return false;
        const byte result = input.readByte(); if (input.read(expected,2400) != 2400) return false;
        Native::Inventory inventory; CharacterRecord character; EquipmentRecord equipped;
        memcpy(character.bytes,party,128); memcpy(equipped.bytes,equipment,16);
        if (!inventory.importRecords(records,1536)) return false;
        const Native::WeaponReadyResult outcome = Native::selectWorldWeapon(catalog,ownership,0,id,profile == 4,inventory,character,equipped);
        memcpy(party,character.bytes,128); memcpy(equipment,equipped.bytes,16);
        if (uint(outcome) != result || memcmp(party,expected,768) || memcmp(equipment,expected + 768,96) ||
            !inventory.exportRecords(actual,1536) || memcmp(actual,expected + 864,1536)) {
            error = Common::String::format("Native weapon transaction differs at case%u item%u profile%u result%u expected%u",i,id,profile,uint(outcome),result); return false;
        }
        committed += outcome == Native::kWeaponReadied || outcome == Native::kWeaponUnreadied;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-weapon-ready\tcases=1111\tcommitted=%u\tfull_party_equipment_inventory=1\tdeferred_recalculation=1\tordinary_UI=0\tcpu_execution=0\n",committed); return true;
}
}
#endif
