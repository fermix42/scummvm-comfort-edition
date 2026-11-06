/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_EQUIPMENT_OWNERSHIP_H
#define NOX_NATIVE_EQUIPMENT_OWNERSHIP_H
#include "nox/native_inventory.h"
namespace Nox { namespace Native {
struct EquipmentOwners {
    int first, second; byte firstCopies, secondCopies, hands;
    EquipmentOwners() : first(-1), second(-1), firstCopies(0), secondCopies(0), hands(0) {}
};
// Resolve existing equipment ownership. This is read-only: Ready eligibility,
// equip/unequip commits, derived stats and inventory UI remain separate.
class EquipmentOwnership {
public:
    EquipmentOwnership() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;
        if (!archive.readRange("NA/SRTN.INVENTORY",0x4d91,14,data) || data.size() != 14) return false;
        for (uint i = 0; i < 6; ++i) { _masks[i] = data[i + 1]; _clear[i] = data[i + 8]; }
        _ready = true; return true;
    }
    bool masks(byte member, byte &set, byte &clear) const {
        if (!_ready || member >= 6) return false;
        set = _masks[member]; clear = _clear[member]; return true;
    }
    bool readyCopies(const InventoryItem &item, byte &output) const {
        if (!_ready) return false;
        byte count = 0;
        for (uint i = 0; i < 6; ++i) {
            count += (item.bytes[3] & _masks[i]) != 0;
            count += (item.bytes[4] & _masks[i]) != 0;
        }
        output = count; return true;
    }
    bool resolve(const Inventory &inventory, byte member, byte category, EquipmentOwners &output) const {
        if (!_ready || member >= 6 || category > 31) return false;
        EquipmentOwners next;
        for (uint i = 0; i < 256; ++i) {
            const InventoryItem &item = *inventory.item(i);
            if (item.empty() || (item.bytes[5] & 31) != category) continue;
            const byte first = (item.bytes[3] & _masks[member]) != 0;
            if (category != 1) {
                if (first) { next.first = i; next.firstCopies = 1; output = next; return true; }
                continue;
            }
            const byte copies = first + ((item.bytes[4] & _masks[member]) != 0);
            if (!copies) continue;
            next.hands += copies + ((item.bytes[5] & 128) != 0);
            if (next.first < 0) {
                next.first = i; next.firstCopies = copies;
                if (copies < 2) continue;
            }
            if (next.second >= 0) return false; // Original malformed third-record fatal domain.
            next.second = i; next.secondCopies = copies;
        }
        output = next; return true;
    }
private:
    bool _ready; byte _masks[6], _clear[6];
};
} }
#endif
