/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_POSITION_VALIDATION_H
#define NOX_NATIVE_POSITION_VALIDATION_H
#include "nox/native_world.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativePosition(HdvArchive &archive, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    const char *name = "nox-native-position-state.bin";
    Native::TraversalRules rules; if (!rules.load(archive, 7)) return false;
    Native::World world(archive); Native::TraversalDecision decision;
    byte visibility[187] = {}, actors[187]; memset(actors, 255, sizeof(actors));
    if (cold) {
        Common::InSaveFile *input = files.openForLoading(name);
        const bool loaded = input && world.load(*input); delete input;
        const Common::Array<byte> *tables = world.currentTables();
        if (!loaded || world.mapId() != 54 || world.x() != 38 || world.y() != 14 || world.field(24) != 4 ||
            !tables || (*tables)[1058] != 58 || (*tables)[1059] != 41 ||
            world.movePositionPhase(Native::kSouth, rules, false, -1, decision) != Native::kPositionMoved || world.y() != 15) {
            error = "Cold native Open/position chain failed"; return false;
        }
		Native::ObjectScene scene;
		if (!world.applyObjectPhase(32, visibility, actors, 255, 119, scene) || (*world.currentTables())[1059] != 40) {
			error = "Cold continued object scene did not settle the opened door"; return false;
		}
        report += "native-position\tphase=cold\topen_and_position_retained=1\tcontinued_move=1\tcpu_execution=0\n";
        return true;
    }
    byte fields[256] = {}; fields[24] = 4;
    if (!world.initialize(7, 54, 38, 15, fields, sizeof(fields))) return false;
    Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES), after(DisposeAfterUse::YES);
    if (!world.save(before) || world.movePositionPhase(Native::kNorth, rules, false, 32, decision) != Native::kPositionBlocked ||
        decision.reason != Native::kTraversalBlockingObject || !world.save(after) || before.size() != after.size() || memcmp(before.getData(), after.getData(), before.size())) {
        error = "Closed native door failed atomic position refusal"; return false;
    }
    if (world.open(38, 14) != Native::kGateAllowed ||
        world.movePositionPhase(Native::kNorth, rules, true, 32, decision) != Native::kPositionBlocked || decision.reason != Native::kTraversalActor ||
        world.movePositionPhase(Native::kNorth, rules, false, 31, decision) != Native::kPositionUnsupported || world.y() != 15 ||
        world.movePositionPhase(Native::kNorth, rules, false, 32, decision) != Native::kPositionMoved || decision.reason != Native::kTraversalPassableObject || world.x() != 38 || world.y() != 14) {
        error = "Native Open did not enable retained position phase"; return false;
    }
    Native::ObjectScene scene;
    if (!world.applyObjectPhase(32, visibility, actors, 255, 119, scene) || (*world.currentTables())[1059] != 41 || scene.lookup[93] != 32 || !scene.draws.empty()) {
        error = "Occupied native door did not retain its opened state"; return false;
    }
    Common::OutSaveFile *output = files.openForSaving(name, false);
    bool saved = output && world.save(*output);
    if (output) { output->finalize(); saved = saved && !output->err(); } delete output;
    if (!saved) return false;
    report += "native-position\tphase=warm\tclosed_refusal_atomic=1\tactual_open_then_move=1\tactor_refusal=1\tinvalid_occupancy_refusal=1\tresolved_occupancy_input=1\tcomplete_turn=0\tcpu_execution=0\n";
    return true;
}
}
#endif
