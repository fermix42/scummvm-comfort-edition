/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LOCK_VALIDATION_H
#define NOX_NATIVE_LOCK_VALIDATION_H
#include "nox/native_lock.h"
namespace Nox {
inline bool validateNativeLock(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','L','C','K') || input.readUint32LE() != 1 || input.readUint32LE() != 297) return false;
    Native::LockRules rules; if (!rules.load(archive)) return false;
    uint damageRequests = 0, opened = 0;
    for (uint i = 0; i < 297; ++i) {
        Native::LockContext context; context.difficulty = input.readByte(); context.lockSkill = input.readByte(); context.trapSkill = input.readByte();
        Native::LockState state; state.picks = input.readByte(); context.map = input.readByte(); state.priorAttempts = input.readByte();
        context.forceOpen = input.readByte() != 0; context.cancelled = input.readByte() != 0; context.selectedMember = 1; context.partyCount = 3;
        Native::GameplayRandom random; if (!random.importState(input.readUint32LE(), 2)) return false;
        const byte originalRefused = input.readByte(), picks = input.readByte(), attempts = input.readByte();
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(), last = input.readByte();
        const uint16 generations = input.readUint16LE(), count = input.readUint16LE();
        Common::Array<Native::LockEffect> effects;
        const Native::LockResult result = rules.attempt(context, state, random, effects);
        bool matches = result != Native::kLockUnsupported && (result == Native::kLockOpened) == (originalRefused == 0) &&
            state.picks == picks && state.priorAttempts == attempts && random.seed() == seed && random.tail() == tail &&
            random.last() == last && random.generationAttempts() == generations && effects.size() == count;
        for (uint j = 0; j < count; ++j) {
            const byte kind = input.readByte(), member = input.readByte(); const uint16 amount = input.readUint16LE();
            if (j >= effects.size() || uint(effects[j].kind) != kind || effects[j].member != member || effects[j].amount != amount) matches = false;
            if (kind == 1) ++damageRequests;
        }
        if (!matches) { error = Common::String::format("Native Search lock attempt differs at case%u result%u", i, uint(result)); return false; }
        if (result == Native::kLockOpened) ++opened;
    }
    if (input.err() || input.pos() != input.size() || damageRequests != 55 || opened != 203) return false;
    report += "native-lock\tcases=297\topened=203\tdamage_requests=55\tparty_callbacks_applied=0\tcpu_execution=0\n"; return true;
}
}
#endif
