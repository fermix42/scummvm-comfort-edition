/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_STARTUP_HOST_H
#define NOX_NATIVE_STARTUP_HOST_H
#include "nox/native_character_creation.h"
#include "nox/native_world_viewport.h"
#include "nox/native_text_window.h"
#include "nox/native_opening_backdrop.h"
#include "nox/native_world_wait.h"
#include "nox/native_world_fixture.h"
#include "nox/native_exploration_save.h"
#include "nox/native_world_status.h"
#include "nox/native_world_status_validation.h"
#include "nox/native_world_animation_validation.h"
#include "nox/native_battlefields_validation.h"
#include "nox/native_encounter_catalog_validation.h"
#include "nox/native_encounter_preparation.h"
#include "nox/native_encounter_formation_validation.h"
#include "nox/native_encounter_admission_validation.h"
#include "nox/native_battle_terrain_validation.h"
#include "nox/native_battle_world_return_validation.h"
#include "nox/native_scripted_treasure_validation.h"
#include "nox/native_battle_restoration_validation.h"
#include "nox/native_battle_spell_damage_validation.h"
#include "nox/native_battle_area_spell_validation.h"
#include "nox/native_battle_directed_spell_validation.h"
#include "nox/native_battle_summon_validation.h"
#include "nox/native_quick_battle_validation.h"
#include "nox/native_combat_mode_validation.h"
#include "nox/native_battle_madcow_validation.h"
#include "nox/native_battle_terminal_validation.h"
#include "nox/native_admission_protection_validation.h"
#include "nox/native_npc_combat_validation.h"
#include "nox/native_battle_geometry_validation.h"
#include "nox/native_battle_presentation_validation.h"
#include "nox/native_battle_motion_validation.h"
#include "nox/native_battle_physical_validation.h"
#include "nox/native_battle_aftermath.h"
#include "nox/native_battle_results_validation.h"
#include "nox/native_battle_action_validation.h"
#include "nox/native_battle_cast.h"
#include "nox/native_combat_fixture.h"
#include "nox/native_player_session.h"
#include "nox/native_player_lifecycle_validation.h"
#include "nox/native_ending.h"
namespace Nox {
// Development event source contains only keycode/ascii pairs. State, expected
// outcomes and resource bytes cannot be supplied by this input channel.
class NativeStartupKeys : public Common::EventSource {
public:
    explicit NativeStartupKeys(Common::File &file) : count(0),_file(file),_release(false) {}
    bool allowMapping() const override { return false; }
    bool pollEvent(Common::Event &event) override {
        if (_release) { event.type=Common::EVENT_KEYUP; event.kbd=_last; _release=false; return true; }
        if (_file.pos()==_file.size()) return false;
        const uint16 key=_file.readUint16LE(),ascii=_file.readUint16LE();
        _last=Common::KeyState(Common::KeyCode(key),ascii);
        event.type=Common::EVENT_KEYDOWN; event.kbd=_last; _release=true; ++count; return true;
    }
    bool finished() const { return !_release && _file.pos()==_file.size(); }
    uint count;
private:
    Common::File &_file; bool _release; Common::KeyState _last;
};
inline Common::Error NoxEngine::runNativeStartup() {
    const bool replay=ConfMan.hasKey("nox_native_startup_keys");
    auto fail=[&](const char *reason) -> Common::Error {
        if (replay) {
            Common::OutSaveFile *receipt=_system->getSavefileManager()->openForSaving("nox-native-validation-failure.txt",false);
            if (receipt) { receipt->writeString(reason); receipt->finalize(); delete receipt; }
        }
        return Common::Error(Common::kReadingFailed,reason);
    };
    Native::PlayerSessionState player(_resources);Native::NewGame &game=player.game;Native::CharacterCreation &creation=player.creation;Native::GlyphFont font;
    if (!font.load(_resources,1)) return fail("Native startup font unavailable");
    Native::WorldStatus &status=player.status;if (!status.load(_resources)) return fail("Native world status resources unavailable");
    if (replay && ConfMan.hasKey("nox_native_battle_reticle_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_reticle_oracle"))) || !validateNativeBattleReticle(_resources,oracle,report)) return fail(report.empty()?"Native target reticle rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-reticle-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_targets_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_targets_oracle"))) || !validateNativeBattleTargets(_resources,oracle,report)) return fail(report.empty()?"Native target selection rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-targets-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_action_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_action_oracle"))) || !validateNativeBattleAction(_resources,oracle,report)) return fail(report.empty()?"Native complete Attack protocol rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-action-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_ranged_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_ranged_oracle"))) || !validateNativeBattlePhysical(_resources,oracle,report)) return fail(report.empty()?"Native ranged pipeline rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-ranged-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_advancement_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_advancement_oracle"))) || !validateNativeBattleAdvancement(_resources,oracle,report)) return fail(report.empty()?"Native battle advancement rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-advancement-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_reward_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_reward_oracle"))) || !validateNativeBattleRewards(_resources,oracle,report)) return fail(report.empty()?"Native battle rewards rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-reward-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_skill_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_skill_oracle"))) || !validateNativeBattleSkills(_resources,oracle,report)) return fail(report.empty()?"Native battle skill settlement rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-skill-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_loot_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_loot_oracle"))) || !validateNativeBattleLoot(_resources,oracle,report)) return fail(report.empty()?"Native battle loot rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-loot-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_round_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_round_oracle"))) || !validateNativeBattleRound(_resources,oracle,report)) return fail(report.empty()?"Native battle-round rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-round-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_physical_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_physical_oracle"))) || !validateNativeBattlePhysical(_resources,oracle,report)) return fail(report.empty()?"Native physical pipeline rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-physical-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_motion_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_motion_oracle"))) || !validateNativeBattleMotion(oracle,report)) return fail(report.empty()?"Native battle motion rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-motion-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_bars_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_bars_oracle"))) || !validateNativeBattleBars(oracle,report)) return fail("Native battle bars rejected");
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-bars-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_frame_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_frame_oracle"))) || !validateNativeBattlePresentation(_resources,oracle,report)) return fail(report.empty()?"Native battle frame rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-frame-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_npc_decision_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_npc_decision_oracle"))) || !validateNativeNpcDecision(oracle,report)) return fail(report.empty()?"Native NPC decision rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-npc-decision-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_npc_targets_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_npc_targets_oracle"))) || !validateNativeNpcTargets(oracle,report)) return fail(report.empty()?"Native NPC targets rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-npc-targets-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_npc_cast_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_npc_cast_oracle"))) || !validateNativeNpcCast(_resources,oracle,report)) return fail(report.empty()?"Native NPC cast rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-npc-cast-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_geometry_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_geometry_oracle"))) || !validateNativeBattleGeometry(_resources,oracle,report)) return fail(report.empty()?"Native battle geometry rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-geometry-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_admission_protection_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_admission_protection_oracle"))) || !validateNativeAdmissionProtection(oracle,report)) return fail(report.empty()?"Native admission protection rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-admission-protection-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_terminal_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_terminal_oracle"))) || !validateNativeBattleTerminal(oracle,report)) return fail(report.empty()?"Native battle terminal policy rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-terminal-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_madcow_protection_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_madcow_protection_oracle"))) || !validateNativeBattleMadCow(_resources,oracle,report)) return fail(report.empty()?"Native protected Mad Cow rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-madcow-protection-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_madcow_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_madcow_oracle"))) || !validateNativeBattleMadCow(_resources,oracle,report)) return fail(report.empty()?"Native Mad Cow rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-madcow-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_combat_mode_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_combat_mode_oracle"))) || !validateNativeCombatMode(oracle,report)) return fail(report.empty()?"Native combat mode rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-combat-mode-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_quick_physical_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_quick_physical_oracle"))) || !validateNativeBattlePhysical(_resources,oracle,report,true)) return fail(report.empty()?"Native Quick physical rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-quick-physical-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_quick_battle_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_quick_battle_oracle"))) || !validateNativeQuickBattle(_resources,oracle,report)) return fail(report.empty()?"Native Quick battle rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-quick-battle-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_summon_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_summon_oracle"))) || !validateNativeBattleSummon(_resources,oracle,report)) return fail(report.empty()?"Native summon rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-summon-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_directed_spell_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_directed_spell_oracle"))) || !validateNativeBattleDirectedSpell(_resources,oracle,report)) return fail(report.empty()?"Native directed spell rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-directed-spell-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_area_spell_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_area_spell_oracle"))) || !validateNativeBattleAreaSpell(_resources,oracle,report)) return fail(report.empty()?"Native area spell rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-area-spell-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_spell_damage_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_spell_damage_oracle"))) || !validateNativeBattleSpellDamage(_resources,oracle,report)) return fail(report.empty()?"Native spell impact rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-spell-damage-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_restoration_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_restoration_oracle"))) || !validateNativeBattleRestoration(_resources,oracle,report)) return fail(report.empty()?"Native battle restoration rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-restoration-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_scripted_treasure_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_scripted_treasure_oracle"))) || !validateNativeScriptedTreasure(_resources,oracle,report)) return fail(report.empty()?"Native scripted treasure rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-scripted-treasure-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_world_return_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_world_return_oracle"))) || !validateNativeBattleWorldReturn(_resources,oracle,report)) return fail(report.empty()?"Native battle world return rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-world-return-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battle_terrain_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battle_terrain_oracle"))) || !validateNativeBattleTerrain(_resources,oracle,report)) return fail(report.empty()?"Native tactical terrain rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battle-terrain-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_transport_admission_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_transport_admission_oracle"))) || !validateNativeTransportAdmission(_resources,oracle,report)) return fail(report.empty()?"Native transport admission rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-transport-admission-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_encounter_admission_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_encounter_admission_oracle"))) || !validateNativeEncounterAdmission(_resources,oracle,report)) return fail(report.empty()?"Native encounter admission rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-encounter-admission-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_general_formation_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_general_formation_oracle"))) || !validateNativeGeneralFormation(_resources,oracle,report)) return fail(report.empty()?"Native general formation rejected":report.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-general-formation-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_encounter_formation_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_encounter_formation_oracle"))) || !validateNativeEncounterFormation(_resources,oracle,report)) return fail("Native fixed encounter formation rejected");
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-encounter-formation-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battlefield_choice_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battlefield_choice_oracle"))) || !validateNativeBattlefieldChoice(_resources,oracle,report)) return fail("Native battlefield choice rejected");
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battlefield-choice-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_encounter_catalog_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_encounter_catalog_oracle"))) || !validateNativeEncounterCatalog(_resources,oracle,report)) return fail("Native encounter descriptor/monster data rejected");
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-encounter-catalog-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_battlefields_oracle")) {
        Common::File oracle;Common::String report;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_battlefields_oracle"))) || !validateNativeBattlefields(_resources,oracle,report)) return fail("Native encounter battlefield data/projection rejected");
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-battlefields-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_animation_oracle")) {
        Common::File oracle;Common::String report,error;const bool cold=ConfMan.hasKey("nox_native_animation_cold") && ConfMan.getBool("nox_native_animation_cold");
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_animation_oracle"))) || !validateNativeWorldAnimation(_resources,oracle,*_system->getSavefileManager(),cold,report,error)) return fail(error.empty()?"Native animation oracle rejected":error.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-animation-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (replay && ConfMan.hasKey("nox_native_status_oracle")) {
        Common::File oracle;Common::String report,error;
        if (!oracle.open(Common::FSNode(ConfMan.getPath("nox_native_status_oracle"))) || !validateNativeWorldStatus(status,oracle,report,error)) return fail(error.empty()?"Native status oracle rejected":error.c_str());
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-status-oracle.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    const Graphics::PixelFormat format(4,8,8,8,0,16,8,0,0);
    initGraphics(560,384,&format); _surface.create(560,384,format);resetAudio();
    Common::File keys;
    if (replay && (!keys.open(Common::FSNode(ConfMan.getPath("nox_native_startup_keys"))) || keys.size()%4 || keys.size()>4096)) return fail("Native startup key file unavailable or malformed");
    NativeStartupKeys source(keys); Common::EventDispatcher *dispatcher=_eventMan->getEventDispatcher();
    if (replay) dispatcher->registerSource(&source,false);
    struct Unregister { Common::EventDispatcher *dispatcher; Common::EventSource *source; ~Unregister() { if (source) dispatcher->unregisterSource(source); } } cleanup={dispatcher,replay?&source:nullptr};
    Native::HgrFrame &frame=player.frame;Native::TextWindow &storyWindow=player.storyWindow;Native::OpeningBackdrop &storyBackdrop=player.storyBackdrop;
    // Completed and queued opening presentation frames are independent of
    // the text stream. Each ordinary UI key presents the queued frame.
    byte &storyAnimation=player.storyAnimation,&nextStoryAnimation=player.nextStoryAnimation;
    bool &redraw=player.redraw,&sceneReady=player.sceneReady,&storyStarted=player.storyStarted;bool storyCapture=false;uint processedKeys=0,processedReleases=0,storyPages=0;Common::String inputTrace,storyTrace;
    Common::MemoryWriteStreamDynamic storyFrames(DisposeAfterUse::YES);
    Native::WorldStory &window=player.window;Native::StoryMapPatches &story=player.story;Native::ActorPaintRules &actors=player.actors;Native::TerrainLighting &lighting=player.lighting;
    Native::OcclusionRules &occlusion=player.occlusion;Native::TilePainter &painter=player.painter;Native::LightState &light=player.light;Native::VisibilityMask &visibility=player.visibility;
    Native::WorldScene &prepared=player.prepared;Native::WorldActorTurnState &turns=player.turns;Native::WorldWait &wait=player.wait;Native::NpcRouteStore &routes=player.routes;
    Native::GameplayRandom &random=player.random;Native::FixedMessageCatalog messages;Native::TextGlyphSurface &text=player.text;uint waitCommands=0,ordinaryCommands=0;Common::String waitTrace,commandTrace;Native::WorldInput &worldInput=player.worldInput;
    Native::WorldAnimation &animation=player.animation;Native::WorldAnimationState &animationState=player.animationState;bool &animationReady=player.animationReady;uint idleFrames=0;Common::String animationTrace;
    Native::PreparedEncounter &preparedEncounter=player.preparedEncounter;bool &encounterPrepared=player.encounterPrepared;Native::EncounterState &encounterState=player.encounterState;
    uint battleMoveCommands=0,battleOccupiedRefusals=0,battleCompleted=0;uint16 lastBattleCredit=0;Common::String battleMoveTrace;
    Native::BattlePresentation &battlePresentation=player.battlePresentation;if (!battlePresentation.load(_resources)) return fail("Native battle presentation resources unavailable");
    Native::BattleRound battleRound;if (!battleRound.load(_resources)) return fail("Native battle rule data unavailable");
    Native::BattleCannon battleCannon;if (!battleCannon.load(_resources)) return fail("Native cannon rule data unavailable");
    Native::BattleCast battleCast;if (!battleCast.load(_resources)) return fail("Native cast rule data unavailable");
    Native::BattleAftermath aftermath;if (!aftermath.load(_resources)) return fail("Native battle results data unavailable");
    auto presentEncounter=[&]() -> bool {
        if (!encounterState.formed || !encounterPrepared) return false;
        if (encounterState.presented) {
            if (!encounterState.oriented) {
                // Older private presented checkpoints predate ordered facing
                // ownership. Upgrade that operation without another clock tick.
                Native::EncounterState next=encounterState;Native::HgrFrame image;
                if (!next.orientInitial(turns.presentation.facing) || !battlePresentation.paint(preparedEncounter,next,next.phase,image)) return false;
                next.frame=image;encounterState=next;
            }
            return true;
        }
        Native::EncounterState next=encounterState;Native::HgrFrame image;
        if (!next.orientInitial(turns.presentation.facing) || !battlePresentation.paint(preparedEncounter,next,next.phase,image) || !Native::WorldMove::completeClock(game.world,game.clock)) return false;
        next.frame=image;next.presented=true;encounterState=next;return true;
    };
    const int idleBudget=replay && ConfMan.hasKey("nox_native_idle_frames")?ConfMan.getInt("nox_native_idle_frames"):0;
    const bool pacedReplay=replay && ConfMan.hasKey("nox_native_idle_paced") && ConfMan.getBool("nox_native_idle_paced");
    if (idleBudget<0 || idleBudget>256 || !animation.load(_resources)) return fail("Native animation setup unavailable or frame budget invalid");
    uint32 &lastAnimationTick=player.lastAnimationTick;lastAnimationTick=_system->getMillis();
    Common::String animationTiming;
    const bool resumed=ConfMan.hasKey("nox_native_startup_resume") && ConfMan.getBool("nox_native_startup_resume");
    auto saveExploration=[&](const char *name) -> bool {
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving(name,false);if (!out) return false;
        bool ok=Native::ExplorationSave::save(*out,game,window,prepared,turns,routes,random,text,frame,visibility,light,worldInput,animationState,encounterState,&player.music);
        out->finalize();ok=ok&&!out->err();delete out;return ok;
    };
    if (resumed) {
        Common::InSaveFile *in=_system->getSavefileManager()->openForLoading("nox-native-exploration.bin");if (!in) return fail("Native exploration checkpoint unavailable");
        const bool ok=Native::ExplorationSave::load(*in,_resources,game,window,prepared,turns,routes,random,text,frame,visibility,light,worldInput,animationState,encounterState,&player.music);delete in;
        if (!ok || !creation.adoptReadyGame(game,_resources) || !wait.load(_resources,game.world.mapType()) || !status.load(_resources,game.world.mapType()) || !battlePresentation.load(_resources,game.world.mapType()) || !messages.load(_resources)) return fail("Native exploration checkpoint rejected");
        if (replay && ConfMan.hasKey("nox_native_world_fixture_type")) {
            const char *fixtureKeys[]={"nox_native_world_fixture_type","nox_native_world_fixture_map","nox_native_world_fixture_x","nox_native_world_fixture_y"};
            byte location[4];
            for (uint i=0;i<4;++i) {
                if (!ConfMan.hasKey(fixtureKeys[i]) || ConfMan.getInt(fixtureKeys[i])<0 || ConfMan.getInt(fixtureKeys[i])>255) return fail("Controlled world location fields invalid");
                location[i]=byte(ConfMan.getInt(fixtureKeys[i]));
            }
            const bool mountedFixture=!game.world.onFoot();
            if (source.count || worldInput.encounter().battle || encounterState.formed) return fail("Controlled world initialization rejected before input");
            if (mountedFixture) {
                if (!worldInput.canRepositionMountedFixture() || !Native::WorldCheckpointFixture::repositionMounted(game,worldInput.transportObject(),location[0],location[1],location[2],location[3])) return fail("Controlled same-map mounted position rejected before input");
            } else if (!Native::WorldCheckpointFixture::relocate(_resources,game,location[0],location[1],location[2],location[3])) return fail("Controlled world initialization rejected before input");
            if (ConfMan.hasKey("nox_native_world_fixture_conversation_setup")) {
                Common::File initial;
                if (!initial.open(Common::FSNode(ConfMan.getPath("nox_native_world_fixture_conversation_setup"))) ||
                        !Native::WorldCheckpointFixture::conversation(game,light,random,initial)) return fail("Controlled source conversation prerequisites rejected before world input");
            }
            if (ConfMan.hasKey("nox_native_world_fixture_party_conditions")) {
                const int health=ConfMan.hasKey("nox_native_world_fixture_party_health")?ConfMan.getInt("nox_native_world_fixture_party_health"):-1;
                if (!Native::WorldCheckpointFixture::conditions(game,ConfMan.get("nox_native_world_fixture_party_conditions"),health)) return fail("Controlled party condition setup rejected before world input");
            }
            if (ConfMan.hasKey("nox_native_world_fixture_full_mana") && ConfMan.getBool("nox_native_world_fixture_full_mana") &&
                !Native::WorldCheckpointFixture::restoreMana(game)) return fail("Controlled source maximum mana initialization failed before input");
            if (ConfMan.hasKey("nox_native_world_fixture_actor_bank")) {
                const char *actorKeys[]={"bank","slot","x","y","state","capability","flags"};byte values[7];
                for (uint i=0;i<7;++i) {
                    const Common::String key=Common::String("nox_native_world_fixture_actor_")+actorKeys[i];
                    if (!ConfMan.hasKey(key) || ConfMan.getInt(key)<0 || ConfMan.getInt(key)>255) return fail("Controlled actor fields invalid before input");
                    values[i]=byte(ConfMan.getInt(key));
                }
                if (!Native::WorldCheckpointFixture::actor(game,values,7)) return fail("Controlled existing actor setup rejected before input");
            }
            const bool retainedProjectileMode=turns.effects.projectileMode;
            window=Native::WorldStory();turns=Native::WorldActorTurnState();turns.effects.projectileMode=retainedProjectileMode;
            if (!mountedFixture) {Native::WorldInput fresh;fresh.inheritCombatHistory(worldInput);worldInput=fresh;}
            turns.presentation.partyObject=worldInput.transportObject();
            byte empty[187]={};
            if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !story.load(_resources) ||
                !actors.load(_resources,location[0]) || !lighting.load(_resources,location[0]) || !occlusion.load(_resources,location[0]) ||
                !painter.load(_resources,location[0]) || !wait.load(_resources,location[0]) || !status.load(_resources,location[0]) ||
                !battlePresentation.load(_resources,location[0]) || !worldInput.loadRules(_resources,location[0]) || !status.initializeFrame(frame)) return fail("Controlled world owner initialization failed");
            if (!Native::WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,game.clock.lightActive,0,empty,turns.presentation,light,visibility,frame,&prepared)) return fail("Controlled world viewport preparation failed");
            if (!Native::WorldCheckpointFixture::standing(_resources,game,prepared)) return fail("Controlled world standing position is blocked");
            if (!status.paint(game,prepared,light,false,frame) || !animationState.synchronize(frame)) return fail("Controlled world status preparation failed");
            if (!saveExploration("nox-native-controlled-world-before-input.bin")) return fail("Controlled world pre-input persistence failed");
        }
        sceneReady=true;animationReady=true;
        if (worldInput.encounter().battle) {
            if (!Native::EncounterPreparation::prepare(_resources,game,prepared,turns,worldInput.encounter(),preparedEncounter,worldInput.scriptedEncounter())) return fail("Native restored encounter preparation reached an unsupported admission");
            encounterPrepared=true;
            if (!encounterState.formed && !encounterState.begin(_resources,game,preparedEncounter,random)) return fail("Native restored contact could not complete admission");
            if (!presentEncounter()) return fail("Native restored encounter presentation unavailable");
        }
    }
    if (replay && ConfMan.hasKey("nox_native_combat_book_setup")) {
        const int spell=ConfMan.getInt("nox_native_combat_book_setup");
        const int mana=ConfMan.hasKey("nox_native_combat_fixture_mana")?ConfMan.getInt("nox_native_combat_fixture_mana"):-1;
        const int health=ConfMan.hasKey("nox_native_combat_fixture_health")?ConfMan.getInt("nox_native_combat_fixture_health"):-1;
        Common::String report;
        if (!resumed || source.count || spell<0 || spell>=64 || !Native::prepareCombatBookFixture(_resources,byte(spell),mana,health,game,encounterState,report)) return fail("Controlled combat book setup rejected before input");
        if (!status.paint(game,prepared,light,false,frame) || !animationState.synchronize(frame) ||
            !battlePresentation.paint(preparedEncounter,encounterState,encounterState.phase,encounterState.frame) ||
            !saveExploration("nox-native-controlled-before-input.bin")) return fail("Controlled combat setup could not persist its pre-operation state");
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-controlled-setup.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(report);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    const bool autoConversation=replay && ConfMan.hasKey("nox_native_conversation_finish") && ConfMan.getBool("nox_native_conversation_finish");
    const bool autoInn=replay && ConfMan.hasKey("nox_native_inn_rest") && ConfMan.getBool("nox_native_inn_rest");
    const bool pauseConversationInput=replay && ConfMan.hasKey("nox_native_conversation_pause_input") && ConfMan.getBool("nox_native_conversation_pause_input");
    const bool autoInventoryReading=replay && ConfMan.hasKey("nox_native_inventory_read_finish") && ConfMan.getBool("nox_native_inventory_read_finish");
    uint conversationAutomaticKeys=0;
    const bool autoPlay=replay && ConfMan.hasKey("nox_native_combat_finish_battle") && ConfMan.getBool("nox_native_combat_finish_battle");
    const bool endingAcceptance=autoPlay && ConfMan.hasKey("nox_native_combat_finish_ending") && ConfMan.getBool("nox_native_combat_finish_ending");
    // Source setting609B=1 selects forced Quick before any battle action.
    // Ask-mode/difficulty-message admission is a separate caller contract.
    const bool forcedQuick=ConfMan.hasKey("nox_native_combat_quick") && ConfMan.getBool("nox_native_combat_quick");
    const bool askQuick=ConfMan.hasKey("nox_native_combat_ask") && ConfMan.getBool("nox_native_combat_ask");
    const int quickLimit=replay && ConfMan.hasKey("nox_native_quick_action_limit")?ConfMan.getInt("nox_native_quick_action_limit"):0;
    Native::QuickBattle quickRules;
    if ((forcedQuick && askQuick) || quickLimit<0 || quickLimit>10000 || ((forcedQuick || askQuick || encounterState.quick.partyCount || encounterState.mode.selection==1) && !quickRules.load(_resources))) return fail("Native Quick policy setup unavailable");
    auto prepareQuick=[&](Native::EncounterState &battle,bool preserveLevel) -> bool {
        Common::Array<byte> group;
        return !battle.round.allyCount && _resources.readRange("NA/SRTN.COMBAT.S",0x113c+uint(preparedEncounter.admission.group)*42,1,group) &&
            Native::QuickBattle::prepare(battle.round.partyCount,battle.round.enemyCount,battle.round.category,(group[0]&128)!=0,preserveLevel,battle.records,battle.quick);
    };
    Native::CombatFixtureInput automatic;uint automaticKeys=0;Common::String automaticTrace;
    if (autoPlay && (!resumed || !encounterState.formed || !automatic.load(_resources))) return fail("Combat input driver requires a resumed controlled battle");
    auto recordStoryFrame=[&]() -> bool {
        Native::HgrFrame current;if (!storyBackdrop.render(current,storyAnimation) || !storyWindow.paint(current)) return false;
        storyFrames.writeUint16LE(uint16(storyWindow.position()));storyFrames.write(current.data(),8192);++storyPages;return !storyFrames.err();
    };
    auto advanceAnimation=[&]() -> bool {
        if (worldInput.wholeFrameMenu()) {++idleFrames;lastAnimationTick=_system->getMillis();return true;}
        if (!sceneReady || !animationReady) return false;
        if (worldInput.encounter().battle) return false;
        Native::WorldAnimationContext context;context.appearance=game.appearance;context.partyObject=turns.presentation.partyObject;context.notice=worldInput.noticeActive();
        Common::Array<Native::WorldAnimationDraw> draws;
        if (!animation.run(game.world,prepared,visibility.cells(),187,light,context,animationState,draws)) return false;
        const uint32 now=_system->getMillis();
        animationTiming+=Common::String::format("%u\t%u\n",idleFrames+1,uint32(now-lastAnimationTick));
        frame=animationState.pages[animationState.target^1];redraw=true;++idleFrames;lastAnimationTick=now;
        animationTrace+=Common::String::format("%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",idleFrames,uint(animationState.phase),uint(animationState.pattern),uint(animationState.groups[0]),uint(animationState.groups[1]),uint(animationState.groups[2]),uint(animationState.groups[3]),uint(animationState.groups[4]),uint(animationState.target),draws.size());
        return true;
    };
    Common::String battleReturnStep;
    auto returnFromBattle=[&]() -> bool {
                    battleReturnStep="result completion";
                    if (!aftermath.returnReady(encounterState)) return false;
                    battleReturnStep="parent contact retirement";
                    if (!aftermath.retireWorldContact(worldInput.encounter(),game,encounterState,worldInput.scriptedEncounter())) return false;
                    const bool scriptedTravel=worldInput.scriptedEncounter();
                    const bool templeTravel=encounterState.exit.phase!=3 && Native::EndingPresentation::revelation(game.world.mapId(),game.world.field(56),game.world.field(59));
                    battleReturnStep="scripted entrance continuation";
                    if (scriptedTravel && !worldInput.continueScriptedEncounter(_resources,game,window,prepared,turns,light,visibility,frame,animationState.phase)) return false;
                    battleReturnStep="Temple map/light/schedule travel";
                    if (templeTravel && !worldInput.continueTempleVictory(_resources,game,window,prepared,turns,light,visibility,frame,animationState.phase)) return false;
                    battleReturnStep="destination rule loading";
                    if (!story.load(_resources) || !actors.load(_resources,game.world.mapType()) || !lighting.load(_resources,game.world.mapType()) ||
                        !occlusion.load(_resources,game.world.mapType()) || !painter.load(_resources,game.world.mapType()) || !status.load(_resources,game.world.mapType()) || !wait.load(_resources,game.world.mapType()) || !worldInput.loadRules(_resources,game.world.mapType())) return false;
                    Native::HgrFrame nextFrame=frame;byte painted[187]={};
                    battleReturnStep="destination viewport/status and encounter retirement";
                    if ((!scriptedTravel && !templeTravel && (!status.initializeFrame(nextFrame) || !Native::WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,
                            game.appearance,0,animationState.phase,painted,turns.presentation,light,visibility,nextFrame,&prepared))) ||
                        !status.paint(game,prepared,light,false,nextFrame) || !animationState.synchronize(nextFrame) || !worldInput.finishEncounter(encounterState.round.protectedTargets)) return false;
                    if (encounterState.exit.phase==3) { turns.compact.freezeTimer=3;turns.npc.freezeTimer=3; }
                    lastBattleCredit=encounterState.records.credit;++battleCompleted;player.music.leaveCombat();
                    frame=nextFrame;const bool partyTargeting=encounterState.attack.partyTargeting;encounterState=Native::EncounterState();encounterState.attack.partyTargeting=partyTargeting;encounterPrepared=false;redraw=true;lastAnimationTick=_system->getMillis();
                    battleReturnStep=Common::String::format("Archus automatic Talk map%u/%u xy%u/%u north slot%u kind%u hour%u",game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),prepared.actors.lookup[76],prepared.actors.kinds[76],game.clock.hour);
                    return !templeTravel || worldInput.beginTempleConversation(_resources,game,prepared,frame);
    };
    const int automaticRoundLimit=ConfMan.hasKey("nox_native_automatic_round_checkpoint")?ConfMan.getInt("nox_native_automatic_round_checkpoint"):0;
    if (automaticRoundLimit<0 || automaticRoundLimit>128) return fail("Native controlled automatic-round checkpoint limit invalid");
    _nativePlayer=&player;
    struct NativeSessionRegistration {NativePlayerSession *&slot;~NativeSessionRegistration(){slot=nullptr;}} nativeRegistration={_nativePlayer};
    _runtimeSession=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
    if (resumed && !_runtimeSession.empty()) Common::CEAchievements().noteDisqualifyingAssistance("nox_native_development_checkpoint");
    if (!resumed && ConfMan.hasKey("save_slot") && ConfMan.getInt("save_slot")>=0) {
        const Common::Error loaded=loadGameState(ConfMan.getInt("save_slot"));if (loaded.getCode()!=Common::kNoError) return loaded;
    }
    const int playerValidation=replay && ConfMan.hasKey("nox_native_player_validation")?ConfMan.getInt("nox_native_player_validation"):0;
    NativePlayerLifecycleProbe playerProbe(*this,player,playerValidation);Common::String playerError;
    if (!playerProbe.start(playerError)) return fail(playerError.c_str());
    if (playerProbe.finished()) return Common::kNoError;
    while (!shouldQuit()) {
        if (!synchronizePlaySession()) return fail("Native player session could not be initialized");
        if (!replay && player.music.ready()) {
            const uint32 now=_system->getMillis();
            if (!player.lastAudioTick) player.lastAudioTick=now;
            const uint elapsed=MIN<uint>(100,uint32(now-player.lastAudioTick));
            if (elapsed>=10 && _nativeMusicAudio->numQueuedStreams()<8) {
                int16 samples[4800];const uint count=elapsed*48;
                if (!player.music.render(samples,count) || !queueNativeMusic(samples,count)) return fail("Native music service failed");
                player.lastAudioTick=now;
            }
        }
        // World queued shots retain the shared projectile presentation mode.
        // It is owned/persisted by the world turn, stable throughout battle.
        battleRound.setProjectileMode(turns.effects.projectileMode);
        if (encounterState.presented && !encounterState.resultPhase && askQuick && !encounterState.mode.selection && !encounterState.quick.partyCount && !encounterState.command && !encounterState.round.actions) {
            bool forbidFlee;
            const bool tactical=Native::CombatModeRules::forcedTactical(encounterState.round.category,preparedEncounter.contactSprite,preparedEncounter.contextFlags,forbidFlee);
            if (forbidFlee) encounterState.exit.control=128;
            byte levels[6];for (uint i=0;i<encounterState.round.partyCount;++i) levels[i]=encounterState.records.stats[i*128+1];Native::CombatAdvice advice;
            if (!Native::CombatModeRules::advice(levels,encounterState.round.partyCount,encounterState.records.stats[769],encounterState.round.mapId,
                    encounterState.records.stats[784],encounterState.round.enemyCount,advice)) return fail("Native encounter mode advice rejected its owned levels");
            encounterState.mode.selection=tactical?2:1;encounterState.mode.average=advice.average;encounterState.mode.difference=advice.difference;encounterState.mode.warning=advice.warning;
            Native::HgrFrame image;const Common::String prompt=battlePresentation.modePrompt(advice.warning);
            if (!battlePresentation.paint(preparedEncounter,encounterState,encounterState.phase,image,tactical?nullptr:&prompt)) return fail("Native combat mode prompt unavailable");
            encounterState.frame=image;redraw=true;
        }
        if (encounterState.presented && !encounterState.resultPhase && forcedQuick && !encounterState.mode.selection && !encounterState.quick.partyCount) {
            bool forbidFlee=false;
            const bool tactical=Native::CombatModeRules::forcedTactical(encounterState.round.category,preparedEncounter.contactSprite,preparedEncounter.contextFlags,forbidFlee);
            if (forbidFlee) encounterState.exit.control=128;
            if (!tactical && !encounterState.command && !encounterState.round.actions) {
                if (!prepareQuick(encounterState,false)) return fail("Native Quick preparation rejected its source-selected roster");
            }
        }
        // A:9E8E..9EA0 reaches BK1:DF50 only after the mode decision and
        // roster preparation. Keep that cue/RNG transaction once per battle,
        // including cold resumes; Ask's pending prompt has not reached it.
        if (encounterState.presented && !encounterState.resultPhase && encounterState.mode.selection!=1 && player.music.ready() && !player.music.battleCue() &&
            !player.music.combat(_resources,random,encounterState.round.mapId,preparedEncounter.contactSprite)) return fail("Native combat music selection failed");
        if (encounterState.presented && encounterState.quick.partyCount && !encounterState.quick.phase) {
            if (quickLimit && encounterState.quick.actions>=uint(quickLimit)) break;
            if (!quickRules.step(random,encounterState.records,encounterState.quick,encounterState.round.protectedTargets)) return fail("Native Quick scheduler/action reached an unsupported branch");
            encounterState.round.actions=encounterState.quick.actions;encounterState.round.rounds=encounterState.quick.rounds;
            encounterState.round.activeParty=encounterState.quick.side?0:encounterState.quick.cursor;
            if (encounterState.quick.phase) {
                const bool won=encounterState.quick.phase==1;
                // Original terminal defeat never enters R's restoration/results
                // path. Keep its terminal records;only victory restores here.
                if (won && !Native::QuickBattle::restore(encounterState.records,encounterState.quick)) return fail("Native Quick temporary metadata could not restore");
                encounterState.quick.phase=won?3:4;encounterState.round.phase=won?Native::kBattleVictory:Native::kBattleDefeat;
            }
            Native::HgrFrame image;const Common::String *notice=encounterState.quick.phase==3?&battlePresentation.quickVictory():nullptr;
            if (!battlePresentation.paint(preparedEncounter,encounterState,encounterState.phase,image,notice)) return fail("Native Quick roster/outcome presentation failed");
            encounterState.frame=image;redraw=true;
        }
        if (encounterState.presented && encounterState.round.phase==Native::kBattleAutomaticTurn && !encounterState.command &&
                !encounterState.resultPhase && !encounterState.mode.selection && !encounterState.quick.partyCount) {
            if (replay && automaticRoundLimit && encounterState.round.rounds+1>=uint(automaticRoundLimit)) break;
            if (!battleRound.automatic(preparedEncounter.terrain.data(),187,encounterState.round,random,encounterState.records))
                return fail("Native automatic status round could not complete");
            Native::HgrFrame image;if (!battlePresentation.paint(preparedEncounter,encounterState,encounterState.phase,image)) return fail("Native automatic status round presentation failed");
            encounterState.frame=image;redraw=true;
        }
        if (redraw) {
            if (!sceneReady && (storyStarted?(!storyBackdrop.render(frame,storyAnimation)||!storyWindow.paint(frame)):!creation.render(font,frame))) return fail("Native startup UI render failed");
            Common::Array<byte> rgb;(encounterState.presented?encounterState.frame:frame).renderRGB(rgb);
            for (uint y=0;y<384;++y) for (uint x=0;x<560;++x) {
                const byte *pixel=rgb.data()+(y*560+x)*3;
                *static_cast<uint32 *>(_surface.getBasePtr(x,y))=format.RGBToColor(pixel[0],pixel[1],pixel[2]);
            }
            _system->copyRectToScreen(_surface.getPixels(),_surface.pitch,0,0,560,384); _system->updateScreen(); redraw=false;
            if (storyCapture && ConfMan.hasKey("nox_native_startup_capture") && ConfMan.getBool("nox_native_startup_capture")) {
                Common::OutSaveFile *image=_system->getSavefileManager()->openForSaving("nox-native-startup-story.png",false);
                if (!image) return Common::kWritingFailed;
                bool ok=Image::writePNG(*image,_surface);image->finalize();ok=ok&&!image->err();delete image;if (!ok) return Common::kWritingFailed;
            }
            storyCapture=false;
        }
        if (replay && encounterState.presented && encounterState.round.phase==Native::kBattleAutomaticTurn && !encounterState.mode.selection && !encounterState.quick.partyCount) continue;
        Common::Event event;bool generated=false;
        if (replay && encounterState.quick.partyCount && !encounterState.quick.phase) continue;
        const byte continuedDirection=sceneReady && !encounterState.presented?worldInput.automaticDirection():255;
        if (continuedDirection<4) {
            const Common::KeyCode directions[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
            event.type=Common::EVENT_KEYDOWN;event.kbd.keycode=directions[continuedDirection];event.kbd.ascii=0;generated=true;
        }
        // Dispatcher may enqueue the whole source at once. Exhaustion alone
        // says nothing about how many queued events reached this handler.
        if (!generated && replay && source.finished() && processedKeys==source.count && processedReleases==source.count) {
            if (playerProbe.active()) {
                if (!playerProbe.next(event,generated,playerError)) return fail(playerError.c_str());
                if (playerProbe.finished()) break;
                if (!generated) return fail("Native player lifecycle driver has no next input");
            } else if (autoInventoryReading && worldInput.inventoryStage()==4) {
                if (++conversationAutomaticKeys>128) return fail("Bounded document paging could not finish its native continuation");
                event.type=Common::EVENT_KEYDOWN;event.kbd.keycode=Common::KEYCODE_RETURN;event.kbd.ascii=13;generated=true;
            } else if ((autoConversation || autoInn) && worldInput.conversationStage()) {
                const byte phase=worldInput.conversationStage();
                if (pauseConversationInput && (phase==3 || phase==5 || phase==6 || phase==9 || phase==10 || phase==11 || phase==15 || phase==16 || phase==17 || phase==20 || (phase>=23 && phase<=25))) break;
                if (phase==1 || ++conversationAutomaticKeys>128) return fail("Bounded native conversation input cannot finish its phase");
                // Real Return acknowledges paging/closing;an empty submitted
                // line uses the original BYE path. No state outcome is set.
                event.type=Common::EVENT_KEYDOWN;event.kbd.keycode=Common::KEYCODE_RETURN;event.kbd.ascii=13;generated=true;
                if (autoInn && (phase==9 || phase==11)) {event.kbd.keycode=Common::KEYCODE_y;event.kbd.ascii='Y';}
                else if (autoInn && phase==10) {event.kbd.keycode=Common::KEYCODE_r;event.kbd.ascii='R';}
            } else if (autoPlay) {
                if (!automaticKeys && !saveExploration("nox-native-after-authored-input.bin")) return Common::kWritingFailed;
                if (!automatic.next(encounterState,preparedEncounter,event)) {
                    if (!saveExploration("nox-native-combat-input-stop.bin")) return Common::kWritingFailed;
                    Common::OutSaveFile *trace=_system->getSavefileManager()->openForSaving("nox-native-combat-input-stop.tsv",false);
                    if (!trace) return Common::kWritingFailed;
                    trace->writeString(automaticTrace);trace->finalize();const bool written=!trace->err();delete trace;if (!written) return Common::kWritingFailed;
                    return fail(Common::String::format("Bounded combat input stopped:steps%u command%u phase%u results%u;exact state/trace saved",automatic.steps,uint(encounterState.command),uint(encounterState.round.phase),uint(encounterState.resultPhase)).c_str());
                }
                if (automatic.finished) break;generated=true;++automaticKeys;
                automaticTrace+=Common::String::format("%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",automaticKeys,uint(event.kbd.keycode),uint(encounterState.command),uint(encounterState.round.phase),
                    uint(encounterState.resultPhase),uint(READ_LE_UINT16(encounterState.records.stats+2)),uint(READ_LE_UINT16(encounterState.records.stats+770)),uint(encounterState.records.credit));
            } else {
                if (idleFrames>=uint(idleBudget)) break;
                if (!pacedReplay) { if (!advanceAnimation()) return fail("Native deterministic idle reached an unsupported animation boundary");continue; }
            }
        }
        if (!generated && !_eventMan->pollEvent(event)) {
            if ((!replay || pacedReplay) && sceneReady && !encounterState.presented && uint32(_system->getMillis()-lastAnimationTick)>=33) {
                if (!advanceAnimation()) return fail("Native live idle reached an unsupported animation boundary");continue;
            }
            if (!replay || pacedReplay) _system->delayMillis(1);continue;
        }
        if (event.type==Common::EVENT_QUIT || event.type==Common::EVENT_RETURN_TO_LAUNCHER) break;
        if (!generated && event.type==Common::EVENT_KEYDOWN) ++processedKeys;
        if (!generated && event.type==Common::EVENT_KEYUP) ++processedReleases;
        if (sceneReady) {
            if (event.type!=Common::EVENT_KEYDOWN) continue;
            if (replay) inputTrace+=Common::String::format("%u\t%u\t%s\t%u\t%u\n",processedKeys,uint(creation.stage()),creation.name().c_str(),creation.gender(),creation.race());
            // The accepted CE Q workflow opens the shared save chooser only
            // at an idle world command; dialogue/combat retain their Q input.
            if (((!encounterState.formed && worldInput.atWorldCommand()) || encounterState.resultPhase==4) && event.kbd.keycode==Common::KEYCODE_q) {
                if (!playerProbe.beforeQ(playerError)) return fail(playerError.c_str());
                const bool saved=saveGameDialog();
                if (!playerProbe.afterQ(saved,playerError)) return fail(playerError.c_str());
                redraw=true;lastAnimationTick=_system->getMillis();continue;
            }
            if (encounterState.presented) {
                const Common::KeyCode key=event.kbd.keycode;
                if (encounterState.mode.selection==1) {
                    const byte answer=event.kbd.ascii?byte(event.kbd.ascii):byte(key);
                    const Native::CombatModeChoice choice=Native::CombatModeRules::choose(255,true,answer);
                    if (!choice.accepted) continue;
                    Native::EncounterState next=encounterState;next.mode.selection=choice.mode?3:2;
                    if (choice.mode && !prepareQuick(next,(next.mode.difference&128)!=0)) return fail("Native selected Quick mode could not prepare its source roster");
                    Native::HgrFrame image;if (!battlePresentation.paint(preparedEncounter,next,next.phase,image)) return fail("Native selected combat mode could not present");
                    next.frame=image;encounterState=next;redraw=true;continue;
                }
                if (encounterState.quick.partyCount && !encounterState.quick.phase) continue;
                if (encounterState.resultPhase==4) continue;
                if (encounterState.resultPhase==5) {
                    Native::HgrFrame restored;
                    if (!battlePresentation.paint(preparedEncounter,encounterState,encounterState.phase,restored) ||
                        !aftermath.acknowledgeRevelation(game,encounterState,random)) return fail("Native Temple revelation acknowledgement failed");
                    encounterState.frame=restored;if (!aftermath.paint(font,encounterState)) return fail("Native Temple results continuation failed");
                    redraw=true;continue;
                }
                if (encounterState.resultPhase==3) {
                    if (!aftermath.abilityInput(key,game,encounterState,random) || !aftermath.paint(font,encounterState)) return fail("Native advancement could not complete its character/result continuation");
                    redraw=true;continue;
                }
                if (encounterState.resultPhase==1) {
                    const bool all=key==Common::KEYCODE_a || key==Common::KEYCODE_ESCAPE;
                    const bool take=all || key==Common::KEYCODE_t || key==Common::KEYCODE_y;
                    if (!take && key!=Common::KEYCODE_l && key!=Common::KEYCODE_n) continue;
                    if (!aftermath.choose(take,all,game,encounterState,random) || !aftermath.paint(font,encounterState)) return fail("Native treasure decision reached an unsupported inventory/result branch");
                    redraw=true;continue;
                }
                if (encounterState.resultPhase==2) {
                    if (!returnFromBattle()) return fail((Common::String("Native battle return failed: ")+battleReturnStep).c_str());
                    continue;
                }
                if (encounterState.round.phase==Native::kBattleVictory && !encounterState.command && (key==Common::KEYCODE_ESCAPE || encounterState.quick.phase==3)) {
                    if (!aftermath.begin(worldInput.encounter(),game,encounterState,random,worldInput.scriptedEncounter(),preparedEncounter.contactSprite)) return fail("Native victory results reached an unsupported level/contact/loot branch");
                    if (encounterState.resultPhase==4) {
                        // Source R:9938..994B replaces enabled music only after
                        // actual victory acknowledgement selects the ending.
                        if (player.music.ready() && !player.music.ending(_resources)) return fail("Native ending music resource unavailable");
                        Native::HgrFrame ending=frame;
                        if (!status.paint(game,prepared,light,false,ending) || !Native::EndingPresentation::paint(_resources,game.world,ending)) return fail("Native authored campaign ending presentation failed");
                        encounterState.frame=ending;
                    } else if (encounterState.resultPhase==5) {
                        if (player.music.ready() && !player.music.revelation(_resources)) return fail("Native Temple revelation music unavailable");
                        Native::HgrFrame revelation=frame;
                        if (!status.paint(game,prepared,light,false,revelation) || !Native::EndingPresentation::paintRevelation(_resources,game.world,revelation)) return fail("Native Temple revelation presentation failed");
                        encounterState.frame=revelation;
                    } else if (!aftermath.paint(font,encounterState)) return fail("Native victory results presentation failed");
                    redraw=true;continue;
                }
                // Source game-over input is terminal. Quit/menu remain host
                // events;ordinary keys cannot award rewards or resume the world.
                if (encounterState.round.phase==Native::kBattleDefeat || encounterState.round.phase==Native::kBattleAutomaticTurn) continue;
                if (encounterState.exit.phase) {
                    Native::EncounterState next=encounterState;
                    if (next.exit.phase==2) next.exit.phase=0;
                    else {
                        byte control=next.exit.control;bool prompt=false,refusal=false;
                        const byte answer=event.kbd.ascii?byte(event.kbd.ascii|128):byte(key|128);
                        const Native::BattleExitDecision decision=Native::BattleTerminal::decideExit(next.records,true,next.exit.control,answer,control,prompt,refusal);
                        next.exit.phase=decision==Native::kBattleExitRefused?2:decision==Native::kBattleExitFled?3:0;next.exit.control=control;
                    }
                    if (next.exit.phase==3) {
                        if (!aftermath.escape(game,next)) return fail("Native flee could not commit its party/currency return");
                        encounterState=next;if (!returnFromBattle()) return fail("Native flee could not restore its parent world");continue;
                    }
                    Native::HgrFrame image;if (!battlePresentation.paint(preparedEncounter,next,next.phase,image)) return fail("Native flee confirmation/refusal presentation failed");
                    next.frame=image;encounterState=next;redraw=true;continue;
                }
                if (!encounterState.command && key==Common::KEYCODE_ESCAPE) {
                    encounterState.exit.phase=1;Native::HgrFrame image;
                    if (!battlePresentation.paint(preparedEncounter,encounterState,encounterState.phase,image)) return fail("Native flee confirmation could not be presented");
                    encounterState.frame=image;redraw=true;continue;
                }
                if (encounterState.command==3) {
                    Native::EncounterState next=encounterState;Native::GameplayRandom after=random;Native::HgrFrame image;
                    const uint member=next.round.activeParty,index=member*4;
                    const byte caster=byte(next.records.compact[index+1]*17+next.records.compact[index]);
                    if (key==Common::KEYCODE_ESCAPE || key==Common::KEYCODE_TAB) {
                        Native::BattleCannon::cancel(next.cannon);next.command=0;next.target=255;
                    } else if (key==Common::KEYCODE_UP || key==Common::KEYCODE_DOWN || key==Common::KEYCODE_LEFT || key==Common::KEYCODE_RIGHT) {
                        const byte direction=key==Common::KEYCODE_UP?0:key==Common::KEYCODE_DOWN?1:key==Common::KEYCODE_RIGHT?2:3;
                        if (!Native::BattleTargets::cycle(next.records,caster,255,next.attack.partyTargeting,direction,next.target)) return fail("Native cannon target cycling failed");
                    } else {
                        if (key!=Common::KEYCODE_RETURN && key!=Common::KEYCODE_a && key!=Common::KEYCODE_f && key!=Common::KEYCODE_SPACE) continue;
                        Native::CannonOutcome result;
                        if (!Native::BattleAttack::remember(next.records,caster,next.target,next.attack) ||
                            !battleCannon.fire(caster,next.target,next.round.category,turns.effects.projectileMode,next.cannon,after,next.records,result,next.round.protectedTargets)) return fail("Native cannon impact failed");
                        next.command=0;next.target=255;
                        if (!battleRound.pass(preparedEncounter.terrain.data(),preparedEncounter.terrain.size(),next.round,after,next.records)) return fail("Native cannon turn retirement failed");
                    }
                    if (!battlePresentation.paint(preparedEncounter,next,next.phase,image)) return fail("Native cannon target presentation failed");
                    next.frame=image;encounterState=next;random=after;redraw=true;continue;
                }
                if (encounterState.command==2) {
                    Native::EncounterState next=encounterState;Native::GameplayRandom after=random;Native::HgrFrame image;Common::String notice;
                    const Native::BattleCastInput handled=battleCast.handle(event,game.book,preparedEncounter.terrain.data(),preparedEncounter.terrain.size(),battleRound,after,next,notice);
                    if (handled==Native::kBattleCastUnsupported) return fail("Native Cast reached an unsupported family/status/NPC continuation");
                    if (handled==Native::kBattleCastIgnored) continue;
                    if (!battlePresentation.paint(preparedEncounter,next,next.phase,image,notice.empty()?nullptr:&notice)) return fail("Native Cast presentation rejected its pending state");
                    next.frame=image;encounterState=next;random=after;redraw=true;continue;
                }
                if (encounterState.command==1) {
                    Native::EncounterState next=encounterState;Native::GameplayRandom after=random;Native::PhysicalOutcome action;Native::HgrFrame image;
                    const uint index=next.round.activeParty*4;const byte caster=byte(next.records.compact[index+1]*17+next.records.compact[index]);
                    if (key==Common::KEYCODE_ESCAPE) Native::BattleAttack::cancel(next.attack);
                    else if (key==Common::KEYCODE_TAB) {
                        if (!Native::BattleAttack::switchHand(next.records,caster,next.attack.partyTargeting,next.attack,next.target)) return fail("Native hand selection could not find its pending target");
                    } else if (key==Common::KEYCODE_UP || key==Common::KEYCODE_DOWN || key==Common::KEYCODE_LEFT || key==Common::KEYCODE_RIGHT) {
                        const byte direction=key==Common::KEYCODE_UP?0:key==Common::KEYCODE_DOWN?1:key==Common::KEYCODE_RIGHT?2:3;
                        if (!Native::BattleAttack::cycle(next.records,caster,next.attack.partyTargeting,direction,next.attack,next.target)) return fail("Native attack target cycling rejected its geometry");
                    } else {
                        if (key!=Common::KEYCODE_RETURN && key!=Common::KEYCODE_a && key!=Common::KEYCODE_f && key!=Common::KEYCODE_SPACE) continue;
                        if (!Native::BattleAttack::remember(next.records,caster,next.target,next.attack) ||
                            !battleRound.attackHand(next.target,next.attack.current!=0,preparedEncounter.terrain.data(),preparedEncounter.terrain.size(),next.round,after,next.records,action) ||
                            !Native::BattleAttack::used(next.records,caster,next.attack.partyTargeting,next.attack,next.target)) return fail("Native tactical attack reached an unsupported weapon/turn policy");
                        if (next.round.phase==Native::kBattleDefeat) next.attack.phase=2;
                    }
                    if (next.attack.phase>=2) {
                        if (next.attack.phase==2 && !battleRound.retire(preparedEncounter.terrain.data(),preparedEncounter.terrain.size(),next.round,after,next.records)) return fail("Native completed Attack could not retire its actor turn");
                        next.attack.resetAction();next.command=0;next.target=255;
                    }
                    if (!battlePresentation.paint(preparedEncounter,next,next.phase,image)) return fail("Native tactical attack presentation reached an unsupported status");
                    next.frame=image;encounterState=next;random=after;redraw=true;continue;
                }
                if (key==Common::KEYCODE_f) {
                    Native::EncounterState next=encounterState;Native::GameplayRandom after=random;Native::HgrFrame image;Native::CannonStep step;
                    const uint member=next.round.activeParty,index=member*4;
                    const byte caster=byte(next.records.compact[index+1]*17+next.records.compact[index]);
                    if (!battleCannon.begin(caster,next.cannon,step)) return fail("Native cannon station cycle failed");
                    if (step==Native::kCannonTarget) {
                        if (!Native::BattleTargets::initial(next.records,caster,255,next.attack.partyTargeting,next.attack.rememberedKind[member],next.attack.rememberedIndex[member],next.target)) return fail("Native cannon initial target failed");
                        if (next.target==255) Native::BattleCannon::cancel(next.cannon);else next.command=3;
                    } else if (step!=Native::kCannonNotHere && !battleRound.pass(preparedEncounter.terrain.data(),preparedEncounter.terrain.size(),next.round,after,next.records)) return fail("Native cannon reload turn failed");
                    const Common::String *notice=step==Native::kCannonTarget?nullptr:&battlePresentation.cannonNotice(step);
                    if (!battlePresentation.paint(preparedEncounter,next,next.phase,image,notice)) return fail("Native cannon command presentation failed");
                    next.frame=image;encounterState=next;random=after;redraw=true;continue;
                }
                if (key==Common::KEYCODE_p) { encounterState.attack.partyTargeting=!encounterState.attack.partyTargeting;continue; }
                if (key==Common::KEYCODE_c) {
                    Native::EncounterState next=encounterState;Native::HgrFrame image;
                    if (!Native::BattleCast::begin(next) || !battlePresentation.paint(preparedEncounter,next,next.phase,image)) return fail("Native Cast could not open its slot prompt");
                    next.frame=image;encounterState=next;redraw=true;continue;
                }
                if (key==Common::KEYCODE_a) {
                    Native::EncounterState next=encounterState;const uint index=next.round.activeParty*4;
                    const byte caster=byte(next.records.compact[index+1]*17+next.records.compact[index]);
                    if (!Native::BattleAttack::begin(next.records,caster,next.attack.partyTargeting,next.attack,next.target)) return fail("Native Attack could not prepare both hands and targets");
                    if (next.attack.phase==1) {
                        next.command=1;Native::HgrFrame image;
                        if (!battlePresentation.paint(preparedEncounter,next,next.phase,image)) return fail("Native Attack target prompt could not be presented");
                        next.frame=image;redraw=true;
                    }
                    else { next.attack.resetAction();next.target=255; }
                    encounterState=next;continue;
                }
                if (key==Common::KEYCODE_SPACE) {
                    Native::EncounterState next=encounterState;Native::GameplayRandom after=random;Native::HgrFrame image;
                    if (!battleRound.pass(preparedEncounter.terrain.data(),preparedEncounter.terrain.size(),next.round,after,next.records) ||
                        !battlePresentation.paint(preparedEncounter,next,next.phase,image)) return fail("Native tactical pass reached an unsupported turn/status policy");
                    next.frame=image;encounterState=next;random=after;redraw=true;continue;
                }
                byte direction;
                switch (event.kbd.keycode) {
                case Common::KEYCODE_UP:direction=0;break;case Common::KEYCODE_DOWN:direction=1;break;
                case Common::KEYCODE_RIGHT:direction=2;break;case Common::KEYCODE_LEFT:direction=3;break;
                default: {
                    // The source dispatcher reports unmapped keys and reprompts
                    // without retiring a turn. Keep known missing commands explicit.
                    const char *pending="F+-=*\t!@#$%^";bool known=false;
                    for (uint i=0;pending[i];++i) if (event.kbd.ascii==uint16(pending[i]) || key==pending[i] || (pending[i]=='F' && key==Common::KEYCODE_f)) known=true;
                    if (known) return fail("Native tactical command reached an unimplemented action boundary");
                    Native::HgrFrame image;const Common::String &notice=battlePresentation.invalidCommand();
                    if (!battlePresentation.paint(preparedEncounter,encounterState,encounterState.phase,image,&notice)) return fail("Native invalid command prompt failed");
                    encounterState.frame=image;redraw=true;continue;
                }
                }
                Native::EncounterState next=encounterState;Native::GameplayRandom after=random;Native::HgrFrame image;bool moved=false;
                if (!battleRound.move(direction,preparedEncounter.terrain.data(),preparedEncounter.terrain.size(),next.round,after,next.records,moved) ||
                    !battlePresentation.paint(preparedEncounter,next,next.phase,image)) return fail("Native tactical movement reached an unimplemented contact/terrain/edge branch");
                next.frame=image;encounterState=next;random=after;redraw=true;
                if (!moved) { ++battleOccupiedRefusals;continue; }
                ++battleMoveCommands;
                battleMoveTrace+=Common::String::format("%u\t%u\t%u\t%u\t%u\t%u\t%08X\t%u\t%u\n",battleMoveCommands,uint(direction),uint(next.records.compact[0]),uint(next.records.compact[1]),uint(next.records.extended[0]),uint(next.records.extended[1]),random.seed(),uint(READ_LE_UINT16(next.records.stats+2)),uint(READ_LE_UINT16(next.records.stats+770)));
                continue;
            }
            Native::WorldInputResult handled=worldInput.handle(event,_resources,game,window,prepared,turns,light,visibility,routes,random,messages,text,frame,animationState.phase);
            if (handled==Native::kWorldInputTransition) {
                const byte type=game.world.mapType();
                if (!wait.load(_resources,type) || !status.load(_resources,type) || !battlePresentation.load(_resources,type) ||
                        !actors.load(_resources,type) || !lighting.load(_resources,type) || !occlusion.load(_resources,type) || !painter.load(_resources,type))
                    return fail("Native map transition resource owners unavailable");
                handled=Native::kWorldInputHandled;
            }
            if (handled==Native::kWorldInputBoundary) return fail("Native world command reached an unsupported encounter/interaction boundary");
            if (handled==Native::kWorldInputUnsupported) return fail("Native world command could not complete its admitted phase");
            if (handled==Native::kWorldInputEncounter) {
                if (!Native::EncounterPreparation::prepare(_resources,game,prepared,turns,worldInput.encounter(),preparedEncounter,worldInput.scriptedEncounter())) return fail("Native encounter preparation reached an unsupported admission");
                if (!encounterState.begin(_resources,game,preparedEncounter,random)) return fail("Native encounter formation reached an unsupported layout/group");
                encounterPrepared=true;
            }
            if (handled==Native::kWorldInputHandled || handled==Native::kWorldInputEncounter) ++ordinaryCommands;
            redraw=true;
            if (handled==Native::kWorldInputIgnored && event.kbd.keycode==Common::KEYCODE_SPACE) {
                Native::WorldActorTurnEvent raised;
                const Native::WorldWaitResult result=wait.run(game.world,window,prepared,turns,game.clock,light,visibility,game.schedule,routes,random,messages,text,frame,game.appearance,animationState.phase,raised,true,4,nullptr,&game.party);
                if (result==Native::kWorldWaitBattle) {
                    if (!worldInput.queueEncounter(raised) || !Native::EncounterPreparation::prepare(_resources,game,prepared,turns,raised,preparedEncounter) ||
                        !encounterState.begin(_resources,game,preparedEncounter,random)) return fail("Native Wait could not retain/admit its actual world contact");
                    encounterPrepared=true;handled=Native::kWorldInputEncounter;
                } else if (result==Native::kWorldWaitDefeat) {worldInput.markDefeated();handled=Native::kWorldInputDefeat;}
                else if (result!=Native::kWorldWaitAdvanced) return fail("Native Wait reached an unsupported effect boundary");
                if (handled!=Native::kWorldInputDefeat && !Native::WorldInput::showMessage(messages,4,text,frame)) return fail("Native Wait message presentation failed");
                ++waitCommands;redraw=true;
                waitTrace+=Common::String::format("%u\t%u\t%u\t%u\t%u\t%u\t%u\n",waitCommands,uint(game.clock.phase),uint(game.clock.hour),uint(game.clock.minute),uint(game.clock.lightTicks),uint(game.clock.lightActive),uint(game.clock.hourlyCounter));
            }
            if (!worldInput.wholeFrameMenu() && !status.paint(game,prepared,light,worldInput.noticeActive(),frame)) return fail("Native party/status presentation failed");
            if (worldInput.worldDefeated() && !battlePresentation.paintWorldDefeat(frame)) return fail("Native world defeat presentation failed");
            // Ordinary commands produced this complete native frame. Both
            // retained images adopt its repaint; cadence fields stay owned.
            if (!animationState.synchronize(frame)) return fail("Native command animation synchronization failed");
            if (handled==Native::kWorldInputEncounter && !presentEncounter()) return fail("Native encounter presentation unavailable");
            lastAnimationTick=_system->getMillis();
            if (handled==Native::kWorldInputIgnored && event.kbd.keycode==Common::KEYCODE_SPACE && ConfMan.hasKey("nox_native_checkpoint_wait") &&
                waitCommands==uint(ConfMan.getInt("nox_native_checkpoint_wait")) && !saveExploration("nox-native-exploration.bin")) return Common::kWritingFailed;
            const byte *records=game.world.currentTables()->data();
            commandTrace+=Common::String::format("%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",processedKeys,uint(event.kbd.keycode),uint(game.world.x()),uint(game.world.y()),uint(game.clock.phase),uint(game.clock.hour),uint(game.clock.minute),uint(records[1027]),uint(records[775]),uint(turns.presentation.facing),uint(worldInput.noticeActive())+2*uint(worldInput.selectingOpen()));
            continue;
        }
        bool beginWorld=false;
        if (!storyStarted) {
            if (!creation.handle(event,game)) return fail("Native character confirmation failed");
            if (creation.stage()==Native::CharacterCreation::kCreated) {
                if (!replay && Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName).empty()) {
                    if (!Common::CEAchievements().startPlaythroughForTarget(_targetName,"nox-archaist","apple2-v119",creation.name())) return fail("Native new game could not create a Play Session");
                    _runtimeSession=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);setTotalPlayTime(0);
                }
                Common::Array<byte> loader;
                if (!storyBackdrop.load(_resources) || !storyBackdrop.render(frame,storyAnimation) || !_resources.read("NA/LOADER.P",loader) || loader.size()!=10514 || loader[8281] ||
                    !storyWindow.begin(_resources,loader.data()+7567,714,5,24,6,17,11,&frame)) return fail("Native opening presentation initialization failed");
                storyStarted=true;
                if (!recordStoryFrame()) return fail("Native opening frame observation failed");
                storyTrace+=Common::String::format("start\t%u\t%u\t%u\t%u\n",storyWindow.position(),uint(storyWindow.column()),uint(storyWindow.row()),uint(storyWindow.waiting()));
            }
        } else {
            const uint previousPosition=storyWindow.position();
            if (event.type==Common::EVENT_KEYDOWN) { storyAnimation=nextStoryAnimation;nextStoryAnimation=byte((nextStoryAnimation+1)&3); }
            if (storyWindow.ended() && event.type==Common::EVENT_KEYDOWN) beginWorld=true;
            else if (!storyWindow.handle(event)) return fail("Native opening text continuation failed");
            if (storyWindow.position()!=previousPosition && !recordStoryFrame()) return fail("Native opening frame observation failed");
            if (event.type==Common::EVENT_KEYDOWN) {
                storyTrace+=Common::String::format("key\t%u\t%u\t%u\t%u\t%u\t%u\n",uint(event.kbd.keycode),storyWindow.position(),uint(storyWindow.column()),uint(storyWindow.row()),uint(storyWindow.waiting()),uint(beginWorld));
                storyCapture=storyWindow.ended()&&!beginWorld;
            }
        }
        if (replay && event.type==Common::EVENT_KEYDOWN)
            inputTrace+=Common::String::format("%u\t%u\t%s\t%u\t%u\n",processedKeys,uint(creation.stage()),creation.name().c_str(),creation.gender(),creation.race());
        if (event.type==Common::EVENT_KEYDOWN) redraw=true;
        if (beginWorld) {
            // The story presentation is native. Initial actor/player turn and
            // party/status/ordinary UI remain separate from this startup path.
            Native::PresentationState presentation;byte previous[187]={},emptyRoutes[3584]={};
            if (!status.initializeFrame(frame) || !window.initialize(game.world,game.world.currentTables()->data()+1536,256) ||
                !story.load(_resources) || !actors.load(_resources,8) || !lighting.load(_resources,8) || !occlusion.load(_resources,8) ||
                !painter.load(_resources,8) || !visibility.loadRules(_resources) || !wait.load(_resources) || !routes.importPages(emptyRoutes,3584) ||
                !random.initializeEntropy(119) || !messages.load(_resources) || !text.initialize(_resources,1) || !worldInput.loadRules(_resources) ||
                !Native::WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,246,0,0,previous,presentation,light,visibility,frame,&prepared)) return fail("Native startup first scene failed");
            turns.presentation=presentation;
            if (!status.paint(game,prepared,light,false,frame)) return fail("Native initial party/status presentation failed");
            if (!animationState.synchronize(frame)) return fail("Native initial animation frame setup failed");
            sceneReady=true;animationReady=true;lastAnimationTick=_system->getMillis();
        }
    }
    if (playerProbe.active() && playerProbe.finished()) return Common::kNoError;
    if (autoPlay) {
        if (endingAcceptance) {
            if (!automatic.finished || !encounterState.presented || encounterState.resultPhase!=4 || encounterState.round.phase!=Native::kBattleVictory || !encounterState.round.actions || encounterState.loot.started || encounterState.advancement.started)
                return fail("Ending input driver did not complete actual combat/victory/terminal presentation");
        } else if (!automatic.finished || encounterState.formed || !battleCompleted) return fail("Combat input driver did not complete battle/results/world return");
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-combat-input.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(endingAcceptance?Common::String::format("ordinary-ending-input\tkeys=%u\tactual_battle_victory=1\tterminal_ending=1\tfollowing_world_input=0\tCPU_execution=0\tstate_injection_after_start=0\tcontrolled_setup=1\n",automaticKeys):Common::String::format("ordinary-combat-input\tkeys=%u\tcompleted_battles=%u\tfollowing_world_input=1\tCPU_execution=0\tstate_injection_after_start=0\tcontrolled_setup=1\n",automaticKeys,battleCompleted));
        out->writeString(automaticTrace);out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    if (!replay) return Common::kNoError;
    if (!sceneReady || !game.ready()) return fail("Native startup input ended before character confirmation");
    if (!saveExploration("nox-native-exploration-final.bin")) return Common::kWritingFailed;
    if (worldInput.encounter().battle) {
        if (!encounterState.formed) return fail("Native encounter formation unavailable");
        Common::OutSaveFile *formation=_system->getSavefileManager()->openForSaving("nox-native-encounter-formation.bin",false);if (!formation) return Common::kWritingFailed;
        bool formationOk=encounterState.save(*formation);formation->finalize();formationOk=formationOk&&!formation->err();delete formation;if (!formationOk) return Common::kWritingFailed;
        const Native::WorldActorTurnEvent &event=worldInput.encounter();
        if (!encounterPrepared) return fail("Native encounter preparation unavailable");
        const Native::EncounterSelection &selected=preparedEncounter.enemy;
        const byte sprite=(*game.world.currentTables())[(event.bank?1280:768)+event.slot+2];
        Common::OutSaveFile *selection=_system->getSavefileManager()->openForSaving("nox-native-encounter-selection.bin",false);if (!selection) return Common::kWritingFailed;
        selection->writeUint32BE(MKTAG('N','E','N','C'));selection->writeUint16LE(1);selection->writeByte(game.world.mapType());selection->writeByte(sprite);
        selection->writeUint16LE(selected.descriptor);selection->write(selected.data,4);selection->write(selected.stats,32);
        selection->finalize();const bool selectionOk=!selection->err();delete selection;if (!selectionOk) return Common::kWritingFailed;
        selection=_system->getSavefileManager()->openForSaving("nox-native-encounter-terrain.bin",false);if (!selection) return Common::kWritingFailed;
        selection->write(preparedEncounter.terrain.data(),preparedEncounter.terrain.size());selection->finalize();const bool terrainOk=!selection->err();delete selection;if (!terrainOk) return Common::kWritingFailed;
        selection=_system->getSavefileManager()->openForSaving("nox-native-encounter-preparation.tsv",false);if (!selection) return Common::kWritingFailed;
        selection->writeString(Common::String::format("native-encounter-preparation\tfield=%u\tlayout_flags=%u\tdeep_water=%u\tmonster=%u\tderived_from_pending_world=1\tclock_retired=%u\tpresented=%u\ttactical_move_commands=%u\tinitial_battle_phase_setup=1\n",uint(preparedEncounter.choice.field),uint(preparedEncounter.choice.layoutFlags),uint(preparedEncounter.choice.deepWater),uint(selected.data[2]),uint(encounterState.presented),uint(encounterState.presented),battleMoveCommands));
        selection->finalize();const bool preparationOk=!selection->err();delete selection;if (!preparationOk) return Common::kWritingFailed;
        Common::OutSaveFile *out=_system->getSavefileManager()->openForSaving("nox-native-encounter-handoff.tsv",false);if (!out) return Common::kWritingFailed;
        out->writeString(Common::String::format("native-encounter-handoff\tbank=%u\tslot=%u\tdirection=%u\tcell=%u\tclock_pending=%u\tpresented=%u\tbattle_completed=0\ttactical_move_commands=%u\toccupied_refusals=%u\tcompatibility_runtime=0\n",uint(event.bank),uint(event.slot),uint(event.direction),uint(event.cell),uint(!encounterState.presented),uint(encounterState.presented),battleMoveCommands,battleOccupiedRefusals));
        out->finalize();const bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
    }
    Common::OutSaveFile *trace=_system->getSavefileManager()->openForSaving("nox-native-startup-input.tsv",false);
    if (!trace) return Common::kWritingFailed;
    trace->writeString(inputTrace); trace->finalize(); const bool traceOk=!trace->err(); delete trace;
    if (!traceOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-battle-movement.tsv",false);if (!trace) return Common::kWritingFailed;
    trace->writeString(battleMoveTrace);trace->finalize();const bool battleTraceOk=!trace->err();delete trace;if (!battleTraceOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-battle-completion.tsv",false);if (!trace) return Common::kWritingFailed;
    trace->writeString(Common::String::format("native-battle-completion\tcompleted=%u\tlast_credit=%u\tpending_results=%u\tparty_XP=%u\tfood=%u\tgold=%u\tcompatibility_runtime=0\n",battleCompleted,uint(lastBattleCredit),uint(encounterState.resultPhase),uint(READ_LE_UINT16(game.party.member(0)->bytes+5)),uint(game.food),uint(game.gold)));
    trace->finalize();const bool completionOk=!trace->err();delete trace;if (!completionOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-startup-wait.tsv",false);if (!trace) return Common::kWritingFailed;
    trace->writeString(waitTrace);trace->finalize();const bool waitTraceOk=!trace->err();delete trace;if (!waitTraceOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-startup-commands.tsv",false);if (!trace) return Common::kWritingFailed;
    trace->writeString(commandTrace);trace->finalize();const bool commandTraceOk=!trace->err();delete trace;if (!commandTraceOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-startup-animation.tsv",false);if (!trace) return Common::kWritingFailed;
    trace->writeString(animationTrace);trace->finalize();const bool animationTraceOk=!trace->err();delete trace;if (!animationTraceOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-startup-animation-report.tsv",false);if (!trace) return Common::kWritingFailed;
    trace->writeString(Common::String::format("native-live-animation\tidle_frames=%u\tphase=%u\tpattern=%u\tnext_target=%u\tinterval_ms=33\tcold_history_restored=%u\tinitial_phase_setup=0\tinitial_pattern_setup=0\tcompatibility_runtime=0\n",idleFrames,uint(animationState.phase),uint(animationState.pattern),uint(animationState.target),uint(resumed)));
    trace->finalize();const bool animationReportOk=!trace->err();delete trace;if (!animationReportOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-startup-animation-timing.tsv",false);if (!trace) return Common::kWritingFailed;
    trace->writeString(animationTiming);trace->finalize();const bool timingOk=!trace->err();delete trace;if (!timingOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-startup-story.tsv",false);if (!trace) return Common::kWritingFailed;
    trace->writeString(storyTrace);trace->finalize();const bool storyOk=!trace->err();delete trace;if (!storyOk) return Common::kWritingFailed;
    trace=_system->getSavefileManager()->openForSaving("nox-native-startup-story-frames.bin",false);if (!trace) return Common::kWritingFailed;
    trace->writeUint32BE(MKTAG('N','N','S','F'));trace->writeUint16LE(1);trace->writeUint16LE(uint16(storyPages));trace->write(storyFrames.getData(),storyFrames.size());trace->finalize();const bool framesOk=!trace->err();delete trace;if (!framesOk) return Common::kWritingFailed;
    Common::OutSaveFile *party=_system->getSavefileManager()->openForSaving("nox-native-startup-party.bin",false);
    if (!party) return Common::kWritingFailed;
    bool ok=game.party.save(*party); party->finalize(); ok=ok&&!party->err(); delete party;
    Common::OutSaveFile *world=_system->getSavefileManager()->openForSaving("nox-native-startup-world.bin",false);
    if (!world) return Common::kWritingFailed;
    ok=game.world.save(*world)&&ok; world->finalize(); ok=ok&&!world->err(); delete world;
    Common::OutSaveFile *clock=_system->getSavefileManager()->openForSaving("nox-native-startup-clock.bin",false);
    if (!clock) return Common::kWritingFailed;
    clock->writeUint32BE(MKTAG('N','C','L','K'));clock->writeUint16LE(1);clock->writeByte(game.clock.phase);clock->writeByte(game.clock.hour);clock->writeByte(game.clock.minute);
    clock->writeByte(game.clock.lightTicks);clock->writeByte(game.clock.lightActive);clock->writeByte(game.clock.hourlyCounter);clock->finalize();ok=ok&&!clock->err();delete clock;
    if (ConfMan.hasKey("nox_native_startup_capture") && ConfMan.getBool("nox_native_startup_capture")) {
        Common::OutSaveFile *image=_system->getSavefileManager()->openForSaving("nox-native-startup-scene.png",false);
        if (!image) return Common::kWritingFailed;
        ok=Image::writePNG(*image,_surface)&&ok; image->finalize(); ok=ok&&!image->err(); delete image;
    }
    Common::OutSaveFile *report=_system->getSavefileManager()->openForSaving("nox-native-startup-report.txt",false);
    if (!report) return Common::kWritingFailed;
    report->writeString(Common::String::format("native-startup\tstatus=%s\tkeys=%u\tname=%s\tgender=%u\trace=%u\tmap=%u/%u\tposition=%u/%u\tcompatibility_runtime=0\topening_story_text=%u\topening_backdrop=%u\topening_story=%u\tinitial_actor_turn=0\tordinary_commands=%u\twait_commands=%u\twait_entropy_setup=119\tmap_light_setup=1\texploration_restored=%u\tstory_performed=%u\tnotice_pending=%u\topen_direction_pending=%u\tcomplete_native_gameplay=0\n",
        ok?"VERIFIED":"IN PROGRESS",source.count,creation.name().c_str(),creation.gender(),creation.race(),game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),uint(storyStarted),uint(storyStarted),uint(storyStarted),ordinaryCommands,waitCommands,uint(resumed),uint(storyStarted),uint(worldInput.noticeActive()),uint(worldInput.selectingOpen())));
    report->finalize(); ok=ok&&!report->err(); delete report;
    return ok?Common::Error(Common::kNoError):Common::Error(Common::kWritingFailed);
}
}
#endif
