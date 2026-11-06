/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WEAPON_READY_H
#define NOX_NATIVE_WEAPON_READY_H
#include "nox/native_equipment_ownership.h"
namespace Nox { namespace Native {
enum WeaponReadyResult { kWeaponReadied = 0, kWeaponCopiesRefused = 2, kWeaponWeightRefused = 3,
    kWeaponDexterityRefused = 4, kWeaponIntelligenceRefused = 5, kWeaponUnreadied = 7, kWeaponUnsupported = 255 };
// Standard world weapon transaction before deferred derived recalculation.
// Shield ownership may be retained/replaced; selecting a new shield is separate.
inline WeaponReadyResult selectWorldWeapon(const ItemCatalog &catalog, const EquipmentOwnership &ownership,
        byte member, byte itemId, bool unready, Inventory &inventory, CharacterRecord &character, EquipmentRecord &equipment) {
    byte mask, clear; ItemDefinition selected; const int target = inventory.find(0,itemId);
    if (target < 0 || !ownership.masks(member,mask,clear) || !catalog.definition(0,itemId,selected) ||
        (selected.bytes[12] & 31) != 1 || !inventory.item(target)->quantity()) return kWeaponUnsupported;
    EquipmentOwners owners; if (!ownership.resolve(inventory,member,1,owners)) return kWeaponUnsupported;
    int removals[2] = {owners.first,owners.second}; bool keepExisting = false;
    const bool twoHanded = (selected.bytes[11] & 128) != 0;
    byte weight = character.bytes[70];
    if (unready) {
        if (!((inventory.item(target)->bytes[3] | inventory.item(target)->bytes[4]) & mask)) return kWeaponUnsupported;
        removals[0] = target; removals[1] = -1; weight -= selected.bytes[2];
    } else {
        if (!twoHanded && owners.hands >= 2 && owners.second >= 0) {
            if (inventory.item(owners.second)->kind() != 0) removals[1] = -1;
            else if (inventory.item(owners.first)->kind() != 0) { removals[0] = owners.second; removals[1] = -1; }
        }
        byte removedWeight = 0;
        for (uint i = 0; i < 2; ++i) if (removals[i] >= 0) {
            const InventoryItem &old = *inventory.item(removals[i]); ItemDefinition definition;
            if (!catalog.definition(old.kind(),old.id(),definition)) return kWeaponUnsupported;
            removedWeight += definition.bytes[2];
        }
        const byte preflightWeight = byte(weight - removedWeight + selected.bytes[2]);
        if (preflightWeight > character.bytes[16]) return kWeaponWeightRefused;
        if (character.bytes[17] < selected.bytes[3]) return kWeaponDexterityRefused;
        if (character.bytes[18] < selected.bytes[4]) return kWeaponIntelligenceRefused;
        byte copies; if (!ownership.readyCopies(*inventory.item(target),copies)) return kWeaponUnsupported;
        if (copies >= inventory.item(target)->quantity()) return kWeaponCopiesRefused;
        const byte combined = byte(weight + selected.bytes[2]);
        keepExisting = !twoHanded && owners.hands == 1 && combined && combined <= character.bytes[16];
        weight = keepExisting ? combined : preflightWeight;
    }
    byte records[1536]; if (!inventory.exportRecords(records,1536)) return kWeaponUnsupported;
    EquipmentRecord next = equipment;
    auto remove = [&](int slot) -> bool {
        const InventoryItem &old = *inventory.item(slot); byte *record = records + slot * 6;
        const bool ownedFirst = (record[3] & mask) != 0; record[3] &= clear;
        if (!ownedFirst || (!unready && twoHanded)) record[4] &= clear;
        if (old.bytes[5] & 128) { next.bytes[0] = next.bytes[1] = 0; next.bytes[2] = 3; return true; }
        uint hand;
        if (next.bytes[0] == old.kind() && next.bytes[1] == old.id()) hand = 0;
        else if (next.bytes[2] == old.kind() && next.bytes[3] == old.id()) hand = 1;
        else return false;
        next.bytes[hand * 2] = 3;
        if (unready && next.bytes[0] && byte(next.bytes[0] + next.bytes[2]) != 2 && next.bytes[2])
            next.bytes[hand * 2] = next.bytes[hand * 2 + 1] = 0;
        return true;
    };
    if (!keepExisting) for (uint i = 0; i < 2; ++i) if (removals[i] >= 0 && !remove(removals[i])) return kWeaponUnsupported;
    if (!unready) {
        byte *record = records + target * 6;
        if (!(record[3] & mask)) record[3] |= mask;
        else if (!(record[4] & mask)) record[4] |= mask;
        else return kWeaponUnsupported;
        if (twoHanded) { next.bytes[0] = 0; next.bytes[1] = itemId; next.bytes[2] = 3; }
        else {
            uint hand;
            if (next.bytes[0] == 3 || (!next.bytes[0] && !next.bytes[1])) hand = 0;
            else if (next.bytes[2] == 3 || (!next.bytes[2] && !next.bytes[3])) hand = 1;
            else return kWeaponUnsupported;
            next.bytes[hand * 2] = 0; next.bytes[hand * 2 + 1] = itemId;
        }
    }
    Inventory nextInventory; if (!nextInventory.importRecords(records,1536)) return kWeaponUnsupported;
    CharacterRecord nextCharacter = character; nextCharacter.bytes[70] = weight;
    inventory = nextInventory; character = nextCharacter; equipment = next;
    return unready ? kWeaponUnreadied : kWeaponReadied;
}
} }
#endif
