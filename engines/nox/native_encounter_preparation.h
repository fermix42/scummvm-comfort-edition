/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_PREPARATION_H
#define NOX_NATIVE_ENCOUNTER_PREPARATION_H
#include "nox/native_new_game.h"
#include "nox/native_world_actor_turn.h"
#include "nox/native_battlefields.h"
#include "nox/native_encounter_catalog.h"
#include "nox/native_encounter_admission.h"
#include "nox/native_world_travel.h"
namespace Nox { namespace Native {
struct PreparedEncounter {
    EncounterSelection enemy;
    BattlefieldChoice choice;
    Common::Array<byte> terrain;
    EncounterAdmission admission;
    byte contextFlags,contactSprite;
    bool protectedTargets;
    PreparedEncounter() : contextFlags(0),contactSprite(0),protectedTargets(false) {}
};
// Deterministic data preparation from the actual pending world turn. This
// does not retire time, place actors, apply damage or start tactical input.
class EncounterPreparation {
public:
    static bool prepare(HdvArchive &archive,NewGame &game,const WorldScene &scene,const WorldActorTurnState &turn,
            const WorldActorTurnEvent &request,PreparedEncounter &output,bool scripted=false) {
        if (!game.ready() || game.world.onFoot()!=(turn.presentation.partyObject==255) ||
            !request.battle || request.bank>1 || request.slot>252 || (request.slot&3) || (request.bank && (request.slot&7)) || request.direction>3 || request.cell>=187) return false;
        // The admitted contact remains owned until late return. Quest effects
        // may already have changed visibility flags before a loot/cold resume;
        // re-admitting against those new flags would lose the retained contact.
        const byte *actor=game.world.currentTables()->data()+(request.bank?1280:768)+request.slot;
        if (!scripted && (!actor[2] || actor[3]==255)) return false;
        if (scripted) {
            WorldTravelPlan plan;
            if (game.world.mapType()!=0 || request.bank || request.slot || request.cell!=93 ||
                WorldTravelRules::enter(0,game.world.mapId(),game.world.x(),game.world.y(),game.world.field(59),game.world.currentTables()->data()+1536,256,plan)!=kTravelSpecialBattle) return false;
        }
        const byte sprite=scripted?133:actor[2];
        Common::Array<byte> data,footer;MapAssetRange range;
        if (!archive.read("NA/GAME",data) || !resolveMapAssets(data,game.world.mapType(),range) || !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        // Original on-foot interior flags: a water-class primary actor adds
        // bit32. Ship/raft and surface/air admission need their own callers.
        const byte flags=scripted?0:EncounterAdmissionRules::transportFlags(game.world.mapType(),sprite,game.appearance,!game.world.onFoot(),footer[60],footer[61]);
        BattlefieldCatalog fields;PreparedEncounter next;EncounterAdmissionRules admissionRules;
        byte location;if (!game.world.locationContext(location)) return false;
        const EncounterContext context={game.world.mapType(),game.world.mapId(),location,byte(request.slot),sprite,footer[64],footer[65]};
        if (!fields.load(archive) || !admissionRules.load(archive) || !admissionRules.select(context,next.admission) ||
            !fields.choose(game.world.mapType(),scripted?63:scene.actors.terrain[93],scripted?63:scene.actors.terrain[request.cell],flags,next.choice) ||
            !fields.project(next.choice.field,game.world.mapType(),next.terrain)) return false;
        if (!scripted && !EncounterAdmissionRules::protection(game.world.currentTables()->data(),1792,game.world.questFields(),256,request.bank,request.slot,next.protectedTargets)) return false;
        next.contextFlags=flags;next.contactSprite=sprite;next.enemy.descriptor=next.admission.descriptor;memcpy(next.enemy.data,next.admission.raw,4);memset(next.enemy.stats,0,32);
        if (next.enemy.fixedMonster()) {
            Common::Array<byte> stats;if (!archive.readRange("NA/SRTN.COMBAT.S",0x263c+uint(next.enemy.data[2])*32,32,stats)) return false;
            memcpy(next.enemy.stats,stats.data(),32);
        }
        output=next;return true;
    }
};
} }
#endif
