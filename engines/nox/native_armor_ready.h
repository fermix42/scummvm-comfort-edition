/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ARMOR_READY_H
#define NOX_NATIVE_ARMOR_READY_H
#include "nox/native_equipment_ownership.h"
namespace Nox { namespace Native {
enum ArmorReadyResult { kArmorReadied = 0, kArmorCopiesRefused = 2, kArmorWeightRefused = 3,
    kArmorDexterityRefused = 4, kArmorIntelligenceRefused = 5, kArmorUnreadied = 7, kArmorUnsupported = 255 };
// World armor transaction before deferred derived-stat recalculation. Caller
// must complete recalculation before treating Ready as a normal player command.
inline ArmorReadyResult selectWorldArmor(const ItemCatalog &catalog, const EquipmentOwnership &ownership,
        byte member, byte itemId, bool unready, Inventory &inventory, CharacterRecord &character, EquipmentRecord &equipment) {
    byte mask, clear; ItemDefinition selected;
    const int selectedSlot = inventory.find(1,itemId);
    if (selectedSlot < 0 || !ownership.masks(member,mask,clear) || !catalog.definition(1,itemId,selected)) return kArmorUnsupported;
    const byte category = selected.bytes[12] & 31;
    const InventoryItem &item = *inventory.item(selectedSlot);
    if (category < 2 || category > 7 || !item.quantity() || (item.bytes[5] & 31) != category) return kArmorUnsupported;
    byte nextWeight = character.bytes[70]; EquipmentOwners owners;
    if (unready) {
        if (!(item.bytes[3] & mask)) return kArmorUnsupported;
        nextWeight -= selected.bytes[2];
    } else {
        if (!ownership.resolve(inventory,member,category,owners)) return kArmorUnsupported;
        if (owners.first >= 0) {
            ItemDefinition previous; const InventoryItem &old = *inventory.item(owners.first);
            if (!catalog.definition(old.kind(),old.id(),previous)) return kArmorUnsupported;
            nextWeight -= previous.bytes[2];
        }
        nextWeight += selected.bytes[2];
        // Preserve the original byte arithmetic and refusal ordering.
        if (nextWeight > character.bytes[16]) return kArmorWeightRefused;
        if (character.bytes[17] < selected.bytes[3]) return kArmorDexterityRefused;
        if (character.bytes[18] < selected.bytes[4]) return kArmorIntelligenceRefused;
        byte copies; if (!ownership.readyCopies(item,copies)) return kArmorUnsupported;
        if (copies >= item.quantity()) return kArmorCopiesRefused;
    }
    byte records[1536]; if (!inventory.exportRecords(records,1536)) return kArmorUnsupported;
    if (unready) records[selectedSlot * 6 + 3] &= clear;
    else {
        if (owners.first >= 0) records[owners.first * 6 + 3] &= clear;
        records[selectedSlot * 6 + 3] |= mask;
    }
    Inventory nextInventory; if (!nextInventory.importRecords(records,1536)) return kArmorUnsupported;
    EquipmentRecord nextEquipment = equipment;
    nextEquipment.bytes[category * 2] = 1; nextEquipment.bytes[category * 2 + 1] = unready ? 0 : itemId;
    CharacterRecord nextCharacter = character; nextCharacter.bytes[70] = nextWeight;
    inventory = nextInventory; character = nextCharacter; equipment = nextEquipment;
    return unready ? kArmorUnreadied : kArmorReadied;
}
} }
#endif
