/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_SHADOW_CHECKPOINT_VALIDATION_H
#define NOX_SHADOW_CHECKPOINT_VALIDATION_H
namespace Nox {
bool Runtime::campaignFixtureShadowCheckpointApproach(const Common::String &sectorA9,const Common::String &sectorB9,Common::String &why) {
    why="Source header, original event code, current foot profile or empty outside position differs";
    if(!atWorldInput()||!canSnapshot()||campaignMapId()||mainram[0x267d]||!campaignCultistDefeated()||(mainram[0x43b]&7)!=6||campaignLogCount(156)!=1||campaignPartyHealth()!=125)return false;
    Common::MemoryReadStream stream(disk.data(),disk.size());HdvArchive image;Common::Array<byte> surface,resident;
    if(!image.open(stream)||!image.read("NA/DATA.MAP.SURF",surface)||!image.read("NA/BS.ROUTINES.BK2",resident)||surface.size()<3586||resident.size()<0x2a45||surface[3]||surface[4]!=2||surface[5]!=14||surface[6])return false;
    auto hash=[&](const byte *bytes,uint size){uint32 value=2166136261u;for(uint i=0;i<size;++i)value=(value^bytes[i])*16777619u;return value;};
    if(hash(resident.data()+0x2a1d,40)!=2354517802u||memcmp(high[0]+0x1a1d,resident.data()+0x2a1d,40))return false;
    const uint table=3586-514;
    // Private source-derived world sectors may be validly recompressed or
    // game-modified. The independent fixture manifest decodes these exact
    // retained source bytes and verifies the two relevant source cells first.
    if(sectorA9.size()!=8||sectorB9.size()!=8)return false;
    for(uint i=0;i<2;++i){const uint sector=i?185:169;uint first=surface[table+sector]|uint(surface[table+257+sector])<<8,last=surface[table+sector+1]|uint(surface[table+258+sector])<<8;if(first<2048||last<=first||uint64(3586)+last-2048>surface.size())return false;const Common::String actual=Common::String::format("%08x",hash(surface.data()+3586+first-2048,last-first)),expected=i?sectorB9:sectorA9;if(actual!=expected){why=Common::String::format("Retained source sector%u differs: actual=%s expected=%s",sector,actual.c_str(),expected.c_str());return false;}}
    // CONTROLLED source-decoded tile77 at152,176 OUTSIDE sectorA9; real Up
    // enters tile0 at152,175. Exact compressed sector/code guards above.
    const byte floorTiles[]={77,0};for(uint tile=0;tile<2;++tile)for(uint i=0;i<6;i+=2)if(floorTiles[tile]>=mainram[0x781+i]&&floorTiles[tile]<mainram[0x782+i])return false;
    for(uint i=0;i<256;i+=8)if(mainram[0xbf00+i]==152&&mainram[0xbf01+i]==176&&mainram[0xbf02+i]&&mainram[0xbf03+i]!=255)return false;
    mainram[0x6cec]=mainram[0x23f8]=152;mainram[0x6ced]=mainram[0x23f9]=176;
    push16(0x7127);push16(0x7127);push16(0xf64f);push16(0x167b);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool NoxEngine::validateShadowCheckpoint(bool cold,Common::String &error) {
    Common::String trace;
    auto report=[&](const char *name,const Common::String &text){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;};
    auto frame=[&](const char *name){_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;bool ok=Image::writePNG(*out,_surface);out->finalize();ok&=!out->err();delete out;return ok;};
    auto fail=[&](const Common::String &why){error=why;trace+=why+Common::String::format("\nPC=%04X map=%u xy=%u,%u flags43B=%02X\n",_runtime->address(),_runtime->campaignMapId(),_runtime->positionX(),_runtime->positionY(),_runtime->campaignFlagByte(0x43b))+_runtime->campaignMachineDiagnostic()+"\n"+_runtime->callerObservation()+"\n"+_runtime->battleDiagnostic();report("nox-shadow-checkpoint-failure.tsv",trace);Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-shadow-checkpoint-failure-memory.bin",false);if(out){_runtime->campaignDiagnosticMemory(*out);out->finalize();delete out;}if(_runtime->canSnapshot()){saveGameState(3,"Shadow diagnostic, not acceptance");saveGameState(3,"Shadow diagnostic, not acceptance");}frame("nox-shadow-checkpoint-failure.png");return false;};
    const bool checkOnly=ConfMan.hasKey("nox_shadow_checkpoint_check_only")&&ConfMan.getBool("nox_shadow_checkpoint_check_only");
    if(!cold||_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Requires isolated offline actual Styx checkpoint");
    const Common::String label=checkOnly?"Shadow automatic checkpoint event":"Vortex phase 38";int handle=-1;for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))if(entry.getDescription()==label)handle=entry.getSaveSlot();
    if(handle<0)return fail("Retained original Styx checkpoint missing");const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
    if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Shadow checkpoint restore failed");Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);
    if(!_runtime->saveState(restored)||restored.size()!=uint(payload.size()-entry.offset)||memcmp(restored.getData(),payload.data()+entry.offset,restored.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=entry.metadata.session)return fail("Shadow checkpoint exact runtime/session differs");
    if(checkOnly){if(!_runtime->atWorldInput()||_runtime->campaignMapId()||_runtime->positionX()!=152||_runtime->positionY()!=175||(_runtime->campaignFlagByte(0x43b)&7)!=7)return fail("Cold original automatic checkpoint result differs");return report("nox-shadow-checkpoint-cold.tsv","PASS\truntime_bytes=identical source_automatic_private_checkpoint_only=1 no_Shadow_battle_claim=1\n");}
    if((_runtime->campaignFlagByte(0x43b)&7)!=6||!_runtime->campaignCultistDefeated()||_runtime->campaignMapId()!=57)return fail("Actual post-journal Styx guidance predecessor differs");
    Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_shadow_checkpoint_approach");_runtime->enableDialogueValidationPolling();
    auto advance=[&](){bool ok=_runtime->advance(100000,true);_runtime->audio().clear();return ok;};
    auto settle=[&](){for(uint i=0;i<12000&&!shouldQuit();++i){if(_runtime->atWorldInput()&&_runtime->canSnapshot())return true;if(_runtime->inBattle())return false;if(!advance())return false;}return false;};
    if(!_runtime->dialogueFixtureExit(58,7)||!settle()||!_runtime->dialogueFixtureExit(54,7)||!settle()||!_runtime->dialogueFixtureExit(0,128)||!settle())return fail("Authored Shadow-sector surface return rejected");
    Common::String sourceWhy;if(!_runtime->campaignFixtureShadowCheckpointApproach(ConfMan.get("nox_shadow_source_A9"),ConfMan.get("nox_shadow_source_B9"),sourceWhy)||!settle())return fail("Source-valid outside Shadow-sector approach rejected: "+sourceWhy);
    if(saveGameState(2,"Shadow automatic checkpoint approach").getCode()!=Common::kNoError||saveGameState(2,"Shadow automatic checkpoint approach").getCode()!=Common::kNoError)return fail("Legal outside-sector approach save failed");
    const Common::String before=_runtime->diskFingerprint();_runtime->enableCallerObservation(true);_runtime->key(11);if(!settle())return fail("Actual sector entry did not return to world; inspect actual scene/battle");
    if(_runtime->campaignMapId()||_runtime->positionX()!=152||_runtime->positionY()!=175||(_runtime->campaignFlagByte(0x43b)&7)!=7||!_runtime->callerObservation().contains("legacy_save_entries=1"))return fail("Actual sector checkpoint/private caller differs");
    trace+=Common::String::format("actual-sector-Up\txy=%u,%u HP=%u flags43B=%02X private_image_before=%s after=%s\n",_runtime->positionX(),_runtime->positionY(),_runtime->campaignPartyHealth(),_runtime->campaignFlagByte(0x43b),before.c_str(),_runtime->diskFingerprint().c_str())+_runtime->callerObservation()+"\n";
    for(uint i=0;i<256;i+=8)trace+=_runtime->campaignActorDiagnostic(byte(i))+"\n";
    if(saveGameState(1,"Shadow automatic checkpoint event").getCode()!=Common::kNoError||saveGameState(1,"Shadow automatic checkpoint event").getCode()!=Common::kNoError)return fail("Actual automatic checkpoint legal CE snapshot failed");frame("nox-shadow-checkpoint-event.png");report("nox-shadow-checkpoint-trace.tsv",trace);
    return report("nox-shadow-checkpoint.tsv","PASS\tactual_Up_sectorA9_internal_save=1 043B01=1 original_private_checkpoint_only=1 Shadow_encounter_NOT_STARTED=1\n");
}
}
#endif
