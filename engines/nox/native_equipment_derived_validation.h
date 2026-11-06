/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_EQUIPMENT_DERIVED_VALIDATION_H
#define NOX_NATIVE_EQUIPMENT_DERIVED_VALIDATION_H
#include "nox/native_equipment_derived.h"
namespace Nox {
inline bool validateNativeEquipmentDerived(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','E','D','R') || input.readUint32LE() != 1 || input.readUint32LE() != 1185) return false;
    Native::ItemCatalog catalog; if (!catalog.load(archive)) return false;
    for (uint i = 0; i < 1185; ++i) {
        CharacterRecord character; EquipmentRecord equipment; byte expected[128];
        if (input.read(character.bytes,128) != 128 || input.read(equipment.bytes,16) != 16 || input.read(expected,128) != 128) return false;
        if (!Native::recalculateEquipment(catalog,equipment,character) || memcmp(character.bytes,expected,128)) {
            uint field = 0; while (field < 128 && character.bytes[field] == expected[field]) ++field;
            error = Common::String::format("Native equipment recalculation differs at case%u field%u",i,field); return false;
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-equipment-derived\tcases=1185\tall_DEX_values=256\tfull_character_record=1\tweapon_armor_fists_names_weight=1\tordinary_UI=0\tcpu_execution=0\n"; return true;
}
}
#endif
