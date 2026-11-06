/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CAST_VALIDATION_H
#define NOX_NATIVE_CAST_VALIDATION_H
#include "nox/native_cast.h"
namespace Nox {
inline bool validateNativeCast(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','C','S','T') || input.readUint32LE() != 1 || input.readUint32LE() != 520) return false;
    Native::CastRules rules; if (!rules.load(archive)) return false;
    uint prepared = 0;
    for (uint i = 0; i < 520; ++i) {
        const byte spell = input.readByte(), requirement = input.readByte(), battle = input.readByte(), intelligence = input.readByte(), mana = input.readByte(), result = input.readByte();
        const byte expectedSpell = input.readByte(), cost = input.readByte(), reach = input.readByte(), target = input.readByte();
        byte slots[20] = {}, after[20]; memset(slots, 255, 10); slots[1] = spell; slots[11] = requirement;
        Native::SpellBook book; if (!book.importState(slots, 20)) return false;
        CharacterRecord caster; caster.bytes[18] = intelligence; caster.bytes[4] = mana; const CharacterRecord before = caster;
        Native::CastPreparation intent;
        const Native::CastResult actual = rules.prepare(book, caster, 1, battle != 0, intent);
        if (uint(actual) != result || memcmp(caster.bytes, before.bytes, 128) || !book.exportState(after, 20) || memcmp(after, slots, 20) ||
                (actual == Native::kCastPrepared && (intent.spell != expectedSpell || intent.manaCost != cost || intent.basePower != reach || intent.levelThreshold != target))) {
            error = Common::String::format("Native player cast preparation differs at case%u result%u", i, uint(actual)); return false;
        }
        prepared += actual == Native::kCastPrepared;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-cast\tcases=520\tprepared=%u\tstandard_player=1\tmana_committed=0\teffects=0\tcpu_execution=0\n", prepared); return true;
}
}
#endif
