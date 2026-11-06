/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_COMMAND_VALIDATION_H
#define NOX_NATIVE_WORLD_COMMAND_VALIDATION_H
#include "nox/native_world_command.h"
#include "nox/native_world_scene_phase.h"
namespace Nox {
inline bool validateNativeWorldCommand(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','C','M','V') || input.readUint32LE() != 1 || input.readUint32LE() != 4) return false;
    Native::StoryMapPatches story; Native::ActorPaintRules actors; Native::TerrainLighting lighting; Native::TraversalRules traversal;
    if (!story.load(archive) || !actors.load(archive,0) || !lighting.load(archive,0) || !traversal.load(archive,0)) return false;
    const Common::KeyCode keys[] = {Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
    const int dx[] = {0,0,1,-1},dy[] = {-1,1,0,0}; const uint cells[] = {76,110,94,92};
    uint moved = 0, blocked = 0;
    for (uint profile = 0; profile < 4; ++profile) {
        const byte x = input.readByte(),y = input.readByte(); byte fields[256],spawns[256],mask[187];
        if (input.read(fields,256) != 256 || input.read(spawns,256) != 256 || input.read(mask,187) != 187) return false;
        for (uint direction = 0; direction < 4; ++direction) {
            byte sourceProbe[29],expected[4]; if (input.read(sourceProbe,29) != 29 || input.read(expected,4) != 4) return false;
            Native::World world(archive); Native::WorldStory window; Native::PresentationState presentation;
            Native::LightState light; Native::VisibilityMask visibility; Native::WorldScene scene;
            if (!world.initialize(0,0,x,y,fields,256) || !window.initialize(world,spawns,256) || !visibility.loadRules(archive) || !visibility.importMask(mask,187) ||
                !Native::WorldScenePhase::run(world,window,story,actors,lighting,0,246,0,presentation,light,visibility,scene)) return false;
            const uint cell = cells[direction]; const byte object = scene.objects.lookup[cell];
            if (scene.actors.terrain[cell] != sourceProbe[1] || byte(scene.actors.lookup[cell] != 255) != sourceProbe[2] || byte(object != 255) != sourceProbe[3] ||
                (object != 255 && (world.currentTables()->data()[1024 + object + 2] != sourceProbe[4] || world.currentTables()->data()[1024 + object + 3] != sourceProbe[5]))) {
                error = Common::String::format("Native command source scene inputs differ at profile%u direction%u",profile,direction); return false;
            }
            Common::Event event; event.type = Common::EVENT_KEYDOWN; event.kbd.keycode = keys[direction]; Native::TraversalDecision decision;
            const Native::PositionMoveResult outcome = Native::WorldCommand::move(event,world,window,scene,traversal,decision);
            const Native::PositionMoveResult expectedOutcome = expected[0] ? Native::kPositionBlocked : expected[1] ? Native::kPositionNeedsInteraction : Native::kPositionMoved;
            if (outcome != expectedOutcome || byte(decision.blocked) != expected[0] || byte(decision.requiresInteraction) != expected[1] || decision.effectiveTile != expected[2] || uint(decision.reason) != expected[3] ||
                world.x() != byte(x + (outcome == Native::kPositionMoved ? dx[direction] : 0)) || world.y() != byte(y + (outcome == Native::kPositionMoved ? dy[direction] : 0))) {
                error = Common::String::format("Native directional command differs at profile%u direction%u",profile,direction); return false;
            }
            moved += outcome == Native::kPositionMoved; blocked += outcome == Native::kPositionBlocked;
        }
    }
    if (moved != 14 || blocked != 2 || input.err() || input.pos() != input.size()) return false;
    report += "native-world-command\tcommands=16\tCE_directional_events=1\tactual_scene_occupancy=1\tchanged_terrain_refusal=1\tactor_refusal=1\tmoved=14\tblocked=2\tcomplete_turn=0\tcpu_execution=0\n"; return true;
}
}
#endif
