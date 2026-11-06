/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VORTEX_CAST_H
#define NOX_NATIVE_VORTEX_CAST_H
#include "nox/native_cast.h"
#include "nox/native_vortex_targets.h"
#include "nox/native_vortex_effect.h"
namespace Nox { namespace Native {
// Lossless battle record ownership. Encounter construction and world-party
// handoff remain separate; importing an oracle is explicitly controlled setup.
struct BattleRecords {
    byte stats[2048], compact[256], extended[256], indices[187], kinds[187];
    uint16 credit;
    BattleRecords() : credit(0) {
        memset(stats, 0, sizeof(stats)); memset(compact, 0, sizeof(compact));
        memset(extended, 0, sizeof(extended)); memset(indices, 255, sizeof(indices)); memset(kinds, 255, sizeof(kinds));
    }
    bool actor(byte cell, VortexVictim &output) const {
        if (cell >= 187 || indices[cell] == 255) return false;
        const byte kind = kinds[cell] & 135, index = indices[cell];
        uint statOffset, actorOffset, size;
        if (kind == 3 && !(index & 3) && index <= 20) { statOffset = index * 32; actorOffset = index; size = 128; }
        else if (kind == 1 && !(index & 7) && index < 128) { statOffset = 768 + index * 4; actorOffset = index; size = 32; }
        else if (kind == 2 && !(index & 7) && index >= 128) { statOffset = 1280 + (index - 128) * 4; actorOffset = index; size = 32; }
        else return false;
        VortexVictim next; next.kind = kind; next.index = index; next.cell = cell;
        memcpy(next.stats, stats + statOffset, size);
        memcpy(next.actor, (kind == 3 ? compact : extended) + actorOffset, kind == 3 ? 4 : 8);
        output = next; return true;
    }
    bool store(const VortexVictim &victim) {
        VortexVictim resolved;
        if (!actor(victim.cell, resolved) || resolved.kind != victim.kind || resolved.index != victim.index) return false;
        const uint statOffset = victim.kind == 3 ? victim.index * 32 : victim.kind == 1 ? 768 + victim.index * 4 : 1280 + (victim.index - 128) * 4;
        memcpy(stats + statOffset, victim.stats, victim.kind == 3 ? 128 : 32);
        memcpy((victim.kind == 3 ? compact : extended) + victim.index, victim.actor, victim.kind == 3 ? 4 : 8);
        return true;
    }
};
struct VortexCastOutcome {
    CastResult result; uint targets, killed;
    VortexCastOutcome() : result(kCastUnsupported), targets(0), killed(0) {}
};
// One logical standard-player cardinal-direction cast. Owns preparation,
// windup marker, mana commit, ordered target effects and accumulated credit.
// Input prompts, graphics, AI/turn retirement and victory are separate.
inline bool castVortex(const CastRules &rules, const VortexEffects &effects, const SpellBook &book,
        byte slot, byte casterCell, byte casterCategory, bool protectedTargets, Direction direction,
        BattleRecords &battle, VortexCastOutcome &output) {
    VortexVictim casterActor;
    if (!battle.actor(casterCell, casterActor) || casterActor.kind != 3 || uint(direction) > uint(kWest)) return false;
    CharacterRecord caster; memcpy(caster.bytes, casterActor.stats, 128);
    CastPreparation intent; VortexCastOutcome outcome;
    outcome.result = rules.prepare(book, caster, slot, true, intent);
    if (outcome.result != kCastPrepared) { output = outcome; return true; }
    if (intent.spell != 12) return false;
    BattleActorCell cells[187];
    for (uint i = 0; i < 187; ++i) {
        if (battle.indices[i] == 255) continue;
        VortexVictim actor; if (!battle.actor(byte(i), actor)) return false;
        cells[i].present = true; cells[i].x = actor.actor[0]; cells[i].y = actor.actor[1];
        cells[i].state = actor.actor[3]; cells[i].kindFlags = battle.kinds[i];
    }
    Common::Array<SpellTarget> targets;
    if (!collectVortexTargets(casterActor.actor[0], casterActor.actor[1], direction, cells, 187, targets)) return false;
    BattleRecords next = battle;
    casterActor.stats[12] = 255; casterActor.stats[4] -= intent.manaCost;
    if (!next.store(casterActor)) return false;
    memcpy(caster.bytes, casterActor.stats, 128);
    for (uint i = 0; i < targets.size(); ++i) {
        VortexVictim victim; if (!next.actor(targets[i].cell, victim)) return false;
        byte indices[187], kinds[187]; memcpy(indices, next.indices, 187); memcpy(kinds, next.kinds, 187);
        VortexHit hit;
        if (!effects.resolve(caster, casterCategory, protectedTargets, victim, indices, kinds, 187, next.credit, hit) || !next.store(victim)) return false;
        memcpy(next.indices, indices, 187); memcpy(next.kinds, kinds, 187);
        outcome.killed += hit.result == kVortexKilled;
    }
    outcome.targets = targets.size(); battle = next; output = outcome; return true;
}
} }
#endif
