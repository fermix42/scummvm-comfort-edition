/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LOCK_H
#define NOX_NATIVE_LOCK_H
#include "nox/native_random.h"
#include "nox/resources.h"
namespace Nox { namespace Native {
struct LockContext {
    byte difficulty, lockSkill, trapSkill, map, selectedMember, partyCount;
    bool forceOpen, cancelled;
};
struct LockState { byte picks, priorAttempts; };
struct LockEffect {
    enum Kind { kPractice, kDamage } kind;
    byte member; uint16 amount;
    LockEffect(Kind k, byte m, uint16 a) : kind(k), member(m), amount(a) {}
};
enum LockResult { kLockUnsupported, kLockNoTools, kLockCancelled, kLockOpened, kLockTrapAvoided, kLockTrapped };
class LockRules {
public:
    LockRules() : _ready(false), _door(false) {}
    bool load(HdvArchive &archive, bool door = false) {
        Common::Array<byte> data;
        // Open reads its full difficulty byte and wraps the doubled table index.
        // Keep reads inside its original loaded resource; Search has level0..10.
        const uint count = door ? 32 : 11;
        if (!archive.readRange("NA/SRTN.INVENTORY", door ? 0x9549 + 0x237 : 0x8a92 + 0x5ba, count * 2, data) || data.size() != count * 2) return false;
        for (uint i = 0; i < count; ++i) _damage[i] = READ_LE_UINT16(data.data() + i * 2) / 4;
        _door = door; _ready = true; return true;
    }
    bool supportsDifficulty(byte difficulty) const { return _door ? (difficulty & 127) < 32 : difficulty <= 10; }
    LockResult attempt(const LockContext &context, LockState &state, GameplayRandom &random, Common::Array<LockEffect> &effects) const {
        if (!_ready || !supportsDifficulty(context.difficulty) || !context.selectedMember || context.selectedMember > context.partyCount || context.partyCount > 6)
            return kLockUnsupported;
        Common::Array<LockEffect> next;
        if (!state.picks) { effects = next; return kLockNoTools; }
        if (context.cancelled) { effects = next; return kLockCancelled; }
        GameplayRandom candidate = random; byte roll = 0;
        bool opened = context.map == 7 ? state.priorAttempts != 0 : context.forceOpen;
        if (context.map != 7 && !context.forceOpen) {
            if (!candidate.next(1, 199, roll)) return kLockUnsupported;
            opened = fold(roll) < threshold(context.lockSkill, context.difficulty);
        }
        if (opened) {
            next.push_back(LockEffect(LockEffect::kPractice, context.selectedMember, 38));
            random = candidate; effects = next; return kLockOpened;
        }
        if (!candidate.next(1, 199, roll)) return kLockUnsupported;
        LockResult result = kLockTrapAvoided;
        if (fold(roll) < threshold(context.trapSkill, context.difficulty))
            next.push_back(LockEffect(LockEffect::kPractice, context.selectedMember, 32));
        else {
            // Original Open damage division leaves RNG upper255; Search restores199.
            if (!candidate.next(1, _door ? 255 : 199, roll)) return kLockUnsupported;
            if (roll < 25) for (byte member = 1; member <= context.partyCount; ++member)
                next.push_back(LockEffect(LockEffect::kDamage, member, _damage[_door ? context.difficulty & 127 : context.difficulty]));
            else next.push_back(LockEffect(LockEffect::kDamage, context.selectedMember, _damage[_door ? context.difficulty & 127 : context.difficulty]));
            result = kLockTrapped;
        }
        --state.picks; ++state.priorAttempts; random = candidate; effects = next; return result;
    }
private:
    static byte threshold(byte skill, byte difficulty) { return byte((skill >> 1) - difficulty * 2 + 50); }
    static byte fold(byte roll) { return roll >= 100 ? roll - 100 : roll; }
    bool _ready, _door; uint16 _damage[32];
};
} }
#endif
