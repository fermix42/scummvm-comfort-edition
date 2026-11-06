/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_DARK_KEY_VALIDATION_H
#define NOX_DARK_KEY_VALIDATION_H
namespace Nox {
bool Runtime::campaignFixtureDratheFinalBlow() {
    if(!_battleValidation||!inBattle()||campaignMapId()!=32||!battlePlayerTurn()||!keys.empty()||key_latch||(pc!=0xd53f&&pc!=0xfde9)||campaignDarkKeyFlag()||campaignLogCount(154)||campaignFinalEnemyHealth()!=3210||campaignPartyHealth()!=125||campaignMana()!=110||!campaignStaffRange())return false;
    // Shared original damage1AC9..1AF0 marks a depleted record FF/HP0.
    const byte death[]={0xa9,0,0x8d,0x81,6,0x8d,0x82,6,0xa9,255,0x8d,0x7f,6};
    if(memcmp(mainram+0x1ae3,death,sizeof(death)))return false;
    const uint count=campaignEnemyCount();if(count<1||count>16||mainram[0xbf02]!=165)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> combat;
    if(!image.open(stream)||!image.read("NA/SRTN.COMBAT.S",combat)||combat.size()<0x33bc||READ_LE_UINT16(combat.data()+0x339e)!=3210||combat[0x33ac]!=165||READ_LE_UINT16(combat.data()+0x333e)!=471||READ_LE_UINT16(combat.data()+0x335e)!=471)return false;
    // Validate the complete canonical cache before preparing any record.
    for(uint i=1;i<count;++i){uint slot=i*8;uint cells=0;
        if(!READ_LE_UINT16(auxram+0x1902+i*32)||(mainram[0xbf02+slot]!=153&&mainram[0xbf02+slot]!=220)||mainram[0xbf03+slot]==255)return false;
        for(uint cell=0;cell<187;++cell)if(mainram[0xa31+cell]==slot&&mainram[0xaec+cell]==1)++cells;
        if(cells!=1)return false;
    }
    // CONTROLLED pre-attack endpoint, never earned kills/damage: defeated
    // secondary records/cache/occupancy and a living wounded primary. Keep
    // total encounter count, definitions, primary defense and quest flags.
    for(uint i=1;i<count;++i){uint slot=i*8;auxram[0x1900+i*32]=255;WRITE_LE_UINT16(auxram+0x1902+i*32,0);mainram[0xbf03+slot]=255;
        for(uint cell=0;cell<187;++cell)if(mainram[0xa31+cell]==slot&&mainram[0xaec+cell]==1){mainram[0xa31+cell]=255;mainram[0xaec+cell]=255;}}
    WRITE_LE_UINT16(auxram+0x1902,1);return true;
}
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool NoxEngine::validateDarkKey(bool cold,Common::String &error) {
    if(ConfMan.hasKey("nox_dark_key_gate")&&ConfMan.getBool("nox_dark_key_gate"))return validateDarkKeyGate(cold,error);
    Common::String trace;
    auto report=[&](const char *name,const Common::String &text){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;};
    auto frame=[&](const char *name){_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;bool ok=Image::writePNG(*out,_surface);out->finalize();ok&=!out->err();delete out;return ok;};
    auto fail=[&](const Common::String &why){error=why;trace+=why+"\n"+_runtime->battleDiagnostic()+"\n"+_runtime->campaignSpellDiagnostic();report("nox-dark-key-failure.tsv",trace);Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-dark-key-failure-memory.bin",false);if(out){_runtime->campaignDiagnosticMemory(*out);out->finalize();delete out;}frame("nox-dark-key-failure.png");return false;};
    const bool missing=ConfMan.hasKey("nox_dark_key_missing")&&ConfMan.getBool("nox_dark_key_missing");
    const bool replay=ConfMan.hasKey("nox_dark_key_replay")&&ConfMan.getBool("nox_dark_key_replay");
    const bool checkOnly=ConfMan.hasKey("nox_dark_key_check_only")&&ConfMan.getBool("nox_dark_key_check_only");
    if(!cold||_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Requires isolated offline cold checkpoint");
    const Common::String label=replay?"Dark Key controlled approach":checkOnly?"Dark Key result":Common::String::format("Vortex phase %u",missing?33u:32u);int handle=-1;
    for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))if(entry.getDescription()==label)handle=entry.getSaveSlot();
    if(handle<0)return fail("Dark Key predecessor missing");
    const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
    if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Dark Key predecessor restore failed");
    Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);
    if(!_runtime->saveState(restored)||restored.size()!=uint(payload.size()-entry.offset)||memcmp(restored.getData(),payload.data()+entry.offset,restored.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=entry.metadata.session)return fail("Dark Key exact runtime/session restore differs");
    if(checkOnly){if(!_runtime->atWorldInput()||!_runtime->campaignDarkKeyFlag()||_runtime->campaignLogCount(154)!=1||_runtime->campaignMapId()!=32)return fail("Dark Key cold reward fields differ");return report("nox-dark-key-cold.tsv","PASS\truntime_bytes=identical actual_key154=1 actual040C08=1\n");}
    if(_runtime->campaignDarkKeyFlag()||_runtime->campaignLogCount(154)||_runtime->campaignRingFound()==missing||_runtime->campaignLogCount(155)!=(missing?0u:1u))return fail("Actual Ring/no-key prerequisite differs");
    Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_dark_key_checkpoint");_runtime->enableDialogueValidationPolling();_runtime->enableSpellValidationObservation();
    auto advance=[&](){bool ok=_runtime->advance(100000,true);_runtime->audio().clear();return ok;};
    auto settle=[&](){for(uint i=0;i<12000&&!shouldQuit();++i){if(_runtime->atWorldInput()&&_runtime->canSnapshot())return true;if(!advance())return false;}return false;};
    if(!replay){
        if(!_runtime->campaignRingDefenseValid()||!_runtime->campaignFixtureStaff())return fail("Controlled unreadied source-valid staff/training rejected");
        uint stage=0,prompts=0;bool readied=false;_runtime->key(9);_runtime->key(9);
        for(uint i=0;i<12000&&!shouldQuit();++i){
            if(_runtime->atWorldInput()&&_runtime->canSnapshot()){readied=stage>=2&&_runtime->campaignStaffRange();break;}
            if(_runtime->campaignTestInputReady()&&!_runtime->atWorldInput()){
                if(++prompts>64)return fail("Actual staff Ready input bound exceeded");
                if(!stage){if(_runtime->address()!=0xa9a1||_runtime->campaignInventoryTab()!=1)_runtime->key(_runtime->address()==0x9966?9:'2');else{int row=_runtime->campaignOwnedStaffRow();if(row<0)return fail("Controlled staff absent from original Weapons list");if(_runtime->campaignInventoryRow()!=row)_runtime->key(10);else{_runtime->key(13);stage=1;}}}
                else if(stage==1){if(_runtime->campaignStaffEquipped()){stage=2;_runtime->key(27);}else _runtime->key('1');}else _runtime->key(27);
            }
            if(!advance())return fail("Actual staff Ready advance failed");
        }
        if(!readied)return fail("Actual staff Ready/derived range not demonstrated");
        auto route=[&](byte id,byte type){return _runtime->dialogueFixtureExit(id,type)&&settle();};
        if(!route(0,128)||!route(31,3)||!route(32,3)||!_runtime->campaignFixtureRingBattleApproach()||!settle())return fail("Authored Dark Key encounter approach failed");
        if(saveGameState(2,"Dark Key controlled approach").getCode()!=Common::kNoError||saveGameState(2,"Dark Key controlled approach").getCode()!=Common::kNoError)return fail("Dark Key legal approach save failed");
    }
    if(_runtime->campaignMapId()!=32||_runtime->positionX()!=15||_runtime->positionY()!=21||!_runtime->campaignStaffRange()||_runtime->campaignMana()!=110)return fail("Retained actual staff/approach differs");
    _runtime->enableBattleValidation();_runtime->key('A');_runtime->key(11);bool tactical=false,prepared=false,attacked=false,returned=false,refused=false;uint prompts=0;
    for(uint i=0;i<24000&&!shouldQuit();++i){
        if(_runtime->atWorldInput()&&_runtime->canSnapshot()){returned=true;break;}
        if(prepared&&missing&&!_runtime->campaignPartyHealth()){refused=true;break;}
        if(prepared&&!_runtime->inBattle())_runtime->enableBattleValidation(false);
        if(_runtime->battleInputReady()){
            if(++prompts>256)return fail("Dark Key original battle input bound exceeded");const uint pc=_runtime->address();
            if((pc==0xd53f||pc==0xfde9)&&_runtime->inBattle()&&_runtime->battlePlayerTurn()){
                if(!prepared){trace+="original-full-admission\t"+_runtime->battleDiagnostic()+"\n";if(_runtime->campaignInvulnerability()!=(missing?1u:0u)||!_runtime->campaignFixtureDratheFinalBlow())return fail("Source-valid controlled final-blow state rejected");prepared=true;trace+="CONTROLLED-before-actual-attack\tprimaryHP1 minions_dead_not_earned\t"+_runtime->battleDiagnostic()+"\n";}
                if(!_runtime->campaignFinalEnemyHealth())_runtime->key(27);else if(_runtime->battleTargeting())_runtime->key(13);else{_runtime->key('A');attacked=true;}
            }else if(pc==0x1324&&!tactical&&_runtime->battleAdmissionSeen()){_runtime->key('T');tactical=true;}
            else if(pc==0x9dd1&&prepared&&!_runtime->campaignFinalEnemyHealth())_runtime->key('L');
            else if(_runtime->battleTargeting())_runtime->key(13);else _runtime->key(13);
        }
        if(!(i%128))trace+=_runtime->battleDiagnostic()+"\n";if(!advance())return fail("Actual Dark Key battle advance failed");
    }
    trace+=_runtime->battleDiagnostic()+"\n";report(replay?"nox-dark-key-replay-trace.tsv":"nox-dark-key-trace.tsv",trace);frame("nox-dark-key-result.png");
    if(!prepared||!attacked)return fail("Actual final-blow operation absent");
    if(missing){if(!refused||_runtime->campaignFinalEnemyHealth()!=1||_runtime->campaignDarkKeyFlag()||_runtime->campaignLogCount(154))return fail("Missing Ring physical attack refusal differs");Common::OutSaveFile *raw=getSaveFileManager()->openForSaving(replay?"nox-dark-key-refusal-replay.bin":"nox-dark-key-refusal.bin",false);if(!raw)return fail("Dark Key refusal diagnostic open failed");bool ok=_runtime->campaignDiagnosticSnapshot(*raw);raw->finalize();ok&=!raw->err();delete raw;if(!ok)return fail("Dark Key refusal diagnostic failed");return report(replay?"nox-dark-key-replay.tsv":"nox-dark-key.tsv","PASS\tcontrolled_final_blow=1 actual_attack_refused=1 key154=0 040C08=0 raw_diagnostic_not_player_save=1\n");}
    if(!returned||!_runtime->campaignPartyHealth()||!_runtime->campaignDarkKeyFlag()||_runtime->campaignLogCount(154)!=1)return fail("Actual Dark Key victory/award/world return absent");
    Common::MemoryWriteStreamDynamic world(DisposeAfterUse::YES);if(!_runtime->saveState(world))return fail("Dark Key legal world serialization failed");Common::OutSaveFile *out=getSaveFileManager()->openForSaving(replay?"nox-dark-key-world-replay.bin":"nox-dark-key-world.bin",false);if(!out)return fail("Dark Key world evidence open failed");out->write(world.getData(),world.size());out->finalize();bool ok=!out->err();delete out;if(!ok)return fail("Dark Key world evidence failed");
    if(saveGameState(1,replay?"Dark Key replay result":"Dark Key result").getCode()!=Common::kNoError||saveGameState(1,replay?"Dark Key replay result":"Dark Key result").getCode()!=Common::kNoError)return fail("Dark Key actual reward save failed");
    return report(replay?"nox-dark-key-replay.tsv":"nox-dark-key.tsv","PASS\tcontrolled_final_blow=1 actual_attack_victory_loot=1 key154=1 040C08=1 earned_full_encounter=0\n");
}
bool Runtime::campaignFixtureDarkKeyGate() {
    if(campaignMapId()!=30||mainram[0x267d]!=3||!campaignDarkKeyDoorClosed())return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> ruin,underworld;
    const byte door[]={34,8,18,18,12,8,0,0},exit[]={39,8,97,2},entry[]={30,150,167,39,8};
    if(!image.open(stream)||!image.read("NA/DATA.MAP.RUIN",ruin)||!image.read("NA/DATA.MAP.UM",underworld)||ruin.size()<40608||underworld.size()<7524||memcmp(ruin.data()+40328,door,8)||memcmp(ruin.data()+40604,exit,4)||memcmp(underworld.data()+7519,entry,5))return false;
    return campaignFixtureFloor(30,33,8);
}
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool NoxEngine::validateDarkKeyGate(bool cold,Common::String &error) {
    Common::String trace;
    auto report=[&](const char *name,const Common::String &text){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;};
    auto frame=[&](const char *name){_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;bool ok=Image::writePNG(*out,_surface);out->finalize();ok&=!out->err();delete out;return ok;};
    auto fail=[&](const Common::String &why){error=why;trace+=why+Common::String::format("\nPC=%04X map=%u xy=%u,%u key=%u flag=%u\n",_runtime->address(),_runtime->campaignMapId(),_runtime->positionX(),_runtime->positionY(),_runtime->campaignLogCount(154),_runtime->campaignDarkKeyFlag());report("nox-dark-key-gate-failure.tsv",trace);Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-dark-key-gate-failure-memory.bin",false);if(out){_runtime->campaignDiagnosticMemory(*out);out->finalize();delete out;}frame("nox-dark-key-gate-failure.png");return false;};
    const bool missing=ConfMan.hasKey("nox_dark_key_missing_key")&&ConfMan.getBool("nox_dark_key_missing_key"),replay=ConfMan.hasKey("nox_dark_key_gate_replay")&&ConfMan.getBool("nox_dark_key_gate_replay"),checkOnly=ConfMan.hasKey("nox_dark_key_gate_check_only")&&ConfMan.getBool("nox_dark_key_gate_check_only");
    if(!cold||_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Requires isolated offline gate checkpoint");
    const Common::String label=replay?"Dark Key gate approach":checkOnly?"Dark Key gate result":missing?"Vortex phase 33":"Dark Key replay result";int handle=-1;
    for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))if(entry.getDescription()==label)handle=entry.getSaveSlot();
    if(handle<0)return fail("Retained actual key/no-key predecessor absent");const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
    if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Gate predecessor restore failed");
    Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);if(!_runtime->saveState(restored)||restored.size()!=uint(payload.size()-entry.offset)||memcmp(restored.getData(),payload.data()+entry.offset,restored.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=entry.metadata.session)return fail("Gate exact runtime/session restore differs");
    auto finalFields=[&](){return _runtime->campaignLogCount(154)==(missing?0u:1u)&&_runtime->campaignDarkKeyFlag()!=missing&&_runtime->campaignMapId()==(missing?30u:97u)&&_runtime->positionX()==(missing?33u:150u)&&_runtime->positionY()==(missing?8u:167u)&&(!missing||_runtime->campaignDarkKeyDoorClosed());};
    if(checkOnly){if(!finalFields())return fail("Actual gate cold fields differ");return report("nox-dark-key-gate-cold.tsv","PASS\truntime_bytes=identical actual_gate_result=1\n");}
    if(_runtime->campaignLogCount(154)!=(missing?0u:1u)||_runtime->campaignDarkKeyFlag()==missing)return fail("Actual key/no-key gate prerequisites differ");
    Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_dark_key_gate_checkpoint");_runtime->enableDialogueValidationPolling();
    auto advance=[&](){bool ok=_runtime->advance(100000,true);_runtime->audio().clear();return ok;};
    auto settle=[&](const char *stage){uint64 polls=_runtime->emptyKeyboardPolls();uint acknowledgements=0;for(uint i=0;i<12000&&!shouldQuit();++i){if(_runtime->atWorldInput()&&_runtime->canSnapshot()){trace+=Common::String::format("%s\tmap=%u xy=%u,%u key=%u flag=%u HP=%u\n",stage,_runtime->campaignMapId(),_runtime->positionX(),_runtime->positionY(),_runtime->campaignLogCount(154),_runtime->campaignDarkKeyFlag(),_runtime->campaignPartyHealth());return true;}if(_runtime->atKeyboardPoll()&&!_runtime->atWorldInput()&&_runtime->emptyKeyboardPolls()>polls+32){if(++acknowledgements>32)return false;_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}if(!advance())return false;}return false;};
    if(!replay){
        auto route=[&](byte id,byte type){return _runtime->dialogueFixtureExit(id,type)&&settle("controlled-authored-route");};
        if(missing&&(!route(0,128)||!route(31,3)))return fail("No-key authored surface/ruins route failed");
        if(!missing&&!route(31,3))return fail("Retained key authored ruins return failed");
        if(!route(30,3)||!_runtime->campaignFixtureDarkKeyGate()||!settle("controlled-outside-door33,8"))return fail("Source-valid original gate adjacency rejected");
        if(saveGameState(2,"Dark Key gate approach").getCode()!=Common::kNoError||saveGameState(2,"Dark Key gate approach").getCode()!=Common::kNoError)return fail("Legal gate approach save failed");
    }
    if(_runtime->campaignMapId()!=30||_runtime->positionX()!=33||_runtime->positionY()!=8||!_runtime->campaignDarkKeyDoorClosed())return fail("Exact outside closed-door state differs");
    _runtime->key('O');_runtime->key(21);if(!settle("actual-Open-Right"))return fail("Actual gate Open failed");
    for(uint step=0;step<(missing?1u:6u);++step){_runtime->key(21);if(!settle("actual-Right-through-gate-to-original-stairs"))return fail("Actual gate walking/transition failed");}
    if(!finalFields())return fail("Actual gate passage/refusal/UM97 arrival differs");
    Common::MemoryWriteStreamDynamic world(DisposeAfterUse::YES);if(!_runtime->saveState(world))return fail("Legal gate result serialization failed");Common::OutSaveFile *out=getSaveFileManager()->openForSaving(replay?"nox-dark-key-gate-world-replay.bin":"nox-dark-key-gate-world.bin",false);if(!out)return fail("Gate world evidence open failed");out->write(world.getData(),world.size());out->finalize();bool ok=!out->err();delete out;if(!ok)return fail("Gate world evidence failed");
    if(saveGameState(1,"Dark Key gate result").getCode()!=Common::kNoError||saveGameState(1,"Dark Key gate result").getCode()!=Common::kNoError)return fail("Actual gate result save failed");report(replay?"nox-dark-key-gate-replay-trace.tsv":"nox-dark-key-gate-trace.tsv",trace);frame("nox-dark-key-gate-result.png");
    return report(replay?"nox-dark-key-gate-replay.tsv":"nox-dark-key-gate.tsv",Common::String::format("PASS\tmissing_key=%u actual_Open_and_steps=1 exact_authored_arrival=%u key154=%u\n",missing,!missing,_runtime->campaignLogCount(154)));
}
}
#endif
