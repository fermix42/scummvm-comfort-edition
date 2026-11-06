/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LOCK_PARTY_VALIDATION_H
#define NOX_NATIVE_LOCK_PARTY_VALIDATION_H
#include "nox/native_search.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeLockParty(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','L','P','Y') || input.readUint32LE() != 1 || input.readUint32LE() != 297) return false;
    Native::LockRules rules; if (!rules.load(archive)) return false;
    uint promotions = 0;
    for (uint i = 0; i < 297; ++i) {
        Native::LockContext context; context.difficulty = input.readByte(); context.lockSkill = input.readByte(); context.trapSkill = input.readByte();
        Native::LockState state; state.picks = input.readByte(); context.map = input.readByte(); state.priorAttempts = input.readByte();
        context.forceOpen = input.readByte() != 0; context.cancelled = input.readByte() != 0; context.selectedMember = 1; context.partyCount = 3;
        Native::GameplayRandom random; if (!random.importState(input.readUint32LE(), 2)) return false;
        input.skip(11); const uint16 count = input.readUint16LE(); input.skip(count * 4);
        byte before[864], expected[864];
        if (input.read(before, 864) != 864 || input.read(expected, 864) != 864) return false;
        const bool expectedPromotion = input.readByte() != 0;
        Common::MemoryWriteStreamDynamic serialized(DisposeAfterUse::YES);
        serialized.writeUint32BE(MKTAG('N','P','A','R')); serialized.writeUint16LE(1); serialized.write(before, 864);
        Common::MemoryReadStream source(serialized.getData(), serialized.size()); Native::Party party;
        if (!party.load(source) || party.memberCount() != 3) return false;
        Common::Array<Native::LockEffect> effects; bool promoted = false;
        if (rules.attempt(context, state, random, effects) == Native::kLockUnsupported ||
                !Native::applyLockPartyEffects(effects, party, promoted)) return false;
        Common::MemoryWriteStreamDynamic actual(DisposeAfterUse::YES);
        if (!party.save(actual) || actual.size() != 870 || memcmp(actual.getData() + 6, expected, 864) || promoted != expectedPromotion) {
            error = Common::String::format("Native lock/party linked records differ at case%u", i); return false;
        }
        promotions += promoted;
    }
    if (input.err() || input.pos() != input.size() || promotions != 165) return false;
    report += "native-lock-party\tcases=297\tpromotions=165\tparty_callbacks_applied=1\tgame_over_presentation=0\tcpu_execution=0\n"; return true;
}
}
#endif
