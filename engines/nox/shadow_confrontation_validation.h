/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_SHADOW_CONFRONTATION_VALIDATION_H
#define NOX_SHADOW_CONFRONTATION_VALIDATION_H
namespace Nox {
bool Runtime::campaignFixtureShadowAttackSeed(uint32 seed) {
    if(!_battleValidation||!campaignSourceShadowPresent()||!battlePlayerTurn()||!keys.empty()||key_latch||(pc!=0xd53f&&pc!=0xfde9))return false;
    // CONTROLLED deterministic PRNG state before A, never a hit/damage result.
    // Original LWR.MAIN.RTNS1089..11BA accepts a four-byte integer state at
    //6307..630A and increments/mixes it; zero is valid, not a stuck state.
    const byte signature[]={0x18,0xad,7,0x63,0x8d,2,0x63,0x69,1};if(memcmp(mainram+0x10e5,signature,sizeof(signature)))return false;
    WRITE_LE_UINT32(mainram+0x6307,seed);return true;
}
bool Runtime::campaignSourceShadowPresent(bool finalPlane) {
    if(!inBattle()||campaignMapId()!=(finalPlane?8:0)||campaignBattlePrimarySprite()!=133||campaignFinalEnemyHealth()!=3210)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> combat;
    return image.open(stream)&&image.read("NA/SRTN.COMBAT.S",combat)&&combat.size()>=0x363c&&READ_LE_UINT16(combat.data()+0x361e)==3210&&combat[0x362c]==133&&!memcmp(auxram+0x1900,combat.data()+0x361c,32);
}
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool NoxEngine::validateShadowConfrontation(bool cold,Common::String &error) {
    Common::String trace;
    auto report=[&](const char *name,const Common::String &text){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;};
    auto frame=[&](const char *name){_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;bool ok=Image::writePNG(*out,_surface);out->finalize();ok&=!out->err();delete out;return ok;};
    auto fail=[&](const Common::String &why){error=why;trace+=why+Common::String::format("\nPC=%04X map=%u xy=%u,%u flags43B=%02X\n",_runtime->address(),_runtime->campaignMapId(),_runtime->positionX(),_runtime->positionY(),_runtime->campaignFlagByte(0x43b))+_runtime->campaignMachineDiagnostic()+"\n"+_runtime->battleDiagnostic();report("nox-shadow-confrontation-failure.tsv",trace);Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-shadow-confrontation-failure-memory.bin",false);if(out){_runtime->campaignDiagnosticMemory(*out);out->finalize();delete out;}if(_runtime->canSnapshot()){saveGameState(3,"Shadow walk diagnostic, not acceptance");saveGameState(3,"Shadow walk diagnostic, not acceptance");}frame("nox-shadow-confrontation-failure.png");return false;};
    const bool finalPlane=ConfMan.hasKey("nox_shadow_final_plane")&&ConfMan.getBool("nox_shadow_final_plane");
    const bool replay=ConfMan.hasKey("nox_shadow_confrontation_replay")&&ConfMan.getBool("nox_shadow_confrontation_replay"),passCase=ConfMan.hasKey("nox_shadow_confrontation_pass")&&ConfMan.getBool("nox_shadow_confrontation_pass");
    if(!cold||_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Requires isolated actual automatic-checkpoint predecessor");
    if(finalPlane&&!passCase)return fail("Final-plane negative requires deliberate pass");
    const Common::String label=replay?"Shadow confrontation start":finalPlane?"Vortex phase 5":"Shadow automatic checkpoint event";int handle=-1;for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))if(entry.getDescription()==label)handle=entry.getSaveSlot();
    if(handle<0)return fail("Actual Shadow sector predecessor missing");const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
    if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Shadow confrontation restore failed");Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);
    if(!_runtime->saveState(restored)||restored.size()!=uint(payload.size()-entry.offset)||memcmp(restored.getData(),payload.data()+entry.offset,restored.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=entry.metadata.session)return fail("Shadow confrontation exact runtime/session differs");
    if(!_runtime->atWorldInput()||_runtime->campaignMapId()!=(finalPlane?8:0)||_runtime->positionX()!=(finalPlane?(replay?121:81):152)||_runtime->positionY()!=(finalPlane?(replay?131:168):175)||(_runtime->campaignFlagByte(0x43b)&7)!=7||_runtime->campaignPartyHealth()!=125||!_runtime->campaignStaffRange())return fail("Actual sector checkpoint/Ready predecessor differs");
    Common::CEAchievements().noteDisqualifyingAssistance("nox_shadow_confrontation_controlled_predecessor");_runtime->enableDialogueValidationPolling();_runtime->enableSpellValidationObservation();
    if(finalPlane&&!replay){trace+="CONTROLLED-final-approach-before-real-Down\tmap8 121,131; no enemy, damage, quest or ending writes\n";if(!_runtime->campaignFixtureFinalApproach(false))return fail("Final-plane authored approach rejected");for(uint settle=0;settle<2000&&!_runtime->atWorldInput();++settle)if(!_runtime->advance(100000,true))return fail("Final approach advance failed");if(!_runtime->atWorldInput())return fail("Final approach input not reached");}
    if(!replay&&(saveGameState(2,"Shadow confrontation start").getCode()!=Common::kNoError||saveGameState(2,"Shadow confrontation start").getCode()!=Common::kNoError))return fail("Legal confrontation input save failed");
    const Common::String walk=ConfMan.hasKey("nox_shadow_walk")?ConfMan.get("nox_shadow_walk"):Common::String("UUUUUUULL");if(walk.empty()||walk.size()>64)return fail("Invalid bounded source pass walk");for(uint i=0;i<walk.size();++i)if(walk[i]!='U'&&walk[i]!='D'&&walk[i]!='L'&&walk[i]!='R'&&walk[i]!='.')return fail("Invalid source pass input");
    uint step=0,prompts=0,passes=0;bool tactical=false,admitted=false,attacked=false,confirmed=false,refused=false,playerWeaponSeen=false;
    for(uint i=0;i<24000&&!shouldQuit();++i){
        if(_runtime->inBattle())_runtime->enableBattleValidation();
        if(admitted)playerWeaponSeen|=_runtime->battleDiagnostic().contains("weapon[side0");
        if(admitted&&!_runtime->campaignPartyHealth()&&_runtime->campaignAtDefeatInput()&&(passCase?(finalPlane?passes>0:passes==1):attacked&&confirmed)){if(!passCase&&!playerWeaponSeen)return fail("Actual attack did not enter damage resolution; inspect original hit roll");refused=true;break;}
        if(_runtime->atWorldInput()&&_runtime->canSnapshot()){
            trace+=Common::String::format("actual-pass-walk\tstep=%u map=%u xy=%u,%u HP=%u flags43B=%02X\n",step,_runtime->campaignMapId(),_runtime->positionX(),_runtime->positionY(),_runtime->campaignPartyHealth(),_runtime->campaignFlagByte(0x43b));
            if(admitted)return fail("Original battle returned to world; inspect actual scripted outcome");if(step>=walk.size())return fail("Bounded source pass walk did not encounter Shadow; investigate source state");
            const char key=walk[step++];_runtime->key(key=='U'?11:key=='D'?10:key=='L'?8:key=='R'?21:' ');
        }
        if(_runtime->battleInputReady()){
            trace+=Common::String::format("actual-battle-input\tPC=%04X attacked=%u confirmed=%u playerWeaponSeen=%u passes=%u\t",_runtime->address(),attacked,confirmed,playerWeaponSeen,passes)+_runtime->battleDiagnostic()+"\n";
            if(++prompts>128)return fail("Shadow battle input bound exceeded");const uint pc=_runtime->address();
            if((pc==0xd53f||pc==0xfde9)&&_runtime->inBattle()&&_runtime->battlePlayerTurn()){
                if(!admitted){trace+="original-Shadow-admission\t"+_runtime->battleDiagnostic()+"\n";if(!_runtime->campaignSourceShadowPresent(finalPlane)||(!finalPlane&&!_runtime->campaignEnemyInvulnerable()))return fail("Observed first encounter differs from original full-health invulnerable Shadow record");admitted=true;frame("nox-first-shadow-admission.png");}
                if(!passCase&&attacked&&confirmed&&playerWeaponSeen&&!_runtime->battleTargeting()){refused=true;break;}
                if(!passCase&&!attacked){const uint32 seed=ConfMan.hasKey("nox_shadow_attack_seed")?uint32(ConfMan.getInt("nox_shadow_attack_seed")):0;if(!_runtime->campaignFixtureShadowAttackSeed(seed))return fail("Source-supported controlled pre-attack RNG seed rejected");trace+=Common::String::format("CONTROLLED-before-attack-RNG-seed\t%u no position/HP/enemy/quest changes\n",seed);}
                if(passCase){if(++passes>(finalPlane?16u:1u))return fail("Source Shadow exceeded bounded deliberate passes");_runtime->key(' ');}
                else if(_runtime->battleTargeting()){_runtime->key(13);confirmed=true;}else{_runtime->key('A');attacked=true;}
            }else if(pc==0x1324&&!tactical&&_runtime->battleAdmissionSeen()){_runtime->key('T');tactical=true;}else if(_runtime->battleTargeting()){_runtime->key(13);confirmed=true;}else _runtime->key(13);
        }
        if(!(i%128))trace+=_runtime->battleDiagnostic()+"\n";if(!_runtime->advance(100000,true))return fail("Actual pass walk/Shadow attack advance failed: "+_runtime->error());_runtime->audio().clear();
    }
    if(!refused||!admitted||_runtime->campaignFinalEnemyHealth()!=3210||(!finalPlane&&!_runtime->campaignEnemyInvulnerable())||_runtime->atEndingInput())return fail("Actual original Shadow result not demonstrated");
    if(passCase&&!finalPlane){if(passes!=1||attacked||playerWeaponSeen||_runtime->campaignEnemyMana()!=128||_runtime->campaignBattleSpell()!=12||!_runtime->campaignAtDefeatInput())return fail("Actual enemy Vortex87MP/125HP defeat/presentation differs");}
    else if(!passCase&&(!attacked||!confirmed||!playerWeaponSeen))return fail("Actual ranged attack/refusal absent");
    Common::OutSaveFile *out=getSaveFileManager()->openForSaving(replay?"nox-first-shadow-refusal-replay.bin":"nox-first-shadow-refusal.bin",false);if(!out)return fail("Shadow refusal diagnostic open failed");bool ok=_runtime->campaignDiagnosticSnapshot(*out);out->finalize();ok&=!out->err();delete out;if(!ok)return fail("Shadow refusal raw diagnostic failed");
    if(finalPlane)trace+="actual-final-plane-defeat-no-ending\n";
    trace+=_runtime->battleDiagnostic()+"\n";report(replay?"nox-first-shadow-replay-trace.tsv":"nox-first-shadow-trace.tsv",trace);frame("nox-first-shadow-refusal.png");
    return report(replay?"nox-first-shadow-replay.tsv":"nox-first-shadow.tsv",Common::String::format("PASS\tactual_pass_walk_admission=1 ranged_refusal=%u deliberate_pass_enemy_defeat_presentation=%u original_HP3210_unchanged=1 source_record_verified=1 no_enemy_HP_or_outcome_control=1 raw_diagnostic_not_combat_save=1\n",!passCase,passCase));
}
}
#endif
