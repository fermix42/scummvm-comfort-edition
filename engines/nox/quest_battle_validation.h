/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_QUEST_BATTLE_VALIDATION_H
#define NOX_QUEST_BATTLE_VALIDATION_H
namespace Nox {
bool Runtime::campaignCultistsSource() const {
    if(!_resourceArchive)return false;Common::MemoryReadStream stream(_resourceArchive->image().data(),_resourceArchive->image().size());HdvArchive image;Common::Array<byte> map,combat;
    if(!image.open(stream)||!image.read("NA/DATA.MAP.UM",map)||!image.read("NA/SRTN.COMBAT.S",combat)||combat.size()<0x2dbc)return false;
    const byte guard[]={215,86,169,5,0,56,0,2};bool source=false;
    for(uint at=1;at+23<map.size()&&map[at]!=255;){const uint length=map[at]?map[at]:256;if(length<23||at+length>map.size())return false;if(map[at+1]==99){const uint following=map[at+7]|uint(map[at+8])<<8|uint(map[at+9])<<16;source=following+776<=map.size()&&!memcmp(map.data()+following+768,guard,8);break;}at+=length;}
    return source&&READ_LE_UINT16(combat.data()+0x2d9e)==267&&combat[0x2dac]==169;
}
bool Runtime::campaignCultistsApproach() const {
    return campaignCultistsSource()&&atWorldInput()&&canSnapshot()&&campaignMapId()==99&&mainram[0x267d]==2&&positionX()==212&&positionY()==89&&mainram[0xbd02]==169&&mainram[0xbd03]!=255&&!(mainram[0x438]&6)&&(mainram[0x40c]&2)&&mainram[0xbd0b]==255&&campaignWorldObjectState(215,100)==0xfe4a&&campaignPartyHealth()==125&&campaignMana()==110;
}
bool Runtime::campaignTempleMonksSource() const {
    if(!_resourceArchive)return false;Common::MemoryReadStream stream(_resourceArchive->image().data(),_resourceArchive->image().size());HdvArchive original;Common::Array<byte> map,combat;
    if(!original.open(stream)||!original.read("NA/DATA.MAP.UM",map)||!original.read("NA/SRTN.COMBAT.S",combat)||combat.size()<0x2d7c)return false;
    bool guard=false;const byte expected[]={215,98,137,5};
    for(uint cursor=1;cursor+23<map.size()&&map[cursor]!=255;){const uint length=map[cursor]?map[cursor]:256;if(length<23||cursor+length>map.size())return false;if(map[cursor+1]==99){const uint following=map[cursor+7]|uint(map[cursor+8])<<8|uint(map[cursor+9])<<16;guard=following+780<=map.size()&&!memcmp(map.data()+following+776,expected,4);break;}cursor+=length;}
    return guard&&READ_LE_UINT16(combat.data()+0x2d3e)==191&&READ_LE_UINT16(combat.data()+0x2d5e)==137&&combat[0x2d4c]==137&&combat[0x2d6c]==137;
}
bool Runtime::campaignTempleMonksApproach() const {
    return campaignTempleMonksSource()&&atWorldInput()&&canSnapshot()&&campaignMapId()==99&&mainram[0x267d]==2&&positionX()==217&&positionY()==101&&mainram[0xbd0a]==137&&mainram[0xbd0b]!=255&&campaignWorldObjectState(215,100)==0xfe4a&&!(mainram[0x42e]&3)&&campaignPartyHealth()==125&&campaignMana()==110&&!auxram[0x1600]&&!auxram[0x1645];
}
bool Runtime::campaignFixtureQuestFinalBlow(byte worldMap,byte actor) {
    const bool cultists=worldMap==99&&actor==0;
    const bool temple=worldMap==99&&actor==8;
    if(!_battleValidation||!inBattle()||!battlePlayerTurn()||!keys.empty()||key_latch||(pc!=0xd53f&&pc!=0xfde9)||campaignMapId()!=worldMap||campaignMana()!=110||((temple||cultists)?(!campaignPartyHealth()||campaignPartyHealth()>125):campaignPartyHealth()!=125)||campaignReadiedSpell()!=12)return false;
    const bool guild=worldMap==50&&actor==120,raque=worldMap==41&&actor==56;if(!guild&&!raque&&!temple&&!cultists)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> source;
    const byte guildDefinition[]={192,50,0,8,0,2,0,77},raqueDefinition[]={192,12,0,1,0,2,0,132};
    const uint offset=guild?19070:30908;const Common::String resource=guild?"NA/DATA.MAP.TOWN2":"NA/DATA.MAP.TOWN";
    if(cultists){if(!campaignCultistsSource()||!campaignCultistsFullRoster()||mainram[0x31fb]!=169)return false;}
    else if(temple){if(!campaignTempleMonksSource()||campaignEnemyCount()!=10||mainram[0x31fb]!=137)return false;for(uint i=0;i<10;++i)if(READ_LE_UINT16(auxram+0x1902+i*32)!=(i<8?191:137)||auxram[0x1910+i*32]!=137)return false;}
    else if(!image.open(stream)||!image.read(resource,source)||source.size()<offset+8||memcmp(source.data()+offset,guild?guildDefinition:raqueDefinition,8)||mainram[0x31fb]!=(guild?207:192))return false;
    const byte death[]={0xa9,0,0x8d,0x81,6,0x8d,0x82,6,0xa9,255,0x8d,0x7f,6};if(memcmp(mainram+0x1ae3,death,sizeof(death)))return false;
    const uint count=campaignEnemyCount();if(!count||count>16)return false;
    for(uint i=0;i<count;++i){if(!READ_LE_UINT16(auxram+0x1902+i*32)||auxram[0x1900+i*32]==255||mainram[0xbf03+i*8]==255)return false;uint cells=0;for(uint cell=0;cell<187;++cell)if(mainram[0xa31+cell]==i*8&&mainram[0xaec+cell]==1)++cells;if(cells!=1)return false;}
    // Original NPC admission is complete and recorded before this distinct
    // controlled combat checkpoint. This is not earned damage or kills.
    // The next actual Cast must earn death, victory and quest/loot effects.
    for(uint i=1;i<count;++i){const uint slot=i*8;auxram[0x1900+i*32]=255;WRITE_LE_UINT16(auxram+0x1902+i*32,0);mainram[0xbf03+slot]=255;for(uint cell=0;cell<187;++cell)if(mainram[0xa31+cell]==slot&&mainram[0xaec+cell]==1){mainram[0xa31+cell]=255;mainram[0xaec+cell]=255;}}
    WRITE_LE_UINT16(auxram+0x1902,1);return true;
}
bool NoxEngine::finishQuestBattle(byte map,byte actor,Common::String &trace,Common::String &error) {
    const bool fullVazarath=map==101&&actor==0&&ConfMan.hasKey("nox_late_full_guardian")&&ConfMan.getBool("nox_late_full_guardian");
    const bool fullCultists=map==99&&actor==0&&ConfMan.hasKey("nox_late_cultist_encounter")&&ConfMan.getBool("nox_late_cultist_encounter")&&!(ConfMan.hasKey("nox_late_staged_cultists")&&ConfMan.getBool("nox_late_staged_cultists"));
    bool prepared=false,cast=false,slot=false,tactical=false,returned=false;uint moves=0,prompts=0;
    for(uint i=0;i<24000&&!shouldQuit();++i){
        if(prepared&&_runtime->atWorldInput()&&_runtime->canSnapshot()){returned=true;break;}
        if(prepared&&!_runtime->inBattle()&&_runtime->canSnapshot())_runtime->enableBattleValidation(false);
        if(_runtime->campaignSceneInputReady())_runtime->key(13);
        else if(_runtime->battleInputReady()){
            if(++prompts>512){error="Quest battle input bound exceeded";return false;}const uint pc=_runtime->address();
            if((pc==0xd53f||pc==0xfde9)&&_runtime->inBattle()&&_runtime->battlePlayerTurn()){
                if(!prepared){trace+="original-quest-full-admission\t"+_runtime->battleDiagnostic()+"\n";
                    if(fullCultists){if(!_runtime->campaignCultistsSource()||_runtime->campaignEnemyCount()!=2||_runtime->campaignMana()!=110||!_runtime->campaignPartyHealth())return false;if(!_runtime->campaignCultistsFullRoster()){error="Original full Cultist roster differs";return false;}trace+="original-full-two-Vhaak-Cultists267 no_enemy_initialization=1\n";}
                    else if(fullVazarath){if(_runtime->campaignEnemyCount()!=1||_runtime->campaignFinalEnemyHealth()!=2600||_runtime->campaignInvulnerability()!=0||_runtime->campaignLogCount(112)!=1||_runtime->campaignMana()!=110||_runtime->campaignPartyHealth()!=125){error="Original full Vazarath/artifact admission differs";return false;}trace+="original-full-Vazarath2600 no_enemy_initialization=1\n";}
                    else{if(!_runtime->campaignFixtureQuestFinalBlow(map,actor)){error="Source-valid quest combat checkpoint rejected";return false;}trace+="CONTROLLED-before-distinct-Cast enemy0HP1 others_dead_not_earned quest_loot_untouched\t"+_runtime->battleDiagnostic()+"\n";}
                    prepared=true;
                }
                if(_runtime->vortexDirectionPending())_runtime->key(_runtime->battleDirectionKey());
                else if(_runtime->vortexEffects()&&_runtime->vortexDamaged())_runtime->key(27);
                else if(_runtime->battleTargeting())_runtime->key(_runtime->battleTargetKey());
                else if(!cast&&!fullVazarath&&_runtime->battleAlignKey()){if(++moves>8){error="Quest battle alignment bound exceeded";return false;}_runtime->key(_runtime->battleAlignKey());}
                else if(!cast){_runtime->key('C');cast=true;}
                else{_runtime->key('1');slot=true;}
            }else if(pc==0x9dd1&&_runtime->vortexEffects())_runtime->key('L');
            else if(pc==0x1324&&!tactical&&(_runtime->battleAdmissionSeen()||(_runtime->inBattle()&&_runtime->campaignEnemyCount()>0))){_runtime->key('T');tactical=true;}
            else if(pc==0x1324&&!_runtime->inBattle())_runtime->key('Y');
            else if(pc==0x1324&&cast&&slot)_runtime->key('Y');
            else if(_runtime->battleTargeting())_runtime->key(_runtime->battleTargetKey());
            else _runtime->key(13);
        }
        if(!(i%128))trace+=_runtime->battleDiagnostic()+"\n";
        if(!_runtime->advance(100000,true)){error=_runtime->error();return false;}_runtime->audio().clear();
    }
    trace+="actual-quest-battle-result\t"+_runtime->battleDiagnostic()+"\n";
    if(!returned||!prepared||!cast||!slot||!_runtime->vortexEffects()||!_runtime->vortexDamaged()||_runtime->campaignMana()!=23||!_runtime->campaignPartyHealth()){error="Actual quest Cast/victory/world return absent";return false;}return true;
}
}
#endif
