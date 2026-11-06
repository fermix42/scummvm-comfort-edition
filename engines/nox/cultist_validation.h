/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_CULTIST_VALIDATION_H
#define NOX_CULTIST_VALIDATION_H
namespace Nox {
bool Runtime::campaignFixtureCultistFinalBlow() {
    if(!_battleValidation||!inBattle()||campaignMapId()!=98||!battlePlayerTurn()||!keys.empty()||key_latch||(pc!=0xd53f&&pc!=0xfde9)||campaignCultistDefeated()||campaignLogCount(156)||campaignFinalEnemyHealth()!=471||campaignCultistTargetHealth()!=471||campaignPartyHealth()!=125||campaignMana()!=110||!campaignStaffRange())return false;
    const byte death[]={0xa9,0,0x8d,0x81,6,0x8d,0x82,6,0xa9,255,0x8d,0x7f,6};if(memcmp(mainram+0x1ae3,death,sizeof(death)))return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> combat;
    if(!image.open(stream)||!image.read("NA/SRTN.COMBAT.S",combat)||combat.size()<0x343c||READ_LE_UINT16(combat.data()+0x33fe)!=471||combat[0x340c]!=170||READ_LE_UINT16(combat.data()+0x2d9e)!=267||combat[0x2dac]!=169||READ_LE_UINT16(combat.data()+0x341e)!=471||combat[0x342c]!=168)return false;
    const uint count=campaignEnemyCount();if(!count||count>16)return false;uint target=16;
    for(uint i=0;i<count;++i){uint cells=0,slot=i*8;const byte sprite=mainram[0xbf02+slot];if((sprite!=168&&sprite!=169&&sprite!=170)||READ_LE_UINT16(auxram+0x1902+i*32)!=(sprite==169?267:471)||mainram[0xbf03+slot]==255)return false;
        if(sprite==170&&target==16)target=i;
        for(uint cell=0;cell<187;++cell)if(mainram[0xa31+cell]==slot&&mainram[0xaec+cell]==1)++cells;if(cells!=1)return false;}
    if(target==16)return false;
    // CONTROLLED endpoint before tested A/Return: source-valid defeated
    // other records/cache/occupancy and living actual High CultistHP1. Not earned
    // kills/damage. Encounter count, defenses, quest/reward outcomes retained.
    for(uint i=0;i<count;++i){if(i==target)continue;uint slot=i*8;auxram[0x1900+i*32]=255;WRITE_LE_UINT16(auxram+0x1902+i*32,0);mainram[0xbf03+slot]=255;
        for(uint cell=0;cell<187;++cell)if(mainram[0xa31+cell]==slot&&mainram[0xaec+cell]==1){mainram[0xa31+cell]=255;mainram[0xaec+cell]=255;}}
    WRITE_LE_UINT16(auxram+0x1902+target*32,1);return true;
}
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool NoxEngine::validateCultist(bool cold,Common::String &error) {
    Common::String trace;
    auto report=[&](const char *name,const Common::String &text){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;};
    auto frame=[&](const char *name){_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;bool ok=Image::writePNG(*out,_surface);out->finalize();ok&=!out->err();delete out;return ok;};
    auto fail=[&](const Common::String &why){error=why;trace+=why+Common::String::format("\nPC=%04X world=%u snapshot=%u\n",_runtime->address(),_runtime->atWorldInput(),_runtime->canSnapshot())+_runtime->campaignMachineDiagnostic()+"\n"+_runtime->campaignConversationDiagnostic()+"\n"+_runtime->battleDiagnostic()+"\n"+_runtime->campaignSpellDiagnostic();report("nox-cultist-failure.tsv",trace);Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-cultist-failure-memory.bin",false);if(out){_runtime->campaignDiagnosticMemory(*out);out->finalize();delete out;}frame("nox-cultist-failure.png");return false;};
    const bool checkOnly=ConfMan.hasKey("nox_cultist_check_only")&&ConfMan.getBool("nox_cultist_check_only"),replay=ConfMan.hasKey("nox_cultist_replay")&&ConfMan.getBool("nox_cultist_replay");
    if(!cold||_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Requires isolated offline retained Temple checkpoint");
    const Common::String label=checkOnly?"High Cultist result":"Vortex phase 36";int handle=-1;for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))if(entry.getDescription()==label)handle=entry.getSaveSlot();
    if(handle<0)return fail("Actual Temple predecessor missing");const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
    if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Cultist predecessor restore failed");Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);
    if(!_runtime->saveState(restored)||restored.size()!=uint(payload.size()-entry.offset)||memcmp(restored.getData(),payload.data()+entry.offset,restored.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=entry.metadata.session)return fail("Cultist exact runtime/session restore differs");
    if(checkOnly){if(!_runtime->atWorldInput()||_runtime->campaignMapId()!=58||!_runtime->campaignCultistDefeated()||!_runtime->campaignJournalTranslated()||_runtime->campaignLogCount(156)!=1)return fail("Cultist cold actual reward/automatic translation differs");return report("nox-cultist-cold.tsv","PASS\truntime_bytes=identical journal156=1 043840=1 actual_teleport58_translation043B02=1\n");}
    if(_runtime->campaignMapId()!=98||_runtime->positionX()!=168||_runtime->positionY()!=157||_runtime->campaignPartyHealth()!=125||_runtime->campaignCultistDefeated()||_runtime->campaignLogCount(156)||!_runtime->campaignStaffRange())return fail("Actual fire crossing/earned retained Jewel predecessor differs");
    Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_cultist_final_blow");_runtime->enableDialogueValidationPolling();_runtime->enableSpellValidationObservation();_runtime->enableBattleValidation();
    _runtime->key(11);bool tactical=false,prepared=false,attacked=false,returned=false;uint prompts=0;
    for(uint i=0;i<24000&&!shouldQuit();++i){
        if(_runtime->atWorldInput()&&_runtime->canSnapshot()){returned=true;break;}
        if(prepared&&!_runtime->inBattle()&&_runtime->canSnapshot()){
            trace+="post-dialogue-world-settle\t"+_runtime->campaignMachineDiagnostic()+"\t"+_runtime->campaignConversationDiagnostic()+"\n";
            _runtime->enableBattleValidation(false);
            if(!_runtime->advance(100000,true))return fail("Original post-dialogue world settling failed");_runtime->audio().clear();continue;
        }
        // Keep pure keyboard-boundary observation through original victory,
        // automatic teleport and multi-page Archus translation. No state skip.
        if(_runtime->battleInputReady()){
            if(prepared&&!_runtime->inBattle())trace+=Common::String::format("postbattle-input\tPC=%04X world=%u snapshot=%u\t",_runtime->address(),_runtime->atWorldInput(),_runtime->canSnapshot())+_runtime->campaignMachineDiagnostic()+"\t"+_runtime->campaignConversationDiagnostic()+"\n";
            if(++prompts>256)return fail("Cultist input bound exceeded");const uint pc=_runtime->address();
            if((pc==0xd53f||pc==0xfde9)&&_runtime->inBattle()&&_runtime->battlePlayerTurn()){
                if(!prepared){trace+="original-full-admission\t"+_runtime->battleDiagnostic()+"\n";if(!_runtime->campaignFixtureCultistFinalBlow())return fail("Source-valid controlled Cultist endpoint rejected");prepared=true;trace+="CONTROLLED-before-actual-attack\tfirst_High_CultistHP1 others_dead_not_earned\t"+_runtime->battleDiagnostic()+"\n";}
                if(!_runtime->campaignCultistTargetHealth())_runtime->key(27);else if(_runtime->battleTargeting())_runtime->key(13);else{_runtime->key('A');attacked=true;}
            }else if(pc==0x1324&&!tactical&&_runtime->battleAdmissionSeen()){_runtime->key('T');tactical=true;}
            else if(pc==0x9dd1&&prepared&&!_runtime->campaignCultistTargetHealth())_runtime->key('L');
            else if(prepared&&!_runtime->inBattle()&&pc==0xb9d4)_runtime->key(27);else _runtime->key(13);
        }
        if(!(i%128))trace+=_runtime->battleDiagnostic()+"\n";if(!_runtime->advance(100000,true))return fail("Actual Cultist opening/battle advance failed");_runtime->audio().clear();
    }
    trace+=_runtime->battleDiagnostic()+"\n";report(replay?"nox-cultist-replay-trace.tsv":"nox-cultist-trace.tsv",trace);frame("nox-cultist-result.png");
    if(!returned||!prepared||!attacked||!_runtime->campaignPartyHealth()||_runtime->campaignMapId()!=58||!_runtime->campaignCultistDefeated()||!_runtime->campaignJournalTranslated()||_runtime->campaignLogCount(156)!=1)return fail("Actual Cultist victory/journal/automatic teleport/translation/world return absent");
    Common::MemoryWriteStreamDynamic world(DisposeAfterUse::YES);if(!_runtime->saveState(world))return fail("Cultist world serialization failed");Common::OutSaveFile *out=getSaveFileManager()->openForSaving(replay?"nox-cultist-world-replay.bin":"nox-cultist-world.bin",false);if(!out)return fail("Cultist result open failed");out->write(world.getData(),world.size());out->finalize();bool ok=!out->err();delete out;if(!ok)return fail("Cultist result write failed");
    if(saveGameState(1,"High Cultist result").getCode()!=Common::kNoError||saveGameState(1,"High Cultist result").getCode()!=Common::kNoError)return fail("Cultist legal result save failed");
    return report(replay?"nox-cultist-replay.tsv":"nox-cultist.tsv","PASS\tactual_Up_opening_battle_attack_victory_loot=1 journal156=1 043840=1 actual_teleport58_translation043B02=1 controlled_endpoint_not_earned_damage=1\n");
}
}
#endif
