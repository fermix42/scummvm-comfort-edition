/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VORTEX_EFFECT_H
#define NOX_NATIVE_VORTEX_EFFECT_H
#include "nox/records.h"
#include "nox/resources.h"
namespace Nox { namespace Native {
struct VortexVictim {
    byte kind, cell, index;
    byte stats[128], actor[8];
    VortexVictim() : kind(0), cell(0), index(0) { memset(stats, 0, sizeof(stats)); memset(actor, 0, sizeof(actor)); }
};
enum VortexHitResult { kVortexKilled, kVortexResisted, kVortexSuppressed };
struct VortexHit {
    VortexHitResult result; uint16 nominalDamage, credit;
    VortexHit() : result(kVortexResisted), nominalDamage(0), credit(0) {}
};
// Standard-player Vortex resolution on a living, already selected actor.
// Ordinary damage rolls are a separate spell path. This owner commits record,
// actor/lookup removal and kill credit; whole-cast input/animation/turn/mana and
// battle rewards-to-party/world progression remain separate responsibilities.
class VortexEffects {
public:
    VortexEffects() : _ready(false), _specialPercent(0) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> percentages, special;
        if (!archive.readRange("NA/BS.AUX.RTN.BK2", 0x2688, 6, percentages) || percentages.size() != 6 ||
                !archive.readRange("NA/BS.AUX.RTN.BK2", 0x2744, 1, special) || special.size() != 1) return false;
        // One original special-case read indexes beyond the six-entry table.
        // Treat its observed resource byte as named compatibility data, never
        // as an executable instruction or a general memory/PC interface.
        memcpy(_percentages, percentages.data(), 6); _specialPercent = special[0]; _ready = true; return true;
    }
    bool resolve(const CharacterRecord &caster, byte casterCategory, bool playerDamageSuppressed,
            VortexVictim &victim, byte *indices, byte *kinds, uint length, uint16 &creditPool, VortexHit &output) const {
        if (!_ready || !indices || !kinds || length != 187 || victim.cell >= 187 || victim.kind < 1 || victim.kind > 3 ||
                indices[victim.cell] != victim.index || (kinds[victim.cell] & 135) != victim.kind || !READ_LE_UINT16(victim.stats + 2)) return false;
        const bool large = (victim.actor[3] & 8) != 0;
        if (large && (victim.kind == 3 || victim.cell % 17 == 16 || victim.cell + 18 >= 187 || !(kinds[victim.cell] & 64))) return false;
        VortexHit hit;
        const byte power = caster.bytes[34] ? caster.bytes[18] : caster.bytes[6] & 127;
        const byte guard = kinds[victim.cell] == 3 ? victim.stats[18] : victim.stats[6] & 127;
        if (power < guard) { output = hit; return true; }
        if (playerDamageSuppressed) { hit.result = kVortexSuppressed; output = hit; return true; }
        VortexVictim next = victim;
        next.stats[2] = next.stats[3] = 0;
        if (victim.kind == 3) next.stats[0] = 255;
        next.actor[3] = 255;
        if (victim.kind != 3) next.actor[7] = 255;
        hit.result = kVortexKilled; hit.nominalDamage = 3000;
        if (victim.kind < 3) {
            const byte casterLevel = caster.bytes[34] ? caster.bytes[1] : caster.bytes[1] & 127;
            const byte targetLevel = victim.stats[1] & 127;
            const uint difference = casterLevel > targetLevel ? casterLevel - targetLevel : targetLevel - casterLevel;
            const bool special = casterCategory == 5 && victim.stats[16] == 188;
            const byte percent = special ? _specialPercent : _percentages[MIN<uint>(difference, 5)];
            const uint16 base = victim.stats[5] | uint16(victim.stats[8]) << 8;
            // Source percentage multiplication retains its low16 bits before
            // division. Preserve that wrap and the special subtract branch.
            const uint16 product = uint16(uint32(base & 32767) * percent);
            const uint16 adjustment = product / 100;
            hit.credit = special || casterLevel >= targetLevel ? uint16(base - adjustment) : uint16(base + adjustment);
        }
        // Source actor state bit8 occupies four viewport cells. Keep removal
        // at the resolved origin; do not rediscover actors from general RAM.
        const uint offsets[] = { 0, 1, 17, 18 };
        for (uint i = 0; i < (large ? 4U : 1U); ++i) indices[victim.cell + offsets[i]] = kinds[victim.cell + offsets[i]] = 255;
        victim = next; creditPool = uint16(creditPool + hit.credit); output = hit; return true;
    }
private:
    bool _ready; byte _specialPercent, _percentages[6];
};
} }
#endif
