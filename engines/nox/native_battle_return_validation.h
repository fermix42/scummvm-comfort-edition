/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_RETURN_VALIDATION_H
#define NOX_NATIVE_BATTLE_RETURN_VALIDATION_H
#include "nox/native_battle_aftermath.h"
namespace Nox {
inline bool validateNativeBattleReturn483(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    using namespace Native;
    report="source battle flags/treasure/retirement";
    if (input.readUint32BE()!=MKTAG('N','R','4','8') || input.readUint32LE()!=1 || input.readUint32LE()!=792) return false;
    uint equivalent=0;
    for (uint i=0;i<792;++i) {
        byte params[4],tables[1792],fields[256],expectedFields[256],queued[3],expected[1792];
        if (input.read(params,4)!=4 || input.read(tables,1792)!=1792 || input.read(fields,256)!=256 || input.read(expectedFields,256)!=256 || input.read(queued,3)!=3 || input.read(expected,1792)!=1792) return false;
        BattleTreasureAward award;
        if (!BattleWorldReturn::effects(tables,1792,fields,256,params[0],params[1],params[2]!=0,&award) || memcmp(fields,expectedFields,256) || award.marker!=queued[0] || (award.marker && (award.kind!=queued[1] || award.id!=queued[2])) ||
            !BattleWorldReturn::retire(tables,1792,fields,256,params[0],params[1],params[2]!=0) || memcmp(tables,expected,1792)) {
            report=Common::String::format("battle return case%u bank%u slot%u canonical%u",i,params[0],params[1],params[3]);return false;
        }
        equivalent+=params[3]!=0;
    }
    if (equivalent!=144 || input.err() || input.pos()!=input.size()) return false;
    report="battle queued reward preflight atomicity";
    byte tables[1792]={},fields[256]={};tables[1285]=126;tables[768]=192;tables[769]=12;tables[771]=4;tables[773]=3;
    BattleTreasureAward invalid;
    if (BattleWorldReturn::effects(tables,1792,fields,256,1,0,false,&invalid) || fields[12] || invalid.marker) return false;
    tables[773]=0;if (BattleWorldReturn::effects(tables,1792,fields,256,1,0,false) || fields[12]) return false;
    // Independent controlled post-victory owner checkpoint, not an earned win
    // or a linked battle. Actual authored UM97 NPC8 supplies the quest/item.
    report="authored UM97 aftermath checkpoint setup";
    Common::ScopedPtr<NewGame> game(new NewGame(archive));if (!game->create("RETURN",0,0)) return false;
    WorldTravelPlan plan;plan.type=2;plan.map=97;plan.x=142;plan.y=170;if (!WorldTravel::commit(game->world,plan)) return false;
    const byte *current=game->world.currentTables()->data(),*definition=current+776,*parent=current+1288;
    if (parent[5]!=126 || definition[0]!=192 || definition[1]!=46 || definition[3]!=8 || definition[5]!=2 || definition[7]!=81) return false;
    const byte beforeFlag=game->world.field(46);const int owned=game->inventory.find(2,81);const uint quantity=owned<0?0:game->inventory.item(owned)->quantity();
    PreparedEncounter field;field.terrain.resize(187);memset(field.terrain.data(),1,187);field.admission.group=124;
    GameplayRandom random;if (!random.initializeEntropy(119)) return false;
    Common::ScopedPtr<EncounterState> battle(new EncounterState);if (!battle->begin(archive,*game,field,random) || !battle->orientInitial(0)) return false;
    battle->presented=true;battle->round.phase=kBattleVictory;
    for (uint i=0;i<battle->round.enemyCount;++i) {battle->records.extended[i*8+3]=255;WRITE_LE_UINT16(battle->records.stats+768+i*32+2,0);}
    WorldActorTurnEvent contact;contact.battle=true;contact.bank=1;contact.slot=8;contact.cell=76;contact.direction=0;
    BattleAftermath aftermath;if (!aftermath.load(archive)) return false;
    report="authored aftermath actual queued item/flags";
    if (!aftermath.begin(contact,*game,*battle,random,false,parent[2]) || !battle->loot.scripted || !battle->loot.finished || !battle->loot.committed || battle->loot.kind!=2 || battle->loot.id!=81 || !aftermath.returnReady(*battle) || game->world.field(46)!=(beforeFlag|8)) return false;
    const int granted=game->inventory.find(2,81);if (granted<0 || game->inventory.item(granted)->quantity()!=quantity+1) return false;
    Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),state(DisposeAfterUse::YES);
    if (!game->save(core) || !battle->save(state)) return false;
    Common::ScopedPtr<NewGame> resumed(new NewGame(archive));Common::ScopedPtr<EncounterState> result(new EncounterState);
    Common::MemoryReadStream coreBytes(core.getData(),core.size()),stateBytes(state.getData(),state.size());
    report="authored result codec/retirement once";
    if (!resumed->load(coreBytes) || !result->load(stateBytes) || !aftermath.returnReady(*result) || !aftermath.retireWorldContact(contact,*resumed,*result) || !aftermath.retireWorldContact(contact,*resumed,*result) || aftermath.begin(contact,*resumed,*result,random)) return false;
    const byte *retired=resumed->world.currentTables()->data()+1288;
    if (retired[3]!=255 || retired[7]!=255 || resumed->inventory.item(resumed->inventory.find(2,81))->quantity()!=quantity+1) return false;
    report="battle-return\texact_source_cases=648\tcanonical_slot248_equivalents=144\tqueued_preflight_refusals=2\tauthored_aftermath_item_codec=1\tretirement_once=1\tearned_win=0\tcampaign=0\taudio_suite=0\n";return true;
}
}
#endif
