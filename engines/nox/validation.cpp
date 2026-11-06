/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nox/nox.h"
#include "nox/records.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/fs.h"

#include "nox/snapshot.h"
#include "nox/saves.h"
#include "nox/enhancement_validation.h"
#include "nox/native_gates_validation.h"
#include "nox/native_maps_validation.h"
#include "nox/native_world_validation.h"
#include "nox/native_dialogue_validation.h"
#include "nox/native_clock_validation.h"
#include "nox/native_dialogue_rules_validation.h"
#include "nox/native_inventory_validation.h"
#include "nox/native_conversation_validation.h"
#include "nox/native_temple_service_validation.h"
#include "nox/native_inn_service_validation.h"
#include "nox/native_inn_membership_validation.h"
#include "nox/native_party_selection_validation.h"
#include "nox/native_battle_cannon_validation.h"
#include "nox/native_world_transport_validation.h"
#include "nox/native_vendor_validation.h"
#include "nox/native_music_validation.h"
#include "nox/native_ending_validation.h"
#include "nox/native_world_transport_motion_validation.h"
#include "nox/native_dialogue_input_validation.h"
#include "nox/native_chain_validation.h"
#include "nox/native_graphics_validation.h"
#include "nox/native_traversal_validation.h"
#include "nox/native_position_validation.h"
#include "nox/native_party_validation.h"
#include "nox/native_view_validation.h"
#include "nox/native_objects_validation.h"
#include "nox/native_visibility_validation.h"
#include "nox/native_light_validation.h"
#include "nox/native_lighting_validation.h"
#include "nox/native_personal_light_validation.h"
#include "nox/native_lighting_chain_validation.h"
#include "nox/native_actor_paint_validation.h"
#include "nox/native_actors_validation.h"
#include "nox/native_tile_paint_validation.h"
#include "nox/native_world_presentation_validation.h"
#include "nox/native_random_validation.h"
#include "nox/native_search_validation.h"
#include "nox/native_lock_validation.h"
#include "nox/native_lock_party_validation.h"
#include "nox/native_page_search_validation.h"
#include "nox/native_book_validation.h"
#include "nox/native_cast_validation.h"
#include "nox/native_vortex_targets_validation.h"
#include "nox/native_spell_power_validation.h"
#include "nox/native_vortex_effect_validation.h"
#include "nox/native_vortex_cast_validation.h"
#include "nox/native_summon_validation.h"
#include "nox/native_battle_records_validation.h"
#include "nox/native_equipment_ownership_validation.h"
#include "nox/native_armor_ready_validation.h"
#include "nox/native_equipment_derived_validation.h"
#include "nox/native_owned_armor_validation.h"
#include "nox/native_weapon_ready_validation.h"
#include "nox/native_owned_weapon_validation.h"
#include "nox/native_new_game_validation.h"
#include "nox/native_story_patches_validation.h"
#include "nox/native_world_story_validation.h"
#include "nox/native_world_scene_validation.h"
#include "nox/native_world_command_validation.h"
#include "nox/native_occlusion_validation.h"
#include "nox/native_world_viewport_validation.h"
#include "nox/native_viewport_maps_validation.h"
#include "nox/native_actor_step_validation.h"
#include "nox/native_actor_spatial_validation.h"
#include "nox/native_actor_outside_validation.h"
#include "nox/native_actor_movement_effects_validation.h"
#include "nox/native_npc_contact_validation.h"
#include "nox/native_npc_world_action_validation.h"
#include "nox/native_actor_contact_validation.h"
#include "nox/native_actor_intent_validation.h"
#include "nox/native_actor_motion_validation.h"
#include "nox/native_npc_actor_actions_validation.h"
#include "nox/native_npc_clock_validation.h"
#include "nox/native_npc_path_validation.h"
#include "nox/native_owned_path_validation.h"
#include "nox/native_npc_route_store_validation.h"
#include "nox/native_npc_route_follow_validation.h"
#include "nox/native_npc_navigation_validation.h"
#include "nox/native_npc_clock_pipeline_validation.h"
#include "nox/native_text_glyph_validation.h"
#include "nox/native_text_window_validation.h"
#include "nox/native_compact_world_action_validation.h"
#include "nox/native_world_actor_turn_validation.h"
#include "nox/native_world_actor_phase_validation.h"
#include "nox/native_surface_offscreen_validation.h"
#include "nox/native_world_large_geometry_validation.h"
#include "nox/native_message_strip_validation.h"
#include "nox/native_npc_route_recovery_validation.h"
#include "nox/native_actor_pursuit_validation.h"
#include "nox/native_npc_action_dispatch_validation.h"
#include "nox/native_npc_schedule_validation.h"

