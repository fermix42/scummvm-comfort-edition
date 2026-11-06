/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_CAMPAIGN_VALIDATION_H
#define NOX_CAMPAIGN_VALIDATION_H
namespace Nox {
// Source-supported shipwreck bookshelf adjacency, before the real Open/Search.
// Only position/camera and redraw continuation are controlled. No quest/item,
// actor or outcome state is written by this setup, nor after input begins.
bool Runtime::campaignFixtureShelf() {
	if (!atWorldInput() || !canSnapshot() || mainram[0x2af9] != 7 || mainram[0x267d] != 8) return false;
	mainram[0x6cec] = mainram[0x23f8] = 22;
	mainram[0x6ced] = mainram[0x23f9] = 10;
	push16(0x7127); push16(0x7127); push16(0xf64f); pc = 0x8f1b;
	key_latch=0; keys.clear(); worldWaiting=false; return true;
}
uint Runtime::campaignLogCount(byte item) {
	// Read the live private image using the existing archive parser. Search writes
	// its inventory transaction there; original immutable HDV is never written.
	Common::MemoryReadStream stream(disk.data(),disk.size()); HdvArchive image;
	Common::Array<byte> inventory;
	if (!image.open(stream) || !image.read("NA/SRTN.INVENTORY",inventory) || inventory.size()<0x1000) return 0xffffffffU;
	uint count=0;
	for(uint i=0xa00;i+6<=0x1000;i+=6)
		if((inventory[i]&15)==2 && inventory[i+1]==item) count+=inventory[i+2];
	return count;
}
bool Runtime::campaignFixturePage(byte map,byte column,byte row,byte mask) {
 if(!atWorldInput()||!canSnapshot()||mainram[0x267d]!=7||mainram[0x2af9]!=map||row>=47)return false;
 bool authored=false;
 for(uint i=0;i+12<=256;++i){const byte *object=mainram+0xbe00+i;
  if(object[0]==column&&object[1]==row&&(object[3]&0xbf)==53&&object[4]==2&&object[5]==97&&object[8]==62&&object[11]==mask)authored=true;
 }
 if(!authored)return false;
 // Masks04/40 have water south; use eastern floor75. Mask80 has a
 // stair trigger south; use western floor56. Masks08/10 use north/east
 // floor56 instead of southern wall48. All setups precede Search;
 // no party health or earned outcome is changed to survive a fixture.
 const byte direction=campaignPageDirection(mask);
 const byte partyColumn=direction==8?column+1:direction==21?column-1:column,partyRow=direction==10?row-1:direction==11?row+1:row;
 mainram[0x6cec]=mainram[0x23f8]=partyColumn;mainram[0x6ced]=mainram[0x23f9]=partyRow;
 push16(0x7127);push16(0x7127);push16(0xf64f);pc=0x8f1b;key_latch=0;keys.clear();worldWaiting=false;return true;
}
bool NoxEngine::validateCampaignPage(bool cold,Common::String &error) {
 if(ConfMan.getInt("nox_campaign_page")==8)return validateCampaignPagesLinked(cold,error);
 Common::String trace;
 auto report=[&](const char *name,const Common::String &text){Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;};
 auto fail=[&](const Common::String &why){error=why;warning("Nox page checkpoint: %s",why.c_str());report("nox-campaign-trace.tsv",trace);report("nox-campaign-failure.tsv",why+"\n"+_runtime->campaignMachineDiagnostic()+"\n");
  _runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-campaign-failure.png",false);if(out){Image::writePNG(*out,_surface);out->finalize();delete out;}return false;};
 const int requested=ConfMan.getInt("nox_campaign_page");
 static const byte masks[]={4,8,16,32,64,128,1},maps[]={78,78,78,78,57,57,57},columns[]={5,27,9,33,32,37,44},rows[]={9,13,12,18,1,36,20};
 if(_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty()||requested<1||requested>7)return fail("Invalid offline page fixture");
 const uint index=requested-1;const byte mask=masks[index];
 if(cold){
  const SaveStateList saves=getMetaEngine()->listSaves(_targetName.c_str(),false);if(saves.size()!=1)return fail("Expected one page save");
  const int handle=saves[0].getSaveSlot();const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> payload;
  if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Page cold load failed");
  Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);
  if(!_runtime->saveState(state)||state.size()!=uint(payload.size()-entry.offset)||memcmp(state.getData(),payload.data()+entry.offset,state.size())||_runtime->campaignLogCount(97)!=1||_runtime->campaignPageFlags()!=mask)return fail("Page cold state mismatch");
  return report("nox-campaign-cold.tsv","PASS\tprovenance=controlled-page-checkpoint\truntime_bytes=identical\tpage_count=1\n");
 }
 if(_runtime->campaignLogCount(97)||_runtime->campaignPageFlags())return fail("Initial world contains page outcome");
 Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_campaign_checkpoint");_runtime->enableDialogueValidationPolling();
 const uint routeCount=maps[index]==57?4:3;uint stage=0;uint64 polls=0;
 for(uint iteration=0;iteration<16000&&!shouldQuit();++iteration){
  trace+=Common::String::format("%u\t%u\t%04X\t%u\t%u\t%02X\t%s\t%s\n",iteration,stage,_runtime->address(),_runtime->positionX(),_runtime->positionY(),_runtime->campaignPageFlags(),_runtime->activeMapResource().c_str(),_runtime->campaignMachineDiagnostic().c_str());
  if(_runtime->atWorldInput()&&_runtime->canSnapshot()){
   if(stage<routeCount){const byte ids[]={0,54,byte(maps[index]==57?58:78),57},types[]={0,7,7,7};if(!_runtime->dialogueFixtureExit(ids[stage],types[stage]))return fail(Common::String::format("Authored page setup exit rejected at stage%u",stage));++stage;}
   else if(stage==routeCount){if(!_runtime->campaignFixturePage(maps[index],columns[index],rows[index],mask))return fail("Source page adjacency rejected");++stage;}
   else if(stage==routeCount+1){if(_runtime->campaignLogCount(97)||_runtime->campaignPageFlags())return fail("Setup injected page outcome");_runtime->key('S');_runtime->key(_runtime->campaignPageDirection(mask));stage=routeCount+2;polls=_runtime->emptyKeyboardPolls();}
   else if(stage==routeCount+2){if(_runtime->campaignLogCount(97)!=1||_runtime->campaignPageFlags()!=mask)return fail("Real page Search failed");_runtime->key('S');_runtime->key(_runtime->campaignPageDirection(mask));stage=routeCount+3;polls=_runtime->emptyKeyboardPolls();}
   else if(stage==routeCount+3){
    if(_runtime->campaignLogCount(97)!=1||_runtime->campaignPageFlags()!=mask)return fail("Repeat page Search duplicated or changed outcome");
    if(saveGameState(1,"Controlled parchment checkpoint").getCode()!=Common::kNoError||saveGameState(1,"Controlled parchment checkpoint").getCode()!=Common::kNoError)return fail("Page save failed");
    report("nox-campaign-trace.tsv",trace);return report("nox-campaign.tsv",Common::String::format("PASS\tprovenance=controlled-page-checkpoint\tpage_mask=%u\tactual_search=1\trepeat_no_duplicate=1\tinitializations=4\n",mask));
   }
  }else if(stage>=routeCount+2&&_runtime->atKeyboardPoll()&&_runtime->emptyKeyboardPolls()>polls+16){_runtime->key(13);polls=_runtime->emptyKeyboardPolls();}
  if(!_runtime->advance(100000,true))return fail(_runtime->error());_runtime->audio().clear();
 }
 return fail("Page checkpoint instruction budget exhausted");
}
bool NoxEngine::validateCampaignCheckpoint(bool cold, Common::String &error) {
 if(ConfMan.hasKey("nox_shadow_confrontation_validation")&&ConfMan.getBool("nox_shadow_confrontation_validation"))return validateShadowConfrontation(cold,error);
    if(ConfMan.hasKey("nox_shadow_checkpoint_validation")&&ConfMan.getBool("nox_shadow_checkpoint_validation"))return validateShadowCheckpoint(cold,error);
    if(ConfMan.hasKey("nox_cultist_validation")&&ConfMan.getBool("nox_cultist_validation"))return validateCultist(cold,error);
    if(ConfMan.hasKey("nox_dark_key_validation")&&ConfMan.getBool("nox_dark_key_validation"))return validateDarkKey(cold,error);
 if(ConfMan.hasKey("nox_spell_ready_validation")&&ConfMan.getBool("nox_spell_ready_validation"))return validateSpellReady(cold,error);
 if(ConfMan.hasKey("nox_vortex_phase")&&ConfMan.getInt("nox_vortex_phase"))return validateVortexChain(cold,error);
 if(ConfMan.hasKey("nox_campaign_page")&&ConfMan.getInt("nox_campaign_page"))return validateCampaignPage(cold,error);
	Common::String trace;
	auto report=[&](const char *name,const Common::String &text){
		Common::OutSaveFile *out=getSaveFileManager()->openForSaving(name,false);if(!out)return false;
		out->write(text.c_str(),text.size());out->finalize();bool ok=!out->err();delete out;return ok;
	};
	auto fail=[&](const Common::String &why){
		error=why;warning("Nox campaign checkpoint: %s",why.c_str());
		report("nox-campaign-trace.tsv",trace);report("nox-campaign-failure.tsv",why+"\n");
		Common::String replies;
		for(const auto &character:_runtime->enhancements().characters())for(const auto &reply:character.replies)
			replies+=Common::String::format("%s\t%u\t%s\n",character.resource.c_str(),character.record,reply.response.c_str());
		report("nox-campaign-failure-replies.tsv",replies);
		_runtime->render(_surface,0);Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-campaign-failure.png",false);
		if(out){Image::writePNG(*out,_surface);out->finalize();delete out;}
		return false;
	};
	if(_targetName!="nox-gameplay-test"||!Common::CEAchievements().getLinkToken().empty())return fail("Requires isolated offline diagnostic target");
	const bool linked=ConfMan.hasKey("nox_campaign_linked")&&ConfMan.getBool("nox_campaign_linked");
	auto recognized=[&](){
		// Journal identity is the runtime NPC selector (1 here), not the decoded
		// packet directory offset (0). Require the real unique response and resource.
		for(const auto &character:_runtime->enhancements().characters())if(character.resource=="NA/DATA.TLK.L004")
			for(const auto &reply:character.replies){Common::String text=reply.response;text.toLowercase();if(text.contains("found the captain's log"))return true;}
		return false;
	};
	if(cold){
		const SaveStateList saves=getMetaEngine()->listSaves(_targetName.c_str(),false);
		if(saves.size()!=1)return fail("Expected one campaign save");
		const int handle=saves[0].getSaveSlot();const Common::String file=getSaveStateName(handle);
		Saves::Entry entry;Common::Array<byte> payload;
		if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,payload)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Cold campaign load failed");
		Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);
		if(!_runtime->saveState(state)||state.size()!=uint(payload.size()-entry.offset)||memcmp(state.getData(),payload.data()+entry.offset,state.size())||_runtime->campaignLogCount()!=1||!(_runtime->campaignLogFlags()&0x20))return fail("Cold campaign state differs");
		if(linked&&!recognized())return fail("Cold linked NPC recognition missing");
		return report("nox-campaign-cold.tsv",Common::String::format("PASS\tprovenance=controlled-checkpoint\truntime_bytes=identical\tlog_count=1\tlinked_recognition=%u\n",linked?1:0));
	}
	if(_runtime->campaignLogCount()!=0 || (_runtime->campaignLogFlags()&0x20))return fail("Checkpoint already contains outcome");
	Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_campaign_checkpoint");
	_runtime->enableDialogueValidationPolling();
	if(!_runtime->campaignFixtureShelf())return fail("Source-supported shipwreck setup rejected");
	uint stage=0,routeIndex=0;uint64 polls=0;
	// Ordinary return route from the real acquisition state; no new setup/mutations.
	static const char *route[]={"<","<","O<","<","<","<","<","<","O<","<","<","^","<","^","<","T<"};
	for(uint iteration=0;iteration<6000&&!shouldQuit();++iteration){
		const uint16 site=_runtime->address();
		trace+=Common::String::format("%u\t%u\t%04X\t%u\t%u\t%02X\t%s\n",iteration,stage,site,_runtime->positionX(),_runtime->positionY(),_runtime->campaignLogFlags(),_runtime->activeMapResource().c_str());
		if(_runtime->atWorldInput()&&_runtime->canSnapshot()){
			if(stage==0){_runtime->key('O');_runtime->key(11);stage=1;}
			else if(stage==1){_runtime->key('S');_runtime->key(11);stage=2;polls=_runtime->emptyKeyboardPolls();}
			else if(stage==2){
				if(_runtime->campaignLogCount()!=1||!(_runtime->campaignLogFlags()&0x20))return fail("Real Search failed to award log");
				_runtime->key('S');_runtime->key(11);stage=3;polls=_runtime->emptyKeyboardPolls();
				continue;
			}else if(stage==3){
				if(_runtime->campaignLogCount()!=1)return fail("Repeat Search duplicated log");
				if(linked){stage=4;continue;}
			}else if(stage==4){
				if(routeIndex>=ARRAYSIZE(route))return fail("Linked route returned without recognized conversation");
				for(const char *key=route[routeIndex++];*key;++key)_runtime->key(*key=='<'?8:*key=='^'?11:*key);
				if(routeIndex==ARRAYSIZE(route)){stage=5;polls=_runtime->emptyKeyboardPolls();}
			}else if(stage==5){
				if(!recognized()||_runtime->campaignLogCount()!=1||!(_runtime->campaignLogFlags()&0x20))return fail("Linked NPC recognition/result failed");
				stage=6;
			}
			if((!linked&&stage==3)||(linked&&stage==6)){
				if(saveGameState(1,"Controlled Captain log checkpoint").getCode()!=Common::kNoError||saveGameState(1,"Controlled Captain log checkpoint").getCode()!=Common::kNoError)return fail("Campaign save failed");
				report("nox-campaign-trace.tsv",trace);
				return report("nox-campaign.tsv",Common::String::format("PASS\tprovenance=controlled-checkpoint\tsetup=shipwreck-position-camera-redraw-only\tactual_open_search=1\tlog_count=1\trepeat_search_no_duplicate=1\tlinked_recognition=%u\tinitializations=1\n",linked?1:0));
			}
		}else if(stage>=2&&_runtime->atKeyboardPoll()&&_runtime->emptyKeyboardPolls()>polls+16){
			_runtime->key(stage==5&&recognized()?27:13);polls=_runtime->emptyKeyboardPolls();
		}
		if(!_runtime->advance(100000,true))return fail(_runtime->error());
		_runtime->audio().clear();
	}
	return fail("Campaign checkpoint instruction budget exhausted");
}
}
#endif
