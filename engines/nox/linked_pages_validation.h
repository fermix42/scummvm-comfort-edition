/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_LINKED_PAGES_VALIDATION_H
#define NOX_LINKED_PAGES_VALIDATION_H
namespace Nox {
bool Runtime::campaignFixturePageTools(bool missing) {
    if(!atWorldInput()||!canSnapshot()||campaignPageFlags()||campaignLogCount(97)||mainram[0x421]>99)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> inventory;
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",inventory))return false;
    // The Search overlay is resource slice8A92, loaded9600. Original9A68
    // tests lockpick counter421;9AC5 consumes one before original trap roll.
    const uint base=0x8a92;const byte test[]={0xad,0x21,4},consume[]={0xce,0x21,4};
    if(inventory.size()<base+0x1000||memcmp(inventory.data()+base+0x9a68-0x9600,test,3)||memcmp(inventory.data()+base+0x9ac5-0x9600,consume,3))return false;
    mainram[0x421]=missing?0:25;return true;
}
bool Runtime::campaignSafeFloor() const {
    if(!atWorldInput()||!canSnapshot()||mainram[0x6002]||mainram[0x6094])return false;
    const byte tile=mainram[0x85d];for(uint i=0;i<6;i+=2)if(tile>=mainram[0x781+i]&&tile<mainram[0x782+i])return false;
    if((tile>=229&&tile<=231)||(tile>=mainram[0x7af]&&tile<mainram[0x7b0])||(tile>=mainram[0x7b1]&&tile<mainram[0x7b2]))return false;
    return true;
}
bool Runtime::campaignFixturePageSearchSeed(uint32 seed) {
    if(!campaignSafeFloor())return false;
    const byte signature[]={0x18,0xad,7,0x63,0x8d,2,0x63,0x69,1};if(memcmp(mainram+0x10e5,signature,sizeof(signature)))return false;
    // CONTROLLED valid original PRNG integer before Search, not a pick/trap
    // result. Keep every skill, HP, object, inventory/page outcome unchanged.
    WRITE_LE_UINT32(mainram+0x6307,seed);return true;
}
bool NoxEngine::validateCampaignPagesLinked(bool cold,Common::String &error) {
    const bool missing=ConfMan.hasKey("nox_pages_missing_tools")&&ConfMan.getBool("nox_pages_missing_tools");
    Common::String trace;
    auto report=[&](const char *name,const Common::String &text){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;};
    auto fail=[&](const Common::String &why){error=why;report("nox-campaign-trace.tsv",trace);report("nox-campaign-failure.tsv",why+Common::String::format("\nPC=%04X map=%u xy=%u,%u pages=%u flags=%02X HP=%u\n",_runtime->address(),_runtime->campaignMapId(),_runtime->positionX(),_runtime->positionY(),_runtime->campaignLogCount(97),_runtime->campaignPageFlags(),_runtime->campaignPartyHealth())+_runtime->campaignMachineDiagnostic());if(_runtime->canSnapshot())saveGameState(3,"Linked pages diagnostic, not acceptance");_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-campaign-failure.png",false);if(out){Image::writePNG(*out,_surface);out->finalize();delete out;}return false;};
    if(_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Linked pages require isolated offline target");
    if(cold){
        int handle=-1;for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))if(entry.getDescription()==(missing?"Page tools refusal":"Seven original pages"))handle=entry.getSaveSlot();if(handle<0)return fail("Linked original pages save absent");
        const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
        if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Linked page cold restore failed");
        Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);if(!_runtime->saveState(restored)||restored.size()!=uint(payload.size()-entry.offset)||memcmp(restored.getData(),payload.data()+entry.offset,restored.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=entry.metadata.session||!_runtime->atWorldInput()||_runtime->campaignPageFlags()!=(missing?0u:253u)||_runtime->campaignLogCount(97)!=(missing?0u:7u)||_runtime->campaignPartyHealth()!=50||_runtime->campaignFlagByte(0x421)>25||(missing&&_runtime->campaignFlagByte(0x421)))return fail("Linked original pages exact cold fields/session differ");
        return report("nox-campaign-cold.tsv",missing?"PASS\truntime_bytes=identical no_lockpick_no_award=1 session=identical\n":"PASS\truntime_bytes=identical seven_actual_Search=1 session=identical\n");
    }
    if(!_runtime->atWorldInput()||!_runtime->canSnapshot()||_runtime->campaignPageFlags()||_runtime->campaignLogCount(97)||_runtime->campaignPartyHealth()!=50)return fail("Linked page original prefix differs");
    Common::CEAchievements().noteDisqualifyingAssistance("nox_linked_pages_controlled_routes");_runtime->enableDialogueValidationPolling();
    if(!_runtime->campaignFixturePageTools(missing))return fail("Source-supported controlled lockpick prerequisite rejected");trace+=Common::String::format("CONTROLLED-before-any-Search-lockpicks\t%u no page/count/flag initialization\n",missing?0u:25u);
    auto advance=[&](){bool ok=_runtime->advance(100000,true);_runtime->audio().clear();return ok;};
    auto settle=[&](){for(uint i=0;i<12000&&!shouldQuit();++i){if(_runtime->atWorldInput()&&_runtime->canSnapshot())return true;if(!advance())return false;}return false;};
    auto route=[&](byte map,byte type){trace+=Common::String::format("CONTROLLED-authored-route-before-next-operation\t%u,%u\n",map,type);return _runtime->dialogueFixtureExit(map,type)&&settle();};
    if(!route(0,0)||!route(54,7)||!route(78,7))return fail("Linked pages initial authored route failed");
    const byte masks[]={4,8,16,32,64,128,1},maps[]={78,78,78,78,57,57,57},columns[]={5,27,9,33,32,37,44},rows[]={9,13,12,18,1,36,20};uint flags=0;
    for(uint page=0;page<7;++page){
        if(page==4&&(!route(54,7)||!route(58,7)||!route(57,7)))return fail("Linked pages library authored route failed");
        if(_runtime->campaignLogCount(97)!=page||_runtime->campaignPageFlags()!=flags||!_runtime->campaignFixturePage(maps[page],columns[page],rows[page],masks[page])||!settle())return fail("Linked source-object adjacency/prior state rejected");
        trace+=Common::String::format("CONTROLLED-before-Search-adjacency\tpage=%u map=%u xy=%u,%u retained_count=%u flags=%02X\n",page+1,maps[page],_runtime->positionX(),_runtime->positionY(),page,flags);
        for(uint repeat=0;repeat<2;++repeat){
            const uint picksBefore=_runtime->campaignFlagByte(0x421),attemptsBefore=_runtime->campaignPagePickAttempts();
            if(!repeat){const uint32 seed=ConfMan.hasKey("nox_pages_seed")?uint32(ConfMan.getInt("nox_pages_seed")):0;if(!_runtime->campaignFixturePageSearchSeed(seed))return fail("Source-supported pre-Search seed/safe floor rejected");trace+=Common::String::format("CONTROLLED-before-Search-original-RNG-state\t%u no skill/HP/page/outcome writes\n",seed);}
            _runtime->key('S');_runtime->key(_runtime->campaignPageDirection(masks[page]));uint64 polls=_runtime->emptyKeyboardPolls();bool done=false;
            for(uint i=0;i<12000&&!shouldQuit();++i){if(!advance())return fail(_runtime->error());if(_runtime->atWorldInput()&&_runtime->canSnapshot()){done=true;break;}if(_runtime->atKeyboardPoll()&&_runtime->emptyKeyboardPolls()>polls+16){_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}}
            if(missing){if(!done||_runtime->campaignLogCount(97)||_runtime->campaignPageFlags()||_runtime->campaignPartyHealth()!=50||_runtime->campaignFlagByte(0x421))return fail("Actual no-lockpick Search refusal differs");if(saveGameState(1,"Page tools refusal").getCode()!=Common::kNoError||saveGameState(1,"Page tools refusal").getCode()!=Common::kNoError)return fail("No-lockpick refusal save failed");report("nox-campaign-trace.tsv",trace);return report("nox-campaign.tsv","PASS\tpage_mask=0\tactual_no_lockpick_Search_no_award=1\n");}
            const uint attempts=_runtime->campaignPagePickAttempts()-attemptsBefore;
            if(!repeat)flags|=masks[page];if(!done||_runtime->campaignLogCount(97)!=page+1||_runtime->campaignPageFlags()!=flags||_runtime->campaignPartyHealth()!=50||attempts>picksBefore||_runtime->campaignFlagByte(0x421)!=picksBefore-attempts||(repeat&&attempts))return fail("Actual linked Search/repeat/count/flag/health/original-DEC421 ledger differs");
            trace+=Common::String::format("actual-Search-completed\tpage=%u repeat=%u count=%u flags=%02X HP=50 picks_before=%u source_DEC421=%u picks_after=%u\n",page+1,repeat,page+1,flags,picksBefore,attempts,_runtime->campaignFlagByte(0x421));
        }
    }
    if(saveGameState(1,"Seven original pages").getCode()!=Common::kNoError||saveGameState(1,"Seven original pages").getCode()!=Common::kNoError)return fail("Linked original page save failed");
    report("nox-campaign-trace.tsv",trace);return report("nox-campaign.tsv","PASS\tpage_mask=253\tseven_actual_Search=1 seven_actual_repeat=1 no_page_count_flag_initialization=1\n");
}
}
#endif
