/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_EQUIPMENT_DERIVED_H
#define NOX_NATIVE_EQUIPMENT_DERIVED_H
#include "nox/native_inventory.h"
namespace Nox { namespace Native {
// Complete full-recalculation variant for one owned character record. Field
// ordering and original quirks are preserved; no CPU or global scratch state.
inline bool recalculateEquipment(const ItemCatalog &catalog, const EquipmentRecord &equipment, CharacterRecord &character) {
    CharacterRecord next = character; byte *r = next.bytes;
    r[70] = r[21] = r[22] = r[23] = r[9] = r[10] = r[65] = 0;
    auto weight = [&](byte amount) { r[70] = byte(MIN<uint>(uint(r[70]) + amount,255)); };
    auto name = [&](uint hand, const ItemDefinition &item) { memcpy(r + (hand ? 109 : 90),item.bytes + 13,19); };
    auto fists = [&](uint hand) { const uint start = hand ? 109 : 90; memset(r + start,173,17); r[start + 17] = 0; };
    for (uint hand = 0; hand < 2; ++hand) {
        const byte kind = equipment.bytes[hand * 2], id = equipment.bytes[hand * 2 + 1];
        if (kind != 0) {
            if (!hand) r[21] = 16; else r[21] |= 1;
            if (kind == 3) fists(hand);
            continue;
        }
        ItemDefinition item; if (!catalog.definition(kind,id,item)) return false;
        const byte *d = item.bytes; const byte type = d[11] & 127;
        name(hand,item); weight(d[2]);
        if (!hand) {
            r[20] = (r[20] & 127) | (d[0] & 128);
            r[21] = byte(type << 4); r[22] = byte(d[9] << 4); r[23] = d[10]; r[64] = d[8];
        } else {
            r[20] = (r[20] & 191) | ((d[0] & 128) ? 64 : 0);
            r[21] |= type; r[22] |= d[9]; if (d[10]) r[23] |= 128; r[65] = d[8];
        }
        const byte half = r[type >= 1 ? 28 : 25] >> 1;
        const byte skill = d[7] < half ? d[7] : MAX<byte>(half,1);
        const uint damage = (d[11] ? 0 : r[16] >> 3) + uint(d[5]) + skill;
        if (damage > 255) {
            // Original second-hand overflow also writes the first-hand field.
            r[9] = 255;
        } else r[hand ? 10 : 9] = byte(damage);
    }
    r[7] = r[11] = 0;
    auto armor = [&](byte kind, byte id) -> bool {
        ItemDefinition item; if (!catalog.definition(kind,id,item)) return false;
        r[7] = byte(r[7] + item.bytes[5]); weight(item.bytes[2]);
        r[11] = byte((r[11] & 127) + item.bytes[0]); return true;
    };
    for (uint hand = 0; hand < 2; ++hand) {
        const byte kind = equipment.bytes[hand * 2], id = equipment.bytes[hand * 2 + 1];
        if (kind == 1) {
            ItemDefinition item; if (!armor(kind,id) || !catalog.definition(kind,id,item)) return false;
            name(hand,item);
        } else if (kind == 3) fists(hand);
    }
    for (uint slot = 2; slot < 8; ++slot)
        if (!armor(equipment.bytes[slot * 2],equipment.bytes[slot * 2 + 1])) return false;
    // The original converts DEX to decimal, DIVIDES by3, adds decimal50,
    // caps95 and stores packed BCD. All256 input values are compared.
    const uint chance = MIN<uint>(50 + r[17] / 3,95);
    r[8] = byte((chance / 10) * 16 + chance % 10);
    character = next; return true;
}
} }
#endif
