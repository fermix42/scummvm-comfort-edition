/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_M44_AWARD_VALIDATION_H
#define NOX_NATIVE_M44_AWARD_VALIDATION_H
#include "nox/native_battle_loot.h"
#include "nox/native_search.h"
namespace Nox {
// Rule-level allocation boundary, NOT an earned inventory or campaign fixture.
// Construct inventory using real catalog definitions and grantOne; no live game,
// map, quest outcome, save or battle result is edited to satisfy an assertion.
inline bool validateNativeM44Awards1136(HdvArchive &archive,Common::String &report) {
    using namespace Native;
    report="M4.4 failed-award atomicity rejected";
    ItemCatalog catalog;BattleLoot loot;if (!catalog.load(archive) || !loot.load(archive)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
    const uint limits[]={102,122,168};uint cases=0;
    for (uint mode=0;mode<4;++mode) {
        const byte kind=mode<2?2:0,id=mode==0?68:mode==1?157:90;
        byte empty[1536]={0};for (uint i=0;i<256;++i) empty[i*6]=254;
        Inventory inventory;if (!inventory.importRecords(empty,1536)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        uint added=0;
        for (uint pass=0;pass<2 && added<256;++pass) for (uint k=0;k<3 && added<256;++k) for (uint ident=0;ident<limits[k] && added<256;++ident) {
            if (k==kind && ident==id) continue;
            ItemDefinition d;if (!catalog.definition(byte(k),byte(ident),d)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
            if (bool(d.bytes[8]&128)!=(pass==0)) continue;
            if (inventory.grantOne(catalog,byte(k),byte(ident))!=kInventoryApplied) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
            ++added;
        }
        if (added!=256 || !inventory.ordinaryCapacityRemaining() || inventory.find(kind,id)>=0) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        byte before[1536],after[1536],fields[256]={0};if (!inventory.exportRecords(before,1536)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        GameplayRandom random;if (!random.importState(0x19231923,2)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        const uint32 seed=random.seed();const byte last=random.last();
        BattleLootState state;state.scripted=mode<2;state.kind=kind;state.id=id;
        if (!loot.begin(1,1,50,75,state)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        // begin resets ordinary offer metadata; the actual generation boundary
        // supplies kind/id when it marks that offer pending.
        if (mode>=2) {state.pending=true;state.kind=kind;state.id=id;}
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!state.save(saved)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        BattleRecords battle;
        const bool accepted=mode<2?loot.advance(battle,inventory,random,state,fields,256):mode==2?
            loot.choose(true,false,battle,inventory,random,state):loot.chooseGroup(true,false,1,33,inventory,random,state);
        Common::MemoryWriteStreamDynamic current(DisposeAfterUse::YES);if (!state.save(current)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        if (accepted || !inventory.exportRecords(after,1536) || memcmp(before,after,1536) ||
            saved.size()!=current.size() || memcmp(saved.getData(),current.getData(),saved.size()) ||
            random.seed()!=seed || random.last()!=last) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        for (uint f=0;f<256;++f) if (fields[f]) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        Common::MemoryReadStream cold(saved.getData(),saved.size());BattleLootState restored;if (!restored.load(cold)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        if (mode<2 && (restored.finished || restored.committed)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        if (mode<2) {
            byte object[12]={1,1,1,53,kind,id,0,64,62,0,0,4};
            if (searchQuestReward(object,12,fields,256,catalog,inventory)!=kSearchRewardUnsupported || object[3]!=53 || fields[62]) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
            DialogueHandoff request;request.kind=DialogueHandoff::kItem;request.itemKind=kind;request.itemId=id;uint16 gold=75;
            if (finishDialogueHandoff(request,catalog,inventory,gold)!=kHandoffNoSpace || gold!=75) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        }
        ++cases;
    }
    // Adjacent normal success and existing-stack merge remain once-only awards.
    for (uint occupied=0;occupied<2;++occupied) {
        byte empty[1536]={0};for (uint i=0;i<256;++i) empty[i*6]=254;
        Inventory inventory;if (!inventory.importRecords(empty,1536)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        if (occupied && inventory.grantOne(catalog,2,157)!=kInventoryApplied) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        BattleLootState state;state.scripted=true;state.kind=2;state.id=157;byte fields[256]={0};BattleRecords battle;GameplayRandom random;
        if (!random.importState(0x19231923,2) || !loot.begin(1,1,50,75,state) || !loot.advance(battle,inventory,random,state,fields,256)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        const int slot=inventory.find(2,157);if (slot<0 || inventory.item(slot)->quantity()!=occupied+1 || !state.finished) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        uint16 food=50,gold=75;if (!loot.commit(food,gold,state) || loot.commit(food,gold,state)) {report=Common::String::format("M4.4 award check failed at line%u",uint(__LINE__));return false;}
        ++cases;
    }
    report=Common::String::format("PASS\tcases=%u\tphysical_full_atomic=4\tsearch_handoff_atomic=2\tnormal_new_merge=2\tpending_codec=4\tlive_saves_changed=0\tearned_reachability_proved=0\n",cases);
    return true;
}
}
#endif
