/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TEST_CLI_H
#define NOX_NATIVE_TEST_CLI_H
#include "engines/ce_test_cli.h"
namespace Nox {
inline Common::JSONValue *NoxEngine::testCLIState() {
    using namespace Common;
    if (!CETestCLI::active()) return CETestCLI::unsupported();
    JSONObject o;o["ok"]=new JSONValue(true);
    o["native_only"]=new JSONValue(true);o["paused"]=new JSONValue(isPaused());
    o["can_restart_encounter"]=new JSONValue(canRestartEncounter());
    o["save_mode"]=new JSONValue((long long)_saveMode);
    o["sound_mode"]=new JSONValue((long long)_nativeSoundMode);o["music_channel_volume"]=new JSONValue((long long)_mixer->getChannelVolume(_nativeMusicSound));
    o["effects_played"]=new JSONValue((long long)_nativeEffectsPlayed);o["effect_samples"]=new JSONValue((long long)_nativeEffectSamples);o["effect_toggles"]=new JSONValue((long long)_nativeEffectToggles);
    if (!_nativePlayer) {o["stage"]=new JSONValue("initializing");return new JSONValue(o);}
    const Native::PlayerSessionState &p=*static_cast<Native::PlayerSessionState *>(_nativePlayer);
    o["stage"]=new JSONValue(_nativeStartupMenu?"title":p.sceneReady?(p.encounterState.presented?"battle":"world"):p.storyStarted?"story":"creation");
    o["map_overview"]=new JSONValue(p.mapOverview);
    o["map_reveal"]=new JSONValue(Native::revealExplorationMap());
    o["creation_stage"]=new JSONValue((long long)p.creation.stage());
    o["name"]=new JSONValue(p.creation.name());o["can_save"]=new JSONValue(canSaveGameStateCurrently());
    o["dialogue_phase"]=new JSONValue((long long)p.worldInput.conversationStage());
    o["visible_reply"]=new JSONValue(p.worldInput.conversationStage()?p.worldInput.visibleReply():String());
    const Native::InventoryWindow *inventoryView=p.encounterState.presented && p.encounterState.inventory.active()?&p.encounterState.inventory:nullptr;
    o["inventory_tab"]=new JSONValue((long long)(inventoryView?inventoryView->tab():p.worldInput.inventoryTab()));o["inventory_sheet_page"]=new JSONValue((long long)(inventoryView?inventoryView->sheetPage():p.worldInput.inventorySheetPage()));
    o["inventory_phase"]=new JSONValue((long long)(inventoryView?inventoryView->phase():p.worldInput.inventoryStage()));
    o["inventory_message"]=new JSONValue(inventoryView?inventoryView->message():p.worldInput.inventoryMessage());
    o["inventory_selected"]=new JSONValue((long long)(inventoryView?inventoryView->selected():p.worldInput.inventorySelected()));
    o["inventory_member"]=new JSONValue((long long)(inventoryView?inventoryView->member():p.worldInput.inventoryMember()));
    o["cast_phase"]=new JSONValue((long long)p.worldInput.castingStage());
    o["notice"]=new JSONValue(p.worldInput.noticeActive());
    o["open_direction"]=new JSONValue(p.worldInput.selectingOpen());
    o["animation_phase"]=new JSONValue((long long)p.animationState.phase);
    o["animation_pattern"]=new JSONValue((long long)p.animationState.pattern);
    o["animation_tick_ms"]=new JSONValue((long long)p.lastAnimationTick);
    o["animation_interval_ms"]=new JSONValue((long long)Native::WorldAnimation::kIntervalMs);
    o["music_ready"]=new JSONValue(p.music.ready());
    o["music_samples"]=new JSONValue((long long)p.music.samples());
    o["music_nonzero_samples"]=new JSONValue((long long)p.music.nonzero());
    o["music_handle_active"]=new JSONValue(_mixer->isSoundHandleActive(_nativeMusicSound));
    o["observed_cells"]=new JSONValue((long long)p.game.discovery.cells().size());
    o["journal_sequence"]=new JSONValue((long long)p.game.discovery.sequence());
    o["journal_characters"]=new JSONValue((long long)p.game.discovery.characters().size());
    if (p.game.ready()) {
        o["map_type"]=new JSONValue((long long)p.game.world.mapType());o["map"]=new JSONValue((long long)p.game.world.mapId());
        o["on_foot"]=new JSONValue(p.game.world.onFoot());o["appearance"]=new JSONValue((long long)p.game.appearance);
        o["transport_object"]=new JSONValue((long long)p.worldInput.transportObject());
        o["x"]=new JSONValue((long long)p.game.world.x());o["y"]=new JSONValue((long long)p.game.world.y());
        o["hour"]=new JSONValue((long long)p.game.clock.hour);o["minute"]=new JSONValue((long long)p.game.clock.minute);
        o["gold"]=new JSONValue((long long)p.game.gold);o["food"]=new JSONValue((long long)p.game.food);
        JSONArray fields;for (uint i=0;i<256;++i) fields.push_back(new JSONValue((long long)p.game.world.field(i)));o["quest_fields"]=new JSONValue(fields);
        JSONArray party;
        for (uint i=0;i<p.game.party.memberCount();++i) {
            const CharacterRecord *member=p.game.party.member(i);JSONObject a;
            a["hp"]=new JSONValue((long long)member->health());a["mp"]=new JSONValue((long long)member->bytes[4]);
            party.push_back(new JSONValue(a));
        }o["party"]=new JSONValue(party);
        JSONArray items;for (uint i=0;i<256;++i) {
            const Native::InventoryItem *item=p.game.inventory.item(i);if (!item || item->empty()) continue;
            JSONObject v;v["slot"]=new JSONValue((long long)i);v["kind"]=new JSONValue((long long)item->kind());
            v["id"]=new JSONValue((long long)item->id());v["quantity"]=new JSONValue((long long)item->quantity());items.push_back(new JSONValue(v));
        }o["inventory"]=new JSONValue(items);
    }
    if (p.encounterState.presented) {
        const Native::EncounterState &b=p.encounterState;
        o["quick_scroll_speed"]=new JSONValue((long long)p.quickDisplay.speed);o["quick_paused"]=new JSONValue(p.quickDisplay.paused);
        o["quick_character_cycles"]=new JSONValue((long long)Native::QuickBattleDisplay::characterCycles(p.quickDisplay.speed));
        o["quick_characters"]=new JSONValue((long long)p.quickDisplay.characters);o["quick_remaining"]=new JSONValue((long long)p.quickDisplay.remaining);
        o["quick_events_shown"]=new JSONValue((long long)p.quickDisplay.shown);o["quick_holding"]=new JSONValue(p.quickDisplay.holding);
        Common::String quickText;for (uint i=0;i<6;++i) {if (i) quickText+='\n';quickText+=p.quickDisplay.lines[i];}o["quick_text"]=new JSONValue(quickText);
        o["battle_phase"]=new JSONValue((long long)b.round.phase);o["battle_command"]=new JSONValue((long long)b.command);
        o["battle_results"]=new JSONValue((long long)b.resultPhase);o["active_party"]=new JSONValue((long long)b.round.activeParty);
        o["advancement_phase"]=new JSONValue((long long)b.advancement.phase);
        o["advancement_member"]=new JSONValue((long long)b.advancement.cursor);
        o["advancement_ability"]=new JSONValue((long long)b.advancement.ability);
        o["advancement_remaining"]=new JSONValue((long long)b.advancement.remaining);
        o["battle_target"]=new JSONValue((long long)b.target);o["battle_exit_phase"]=new JSONValue((long long)b.exit.phase);
        o["battle_exit_control"]=new JSONValue((long long)b.exit.control);o["battle_actions"]=new JSONValue((long long)b.round.actions);
        JSONArray tacticalParty;for (uint i=0;i<b.round.partyCount;++i) {
            const byte *actor=b.records.compact+i*4,*stats=b.records.stats+i*128;JSONObject a;
            a["x"]=new JSONValue((long long)actor[0]);a["y"]=new JSONValue((long long)actor[1]);
            a["hp"]=new JSONValue((long long)READ_LE_UINT16(stats+2));a["mp"]=new JSONValue((long long)stats[4]);
            a["status"]=new JSONValue((long long)actor[3]);a["hand_kinds"]=new JSONValue((long long)stats[21]);a["hand_ranges"]=new JSONValue((long long)stats[22]);
            tacticalParty.push_back(new JSONValue(a));
        }o["battle_party"]=new JSONValue(tacticalParty);
        JSONArray enemies;for (uint i=0;i<b.round.enemyCount;++i) {
            const byte *actor=b.records.extended+i*8,*stats=b.records.stats+768+i*32;JSONObject a;
            a["x"]=new JSONValue((long long)actor[0]);a["y"]=new JSONValue((long long)actor[1]);
            a["present"]=new JSONValue(actor[3]!=255);a["hp"]=new JSONValue((long long)READ_LE_UINT16(stats+2));
            a["sprite"]=new JSONValue((long long)stats[16]);enemies.push_back(new JSONValue(a));
        }o["enemies"]=new JSONValue(enemies);
    }
    return new JSONValue(o);
}
inline Common::JSONValue *NoxEngine::testCLICommand(Common::JSONValue &request) {
    if (!CETestCLI::active() || !_nativePlayer) return CETestCLI::unsupported();
    const Common::String cmd=CETestCLI::stringField(request,"cmd");
    if (cmd=="save") {
        const int slot=CETestCLI::intField(request,"slot");
        const Common::String description=CETestCLI::stringField(request,"description");
        if (!canSaveGameStateCurrently() || !allowedSaveSlot(_saveMode,slot)) return CETestCLI::error("save_not_allowed");
        if (saveGameState(slot,description.empty()?"Campaign":description).getCode()!=Common::kNoError) return CETestCLI::error("save_failed");
        return testCLIState();
    }
    if (cmd=="load") {
        const int handle=CETestCLI::intField(request,"chooser_slot",CETestCLI::intField(request,"slot"));
        if (handle<0 || loadGameState(handle).getCode()!=Common::kNoError) return CETestCLI::error("load_failed");
        return testCLIState();
    }
    return CETestCLI::unsupported();
}
}
#endif