namespace Nox {
struct ResourceValidationFailure {
	Common::SaveFileManager &files;
	Common::String &error;
	bool passed;
	ResourceValidationFailure(Common::SaveFileManager &manager, Common::String &message) : files(manager), error(message), passed(false) {}
	~ResourceValidationFailure() {
		if (passed) return;
		Common::OutSaveFile *out = files.openForSaving("nox-native-validation-failure.txt", false);
		if (!out) return;
		const Common::String message = error.empty() ? "Validation failed without a detailed error" : error;
		out->write(message.c_str(), message.size()); out->finalize(); delete out;
	}
};
bool NoxEngine::validateResources(Common::String &error) {
	ResourceValidationFailure failure(*getSaveFileManager(), error);
	Common::String report;
	if (!validateNativeGates(_resources, report, error)) return false;
	if (!validateNativeMaps(_resources, report, error)) return false;
	if (!validateNativeDialogue(_resources, report, error)) return false;
	if (!validateNativeClock(report, error)) return false;
	if (ConfMan.hasKey("nox_native_search_reward")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_search_reward") + "/input.bin")))) { error = "Missing original Search reward outcomes"; return false; }
        if (!validateNativeSearchReward(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_lock")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_lock") + "/input.bin")))) { error = "Missing original lock outcomes"; return false; }
        if (!validateNativeLock(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_lock_party")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_lock_party") + "/input.bin")))) { error = "Missing original lock party outcomes"; return false; }
        if (!validateNativeLockParty(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_page_search")) {
        Common::File input, chain; byte acquiredFields[256]; Native::Inventory acquiredInventory, boundInventory;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_page_search") + "/input.bin"))) ||
                !ConfMan.hasKey("nox_native_pages_chain") || !chain.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_pages_chain") + "/input.bin")))) {
            error = "Missing original linked Search/dialogue outcomes"; return false;
        }
        const bool coldCheckpoint = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        const bool acquired = coldCheckpoint ? validateNativeQuestCheckpoint(_resources, *_saveFileMan, input, acquiredFields, acquiredInventory, report, error) :
                validateNativePageSearch(_resources, input, acquiredFields, acquiredInventory, report, error, _saveFileMan);
        if (!acquired || !validateNativeChain(_resources, chain, report, error, acquiredFields, &acquiredInventory, &boundInventory)) return false;
        if (ConfMan.hasKey("nox_native_book")) {
            Common::File caster; Native::SpellBook retainedBook;
            if (!caster.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_book") + "/caster.bin"))) ||
                    !validateNativeBoundBook(_resources, boundInventory, caster, report, error, *_saveFileMan, coldCheckpoint, &retainedBook)) return false;
            if (ConfMan.hasKey("nox_native_vortex_cast")) {
                Common::File battle;
                if (!battle.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_vortex_cast") + "/input.bin"))) ||
                    !validateNativeBoundVortex(_resources, retainedBook, battle, report, error, coldCheckpoint)) return false;
            }
        }
        report += "native-search-dialogue-link\tactual_native_Search_pages=7\tAxea_Valess_Nystul=1\tpage_outcome_initialization=0\tcontrolled_travel=1\n";
    }
	if (ConfMan.hasKey("nox_native_book")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_book") + "/input.bin")))) { error = "Missing original book outcomes"; return false; }
        if (!validateNativeBook(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_cast_preparation")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_cast_preparation") + "/input.bin")))) { error = "Missing original cast preparation outcomes"; return false; }
        if (!validateNativeCast(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_vortex_targets")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_vortex_targets") + "/input.bin")))) { error = "Missing original Vortex target outcomes"; return false; }
        if (!validateNativeVortexTargets(input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_spell_power")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_spell_power") + "/input.bin")))) { error = "Missing original spell power outcomes"; return false; }
        if (!validateNativeSpellPower(input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_vortex_effect")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_vortex_effect") + "/input.bin")))) { error = "Missing original Vortex effect outcomes"; return false; }
        if (!validateNativeVortexEffect(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_vortex_cast")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_vortex_cast") + "/input.bin")))) { error = "Missing original logical Vortex outcomes"; return false; }
        if (!validateNativeVortexCast(_resources, input, report, error, *_saveFileMan, ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold"))) return false;
    }
	if (ConfMan.hasKey("nox_native_summon")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_summon") + "/input.bin")))) { error = "Missing original summon allocation outcomes"; return false; }
        if (!validateNativeSummonPool(input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_battle_records")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_battle_records") + "/input.bin")))) { error = "Missing original battle record outcomes"; return false; }
        if (!validateNativeBattleRecords(input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_equipment_ownership")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_equipment_ownership") + "/input.bin")))) { error = "Missing original equipment ownership outcomes"; return false; }
        if (!validateNativeEquipmentOwnership(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_armor_ready")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_armor_ready") + "/input.bin")))) { error = "Missing original armor transaction outcomes"; return false; }
        if (!validateNativeArmorReady(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_equipment_derived")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_equipment_derived") + "/input.bin")))) { error = "Missing original equipment recalculation outcomes"; return false; }
        if (!validateNativeEquipmentDerived(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_owned_armor")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_owned_armor") + "/input.bin")))) { error = "Missing original linked armor/recalculation outcomes"; return false; }
        if (!validateNativeOwnedArmor(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_weapon_ready")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_weapon_ready") + "/input.bin")))) { error = "Missing original weapon transaction outcomes"; return false; }
        if (!validateNativeWeaponReady(_resources, input, report, error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_schedule")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        const Common::String directory = ConfMan.get("nox_native_npc_schedule"); Common::File input,first;
        if (!cold && (!input.open(Common::FSNode(Common::Path(directory + "/input.bin"))) || !validateNativeNpcSchedule(_resources,input,*_saveFileMan,report,error))) return false;
        if (cold && (!input.open(Common::FSNode(Common::Path(directory + "/input.bin"))) || !validateNativeNpcScheduleCold(_resources,input,*_saveFileMan,report,error))) return false;
        if (!first.open(Common::FSNode(Common::Path(directory + "/first-world.bin"))) || !validateNativeNewGameWorld(_resources,first,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_action_dispatch")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input,continuation; const Common::String folder = ConfMan.get("nox_native_npc_action_dispatch");
        if (!input.open(Common::FSNode(Common::Path(folder + "/input.bin"))) || !continuation.open(Common::FSNode(Common::Path(folder + "/cold.bin"))) || !validateNativeNpcActionDispatch(_resources,input,continuation,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_actor_pursuit")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_pursuit") + "/input.bin"))) || !validateNativeActorPursuit(input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_route_recovery")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input,continuation; const Common::String folder = ConfMan.get("nox_native_npc_route_recovery");
        if (!input.open(Common::FSNode(Common::Path(folder + "/input.bin"))) || !continuation.open(Common::FSNode(Common::Path(folder + "/cold.bin"))) || !validateNativeNpcRouteRecovery(_resources,input,continuation,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_message_strip")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input,initial,continuation; const Common::String folder = ConfMan.get("nox_native_message_strip");
        if (!input.open(Common::FSNode(Common::Path(folder + "/input.bin"))) || !initial.open(Common::FSNode(Common::Path(folder + "/initial.bin"))) || !continuation.open(Common::FSNode(Common::Path(folder + "/cold.bin"))) || !validateNativeMessageStrip(_resources,input,initial,continuation,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_text_glyph")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input,continuation; const Common::String folder = ConfMan.get("nox_native_text_glyph");
        if (!input.open(Common::FSNode(Common::Path(folder + "/input.bin"))) || !continuation.open(Common::FSNode(Common::Path(folder + "/cold.bin"))) || !validateNativeTextGlyph(_resources,input,continuation,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_world_large_geometry")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_world_large_geometry")+"/input.bin"))) ||
            !validateNativeWorldLargeGeometry(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_surface_offscreen")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_surface_offscreen")+"/input.bin"))) ||
            !validateNativeSurfaceOffscreen(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_world_actor_phase")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_world_actor_phase")+"/input.bin"))) ||
            !validateNativeWorldActorPhase(input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_world_actor_turn")) {
        const bool cold=ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input,continuation;const Common::String folder=ConfMan.get("nox_native_world_actor_turn");
        if (!input.open(Common::FSNode(Common::Path(folder+"/input.bin"))) || !continuation.open(Common::FSNode(Common::Path(folder+"/cold.bin"))) ||
            !validateNativeWorldActorTurn(_resources,input,continuation,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_compact_world")) {
        const bool cold=ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input,continuation;const Common::String folder=ConfMan.get("nox_native_compact_world");
        if (!input.open(Common::FSNode(Common::Path(folder+"/input.bin"))) || !continuation.open(Common::FSNode(Common::Path(folder+"/cold.bin"))) ||
            !validateNativeCompactWorldAction(_resources,input,continuation,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_text_window")) {
        const bool cold=ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_text_window")+"/input.bin"))) ||
            !validateNativeTextWindow(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_clock_pipeline")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_clock_pipeline") + "/input.bin"))) || !validateNativeNpcClockPipeline(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_navigation")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_navigation") + "/input.bin"))) || !validateNativeNpcNavigation(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_route_follow")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_route_follow") + "/input.bin"))) || !validateNativeNpcRouteFollow(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_route_store")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_route_store") + "/input.bin"))) || !validateNativeNpcRouteStore(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_owned_path")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_owned_path") + "/input.bin"))) || !validateNativeOwnedPath(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_path")) {
        const bool cold = ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_path") + "/input.bin"))) || !validateNativePath(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_prefetch")) {
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_prefetch") + "/input.bin"))) || !validateNativeNpcClock(_resources,input,report,error,false,true)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_reentry")) {
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_reentry") + "/input.bin"))) || !validateNativeNpcClock(_resources,input,report,error,true)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_clock")) {
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_clock") + "/input.bin"))) || !validateNativeNpcClock(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_actor_actions")) {
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_actor_actions") + "/input.bin"))) || !validateNativeNpcActorActions(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_actor_motion")) {
        Common::File input; if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_motion") + "/input.bin"))) || !validateNativeActorMotion(input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_actor_intent")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_intent") + "/input.bin")))) { error = "Missing original actor intent/RNG outcomes"; return false; }
        if (!validateNativeActorIntent(input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_actor_step")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_step") + "/input.bin")))) { error = "Missing original world actor destination outcomes"; return false; }
        if (!validateNativeActorStep(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_actor_movement_effects")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_movement_effects") + "/input.bin"))) || !validateNativeActorMovementEffects(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_actor_outside")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_outside") + "/input.bin"))) || !validateNativeActorOutside(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_actor_spatial")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_spatial") + "/input.bin")))) { error="Missing original actor spatial outcomes"; return false; }
        const bool cold=ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        if (!validateNativeActorSpatial(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_world_action")) {
        Common::File input,continuation; const bool cold=ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        const Common::String folder=ConfMan.get("nox_native_npc_world_action");
        if (!input.open(Common::FSNode(Common::Path(folder + "/input.bin"))) || !continuation.open(Common::FSNode(Common::Path(folder + "/cold.bin"))) || !validateNativeNpcWorldAction(_resources,input,continuation,*_saveFileMan,cold,report,error)) return false;
    }
    for (uint variant=0;variant<4;++variant) {
        const char *keys[]={"nox_native_npc_world_free","nox_native_npc_large_contact","nox_native_npc_world_effects","nox_native_npc_world_ranged"};
        const char *prefixes[]={"native-npc-world-free","native-npc-large-contact","native-npc-world-effects","native-npc-world-ranged"};
        const char *families[]={"nox-native-world-free","nox-native-large-action","nox-native-world-effects","nox-native-world-ranged"};
        const char *key=keys[variant];
        if (!ConfMan.hasKey(key)) continue;
        Common::File input,continuation; const Common::String folder=ConfMan.get(key);
        const bool cold=ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        if (!input.open(Common::FSNode(Common::Path(folder + "/input.bin"))) || !continuation.open(Common::FSNode(Common::Path(folder + "/cold.bin"))) ||
            !validateNativeNpcWorldAction(_resources,input,continuation,*_saveFileMan,cold,report,error,prefixes[variant],families[variant])) return false;
    }
    if (ConfMan.hasKey("nox_native_npc_contact")) {
        Common::File input; const bool cold=ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold");
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_npc_contact") + "/input.bin"))) || !validateNativeNpcContact(_resources,input,*_saveFileMan,cold,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_actor_contact")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_contact") + "/input.bin"))) || !validateNativeActorContact(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_viewport_maps")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_viewport_maps") + "/input.bin")))) { error = "Missing original campaign-map viewport outcomes"; return false; }
        if (!validateNativeViewportMaps(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_world_viewport")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_world_viewport") + "/input.bin")))) { error = "Missing actual original full viewport raster outcomes"; return false; }
        if (!validateNativeWorldViewport(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_occlusion")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_occlusion") + "/input.bin")))) { error = "Missing original opacity/shadow/repaint outcomes"; return false; }
        if (!validateNativeOcclusion(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_world_command")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_world_command") + "/input.bin")))) { error = "Missing original scene-to-traversal outcomes"; return false; }
        if (!validateNativeWorldCommand(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_world_scene")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_world_scene") + "/input.bin")))) { error = "Missing original composed world scenes"; return false; }
        if (!validateNativeWorldScene(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_world_story")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_world_story") + "/input.bin")))) { error = "Missing original retained world story effects"; return false; }
        if (!validateNativeWorldStory(_resources,input,*_saveFileMan,ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold"),report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_story_patches")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_story_patches") + "/input.bin")))) { error = "Missing original story map effects"; return false; }
        if (!validateNativeStoryPatches(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_new_game")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_new_game") + "/input.bin")))) { error = "Missing actual original first-world reference"; return false; }
        if (!validateNativeNewGame(_resources,input,report,error)) return false;
    }
	if (ConfMan.hasKey("nox_native_owned_weapon")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_owned_weapon") + "/input.bin")))) { error = "Missing original linked weapon/recalculation outcomes"; return false; }
        if (!validateNativeOwnedWeapon(_resources, input, report, error)) return false;
    }
	if (ConfMan.hasKey("nox_native_rng")) {
		const Common::String directory = ConfMan.get("nox_native_rng"); Common::File input, cold;
		if (!input.open(Common::FSNode(Common::Path(directory + "/input.bin"))) || !cold.open(Common::FSNode(Common::Path(directory + "/cold.bin")))) { error = "Missing original RNG outcomes"; return false; }
		if (!validateNativeRandom(input, report, error) || !validateNativeRandomPersistence(*_saveFileMan, cold,
				ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold"), report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_world_presentation")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_world_presentation") + "/input.bin")))) { error = "Missing original world presentation outcomes"; return false; }
		if (!validateNativeWorldPresentation(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_tile_paint")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_tile_paint") + "/input.bin")))) { error = "Missing original tile raster outcomes"; return false; }
		if (!validateNativeTilePaint(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_actors")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actors") + "/input.bin")))) { error = "Missing original actor admission outcomes"; return false; }
		if (!validateNativeActors(input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_actor_paint")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_actor_paint") + "/input.bin")))) { error = "Missing original actor paint outcomes"; return false; }
		if (!validateNativeActorPaint(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_lighting_chain")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_lighting_chain") + "/input.bin")))) { error = "Missing original linked lighting outcomes"; return false; }
		if (!validateNativeLightingChain(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_personal_light")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_personal_light") + "/input.bin")))) { error = "Missing original additional-light outcomes"; return false; }
		if (!validateNativePersonalLight(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_terrain_light")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_terrain_light") + "/input.bin")))) { error = "Missing original terrain-light outcomes"; return false; }
		if (!validateNativeTerrainLighting(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_light")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_light") + "/input.bin")))) { error = "Missing original light-mask outcomes"; return false; }
		if (!validateNativeLight(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_visibility")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_visibility") + "/input.bin")))) { error = "Missing original visibility reveal outcomes"; return false; }
		if (!validateNativeVisibility(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_objects")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_objects") + "/input.bin")))) { error = "Missing original object-phase outcomes"; return false; }
		if (!validateNativeObjects(input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_view")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_view") + "/input.bin")))) { error = "Missing historical terrain projection input"; return false; }
		if (!validateNativeView(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_party")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_party") + "/input.bin")))) {
			error = "Missing local original party creation outcomes"; return false;
		}
		if (!validateNativePartyCreation(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_traversal")) {
		const Common::String directory = ConfMan.get("nox_native_traversal");
		Common::File input, expected;
		if (!input.open(Common::FSNode(Common::Path(directory + "/input.bin"))) || !expected.open(Common::FSNode(Common::Path(directory + "/expected.bin")))) {
			error = "Missing local native traversal comparison input"; return false;
		}
		if (!validateNativeTraversal(_resources, input, expected, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_graphics")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_graphics") + "/input.bin")))) {
			error = "Missing local native graphics comparison input"; return false;
		}
		if (!validateNativeGraphics(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_dialogue_input")) {
		const Common::String directory = ConfMan.get("nox_native_dialogue_input");
		Common::File input, expected;
		if (!input.open(Common::FSNode(Common::Path(directory + "/input.bin"))) || !expected.open(Common::FSNode(Common::Path(directory + "/expected.bin")))) {
			error = "Missing local native lexical comparison input"; return false;
		}
		if (!validateNativeDialogueInput(_resources, input, expected, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_pages_chain")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_pages_chain") + "/input.bin")))) {
			error = "Missing local native pages chain input"; return false;
		}
		if (!validateNativeChain(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_inventory_oracle")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_inventory_oracle") + "/input.bin")))) {
			error = "Missing local native inventory comparison input"; return false;
		}
		if (!validateNativeInventory(_resources, input, report, error)) return false;
	}
	if (ConfMan.hasKey("nox_native_dialogue_oracle")) {
		const Common::String directory = ConfMan.get("nox_native_dialogue_oracle");
		Common::File input, expected, effects;
		if (!input.open(Common::FSNode(Common::Path(directory + "/input.bin"))) || !expected.open(Common::FSNode(Common::Path(directory + "/expected.bin")))) {
			error = "Missing local native dialogue comparison input"; return false;
		}
		const Common::FSNode effectsNode(Common::Path(directory + "/effects.bin"));
		if (effectsNode.exists() && !effects.open(effectsNode)) { error = "Cannot read local dialogue effects comparison"; return false; }
		if (!validateNativeDialogueRules(input, expected, report, error, effects.isOpen() ? &effects : nullptr)) return false;
	}
	if (ConfMan.hasKey("nox_native_dialogue_selection")) {
		const Common::String directory = ConfMan.get("nox_native_dialogue_selection");
		Common::File input, expected;
		if (!input.open(Common::FSNode(Common::Path(directory + "/input.bin"))) || !expected.open(Common::FSNode(Common::Path(directory + "/expected.bin")))) {
			error = "Missing local native dialogue selection input"; return false;
		}
		if (!validateNativeDialogueRules(input, expected, report, error, nullptr, true)) return false;
	}
	if (ConfMan.hasKey("nox_native_dialogue_openings")) {
		const Common::String directory = ConfMan.get("nox_native_dialogue_openings");
		Common::File input, expected;
		if (!input.open(Common::FSNode(Common::Path(directory + "/input.bin"))) || !expected.open(Common::FSNode(Common::Path(directory + "/expected.bin")))) {
			error = "Missing local native dialogue opening input"; return false;
		}
		if (!validateNativeDialogueRules(input, expected, report, error, nullptr, true, true)) return false;
	}
    if (ConfMan.hasKey("nox_native_transport_motion")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_transport_motion") + "/input.bin")))) return false;
        if (!validateNativeTransportMotion(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_vendor")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_vendor")+"/input.bin"))) || !validateNativeVendor(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_music")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_music")+"/input.bin"))) || !validateNativeMusic(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_ending")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_ending")+"/input.bin"))) || !validateNativeEndingBranches(input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_transport_control")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_transport_control") + "/input.bin")))) return false;
        if (!validateNativeTransportControl(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_cannon")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_cannon") + "/input.bin")))) return false;
        if (!validateNativeCannon(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_party_selector")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_party_selector") + "/input.bin")))) return false;
        if (!validateNativePartySelection(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_inn_membership")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_inn_membership") + "/input.bin")))) return false;
        if (!validateNativeInnMembership(_resources,input,report,error)) return false;
    }
    if (ConfMan.hasKey("nox_native_inn")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_inn") + "/input.bin")))) return false;
        if (!validateNativeInnService(_resources,input,report,error)) return false;
    }
	if (ConfMan.hasKey("nox_native_temple")) {
        Common::File input;
        if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_temple") + "/input.bin")))) return false;
        if (!validateNativeTempleService(_resources,input,report,error)) return false;
    }
	if (ConfMan.hasKey("nox_native_conversation")) {
		Common::File input;
		if (!input.open(Common::FSNode(Common::Path(ConfMan.get("nox_native_conversation") + "/input.bin")))) {
			error = "Missing local native conversation comparison input"; return false;
		}
		if (!validateNativeConversation(_resources, input, report, error)) return false;
	}
	if (!validateNativeWorld(_resources, *_saveFileMan, ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold"), report, error)) return false;
	if (!validateNativePosition(_resources, *_saveFileMan, ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold"), report, error)) return false;
	if (!validateNativePartyPersistence(_resources, *_saveFileMan, ConfMan.hasKey("nox_native_world_cold") && ConfMan.getBool("nox_native_world_cold"), report, error)) return false;
	uint files = 0, negative = 0, recordCases = 0;
	for (HdvArchive::Entries::const_iterator it = _resources.entries().begin(); it != _resources.entries().end(); ++it) {
		if (it->_value.storage == 13)
			continue;
		Common::Array<byte> data, split;
		if (!_resources.read(it->_key, data)) { error = _resources.error(); return false; }
		Common::MemoryReadStream stream(data.data(), data.size());
		report += Common::String::format("resource\t%s\t%u\t%s\n", it->_key.c_str(), data.size(), Common::computeStreamMD5AsString(stream).c_str());
		Common::String lower = it->_key;
		lower.toLowercase();
		const uint32 offset = MIN<uint32>(509, data.size());
		const uint32 count = MIN<uint32>(1031, data.size() - offset);
		if (!_resources.readRange(lower, offset, count, split) || split.size() != count ||
		    (count && memcmp(split.data(), data.data() + offset, count))) {
			error = "Case-insensitive resource range mismatch"; return false;
		}
		Common::Array<byte> original = split;
		if (_resources.readRange(it->_key, 0xffffffffU, 2, split) || split != original) {
			error = "Overflowing range accepted or changed output"; return false;
		}
		++files;
	}
	Common::Array<byte> unchanged;
	unchanged.push_back(73);
	if (_resources.read("NA", unchanged) || unchanged.size() != 1 || unchanged[0] != 73 ||
	    _resources.read("NA/NOT.A.RESOURCE", unchanged) || unchanged.size() != 1 || unchanged[0] != 73) {
		error = "Directory/missing resource accepted or changed output"; return false;
	}
	negative += 2;
	// Each malformed image is a private copy; no input stream is ever writable.
	for (uint fault = 0; fault < 5; ++fault) {
		Common::Array<byte> bad = _resources.image();
		if (fault == 0) bad.resize(bad.size() - 1);
		if (fault == 1) bad[1028 + 37] ^= 1;
		if (fault == 2) WRITE_LE_UINT16(bad.data() + 1026, 2);
		if (fault == 3) bad[1028 + 31] = 0;
		if (fault == 4) {
			// All live root directory entries retain their original shape;
			// corrupt one subdirectory pointer to recurse into the root.
			bool found = false;
			for (uint slot = 1; slot < 13; ++slot) {
				byte *e = bad.data() + 1028 + slot * 39;
				if ((e[0] >> 4) == 13) { WRITE_LE_UINT16(e + 17, 2); found = true; break; }
			}
			if (!found) { error = "Missing root directory validation fixture"; return false; }
		}
		Common::MemoryReadStream stream(bad.data(), bad.size());
		HdvArchive rejected;
		if (rejected.open(stream) || rejected.error().empty()) { error = "Malformed HDV accepted"; return false; }
		++negative;
	}
	// A directory-valid image with an out-of-volume payload pointer must fail
	// at the resource read and preserve the caller's previous output.
	{
		Common::Array<byte> bad = _resources.image();
		Common::String name;
		for (uint slot = 1; slot < 13; ++slot) {
			byte *e = bad.data() + 1028 + slot * 39;
			if ((e[0] >> 4) >= 1 && (e[0] >> 4) <= 3) {
				name = Common::String((const char *)e + 1, e[0] & 15);
				WRITE_LE_UINT16(e + 17, 65535); break;
			}
		}
		Common::MemoryReadStream stream(bad.data(), bad.size());
		HdvArchive rejected;
		if (name.empty() || !rejected.open(stream) || rejected.read(name, unchanged) || unchanged.size() != 1 || unchanged[0] != 73) {
			error = "Bad payload pointer accepted or modified output"; return false;
		}
		++negative;
	}
	for (uint value = 0; value < 256; value += 85)
		for (uint practice = 0; practice < 256; ++practice)
			for (uint threshold = 0; threshold < 256; ++threshold) {
				StatProgress stat = {byte(value), byte(practice), byte(threshold)};
				uint next = (practice + 1) & 255;
				const bool promoted = next >= threshold;
				if (stat.advance() != promoted || stat.practice != (promoted ? 0 : next) || stat.value != MIN(value + (promoted ? 1U : 0U), 255U)) {
					error = "Native stat boundary regression"; return false;
				}
				++recordCases;
			}
	const uint health[] = {0, 1, 255, 256, 65535};
	const uint damage[] = {0, 1, 254, 255, 256, 65535};
	for (uint i = 0; i < ARRAYSIZE(health); ++i)
		for (uint j = 0; j < ARRAYSIZE(damage); ++j) {
			CharacterRecord character;
			memset(character.bytes, 73, sizeof(character.bytes));
			character.setHealth(health[i]); character.damage(damage[j]);
			if (character.health() != (health[i] > damage[j] ? health[i] - damage[j] : 0) || character.bytes[0] != (health[i] > damage[j] ? 73 : 255)) {
				error = "Native health boundary regression"; return false;
			}
			for (uint n = 0; n < sizeof(character.bytes); ++n)
				if (n != 0 && n != 2 && n != 3 && character.bytes[n] != 73) {
					error = "Native character opaque bytes changed"; return false;
				}
			++recordCases;
		}
	for (uint current = 0; current < 256; ++current)
		for (uint added = 0; added < 256; ++added) {
			if (equipmentWeight(current, added) != MIN(current + added, 255U)) { error = "Native equipment weight regression"; return false; }
			++recordCases;
		}
	EquipmentRecord equipment;
	memset(equipment.bytes, 71, sizeof(equipment.bytes));
	if (equipment.ready(0, 1, 0) || equipment.bytes[0] != 71 || !equipment.ready(0, 1, 129) || equipment.bytes[0] != 0 || equipment.bytes[1] != 1 || equipment.bytes[2] != 3 || !equipment.ready(0, 2, 1) || equipment.bytes[2] != 0 || equipment.bytes[3] != 2) {
		error = "Native ready/two-handed equipment regression"; return false;
	}
	++recordCases;
	Common::Array<byte> item;
	if (!_resources.readRange("NA/SRTN.INVENTORY", 0x1020, 32, item, true)) { error = _resources.error(); return false; }
	ItemDefinition definition;
	memcpy(definition.bytes, item.data(), 32);
	if (definition.name() != "Hunting Knife" || definition.weight() != 1 || weaponSkillBonus(0, 0, 0, 0) != 1 || weaponSkillBonus(0, 8, 0, 0) != 0 || weaponSkillBonus(1, 200, 10, 9) != 5) {
		error = "Native item definition or weapon skill regression"; return false;
	}
	++recordCases;
	// Storage fixtures are not gameplay saves. Exercise real SaveFileManager
	// writes and recovery, including an interrupted newer generation.
	SnapshotStore store(*getSaveFileManager(), "nox-storage-fixture");
	getSaveFileManager()->removeSavefile(store.filename(0));
	getSaveFileManager()->removeSavefile(store.filename(1));
	const byte first[]={1,2,3,4},second[]={5,6,7,8};
	Common::String written; Common::Array<byte> restored; uint32 sequence=0;uint generation=0;
	if (!store.write(first,4,written) || !store.write(second,4,written) ||
	    !store.latest(restored,sequence,generation) || sequence!=2 || memcmp(restored.data(),second,4)) {
		error="Snapshot generation round-trip failed";return false;
	}
	Common::OutSaveFile *partial=getSaveFileManager()->openForSaving(written,false);
	if(!partial){error="Cannot simulate interrupted snapshot";return false;}
	partial->writeUint32BE(MKTAG('N','X','S','V'));partial->finalize();delete partial;
	if(!store.latest(restored,sequence,generation)||sequence!=1||memcmp(restored.data(),first,4)){
		error="Interrupted snapshot lost previous generation";return false;
	}
	if(!store.write(second,4,written)||!store.latest(restored,sequence,generation)||sequence!=2||memcmp(restored.data(),second,4)){
		error="Snapshot replacement after interruption failed";return false;
	}
	// Independently encoded M1-v1 golden envelope (Python hashlib/struct).
	// This is four synthetic bytes, never original game/resource content.
	const byte legacy[]={0x4e,0x58,0x53,0x56,0x01,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x04,0x00,0x00,0x00,
		0xd9,0x78,0x16,0xfa,0xc4,0xa4,0x89,0xf4,0xc8,0xa2,0x3b,0xe4,0xb8,0x50,0xfc,0xd5,0x01,0x02,0x03,0x04};
	SnapshotStore migration(*getSaveFileManager(),"nox-schema-fixture");
	getSaveFileManager()->removeSavefile(migration.filename(0));getSaveFileManager()->removeSavefile(migration.filename(1));
	Common::OutSaveFile *legacyOut=getSaveFileManager()->openForSaving(migration.filename(0),false);
	if(!legacyOut){error="Cannot create legacy schema fixture";return false;}
	legacyOut->write(legacy,sizeof(legacy));legacyOut->finalize();const bool legacyFailed=legacyOut->err();delete legacyOut;
	if(legacyFailed||!migration.read(0,restored,sequence)||sequence!=7||restored.size()!=4||memcmp(restored.data(),first,4)){
		error="M1-v1 schema fixture rejected";return false;
	}
	if(!migration.write(second,4,written)||!migration.latest(restored,sequence,generation)||sequence!=8||generation!=1||memcmp(restored.data(),second,4)){
		error="Mixed legacy/new schema upgrade failed";return false;
	}
	Common::InSaveFile *newFormat=getSaveFileManager()->openForLoading(written);
	if(!newFormat){error="Cannot inspect new schema";return false;}
	Common::Array<byte> encoded;encoded.resize(newFormat->size());
	const bool captured=newFormat->read(encoded.data(),encoded.size())==encoded.size()&&!newFormat->err();delete newFormat;
	if(!captured||encoded.size()!=40||READ_LE_UINT32(encoded.data()+4)!=2||READ_BE_UINT32(encoded.data()+36)!=MKTAG('N','X','O','K')){
		error="New schema lacks explicit completion marker";return false;
	}
	for(uint fault=0;fault<4;++fault){
		Common::Array<byte> damaged=encoded;
		if(fault==0)damaged.resize(damaged.size()-4);
		else if(fault==1)damaged[damaged.size()-1]^=1;
		else if(fault==2)damaged[32]^=1;
		else damaged[4]=3;
		Common::OutSaveFile *invalid=getSaveFileManager()->openForSaving(written,false);
		if(!invalid){error="Cannot create invalid schema fixture";return false;}
		invalid->write(damaged.data(),damaged.size());invalid->finalize();const bool invalidFailed=invalid->err();delete invalid;
		uint32 rejectedSequence=0;Common::Array<byte> rejected;
		if(invalidFailed||migration.read(1,rejected,rejectedSequence)||!migration.latest(restored,sequence,generation)||sequence!=7||generation!=0||memcmp(restored.data(),first,4)){
			error="Invalid new schema did not preserve legacy generation";return false;
		}
	}
	Common::String identity="v119";
	const char *identityResources[]={"NA/LOADER.P","NA/GAME","NA/BS.ROUTINES.BK2"};
	for(uint i=0;i<ARRAYSIZE(identityResources);++i){
		Common::Array<byte> resourceBytes;if(!_resources.read(identityResources[i],resourceBytes)){error="Cannot inspect compatibility resources";return false;}
		Common::MemoryReadStream resourceInput(resourceBytes.data(),resourceBytes.size());
		identity+=":"+Common::computeStreamMD5AsString(resourceInput);
	}
	if(identity!=HdvArchive::baseV119Identity()){error="Resource compatibility identity differs from actual immutable signatures";return false;}
	Saves::Metadata metadata;metadata.target="nox-resource-test";metadata.session="schema-fixture";metadata.description="Schema fixture";metadata.slot=999;metadata.playtime=1234;
	auto encodedMetadata=[](const Saves::Metadata &value){
		Common::MemoryWriteStreamDynamic output(DisposeAfterUse::YES);Saves::writeMetadata(output,value);
		Common::Array<byte> bytes;bytes.resize(output.size());memcpy(bytes.data(),output.getData(),output.size());return bytes;
	};
	auto acceptsMetadata=[](const Common::Array<byte> &bytes,const Saves::Metadata &expected,bool valid){
		Common::MemoryReadStream input(bytes.data(),bytes.size());Saves::Metadata decoded;decoded.description="unchanged";
		const bool accepted=Saves::readMetadata(input,decoded);
		if(!valid)return !accepted&&decoded.description=="unchanged";
		return accepted&&input.pos()==input.size()&&decoded.target==expected.target&&decoded.session==expected.session&&
			decoded.description==expected.description&&decoded.slot==expected.slot&&decoded.playtime==expected.playtime&&
			decoded.mode==expected.mode&&decoded.resourceIdentity==expected.resourceIdentity&&decoded.year==expected.year&&
			decoded.month==expected.month&&decoded.day==expected.day&&decoded.hour==expected.hour&&decoded.minute==expected.minute&&
			decoded.second==expected.second&&decoded.preview.size()==expected.preview.size()&&
			(decoded.preview.empty()||!memcmp(decoded.preview.data(),expected.preview.data(),decoded.preview.size()));
	};
	auto extraMetadata=[](Common::Array<byte> bytes,uint32 tag,byte flags,const byte *value,uint32 length){
		Common::MemoryWriteStreamDynamic chunk(DisposeAfterUse::YES);chunk.writeUint32BE(tag);chunk.writeByte(flags);chunk.writeUint32LE(length);chunk.write(value,length);
		for(uint32 i=0;i<chunk.size();++i)bytes.push_back(chunk.getData()[i]);
		WRITE_LE_UINT32(bytes.data()+4,bytes.size()-8);return bytes;
	};
	Common::MemoryWriteStreamDynamic oldMetadata(DisposeAfterUse::YES);
	oldMetadata.writeUint32BE(MKTAG('N','S','M','1'));Saves::writeText(oldMetadata,metadata.target);Saves::writeText(oldMetadata,metadata.session);Saves::writeText(oldMetadata,metadata.description);
	oldMetadata.writeUint32LE(metadata.slot);oldMetadata.writeUint32LE(metadata.playtime);
	Common::Array<byte> legacyMetadata;legacyMetadata.resize(oldMetadata.size());memcpy(legacyMetadata.data(),oldMetadata.getData(),oldMetadata.size());
	if(!acceptsMetadata(legacyMetadata,metadata,true)||!acceptsMetadata(encodedMetadata(metadata),metadata,true)){error="Legacy/Modern metadata migration failed";return false;}
	metadata.mode=kClassicSaveMode;metadata.slot=1;
	const Common::Array<byte> classic=encodedMetadata(metadata);
	if(!acceptsMetadata(classic,metadata,true)){error="Classic metadata round-trip failed";return false;}
	Saves::Metadata incompatible=metadata;incompatible.resourceIdentity="unsupported-resource";
	if(!acceptsMetadata(encodedMetadata(incompatible),metadata,false)){error="Incompatible resource identity accepted";return false;}
	incompatible=metadata;incompatible.slot=0;
	if(!acceptsMetadata(encodedMetadata(incompatible),metadata,false)){error="Classic slot outside 1/2 accepted";return false;}
	Common::Array<byte> invalidMode=classic;invalidMode[invalidMode.size()-1]=2;
	const byte extension[]={9,8,7},modeValue[]={0};
	Common::Array<byte> invalidLength=classic;WRITE_LE_UINT32(invalidLength.data()+4,Saves::kMaximumMetadata+1);
	Common::Array<byte> missingMode=classic;missingMode.resize(missingMode.size()-10);WRITE_LE_UINT32(missingMode.data()+4,missingMode.size()-8);
	Common::Array<byte> unknownFormat=classic;unknownFormat[3]='3';
	if(!acceptsMetadata(invalidMode,metadata,false)||
	   !acceptsMetadata(extraMetadata(classic,MKTAG('F','U','T','R'),0,extension,3),metadata,true)||
	   !acceptsMetadata(extraMetadata(classic,MKTAG('F','U','T','R'),1,extension,3),metadata,false)||
	   !acceptsMetadata(extraMetadata(classic,MKTAG('M','O','D','E'),1,modeValue,1),metadata,false)||
	   !acceptsMetadata(invalidLength,metadata,false)||
	   !acceptsMetadata(extraMetadata(classic,MKTAG('F','U','T','R'),2,extension,3),metadata,false)||
	   !acceptsMetadata(missingMode,metadata,false)||!acceptsMetadata(unknownFormat,metadata,false)){
		error="Bounded metadata mandatory/optional chunk contracts failed";return false;
	}
	Saves::Metadata rich=metadata;rich.year=2024;rich.month=2;rich.day=29;rich.hour=23;rich.minute=59;rich.second=59;
	rich.preview.resize(Saves::kPreviewBytes);for(uint i=0;i<rich.preview.size();++i)rich.preview[i]=byte(i*37);
	const Common::Array<byte> richBytes=encodedMetadata(rich);
	if(!acceptsMetadata(richBytes,rich,true)){error="Date/actual-sized preview metadata round-trip failed";return false;}
	Saves::Metadata badDate=rich;badDate.year=2023;
	if(!acceptsMetadata(encodedMetadata(badDate),rich,false)){error="Invalid non-leap date accepted";return false;}
	badDate=rich;badDate.year=1899;
	if(!acceptsMetadata(encodedMetadata(badDate),rich,false)){error="Invalid date year accepted";return false;}
	badDate=rich;badDate.hour=24;
	if(!acceptsMetadata(encodedMetadata(badDate),rich,false)){error="Invalid date time accepted";return false;}
	const byte dateBytes[]={0xe8,7,2,29,23,59,59};
	Common::Array<byte> shortPreview=richBytes;shortPreview.resize(shortPreview.size()-1);WRITE_LE_UINT32(shortPreview.data()+4,shortPreview.size()-8);
	if(!acceptsMetadata(extraMetadata(classic,MKTAG('D','A','T','E'),0,dateBytes,6),metadata,false)||
	   !acceptsMetadata(extraMetadata(richBytes,MKTAG('D','A','T','E'),0,dateBytes,7),metadata,false)||
	   !acceptsMetadata(extraMetadata(classic,MKTAG('P','5','6','5'),0,extension,3),metadata,false)||
	   !acceptsMetadata(extraMetadata(richBytes,MKTAG('P','5','6','5'),0,extension,3),metadata,false)||
	   !acceptsMetadata(shortPreview,metadata,false)){
		error="Bounded optional date/preview negative contracts failed";return false;
	}
	uint enhancementCases = 0;
	if (!validateEnhancements(error, enhancementCases)) return false;
	report += Common::String::format("PASS\tfiles=%u\tnegative=%u\trecord_cases=%u\tstorage_cases=3\tschema_cases=7\tmetadata_cases=22\tenhancement_cases=%u\n", files, negative, recordCases, enhancementCases);
	Common::OutSaveFile *out = getSaveFileManager()->openForSaving("nox-resource-validation.tsv", false);
	if (!out) { error = "Cannot open resource validation report"; return false; }
	out->write(report.c_str(), report.size()); out->finalize();
	const bool failed = out->err();
	delete out;
	if (failed) { error = "Failed writing resource validation report"; return false; }
	failure.passed = true;
	return true;
}
} // namespace Nox
