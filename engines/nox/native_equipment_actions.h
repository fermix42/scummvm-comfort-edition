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
            byte member, byte itemId, bool unready, Party &party, Inventory &inventory,byte *lastRemovedId=nullptr,bool inCombat=false) {
        if (!party._ready || member >= party.memberCount()) return kWeaponUnsupported;
        Party nextParty = party; Inventory nextInventory = inventory;
        const WeaponReadyResult result = selectWorldWeapon(catalog,ownership,member,itemId,unready,nextInventory,
            nextParty._members[member],nextParty._equipment[member],0,lastRemovedId);
        if (result != kWeaponReadied && result != kWeaponUnreadied) return result;
        if (!recalculateSelection(catalog,party,nextParty,inCombat)) return kWeaponUnsupported;
        party = nextParty; inventory = nextInventory; return result;
    }
    // Source9A12/B563 recalculates only globally dirty equipment domains,
    // preserving transaction weight and unrelated temporary combat effects.
    static bool recalculateSelection(const ItemCatalog &catalog,const Party &before,Party &after,bool inCombat) {
        bool weapons=!inCombat,armor=!inCombat;
        if (inCombat) for (uint i=0;i<after.memberCount();++i) {
            const byte *old=before._equipment[i].bytes,*now=after._equipment[i].bytes;
            for (uint hand=0;hand<2;++hand) if (memcmp(old+hand*2,now+hand*2,2)) {
                weapons|=old[hand*2]==0 || now[hand*2]==0;
                armor|=old[hand*2]==1 || now[hand*2]==1;
            }
            armor|=memcmp(old+4,now+4,12)!=0;
        }
        if (!weapons && !armor) return true;
        for (uint i=0;i<after.memberCount();++i)
            if (!recalculateEquipment(catalog,after._equipment[i],after._members[i],weapons,armor,!inCombat)) return false;
        return true;
    }
    // Commit through the actual Party/Inventory owners. A failed prerequisite
    // or unsupported recalculation changes neither owner.
    static ArmorReadyResult selectArmor(const ItemCatalog &catalog, const EquipmentOwnership &ownership,
            byte member, byte itemId, bool unready, Party &party, Inventory &inventory,byte *lastRemovedId=nullptr,bool inCombat=false) {
        if (!party._ready || member >= party.memberCount()) return kArmorUnsupported;
        if (inCombat) {
            const int slot=inventory.find(1,itemId);
            if (slot<0) return kArmorUnsupported;
            if (inventory.item(slot)->bytes[5]!=1) return ArmorReadyResult(6);
        }
        Party nextParty = party; Inventory nextInventory = inventory;
        ItemDefinition definition;if (!catalog.definition(1,itemId,definition)) return kArmorUnsupported;
        const ArmorReadyResult result = (definition.bytes[12]&31)==1 ? ArmorReadyResult(selectWorldWeapon(catalog,ownership,member,itemId,unready,nextInventory,nextParty._members[member],nextParty._equipment[member],1,lastRemovedId)) :
            selectWorldArmor(catalog,ownership,member,itemId,unready,nextInventory,nextParty._members[member],nextParty._equipment[member],lastRemovedId);
        if (result != kArmorReadied && result != kArmorUnreadied) return result;
        if (!recalculateSelection(catalog,party,nextParty,inCombat)) return kArmorUnsupported;
        party = nextParty; inventory = nextInventory; return result;
    }
};
} }
#endif
