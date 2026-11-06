/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_EQUIPMENT_ACTIONS_H
#define NOX_NATIVE_EQUIPMENT_ACTIONS_H
#include "nox/native_party.h"
#include "nox/native_armor_ready.h"
#include "nox/native_equipment_derived.h"
#include "nox/native_weapon_ready.h"
namespace Nox { namespace Native {
class EquipmentActions {
public:
    static bool recalculateAll(const ItemCatalog &catalog, Party &party) {
        if (!party._ready) return false;
        Party next = party;
        for (uint i = 0; i < next.memberCount(); ++i)
            if (!recalculateEquipment(catalog,next._equipment[i],next._members[i])) return false;
        party = next; return true;
    }
    static WeaponReadyResult selectWeapon(const ItemCatalog &catalog, const EquipmentOwnership &ownership,
            byte member, byte itemId, bool unready, Party &party, Inventory &inventory) {
        if (!party._ready || member >= party.memberCount()) return kWeaponUnsupported;
        Party nextParty = party; Inventory nextInventory = inventory;
        const WeaponReadyResult result = selectWorldWeapon(catalog,ownership,member,itemId,unready,nextInventory,
            nextParty._members[member],nextParty._equipment[member]);
        if (result != kWeaponReadied && result != kWeaponUnreadied) return result;
        for (uint i = 0; i < nextParty.memberCount(); ++i)
            if (!recalculateEquipment(catalog,nextParty._equipment[i],nextParty._members[i])) return kWeaponUnsupported;
        party = nextParty; inventory = nextInventory; return result;
    }
    // Commit through the actual Party/Inventory owners. A failed prerequisite
    // or unsupported recalculation changes neither owner.
    static ArmorReadyResult selectArmor(const ItemCatalog &catalog, const EquipmentOwnership &ownership,
            byte member, byte itemId, bool unready, Party &party, Inventory &inventory) {
        if (!party._ready || member >= party.memberCount()) return kArmorUnsupported;
        Party nextParty = party; Inventory nextInventory = inventory;
        const ArmorReadyResult result = selectWorldArmor(catalog,ownership,member,itemId,unready,nextInventory,
            nextParty._members[member],nextParty._equipment[member]);
        if (result != kArmorReadied && result != kArmorUnreadied) return result;
        for (uint i = 0; i < nextParty.memberCount(); ++i)
            if (!recalculateEquipment(catalog,nextParty._equipment[i],nextParty._members[i])) return kArmorUnsupported;
        party = nextParty; inventory = nextInventory; return result;
    }
};
} }
#endif
