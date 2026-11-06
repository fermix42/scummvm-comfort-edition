/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_SPELL_VALIDATION_H
#define NOX_SPELL_VALIDATION_H
namespace Nox {
bool Runtime::campaignRetainedStaffValid() {
    if(!atWorldInput()||!canSnapshot()||!campaignStaffRange()||auxram[0x1612]!=110||auxram[0x161c]!=64||auxram[0x162c]||campaignMana()!=110)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> inventory;
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",inventory)||inventory.size()<0x1000)return false;
    for(uint at=0xa00;at+6<=0x1000;at+=6)if((inventory[at]&15)==0&&inventory[at+1]==71&&inventory[at+2]==1&&inventory[at+3]==4)return true;return false;
}
bool Runtime::campaignFixtureMage() {
    if(!atWorldInput()||!canSnapshot()||mainram[0x70a3]!=1||auxram[0x1600]||auxram[0x1645]||auxram[0x1601]!=1||READ_LE_UINT16(auxram+0x1648)!=50||campaignLogCount(12)!=1)return false;
    Common::MemoryReadStream image(disk.data(),disk.size());HdvArchive archive;Common::Array<byte> combat,inventory;
    if(!archive.open(image)||!archive.read("NA/SRTN.COMBAT.S",combat)||!archive.read("NA/SRTN.INVENTORY",inventory)||combat.size()<0x4430||inventory.size()<0x2da0)return false;
    if(READ_LE_UINT16(combat.data()+0x442c)!=32000||READ_LE_UINT16(combat.data()+0x442e)!=65535||inventory[0x2c00+12*32+4]!=110)return false;
    // Controlled reachable level10 profile: nine twelve-point allocations,
    // STR8, DEX14, INT110; XP32000/max-level sentinel. Manual appendix p105
    // gives HP125 for STR8 at level10; maximum MP equals INT (manual p55).
    // Source combatResults adds six to record2B..30 after each level; retain
    // untrained skill triplets and zero remaining prayer minutes.
    for(uint i=0x2b;i<=0x30;++i)if(auxram[0x1600+i])return false;
    auxram[0x1601]=10;WRITE_LE_UINT16(auxram+0x1602,125);auxram[0x1604]=110;
    WRITE_LE_UINT16(auxram+0x1605,32000);auxram[0x1610]=8;auxram[0x1611]=14;auxram[0x1612]=110;
    WRITE_LE_UINT16(auxram+0x1643,65535);auxram[0x1647]=0;WRITE_LE_UINT16(auxram+0x1648,125);
    for(uint i=0x2b;i<=0x30;++i)auxram[0x1600+i]=54;
    mainram[0x2bff]=1;return true;
}
bool Runtime::campaignFixtureStaff() {
    if(!atWorldInput()||!canSnapshot()||auxram[0x1612]!=110||auxram[0x1610]!=8||auxram[0x162c]!=54||uint(auxram[0x161c])+54>255)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> inventory;
    if(!image.open(stream)||!image.read("NA/SRTN.INVENTORY",inventory)||inventory.size()<0x1900)return false;
    const byte *definition=inventory.data()+0x1000+71*32;
    const byte name[]={0xc4,0xf2,0xe1,0xeb,0xe5,0xa0,0xd3,0xf4,0xe1,0xe6,0xe6,0};
    if(definition[0]!=128||definition[2]!=3||definition[4]!=104||definition[9]!=15||memcmp(definition+13,name,sizeof(name)))return false;
    uint slot=0;for(uint at=0xa00;at+6<=0x1000;at+=6){if((inventory[at]&15)==0&&inventory[at+1]==71)return false;if(!slot&&inventory[at]==254)slot=at;}
    if(!slot)return false;for(uint i=0;i<6;++i)if(inventory[slot+i]!=254)return false;
    const HdvArchive::Entry *entry=image.entry("NA/SRTN.INVENTORY");if(!entry||entry->storage!=2)return false;
    // Controlled owned but UNREADIED two-handed weapon. The real Ready UI
    // must choose its wearer, update ownership, and recalculate derived stats.
    const byte record[]={0xa0,71,1,0,0,1};Common::Array<byte> prepared=disk;
    for(uint i=0;i<6;++i){const uint logical=slot+i,index=logical/512,table=uint(entry->key)*512;if(index>=256||table+512>prepared.size())return false;const uint block=prepared[table+index]|uint(prepared[table+256+index])<<8;if(!block||uint64(block)*512+logical%512>=prepared.size())return false;prepared[block*512+logical%512]=record[i];}
    // Source questTraining973F/974B: spend the accumulated54 Range lessons
    // as whole-point increases; fractional practice/threshold stay unchanged.
    // Explicit controlled training, not an earned trainer transaction.
    disk=prepared;auxram[0x161c]+=54;auxram[0x162c]=0;mainram[0x2bff]=1;return true;
}
bool Runtime::campaignFixtureFinalApproach(bool small) {
    if(!atWorldInput()||!canSnapshot()||campaignReadiedSpell()!=12||campaignMana()!=110)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> surface;
    if(!image.open(stream))return false;
    if(small){
        if(!image.read("NA/DATA.MAP.KEEP",surface))return false;
        uint tile=0;for(uint at=1;at+23<surface.size()&&surface[at]!=255;at+=surface[at]){if(surface[at]<23)return false;if(surface[at+1]==7){tile=surface[at+2]|uint(surface[at+3])<<8|uint(surface[at+4])<<16;break;}}
        const uint square=((35/16)*3+25/16)*256+(35%16)*16+25%16;
        if(!tile||tile+2304>surface.size()||surface[tile+square]!=83)return false;
        mainram[0x267d]=8;mainram[0x2af9]=7;mainram[0x6cec]=mainram[0x23f8]=25;mainram[0x6ced]=mainram[0x23f9]=35;
    }else{
        if(mainram[0x2af9]!=8||(campaignTransportFlags()&0x38)!=0x38||campaignLogCount(157)!=1)return false;
        if(!image.read("NA/DATA.MAP.SURF",surface)||surface.size()<0x4b4d)return false;
        const byte expected[]={121,132,8,0};if(memcmp(surface.data()+0x4b49,expected,4))return false;
        mainram[0x267d]=0;mainram[0x2af9]=8;mainram[0x6cec]=mainram[0x23f8]=121;mainram[0x6ced]=mainram[0x23f9]=131;
    }
    push16(0x7127);push16(0x7127);push16(0xf64f);push16(0x167b);pc=0x8f1b;
    key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool Runtime::campaignFixtureWoundedShadow(bool wound,bool melee) {
    if(!inBattle()||!battlePlayerTurn()||_vortexEffects||mainram[0x2efb]!=1||mainram[0xbf02]!=133||mainram[0xbf03]==255||campaignMana()!=110)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> combat;
    if(!image.open(stream)||!image.read("NA/SRTN.COMBAT.S",combat)||combat.size()<0x363c||combat[0x361d]!=10||READ_LE_UINT16(combat.data()+0x361e)!=3210||combat[0x362c]!=133||READ_LE_UINT16(auxram+0x1902)!=3210)return false;
    // Controlled living final-blow precondition, BEFORE C/Cast. Not earned
    // damage and not a dead/victory flag; all original defenses retained.
    const uint slot=mainram[0x620];const int cell=(int(mainram[0xbd01+slot])-int(mainram[0x6ced])+5)*17+int(mainram[0xbd00+slot])-int(mainram[0x6cec])+8;
    if(cell<0||cell>=187||mainram[0xa31+cell]!=slot||mainram[0xaec+cell]!=3)return false;
    if(wound&&melee){WRITE_LE_UINT16(auxram+0x1902,1);return true;}
    if(wound){
        // Controlled pre-cast position on adjacent empty floor, retaining
        // original actor record and both canonical occupancy grids. Not earned
        // movement or damage; no cast/effect/victory transition has begun.
        const int next=melee?cell-33:cell+1;
        if(mainram[0xbd00+slot]!=4||mainram[0xbd01+slot]!=8||mainram[0xbf00]!=5||mainram[0xbf01]!=4||next>=187||mainram[0xa31+next]!=255||mainram[0xaec+next]!=255||mainram[0x800+next]!=64||mainram[0x8bb+next]!=0||mainram[0x606]!=cell||mainram[0x2af8]!=cell)return false;
        mainram[0xa31+cell]=255;mainram[0xaec+cell]=255;
        mainram[0xbd00+slot]=5;mainram[0xa31+next]=slot;mainram[0xaec+next]=3;
        mainram[0xbd01+slot]=melee?6:8;mainram[0x644]=5;mainram[0x645]=melee?6:8;mainram[0x606]=mainram[0x2af8]=byte(next);
        WRITE_LE_UINT16(auxram+0x1902,1);
    }return true;
}
bool Runtime::campaignFixtureGuardianRecovery() {
    if(!atWorldInput()||!canSnapshot()||!(((campaignMapId()==101||campaignMapId()==99)&&mainram[0x267d]==2)||(campaignMapId()==74&&mainram[0x267d]==6))||campaignMana()!=23||!campaignPartyHealth()||campaignReadiedSpell()!=12||mainram[0x70a3]!=1||auxram[0x1601]!=10||auxram[0x1610]!=8||auxram[0x1611]!=14||auxram[0x1612]!=110||READ_LE_UINT16(auxram+0x1648)!=125)return false;
    // Explicit controlled between-battle recovery, before the next operation.
    // No inventory, quest, actor, enemy, reward or map field is changed.
    WRITE_LE_UINT16(auxram+0x1602,125);auxram[0x1604]=110;return true;
}
bool Runtime::campaignFixtureGuardianApproach(uint guardian) {
    if(guardian<1||guardian>3||!atWorldInput()||!canSnapshot()||mainram[0x2af9]!=101||mainram[0x267d]!=2||campaignMana()!=110||campaignReadiedSpell()!=12)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> data,combat;
    const uint actor=(guardian-1)*16,column=guardian==1?86:108,row=guardian==1?126:guardian==2?100:148,sprite=guardian==1?234:guardian==2?177:163;
    const byte definition[]={224,57,0,byte(1u<<(guardian-1)),0,2,0,byte(150+guardian),0,byte(guardian==1?63:guardian==2?61:12),0,byte(guardian==1?1:guardian==2?64:4)};
    const uint definitionAt=49450+actor,sourceActor=49962+actor,monster=guardian==1?0x327c:guardian==2?0x325c:0x329c;
    if(!image.open(stream)||!image.read("NA/DATA.MAP.UM",data)||!image.read("NA/SRTN.COMBAT.S",combat)||data.size()<sourceActor+8||combat.size()<monster+32||memcmp(data.data()+definitionAt,definition,12)||data[sourceActor]!=column||data[sourceActor+1]!=row||data[sourceActor+2]!=sprite||data[sourceActor+5]!=126||combat[monster+16]!=sprite||(combat[monster+6]&127)!=105)return false;
    if(mainram[0xbf00+actor]!=column||mainram[0xbf01+actor]!=row||mainram[0xbf02+actor]!=sprite||mainram[0xbf05+actor]!=126||memcmp(mainram+0xbd00+actor,definition,12))return false;
    // Original UM101 compressed sectors decode the south adjacency to floor76.
    for(uint i=0;i<6;i+=2)if(118>=mainram[0x781+i]&&118<mainram[0x782+i])return false;
    for(uint i=0;i<256;i+=8)if(mainram[0xbf00+i]==column&&mainram[0xbf01+i]==row+1&&mainram[0xbf02+i]&&mainram[0xbf03+i]!=255)return false;
    mainram[0x6cec]=mainram[0x23f8]=column;mainram[0x6ced]=mainram[0x23f9]=row+1;
    push16(0x7127);push16(0x7127);push16(0xf64f);push16(0x167b);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool NoxEngine::validateSpellReady(bool cold,Common::String &error) {
    Common::String trace;
    auto report=[&](const Common::String &name,const Common::String &body){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(body.c_str(),body.size());out->finalize();bool ok=!out->err();delete out;return ok;};
    auto frame=[&](const Common::String &name){_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;bool ok=Image::writePNG(*out,_surface);out->finalize();ok&=!out->err();delete out;return ok;};
    auto state=[&](const char *label){trace+=Common::String::format("%s\t%04X\t%s\t%s\n",label,_runtime->address(),_runtime->campaignSpellDiagnostic().c_str(),_runtime->campaignMachineDiagnostic().c_str());};
    auto fail=[&](const Common::String &why){Common::OutSaveFile *memory=getSaveFileManager()->openForSaving("nox-spell-failure-memory.bin",false);if(memory){_runtime->campaignDiagnosticMemory(*memory);memory->finalize();delete memory;}error=why;state("failure");trace+=_runtime->battleDiagnostic()+"\n";report("nox-spell-failure.tsv",why+"\n"+trace);frame("nox-spell-failure.png");return false;};
    const uint guardian=ConfMan.hasKey("nox_guardian_validation")?ConfMan.getInt("nox_guardian_validation"):0;
    const bool guardianReplay=ConfMan.hasKey("nox_guardian_replay")&&ConfMan.getBool("nox_guardian_replay");
    const bool guardianMissing=ConfMan.hasKey("nox_guardian_missing")&&ConfMan.getBool("nox_guardian_missing");
    const uint guardianPrefix=ConfMan.hasKey("nox_guardian_predecessor")?ConfMan.getInt("nox_guardian_predecessor"):0;
    if(guardianPrefix>3||(guardianPrefix&&(!guardian||guardianMissing||guardianPrefix==guardian)))return fail("Invalid linked guardian predecessor");
    if(guardian>3)return fail("Invalid guardian");
    const bool ackCold=ConfMan.hasKey("nox_ending_ack_cold")&&ConfMan.getBool("nox_ending_ack_cold");
    const bool melee=ConfMan.hasKey("nox_ending_melee_validation")&&ConfMan.getBool("nox_ending_melee_validation");
    const bool transportPrefix=ConfMan.hasKey("nox_spell_transport_predecessor")&&ConfMan.getBool("nox_spell_transport_predecessor");
    const bool smallEncounter=ConfMan.hasKey("nox_spell_small_encounter")&&ConfMan.getBool("nox_spell_small_encounter");
    const bool approachOnly=ConfMan.hasKey("nox_spell_approach_check_only")&&ConfMan.getBool("nox_spell_approach_check_only");
    const bool fullBoss=ConfMan.hasKey("nox_spell_full_health_boss")&&ConfMan.getBool("nox_spell_full_health_boss");
    const bool battleOnly=ConfMan.hasKey("nox_spell_battle_validation")&&ConfMan.getBool("nox_spell_battle_validation");
    const bool castOnly=ConfMan.hasKey("nox_spell_cast_validation")&&ConfMan.getBool("nox_spell_cast_validation");
    const bool checkOnly=ConfMan.hasKey("nox_spell_ready_check_only")&&ConfMan.getBool("nox_spell_ready_check_only");
    if(!cold||_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Requires isolated offline restored spell checkpoint");
    const Common::String label=guardian?((approachOnly||guardianReplay)?Common::String::format("Guardian %u approach",guardian):checkOnly?Common::String::format("Guardian %u result",guardian):guardianPrefix?Common::String::format("Guardian %u result",guardianPrefix):guardianMissing?(guardian==1?Common::String("Vortex phase 14"):Common::String("Vortex world cast")):Common::String(guardian==3?"Vortex phase 26":"Vortex phase 16")):battleOnly?(approachOnly?(guardian?Common::String::format("Guardian %u approach",guardian):smallEncounter?"Vortex battle approach controlled":"Vortex final approach controlled"):checkOnly?(melee?(ackCold?"Ending acknowledged":"Final ending presentation"):"Vortex battle effect"):transportPrefix?"Vortex phase 5":"Vortex world cast"):castOnly?(checkOnly?"Vortex world cast":"Vortex spell ready"):(checkOnly?Common::String("Vortex spell ready"):ConfMan.hasKey("nox_spell_ready_predecessor")?ConfMan.get("nox_spell_ready_predecessor"):Common::String("Vortex phase 3"));int handle=-1;
    for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))if(entry.getDescription()==label)handle=entry.getSaveSlot();
    if(handle<0)return fail("Missing spell predecessor");
    const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
    if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Spell predecessor load failed");
    Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);
    if(!_runtime->saveState(restored)||restored.size()!=uint(payload.size()-entry.offset)||memcmp(restored.getData(),payload.data()+entry.offset,restored.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=entry.metadata.session)return fail("Spell exact predecessor restoration differs");
    if(_runtime->campaignPageFlags()!=255||(_runtime->campaignResearchFlags()&3)!=3||_runtime->campaignLogCount(12)!=1)return fail("Earned binding outcome missing");
    if(checkOnly&&approachOnly){if(!_runtime->atWorldInput()||_runtime->campaignMapId()!=(guardian?101:smallEncounter?7:8)||_runtime->positionX()!=(guardian?(guardian==1?86:108):smallEncounter?25:121)||_runtime->positionY()!=(guardian?(guardian==1?127:guardian==2?101:149):smallEncounter?35:131)||_runtime->campaignMana()!=110)return fail("Pre-ending cold fields differ");frame(smallEncounter?"nox-vortex-rat-approach-cold.png":"nox-vortex-final-approach-cold.png");return report("nox-spell-approach-cold.tsv",Common::String::format("PASS\truntime_bytes=identical\tcontrolled_approach_map=%u\tMP=110\n",_runtime->campaignMapId()));}
    if(checkOnly){if(battleOnly&&!((smallEncounter||guardian)?_runtime->atWorldInput():_runtime->atEndingInput()))return fail("Cold ending boundary differs");if(_runtime->campaignReadiedSpell()!=12||_runtime->campaignReadiedIntelligence()!=110||(battleOnly&&!melee?_runtime->campaignMana()>=110:_runtime->campaignMana()!=110))return fail("Cold readied spell fields differ");if(melee){
            if(!frame(ackCold?"nox-ending-ack-cold.png":"nox-ending-cold.png"))return fail("Ending cold frame failed");
            if(!ackCold){
                _runtime->enableDialogueValidationPolling();_runtime->key(13);bool returned=false;
                for(uint i=0;i<12000&&!shouldQuit();++i){if(!_runtime->advance(100000,true))return fail("Ending acknowledgement advance failed");_runtime->audio().clear();if(_runtime->atEndingInput()&&_runtime->canSnapshot()){returned=true;break;}}
                if(!returned||!frame("nox-ending-ack.png"))return fail("Original ending acknowledgement loop did not return");
                if(saveGameState(1,"Ending acknowledged").getCode()!=Common::kNoError)return fail("Acknowledged ending save failed");
            }
        }
        return report(ackCold?"nox-ending-ack-cold.tsv":"nox-spell-cold.tsv",Common::String::format("PASS\truntime_bytes=identical\tspell_slot1=12\tminimum_INT=110\tMP=%u\n",_runtime->campaignMana()));}
    _runtime->enableDialogueValidationPolling();_runtime->enableSpellValidationObservation();
    Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_spell_checkpoint");
    auto advance=[&](){bool ok=_runtime->advance(100000,true);_runtime->audio().clear();return ok;};
    if(battleOnly){
        if(!guardian&&!smallEncounter&&!transportPrefix)return fail("Final approach requires actual transport predecessor");
        if(transportPrefix&&(_runtime->campaignMapId()!=8||(_runtime->campaignTransportFlags()&0x38)!=0x38||_runtime->campaignLogCount(157)!=1))return fail("Actual transport predecessor missing");
        auto settle=[&](){for(uint i=0;i<12000&&!shouldQuit();++i){if(_runtime->atWorldInput()&&_runtime->canSnapshot())return true;if(!advance())return false;}return false;};
        if(guardian&&!guardianReplay){
            if(guardianPrefix){state("before-controlled-between-guardian-recovery");if(_runtime->campaignLogCount(byte(150+guardianPrefix))!=1||!_runtime->campaignFixtureGuardianRecovery())return fail("Linked guardian controlled recovery rejected");state("after-controlled-between-guardian-recovery");}
            const bool artifact=guardian==1?bool(_runtime->campaignHairnetFlags()&1):guardian==2?bool(_runtime->campaignResearchFlags()&64):_runtime->campaignWaterCondition();
            if(artifact==guardianMissing||_runtime->campaignLogCount(byte(150+guardian)))return fail("Guardian artifact/reward predecessor differs");
            auto setupGuardianExit=[&](byte id,byte type){return _runtime->dialogueFixtureExit(id,type)&&settle();};
            if(_runtime->campaignMapId()==70&&(!setupGuardianExit(71,8)||!setupGuardianExit(0,128)||!setupGuardianExit(54,7)||!setupGuardianExit(78,7)||!setupGuardianExit(97,2)))return fail("Guardian retained Hairnet authored route failed");
            if(_runtime->campaignMapId()==104&&!setupGuardianExit(97,2))return fail("Guardian binding cave authored return failed");
            if(_runtime->campaignMapId()!=101&&(_runtime->campaignMapId()!=97||!setupGuardianExit(98,2)||!setupGuardianExit(99,2)||!setupGuardianExit(100,2)||!setupGuardianExit(101,2)))return fail("Guardian authored depths route failed");
        }
        if(melee&&ConfMan.hasKey("nox_ending_retained_staff")&&ConfMan.getBool("nox_ending_retained_staff")){
            if(!_runtime->campaignRetainedStaffValid())return fail("Actual retained source-valid equipped staff/training/range differs");state("actual-staff-readiness-retained-no-new-initialization");
        }else if(melee){
            state("before-controlled-unreadied-staff");if(!_runtime->campaignFixtureStaff())return fail("Source-valid unreadied Drake Staff fixture rejected");
            uint stage=0,prompts=0,exits=0;bool readied=false;_runtime->key(9);_runtime->key(9);
            for(uint i=0;i<12000&&!shouldQuit();++i){
                if(_runtime->atWorldInput()&&_runtime->canSnapshot()){readied=stage>=2&&_runtime->campaignStaffRange();break;}
                if(_runtime->campaignTestInputReady()&&!_runtime->atWorldInput()){
                    state("actual-weapon-ready-input");
                    if(stage==0){if(_runtime->address()!=0xa9a1||_runtime->campaignInventoryTab()!=1){if(++prompts>8)return fail("Weapons tab did not open");_runtime->key(_runtime->address()==0x9966?9:'2');}
                        else{const int row=_runtime->campaignOwnedStaffRow();if(row<0)return fail("Controlled owned staff missing from actual list");if(_runtime->campaignInventoryRow()!=row)_runtime->key(10);else{_runtime->key(13);stage=1;}}}
                    else if(stage==1){if(_runtime->campaignStaffEquipped()){stage=2;_runtime->key(27);}else{if(++prompts>12)return fail("Actual staff wearer/equip failed");_runtime->key('1');}}
                    else{if(++exits>8)return fail("Weapon inventory close failed");_runtime->key(27);}
                }
                if(!advance())return fail(_runtime->error());
            }
            if(!readied)return fail("Actual staff Ready/derived range not demonstrated");state("actual-weapon-ready-completed");frame("nox-final-staff-readied.png");
        }
        state("controlled-final-approach-before");if(!guardianReplay&&(!(guardian?_runtime->campaignFixtureGuardianApproach(guardian):_runtime->campaignFixtureFinalApproach(smallEncounter))||!settle()))return fail("Controlled authored final approach failed");
        state("controlled-final-approach-after");frame(smallEncounter?"nox-vortex-rat-approach.png":"nox-vortex-final-approach.png");
        if(!guardianReplay&&(saveGameState(2,guardian?Common::String::format("Guardian %u approach",guardian):smallEncounter?"Vortex battle approach controlled":"Vortex final approach controlled").getCode()!=Common::kNoError||saveGameState(2,guardian?Common::String::format("Guardian %u approach",guardian):smallEncounter?"Vortex battle approach controlled":"Vortex final approach controlled").getCode()!=Common::kNoError))return fail("Pre-ending checkpoint save failed");
        if(smallEncounter&&!_runtime->controlledRatPresent())return fail("Original living shipwreck Rat absent");
        _runtime->enableBattleValidation();if(smallEncounter||guardian){_runtime->key('A');_runtime->key(guardian?11:21);}else _runtime->key(10);
        bool tactical=false,cast=false,slot=false,returned=false,ending=false,guardianChecked=false,guardianRefused=false;uint prompts=0,moves=0;
        for(uint i=0;i<24000&&!shouldQuit();++i){
            if(_runtime->atWorldInput()&&_runtime->canSnapshot()){returned=true;break;}
            if((smallEncounter||guardian)&&!_runtime->inBattle()&&_runtime->vortexEffects()&&_runtime->vortexDamaged())_runtime->enableBattleValidation(false);
            if(_runtime->atEndingInput()&&_runtime->canSnapshot()){ending=true;frame("nox-vortex-ending-presentation.png");break;}
            if(!_runtime->campaignPartyHealth()){
                if(guardianMissing&&guardianChecked&&_runtime->vortexEffects()&&_runtime->campaignMana()==23&&_runtime->campaignVortexTargetSeen()&&!_runtime->vortexDamaged()&&_runtime->campaignInvulnerability()==1&&_runtime->campaignFinalEnemyHealth()==(guardian==1?2600u:guardian==2?1950u:1300u)&&!_runtime->atEndingInput()){guardianRefused=true;break;}
                return fail("Actual party defeated before completion");
            }
            if(_runtime->battleInputReady()){
                trace+=Common::String::format("battle-input\t%04X\t%s\t%s\n",_runtime->address(),_runtime->battleDiagnostic().c_str(),_runtime->campaignMachineDiagnostic().c_str());
                if(++prompts>300)return fail("Battle input limit");
                const uint pc=_runtime->address();if(melee&&pc==0xf093)frame("nox-final-weapon-result.png");
                if(_runtime->inBattle()&&_runtime->battlePlayerTurn()&&(pc==0xfde9||pc==0xd53f)){
                    if(guardian&&!guardianChecked){trace+=Common::String::format("original-guardian-admission\tid=%u invulnerability=%u HP=%u\n",guardian,_runtime->campaignInvulnerability(),_runtime->campaignFinalEnemyHealth());if(_runtime->campaignInvulnerability()!=(guardianMissing?1u:0u)||_runtime->campaignFinalEnemyHealth()!=(guardian==1?2600u:guardian==2?1950u:1300u))return fail("Original guardian admission/stat/ artifact effect differs");guardianChecked=true;}
                    if(guardianMissing&&_runtime->vortexEffects()&&_runtime->campaignMana()==23&&!_runtime->battleTargeting()&&!_runtime->vortexDirectionPending()){
                        if(_runtime->vortexDamaged()||!_runtime->campaignVortexTargetSeen()||_runtime->campaignFinalEnemyHealth()!=(guardian==1?2600u:guardian==2?1950u:1300u))return fail("Missing artifact failed original invulnerability");guardianRefused=true;break;
                    }
                    if(_runtime->vortexDirectionPending())_runtime->key(_runtime->battleDirectionKey());
                    else if((melee&&!_runtime->campaignFinalEnemyHealth())||(_runtime->vortexEffects()&&_runtime->vortexDamaged()&&_runtime->campaignMana()<110)){frame("nox-vortex-battle-effect.png");_runtime->key(27);}
                    else if(_runtime->battleTargeting())_runtime->key(melee?13:_runtime->battleTargetKey());
                    else if(!cast&&smallEncounter&&_runtime->battleAlignKey()){if(++moves>8)return fail("Actual battle alignment move limit");_runtime->key(_runtime->battleAlignKey());}
                    else if(!cast){trace+="precast-state-before\t"+_runtime->battleDiagnostic()+"\n";if(!guardian&&!smallEncounter&&!_runtime->campaignFixtureWoundedShadow(!fullBoss,melee))return fail("Valid controlled wounded Shadow setup rejected");trace+="precast-state-after\t"+_runtime->battleDiagnostic()+"\n";
                        if(smallEncounter){Common::OutSaveFile *reference=getSaveFileManager()->openForSaving("nox-battle-precast-reference.bin",false);if(!reference)return fail("Battle reference open failed");bool ok=_runtime->campaignBattleReference(*reference);reference->finalize();ok&=!reference->err();delete reference;if(!ok)return fail("Battle reference capture failed");}
                        _runtime->key(melee?'A':'C');cast=true;}
                    else{_runtime->key(melee?'A':'1');slot=true;}
                }else if(pc==0x9dd1&&_runtime->vortexEffects())_runtime->key('L');
                else if(pc==0x1324&&!tactical&&_runtime->battleAdmissionSeen()){_runtime->key('T');tactical=true;}
                else if(pc==0x1324&&!_runtime->inBattle())_runtime->key('Y');
                else if(pc==0x1324&&cast&&slot)_runtime->key('Y');
                else if(_runtime->battleTargeting())_runtime->key(melee?13:_runtime->battleTargetKey());
                else _runtime->key(13);
            }
            if(!(i%256))trace+=_runtime->battleDiagnostic()+"\n";
            if(!advance())return fail(_runtime->error());
        }
        trace+=_runtime->battleDiagnostic()+"\n";report("nox-spell-trace.tsv",trace);frame("nox-vortex-battle-return.png");
        if(guardianMissing){if(!guardianRefused||!guardianChecked||_runtime->campaignLogCount(byte(150+guardian)))return fail("Missing artifact real Cast refusal not demonstrated");
            Common::OutSaveFile *diagnostic=getSaveFileManager()->openForSaving(guardianReplay?"nox-guardian-refusal-replay.bin":"nox-guardian-refusal.bin",false);if(!diagnostic)return fail("Refusal diagnostic open failed");bool captured=_runtime->campaignDiagnosticSnapshot(*diagnostic);diagnostic->finalize();captured&=!diagnostic->err();delete diagnostic;if(!captured)return fail("Refusal diagnostic capture failed");
            return report("nox-spell-ready.tsv",Common::String::format("PASS\tguardian=%u actual_vortex_cast=1 original_full_HP_retained=1 invulnerability=1 MP=23 result_persistence=NOT STARTED\n",guardian));}
        if(((smallEncounter||guardian)?!returned:!ending)||(melee?_runtime->campaignFinalEnemyHealth()!=0:(!_runtime->vortexEffects()||!_runtime->vortexDamaged()))||_runtime->campaignMana()!=(melee?110u:23u)||!_runtime->campaignPartyHealth())return fail("Actual Vortex battle effect/return not demonstrated");
        if(guardian&&(!guardianChecked||_runtime->campaignLogCount(byte(150+guardian))!=1))return fail("Actual guardian Jewel reward absent");
        if(saveGameState(1,guardian?Common::String::format("Guardian %u result",guardian):melee?"Final ending presentation":"Vortex battle effect").getCode()!=Common::kNoError||saveGameState(1,guardian?Common::String::format("Guardian %u result",guardian):melee?"Final ending presentation":"Vortex battle effect").getCode()!=Common::kNoError)return fail("Battle result save failed");
        if(melee)return report("nox-spell-ready.tsv","PASS\tactual_weapon_final_blow=1\tactual_ending_presentation=1\tcontrolled_living_boss_HP1_not_earned=1\tMP=110\n");
        return report("nox-spell-ready.tsv",Common::String::format("PASS\tactual_vortex_dispatch=1\tenemy_HP_decreased=1\tMP=23\tactual_ending_presentation=%u\tcontrolled_wounded_boss_not_earned=%u\tactual_alignment_movement=1\n",ending,!fullBoss&&!smallEncounter));
    }
    if(castOnly){
        if(_runtime->campaignReadiedSpell()!=12||_runtime->campaignMana()!=110)return fail("Missing actual readiness predecessor");
        // Original 863D/864C selector confirms with Return; numeric1 is ignored.
        _runtime->key('C');_runtime->key('1');_runtime->key(13);
        bool returned=false;
        for(uint i=0;i<2000&&!shouldQuit();++i){
            if(_runtime->atWorldInput()&&_runtime->canSnapshot()){returned=true;break;}
            if(!(i%128))state("world-cast-running");if(!advance())return fail(_runtime->error());
        }
        state("world-cast-return");frame("nox-spell-world-cast-return.png");
        if(!returned||!_runtime->campaignWorldCastRefusals()||_runtime->campaignMana()!=110||_runtime->campaignLogCount(12)!=1||_runtime->campaignReadiedSpell()!=12)return fail("World battle-only refusal not completed");
        if(saveGameState(1,"Vortex world cast").getCode()!=Common::kNoError||saveGameState(1,"Vortex world cast").getCode()!=Common::kNoError)return fail("World cast save failed");
        report("nox-spell-trace.tsv",trace);return report("nox-spell-ready.tsv","PASS\tactual_world_battle_only_refusal=1\tMP=110\tspell_slot1=12\tbattle_effect=NOT STARTED\n");
    }
    auto ready=[&](bool eligible){
        uint stage=0,navigation=0,exits=0;
        _runtime->key(9);_runtime->key(9);_runtime->key('5');
        for(uint i=0;i<12000&&!shouldQuit();++i){
            if(_runtime->atWorldInput()&&_runtime->canSnapshot()){state("inventory-returned");return stage>=2;}
            if(_runtime->campaignTestInputReady()&&!_runtime->atWorldInput()){
                state("inventory-input");
                if(stage==0){
                    if(_runtime->address()!=0xa9a1||_runtime->campaignInventoryTab()!=4){if(++navigation>8)return false;_runtime->key(navigation<3?'5':9);}
                    else{const int row=_runtime->campaignOwnedSpellRow();if(row<0)return false;if(_runtime->campaignInventoryRow()!=row)_runtime->key(_runtime->campaignInventoryRow()<row?10:11);else{_runtime->key(13);stage=1;}}
                }else if(stage==1){
                    if(eligible){if(_runtime->address()==0xa9a1&&_runtime->campaignReadiedSpell()!=12)return false;_runtime->key('1');}
                    else frame("nox-spell-low-int-refusal.png");
                    stage=2;if(!eligible)_runtime->key(27);
                }else{if(eligible&&_runtime->campaignReadiedSpell()!=12)return false;if(eligible&&!exits)frame("nox-spell-assigned-slot.png");if(++exits>8)return false;_runtime->key(27);}
            }
            if(!(i%128))state("inventory-running");if(!advance())return false;
        }return false;
    };
    const byte before=_runtime->campaignReadiedSpell();state("original-starter-before-ready");
    if(!ready(false)||_runtime->campaignReadiedSpell()!=before||!_runtime->campaignSpellReadyRefusals()||_runtime->campaignLogCount(12)!=1)return fail("Actual insufficient-INT ready refusal failed");
    state("before-controlled-caster");if(!_runtime->campaignFixtureMage())return fail("Source-supported level10 caster rejected");state("controlled-caster-not-earned");
    if(!ready(true)||_runtime->campaignReadiedSpell()!=12||_runtime->campaignReadiedIntelligence()!=110||_runtime->campaignLogCount(12)!=1)return fail("Actual Nox Vortex ready failed");
    state("spell-readied");frame("nox-spell-readied.png");
    // Casting and battle effect remain separate, unverified acceptance tests.
    if(saveGameState(1,"Vortex spell ready").getCode()!=Common::kNoError||saveGameState(1,"Vortex spell ready").getCode()!=Common::kNoError)return fail("Readied spell save failed");
    report("nox-spell-trace.tsv",trace);return report("nox-spell-ready.tsv","PASS\tactual_low_INT_refusal=1\tactual_ready_slot1=12\tminimum_INT=110\tcasting=NOT STARTED\tbattle_effect=NOT STARTED\tcontrolled_caster_not_earned=1\n");
}
}
#endif
