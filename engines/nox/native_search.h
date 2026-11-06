/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SEARCH_H
#define NOX_NATIVE_SEARCH_H
#include "nox/native_inventory.h"
#include "nox/native_party.h"
#include "nox/native_lock.h"
#include "nox/native_graphics.h"
namespace Nox {
namespace Native {
enum SearchRewardResult { kSearchRewardAwarded, kSearchRewardRepeated, kSearchRewardCapacity, kSearchRewardUnsupported };
enum SearchResult { kSearchUnsupported, kSearchNothing, kSearchNoTools, kSearchCancelled, kSearchRetry, kSearchAwarded, kSearchCapacity, kSearchRepeated };
struct SearchState { byte priorLockAttempts; SearchState() : priorLockAttempts(0) {} };
class SearchRules {
public:
    SearchRules() : _type(255) {}
    bool load(HdvArchive &archive, byte type) {
        Common::Array<byte> game, range; MapAssetRange assets;
        if (!archive.read("NA/GAME", game) || !resolveMapAssets(game, type, assets) ||
                !archive.readRange(assets.resource, assets.offset + assets.size + 26, 2, range) || range.size() != 2 || !_lock.load(archive)) return false;
        _lower = range[0]; _upper = range[1]; _type = type; return true;
    }
    byte mapType() const { return _type; }
    bool locked(byte tile) const { return tile >= _lower && tile < _upper; }
    const LockRules &locks() const { return _lock; }
private:
    byte _type, _lower, _upper; LockRules _lock;
};

// Commit one attempt's verified effects together. UI/death checks belong to
// the scheduler after this operation, rather than inside a record mutation.
inline bool applyLockPartyEffects(const Common::Array<LockEffect> &effects, Party &party, bool &promoted) {
    Party candidate = party; bool advanced = false;
    for (uint i = 0; i < effects.size(); ++i) {
        const LockEffect &effect = effects[i];
        if (!effect.member || effect.member > candidate.memberCount()) return false;
        if (effect.kind == LockEffect::kDamage) {
            if (!candidate.damage(effect.member - 1, effect.amount)) return false;
        } else if (effect.kind == LockEffect::kPractice) {
            bool changed = false;
            if (effect.amount > 255 || !candidate.practiceLockSkill(effect.member - 1, byte(effect.amount), changed)) return false;
            advanced |= changed;
        } else return false;
    }
    party = candidate; promoted = advanced; return true;
}

// Authored quest-reward phase AFTER a lock/trap operation has allowed access.
// This is deliberately separate from target selection, locks, gold and loot.
// The caller owns those phases; passing an unlocked fixture is controlled setup.
inline SearchRewardResult searchQuestReward(byte *object, uint objectBytes, byte *fields, uint fieldBytes,
        const ItemCatalog &catalog, Inventory &inventory) {
    if (!object || objectBytes < 12 || !fields || fieldBytes != 256 ||
            (object[3] & 191) != 53 || (object[7] & 192) != 64 || object[4] > 2)
        return kSearchRewardUnsupported;
    ItemDefinition definition;
    if (!catalog.definition(object[4], object[5], definition)) return kSearchRewardUnsupported;
    // Original Search consumes the object's searchable marker even on repeat
    // and inventory-capacity refusal. Preserve that observable ordering.
    if (fields[object[8]] & object[11]) { object[3] = 0; return kSearchRewardRepeated; }
    Inventory next = inventory;
    const bool capacity = !(definition.bytes[8] & 128) && !next.ordinaryCapacityRemaining();
    if (!capacity && next.grantOne(catalog, object[4], object[5]) != kInventoryApplied)
        return kSearchRewardUnsupported; // Original full-slot fatal path is not qualified here.
    object[3] = 0;
    fields[object[8]] |= object[11];
    if (object[4] == 2 && (definition.bytes[8] & 32)) ++fields[definition.bytes[7]];
    if (capacity) return kSearchRewardCapacity;
    inventory = next;
    return kSearchRewardAwarded;
}
}
}
#endif
