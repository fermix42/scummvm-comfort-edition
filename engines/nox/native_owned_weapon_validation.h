/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_OWNED_WEAPON_VALIDATION_H
#define NOX_NATIVE_OWNED_WEAPON_VALIDATION_H
#include "nox/native_equipment_actions.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeOwnedWeapon(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','O','W','R') || input.readUint32LE() != 1 || input.readUint32LE() != 1111) return false;
    Native::ItemCatalog catalog; Native::EquipmentOwnership ownership;
    if (!catalog.load(archive) || !ownership.load(archive)) return false;
    uint committed = 0;
    for (uint i = 0; i < 1111; ++i) {
        const byte id = input.readByte(), profile = input.readByte();
        byte records[1536], partyBytes[870], expected[2400], actualRecords[1536];
        memcpy(partyBytes,"NPAR",4); WRITE_LE_UINT16(partyBytes + 4,1);
        if (input.read(partyBytes + 6,864) != 864 || input.read(records,1536) != 1536) return false;
        const byte result = input.readByte(); if (input.read(expected,2400) != 2400) return false;
        Common::MemoryReadStream partyInput(partyBytes,870); Native::Party party; Native::Inventory inventory;
        if (!party.load(partyInput) || party.memberCount() != 1 || !inventory.importRecords(records,1536)) return false;
        const Native::WeaponReadyResult outcome = Native::EquipmentActions::selectWeapon(catalog,ownership,0,id,profile == 4,party,inventory);
        Common::MemoryWriteStreamDynamic actual(DisposeAfterUse::YES);
        if (uint(outcome) != result || !party.save(actual) || actual.size() != 870 || memcmp(actual.getData() + 6,expected,864) ||
            !inventory.exportRecords(actualRecords,1536) || memcmp(actualRecords,expected + 864,1536)) {
            error = Common::String::format("Native owned weapon Ready/recalculation differs at case%u",i); return false;
        }
        committed += outcome == Native::kWeaponReadied || outcome == Native::kWeaponUnreadied;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-owned-weapon\tcases=1111\tcomplete_commits=%u\tactual_Party_Inventory_owners=1\tderived_recalculation=1\tfull_records=1\tordinary_UI=0\tcpu_execution=0\n",committed); return true;
}
}
#endif
