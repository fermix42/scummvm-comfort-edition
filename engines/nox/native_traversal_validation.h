/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TRAVERSAL_VALIDATION_H
#define NOX_NATIVE_TRAVERSAL_VALIDATION_H
#include "nox/native_traversal.h"
namespace Nox {
inline bool validateNativeTraversal(HdvArchive &archive, Common::SeekableReadStream &input,
        Common::SeekableReadStream &expected, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','T','R','V') || expected.readUint32BE() != MKTAG('N','T','R','V') || input.readUint32LE() != 1 || expected.readUint32LE() != 1) return false;
    const uint count = input.readUint32LE();
    if (count != 1054720 || expected.readUint32LE() != count || input.size() - input.pos() != int64(count) * 29 || expected.size() - expected.pos() != int64(count) * 4) return false;
    Native::TraversalRules rules; byte previous = 255;
    for (uint i = 0; i < count; ++i) {
        const byte type = input.readByte(); Native::TraversalProbe probe;
        probe.terrain = input.readByte(); probe.actorPresent = input.readByte() != 0; probe.objectPresent = input.readByte() != 0;
        probe.objectTile = input.readByte(); probe.objectState = input.readByte(); probe.movementFlag = input.readByte();
        byte ranges[6], scripts[16], golden[4];
        if (input.read(ranges, 6) != 6 || input.read(scripts, 16) != 16 || expected.read(golden, 4) != 4) return false;
        if (type != previous) { if (!rules.load(archive, type)) return false; previous = type; }
        Native::TraversalDecision actual;
        if (memcmp(ranges, rules.ranges(), 6) || memcmp(scripts, rules.scripts(), 16) || !rules.probe(probe, actual) ||
            actual.blocked != (golden[0] != 0) || actual.requiresInteraction != (golden[1] != 0) || actual.effectiveTile != golden[2] || uint(actual.reason) != golden[3]) {
            error = Common::String::format("Native traversal differs at controlled case%u", i); return false;
        }
    }
    Native::TraversalRules empty; Native::TraversalProbe probe; Native::TraversalDecision untouched;
    untouched.effectiveTile = 77;
    if (empty.probe(probe, untouched) || untouched.effectiveTile != 77 || input.err() || expected.err()) return false;
    report += Common::String::format("native-traversal\tcases=%u\tstyles=8\tall_object_states=256\tactor_priority=1\tunready_atomic=1\townership=controlled-rules\n", count);
    return true;
}
}
#endif
