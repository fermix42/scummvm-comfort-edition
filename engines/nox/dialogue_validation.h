/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_DIALOGUE_VALIDATION_H
#define NOX_DIALOGUE_VALIDATION_H
#include "nox/runtime.h"
#include "nox/saves.h"
#include "image/png.h"
namespace Nox {
bool Runtime::dialogueFixtureExit(byte id, byte type) {
	if (!atWorldInput() || !canSnapshot()) return false;
	for (uint offset = 0; offset < 256; offset += 4) {
		const byte *exit = mainram + 0x6ab0 + offset;
		if (exit[2] & 128) break;
		if (exit[2] == id && exit[3] == type) {
			if(type & 128) {
				// Return descriptors hold destination world coordinates. The
				// original leave-map routine scans and applies this descriptor;
				// it is not a local square for the entrance scanner at F515.
				push16(0x7127);push16(0x7127);pc=0xf474;
				key_latch=0;keys.clear();worldWaiting=false;return true;
			}
			mainram[0x6cec] = mainram[0x23f8] = exit[0]; mainram[0x6ced] = mainram[0x23f9] = exit[1];
			push16(0x7127); push16(0x7127); push16(0xf514); pc = 0x8f1b;
			key_latch = 0; keys.clear(); worldWaiting = false; return true;
		}
	}
	return false;
}
bool Runtime::dialogueFixtureAdjacentActor(byte actor) {
	if (!atWorldInput() || !canSnapshot() || currentMapResource() != "NA/DATA.MAP.TOWN2" || mainram[0x2af9] != 46 || (actor & 7)) return false;
	const byte column = mainram[0xbf00 + actor], row = mainram[0xbf01 + actor];
	if (column >= 48 || row >= 47) return false;
	mainram[0x6cec] = mainram[0x23f8] = column; mainram[0x6ced] = mainram[0x23f9] = row + 1;
	push16(0x7127); push16(0x7127); push16(0xf64f); pc = 0x8f1b;
	key_latch = 0; keys.clear(); worldWaiting = false; return true;
}
bool NoxEngine::validateDialogueFixture(bool cold, Common::String &error) {
	Common::String trace, knowledge;
	auto fail = [&](const Common::String &message) {
		error = message; warning("Nox dialogue fixture: %s", message.c_str());
		Common::OutSaveFile *out = getSaveFileManager()->openForSaving("nox-dialogue-trace.tsv",false);
		if(out){out->write(trace.c_str(),trace.size());out->finalize();delete out;}
		return false;
	};
	const bool online=ConfMan.hasKey("nox_online_validation")&&ConfMan.getBool("nox_online_validation");
	const Common::String endpoint=ConfMan.hasKey("ce_achievements_api_base_url",Common::ConfigManager::kApplicationDomain)?ConfMan.get("ce_achievements_api_base_url",Common::ConfigManager::kApplicationDomain):Common::String();
	if (_targetName != "nox-gameplay-test" || (!online&&!Common::CEAchievements().getLinkToken().empty()) ||
	    (online&&(Common::CEAchievements().getLinkToken().empty()||(endpoint!="http://hermesbox:8123/api/v1"&&!endpoint.hasPrefix("http://127.0.0.1:")))))return fail("Requires isolated offline or dedicated test-server diagnostic target");
	auto waitOnline=[&](){
		if(!online)return true;
		const uint32 deadline=_system->getMillis()+20000;
		while(_system->getMillis()<deadline&&(Common::CEAchievements().isSyncBusy()||Common::CEAchievements().getPendingEventCount())){
			Common::CEAchievements().retryQueuedEvents();_system->delayMillis(10);
		}
		return !Common::CEAchievements().isSyncBusy()&&!Common::CEAchievements().getPendingEventCount();
	};
	auto frame = [&](const Common::String &name) {
		_runtime->render(_surface, 0); _system->copyRectToScreen(_surface.getPixels(), _surface.pitch, 0, 0, 560, 384); _system->updateScreen();
		Common::OutSaveFile *out = getSaveFileManager()->openForSaving(name, false);
		if (!out) return false;
		const bool encoded = Image::writePNG(*out, _surface); out->finalize(); const bool ok = encoded && !out->err(); delete out; return ok;
	};
	auto report = [&](const char *name, const Common::String &text) {
		Common::OutSaveFile *out = getSaveFileManager()->openForSaving(name, false); if (!out) return false;
		out->write(text.c_str(), text.size()); out->finalize(); const bool ok = !out->err(); delete out; return ok;
	};
	const bool lifecycle=ConfMan.hasKey("nox_dialogue_lifecycle")&&ConfMan.getBool("nox_dialogue_lifecycle");
	const Common::String progressProbe="nox_m2_state_owner_probe"; // Diagnostic scalar, no achievement definition or unlock.
	auto sessionCasual=[&](const Common::String &id,bool expected){
		for(const auto &session:Common::CEAchievements().listPlaythroughsForTarget(_targetName))if(session.clientPlaythroughId==id)return session.casual==expected;
		return false;
	};
	auto reasonsMatch=[&](bool fresh){
		const Common::Array<Common::String> flags=Common::CEAchievements().getAssistanceFlags();
		if(fresh)return flags.empty();
		const bool legacy=ConfMan.hasKey("nox_assistance_legacy")&&ConfMan.getBool("nox_assistance_legacy");
		bool fixture=false,tainted=false,legacyMarker=false;
		for(const auto &flag:flags){
			if(flag=="nox_controlled_dialogue_fixture")fixture=true;
			else if(flag=="legacy_assistance_history"&&legacy)legacyMarker=true;
			else if(flag=="tainted_save_loaded")tainted=true;
			else return false;
		}
		// Signed same-owner audit history can recover the specific reason in
		// addition to the conservative marker retained from a version1 registry.
		return fixture&&tainted&&(!legacy||legacyMarker)&&flags.size()==(legacy?3u:2u);
	};
	if(cold&&lifecycle){
		const SaveStateList saves=getMetaEngine()->listSaves(_targetName.c_str(),false);
		if(saves.size()!=3)return fail("Lifecycle cold fixture requires three logical saves");
		Common::String sessionA,sessionB;
		for(const auto &save:saves){
			const Common::String file=getSaveStateName(save.getSaveSlot());Saves::Entry entry;Common::Array<byte> bytes;
			if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,bytes))return fail("Invalid lifecycle entry");
			if(entry.metadata.description=="Fresh session B")sessionB=entry.metadata.session;else sessionA=entry.metadata.session;
		}
		if(sessionA.empty()||sessionB.empty()||sessionA==sessionB)return fail("Lifecycle session identities collide");
		const char *labels[]={"Observed dialogue fixture","Fresh session B","Before dialogue","Observed dialogue fixture"};
		const char *frames[]={"nox-lifecycle-cold-after.png","nox-lifecycle-cold-b.png","nox-lifecycle-cold-before.png","nox-lifecycle-cold-after.png"};
		for(uint pass=0;pass<ARRAYSIZE(labels);++pass){
			int handle=-1;
			for(const auto &save:saves)if(save.getDescription()==labels[pass])handle=save.getSaveSlot();
			if(handle<0)return fail("Lifecycle save label missing");
			const Common::String file=getSaveStateName(handle);Saves::Entry entry;Common::Array<byte> bytes;
			if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,bytes)||loadGameState(handle).getCode()!=Common::kNoError)return fail("Lifecycle cold switch failed");
			Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);
			const bool after=Common::String(labels[pass])=="Observed dialogue fixture";
			const bool fresh=Common::String(labels[pass])=="Fresh session B";
			if(!_runtime->saveState(state)||state.size()!=uint(bytes.size()-entry.offset)||memcmp(state.getData(),bytes.data()+entry.offset,state.size())||_runtime->partySize()!=(after?2:1)||_runtime->enhancements().sequence()!=(after?9:0)||_saveMode!=entry.metadata.mode||_runtimeSession!=entry.metadata.session||_audio->numQueuedStreams())return fail("Lifecycle restored different owner/world/mode/audio");
			if(!sessionCasual(sessionA,true)||!sessionCasual(sessionB,false))return fail("Older load rolled back permanent history or tainted another session");
			if(Common::CEAchievements().getPopupProgress(progressProbe)!=(after?22U:fresh?33U:11U))return fail("Save-associated CE progress leaked or failed to restore");
			if(!reasonsMatch(fresh))return fail("Permanent assistance reasons leaked or rolled back across a loaded save");
			if(!waitOnline())return fail("Cold lifecycle queue did not synchronize");
			if(!frame(frames[pass]))return fail("Cannot capture lifecycle cold frame");
		}
		return report("nox-dialogue-lifecycle-cold.tsv","PASS\tprovenance=authored-exit-adjacency-fixture\tloads=4\tolder_journal=0\tnewer_journal=9\tparty_restored=1\truntime_bytes=identical\tsession_modes=1\tpermanent_casual=1\tclean_session_b=1\tce_progress_isolated=1\tassistance_reasons_isolated=1\n");
	}
	if (cold) {
		const SaveStateList saves = getMetaEngine()->listSaves(_targetName.c_str(), false);
		if (saves.size() != 1) return fail("Cold fixture requires one logical dialogue save");
		const Common::String file = getSaveStateName(saves[0].getSaveSlot());
		Saves::Entry entry; Common::Array<byte> payload;
		if (!Saves::readGeneration(*getSaveFileManager(), file.substr(0,file.size()-3), file.lastChar()-'0', _targetName, entry, payload) || loadGameState(saves[0].getSaveSlot()).getCode() != Common::kNoError) return fail("Cold dialogue load failed");
		Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);
		if (!_runtime->saveState(state) || state.size() != uint(payload.size()-entry.offset) || memcmp(state.getData(),payload.data()+entry.offset,state.size()) || _audio->numQueuedStreams() || _saveMode != entry.metadata.mode) return fail("Cold dialogue world/journal/mode/audio differs");
		bool casual = false;
		for (const auto &session : Common::CEAchievements().listPlaythroughsForTarget(_targetName)) if (session.clientPlaythroughId == entry.metadata.session) casual = session.casual;
		if (!casual || !frame("nox-dialogue-cold.png")) return fail("Assisted fixture provenance or cold frame missing");
		return report("nox-dialogue-cold.tsv", "PASS\tprovenance=authored-exit-adjacency-fixture\truntime_bytes=identical\tjournal=1\tsaved_mode=1\tassisted=1\taudio_queue=0\n");
	}
	if(lifecycle){
		const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
		Common::CEAchievements().setPopupProgress(progressProbe,11);
		if(!sessionCasual(session,false)||_runtime->partySize()!=1||_runtime->enhancements().sequence()||saveGameState(1,"Before dialogue").getCode()!=Common::kNoError||saveGameState(1,"Before dialogue").getCode()!=Common::kNoError||!frame("nox-lifecycle-before.png"))return fail("Cannot preserve clean pre-dialogue world");
	}
	Common::CEAchievements().noteDisqualifyingAssistance("nox_controlled_dialogue_fixture");
	_runtime->enableDialogueValidationPolling();
	uint stage = 0, question = 0, captured = 0; bool confirmed = false, acknowledged = false;
	uint64 lastInputPoll = 0;
	auto key = [&](Common::KeyCode code, uint16 ascii) {
		Common::Event event; event.type = Common::EVENT_KEYDOWN; event.kbd = Common::KeyState(code,ascii); _eventMan->pushEvent(event);
		event.type = Common::EVENT_KEYUP; _eventMan->pushEvent(event);
	};
	auto text = [&](const char *value) { for (; *value; ++value) key(Common::KeyCode(*value >= 'A' && *value <= 'Z' ? *value + 32 : *value), *value); key(Common::KEYCODE_RETURN,13); };
	static const byte ids[] = {0,49,48,46}, types[] = {0,5,5,5};
	static const char *questions[] = {"NAME","ZZZZ","JOB","WORK","DOCKS","THIEVES","OPERATION","JOIN"};
	for (uint iteration = 0; iteration < 16000 && !shouldQuit(); ++iteration) {
		const uint16 site = _runtime->address();
		trace += Common::String::format("%u\t%u\t%04X\t%u\t%u\t%u\t%llu\t%s\t%s\n",iteration,stage,site,_runtime->positionX(),_runtime->positionY(),question,(unsigned long long)_runtime->enhancements().sequence(),_runtime->activeMapResource().c_str(),_runtime->diagnostic().c_str());
		if(!(iteration%128))report("nox-dialogue-trace.tsv",trace);
		if (_runtime->atWorldInput()) {
			if (stage < 4) { if (!_runtime->dialogueFixtureExit(ids[stage],types[stage])) return fail(Common::String::format("Authored exit setup failed at stage %u",stage)); ++stage; }
			else if (stage == 4) { key(Common::KEYCODE_w,'W'); text("4"); stage = 5; }
			else if (stage == 5) { if (!_runtime->dialogueFixtureAdjacentActor(0x58)) return fail("Cannot establish documented Cutter adjacency"); stage = 6; }
			else if (stage == 6) { key(Common::KEYCODE_t,'T'); key(Common::KEYCODE_UP,0); stage = 7; lastInputPoll=_runtime->emptyKeyboardPolls(); }
			else if (stage == 7) {
				if (question != ARRAYSIZE(questions) || !confirmed || _runtime->partySize() != 2 || _runtime->enhancements().sequence() < 8 || !_runtime->canSnapshot()) return fail("Dialogue returned before resolved questions/recruitment completed");
				bool named = false;
				for (const auto &character : _runtime->enhancements().characters()) if (character.name == "Cutter" && character.replies.size() >= 8 && !character.topics.empty()) named = true;
				const int slot=lifecycle?2:1;
				if(lifecycle)Common::CEAchievements().setPopupProgress(progressProbe,22);
				if (!named || saveGameState(slot,"Observed dialogue fixture").getCode() != Common::kNoError || saveGameState(slot,"Observed dialogue fixture").getCode() != Common::kNoError || !frame("nox-dialogue-saved.png")) return fail("Observed dialogue owner/save/name verification failed");
				report("nox-dialogue-trace.tsv", trace);
				if(!report("nox-dialogue-knowledge.tsv",knowledge))return fail("Cannot persist intermediate knowledge states");
				if(!report("nox-dialogue.tsv", Common::String::format("PASS\tprovenance=authored-exit-adjacency-fixture\tquestions=%u\tresolved_replies=%llu\tactual_talk=1\tnamed_recruit=1\tengine_saves=2\tassisted=1\n",question,(unsigned long long)_runtime->enhancements().sequence())))return fail("Cannot persist dialogue report");
				if(!lifecycle)return true;
				if(!waitOnline())return fail("Session A queue did not synchronize");
				const Common::String sessionA=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
				const SaveMode modeA=_saveMode;
				ConfMan.setBool("nox_classic_saves",modeA!=kClassicSaveMode);
				if(!Common::CEAchievements().startPlaythroughForTarget(_targetName,"nox-archaist","apple2-v119","M2 fresh session B")||canSaveGameStateCurrently()||saveGameState(1,"Wrong lifecycle owner").getCode()==Common::kNoError||!waitOnline()||!synchronizePlaySession())return fail("New lifecycle session retained old owner or failed synchronization");
				const Common::String sessionB=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
				if(sessionA==sessionB||_saveMode==modeA||!sessionCasual(sessionA,true)||!sessionCasual(sessionB,false)||!_runtime->enhancements().characters().empty()||!_runtime->enhancements().cells().empty()||_runtime->enhancements().sequence()||_audio->numQueuedStreams())return fail("Fresh lifecycle session inherited previous state/history/mode");
				if(Common::CEAchievements().getPopupProgress(progressProbe))return fail("New session inherited CE progress from A");
				if(!Common::CEAchievements().getAssistanceFlags().empty())return fail("Fresh B inherited A assistance reasons");
				ConfMan.setBool("nox_classic_saves",modeA==kClassicSaveMode);
				stage=8;continue;
			} else if(stage==11){
				key(Common::KEYCODE_RIGHT,0);stage=12;
			} else if(stage==12){
				key(Common::KEYCODE_o,'O');key(Common::KEYCODE_UP,0);stage=13;
			} else if(stage==13){
				const Common::String sessionB=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
				if(Common::CEAchievements().getPopupProgress(progressProbe))return fail("Fresh B journey acquired previous CE progress");
				Common::CEAchievements().setPopupProgress(progressProbe,33);
				if(_runtime->positionX()!=25||_runtime->positionY()!=38||_runtime->partySize()!=1||_runtime->enhancements().sequence()||!_runtime->enhancements().characters().empty()||!sessionCasual(sessionB,false)||saveGameState(1,"Fresh session B").getCode()!=Common::kNoError||saveGameState(1,"Fresh session B").getCode()!=Common::kNoError||!frame("nox-lifecycle-b.png"))return fail("Cannot preserve independent fresh session B");
				if(!waitOnline())return fail("Session B queue did not synchronize");
				return report("nox-dialogue-lifecycle.tsv","PASS\tprovenance=authored-exit-adjacency-fixture\tlogical_saves=3\tsessions=2\tnew_session_empty=1\tbefore_assistance_save=1\tactual_new_game_b=1\tdistinct_saved_modes=1\n");
			}
		} else if(stage==8&&site==0xa06b){key(Common::KEYCODE_DOWN,0);key(Common::KEYCODE_RETURN,13);stage=9;
		} else if(stage==9&&site==0x1324){key(Common::KEYCODE_y,'Y');stage=10;
		} else if(stage==10&&site==0x1371){text("TRACE");stage=11;
		} else if(stage==12&&site==0xd560&&_runtime->atKeyboardPoll()&&_runtime->positionX()==25&&_runtime->positionY()==38){key(Common::KEYCODE_o,'O');key(Common::KEYCODE_UP,0);stage=13;
		} else if(stage>=8&&((site==0xd560&&!_runtime->inWorldScene())||site==0x1324||site==0x1371)){key(Common::KEYCODE_RETURN,13);
		} else if(stage==7 && _runtime->emptyKeyboardPolls()-lastInputPoll>=32) {
			// Match the standalone fixture's actual empty-read observation,
			// without freezing a nonblocking renderer poll at a guessed PC.
			if(question<ARRAYSIZE(questions) && _runtime->enhancements().sequence()==question+1){text(questions[question++]);lastInputPoll=_runtime->emptyKeyboardPolls();}
			else if(question==ARRAYSIZE(questions) && !confirmed){key(Common::KEYCODE_y,'Y');confirmed=true;lastInputPoll=_runtime->emptyKeyboardPolls();}
			else if(confirmed && !acknowledged){key(Common::KEYCODE_RETURN,13);acknowledged=true;lastInputPoll=_runtime->emptyKeyboardPolls();}
		}
		Common::Event event;
		while (_eventMan->pollEvent(event)) if (event.type == Common::EVENT_KEYDOWN) {
			uint value = event.kbd.ascii;
			switch(event.kbd.keycode) { case Common::KEYCODE_UP:value=11;break;case Common::KEYCODE_DOWN:value=10;break;case Common::KEYCODE_LEFT:value=8;break;case Common::KEYCODE_RIGHT:value=21;break;default:break; }
			if (value && value < 128) _runtime->key(value);
		}
		if (!_runtime->advance(100000,true)) { report("nox-dialogue-trace.tsv",trace); return fail(_runtime->error()); }
		if (_runtime->enhancements().sequence() > captured) {
			captured = uint(_runtime->enhancements().sequence());
			lastInputPoll=_runtime->emptyKeyboardPolls();
			for(const auto &character:_runtime->enhancements().characters())for(const auto &topic:character.topics)
				knowledge+=Common::String::format("%u\t%s\t%u\t%s\t%u\n",captured,character.resource.c_str(),character.record,topic.key.c_str(),topic.used?1:0);
			if (!frame(Common::String::format("nox-dialogue-reply-%u.png",captured))) return fail("Cannot capture actual resolved reply frame");
		}
		Common::Array<int16> &samples = _runtime->audio();
		if (!samples.empty() && _audio->numQueuedStreams() < 8) {
			byte *buffer = (byte *)malloc(samples.size()*2); if (!buffer) return fail("Cannot queue fixture audio");
			for (uint i=0;i<samples.size();++i) WRITE_LE_UINT16(buffer+i*2,samples[i]);
			_audio->queueBuffer(buffer,samples.size()*2,DisposeAfterUse::YES,Audio::FLAG_16BITS|Audio::FLAG_LITTLE_ENDIAN);
		}
		samples.clear(); _system->delayMillis(1);
	}
	report("nox-dialogue-trace.tsv",trace); frame("nox-dialogue-failure.png"); return fail("Dialogue fixture timed out or quit");
}
} // namespace Nox
#endif
