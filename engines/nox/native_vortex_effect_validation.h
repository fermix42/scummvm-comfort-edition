/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VORTEX_EFFECT_VALIDATION_H
#define NOX_NATIVE_VORTEX_EFFECT_VALIDATION_H
#include "nox/native_vortex_effect.h"
namespace Nox {
inline bool validateNativeVortexEffect(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','V','H','T') || input.readUint32LE() != 1 || input.readUint32LE() != 3600) return false;
    Native::VortexEffects effects; if (!effects.load(archive)) return false;
    uint killed = 0;
    for (uint i = 0; i < 3600; ++i) {
        CharacterRecord caster; Native::VortexVictim victim; byte indices[187], kinds[187];
        if (input.read(caster.bytes, 128) != 128) return false;
        victim.kind = input.readByte(); const byte large = input.readByte(), protection = input.readByte(), category = input.readByte(); uint16 pool = input.readUint16LE();
        victim.cell = 94; victim.index = 4;
        if (input.read(victim.stats, 128) != 128 || input.read(victim.actor, 8) != 8 || input.read(indices, 187) != 187 || input.read(kinds, 187) != 187) return false;
        byte expectedStats[128], expectedActor[8], expectedIndices[187], expectedKinds[187];
        if (input.read(expectedStats, 128) != 128 || input.read(expectedActor, 8) != 8 || input.read(expectedIndices, 187) != 187 || input.read(expectedKinds, 187) != 187) return false;
        const byte result = input.readByte(), death = input.readByte(); const uint16 damage = input.readUint16LE(), expectedPool = input.readUint16LE();
        Native::VortexHit hit;
        if (bool(victim.actor[3] & 8) != (large != 0) || !effects.resolve(caster, category, protection != 0, victim, indices, kinds, 187, pool, hit) ||
                (hit.result == Native::kVortexKilled ? 0 : 3) != result || (hit.result == Native::kVortexKilled ? 2 : 0) != death || hit.nominalDamage != damage || pool != expectedPool ||
                memcmp(victim.stats, expectedStats, 128) || memcmp(victim.actor, expectedActor, 8) || memcmp(indices, expectedIndices, 187) || memcmp(kinds, expectedKinds, 187)) {
            error = Common::String::format("Native Vortex target resolution differs at case%u", i); return false;
        }
        killed += hit.result == Native::kVortexKilled;
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-vortex-effect\tcases=3600\tkilled=%u\tfull_record_actor_lookup_credit=1\twhole_cast=0\tmana_committed=0\tcpu_execution=0\n", killed); return true;
}
}
#endif
