/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nox/nox.h"
#include "engines/util.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/events.h"
#include "common/system.h"
#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "nox/runtime.h"
#include "image/png.h"
#include "common/savefile.h"
#include "nox/snapshot.h"
#include "nox/saves.h"
#include "engines/metaengine.h"
#include "nox/host_validation.h"
#include "nox/metadata_validation.h"
#include "nox/caller_validation.h"
#include "gui/saveload-dialog.h"
#include "nox/grid_validation.h"
#include "nox/enhancement_fixture.h"
#include "nox/dialogue_validation.h"
#include "nox/linked_pages_validation.h"
#include "nox/search_object_validation.h"
#include "nox/campaign_validation.h"
#include "nox/vortex_validation.h"
#include "nox/spell_validation.h"
#include "nox/dark_key_validation.h"
#include "nox/cultist_validation.h"
#include "nox/quest_battle_validation.h"
#include "nox/shadow_checkpoint_validation.h"
#include "nox/shadow_confrontation_validation.h"
#include "nox/management_validation.h"
#include "nox/history_validation.h"
#include "nox/io_validation.h"
#include "nox/profile_validation.h"
#include "nox/native_startup_host.h"

namespace Nox {
struct ValidationTrace {
	Common::SaveFileManager *manager;
	Common::String text;
	bool enabled;
	const char *filename;
	~ValidationTrace() {
		if (!enabled) return;
		Common::OutSaveFile *out=manager->openForSaving(filename,false);
		if (out) {out->write(text.c_str(),text.size());out->finalize();delete out;}
	}
};
NoxEngine::~NoxEngine() {
	_mixer->stopHandle(_sound);
    _mixer->stopHandle(_nativeMusicSound);
	delete _runtime;
	_surface.free();
}
void NoxEngine::resetAudio() {
	_mixer->stopHandle(_sound);
	_audio=Audio::makeQueuingAudioStream(NOX_AUDIO_HZ,false);
	_mixer->playStream(Audio::Mixer::kPlainSoundType,&_sound,_audio);
    _mixer->stopHandle(_nativeMusicSound);
    _nativeMusicAudio=Audio::makeQueuingAudioStream(NOX_AUDIO_HZ,false);
    _mixer->playStream(Audio::Mixer::kMusicSoundType,&_nativeMusicSound,_nativeMusicAudio);
}
void NoxEngine::pauseEngineIntern(bool pause) {
    Engine::pauseEngineIntern(pause);
    if (_nativePlayer) _nativePlayer->resetPresentationClock();
}
bool NoxEngine::queueNativeMusic(const int16 *samples,uint count) {
    if (!samples || !count || count>48000 || !_nativeMusicAudio) return false;
    byte *buffer=static_cast<byte *>(malloc(count*2));if (!buffer) return false;
    for (uint i=0;i<count;++i) WRITE_LE_UINT16(buffer+i*2,uint16(samples[i]));
    _nativeMusicAudio->queueBuffer(buffer,count*2,DisposeAfterUse::YES,Audio::FLAG_16BITS|Audio::FLAG_LITTLE_ENDIAN);return true;
}
Common::String NoxEngine::getSaveStateName(int slot) const { return getMetaEngine()->getSavegameFile(slot,_targetName.c_str()); }
void NoxEngine::configureRuntimeObservation(Runtime &runtime) {
	runtime.enableCallerObservation(_targetName=="nox-gameplay-test"&&ConfMan.hasKey("nox_interface_validation")&&ConfMan.getBool("nox_interface_validation"));
	// The synthetic owner matrix deliberately supplies its own journal/cells.
	// Normal gameplay always observes real events; this is not a user option.
	runtime.suppressEnhancementCaptureForFixture(_targetName=="nox-gameplay-test"&&ConfMan.hasKey("nox_enhancement_fixture")&&ConfMan.getBool("nox_enhancement_fixture"));
}
bool NoxEngine::synchronizePlaySession() {
	const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	if(session==_runtimeSession)return true;
    if (_nativePlayer) {
        if (!_nativePlayer->reset()) return false;
        Common::CEAchievements().resetPopupState();_runtimeSession=session;
        _saveMode=ConfMan.hasKey("nox_classic_saves")&&ConfMan.getBool("nox_classic_saves")?kClassicSaveMode:kModernSaveMode;
        setTotalPlayTime(0);resetAudio();return true;
    }
	Runtime *candidate=new Runtime();
	if(!candidate->initialize(_resources)){delete candidate;return false;}
	configureRuntimeObservation(*candidate);
	Common::CEAchievements().resetPopupState();
	delete _runtime;_runtime=candidate;_runtimeSession=session;
	_saveMode=ConfMan.hasKey("nox_classic_saves")&&ConfMan.getBool("nox_classic_saves")?kClassicSaveMode:kModernSaveMode;
	setTotalPlayTime(0);resetAudio();
	return true;
}
bool NoxEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	const bool ready=(_nativePlayer?_nativePlayer->canSave():_runtime&&_runtime->canSnapshot())&&!session.empty()&&session==_runtimeSession;
	if(!ready&&msg)*msg=Common::U32String("Save at an idle world command or the final ending screen in a Play Session.");
	return ready;
}
bool NoxEngine::canLoadGameStateCurrently(Common::U32String *) { return _nativePlayer || _runtime; }
Common::Error NoxEngine::saveGameState(int slot,const Common::String &description,bool autosave) {
	if(autosave||!allowedSaveSlot(_saveMode,slot)||description.size()>512||!canSaveGameStateCurrently())return Common::Error(Common::kWritingFailed,"Cannot save at the current game state or slot");
	Saves::Metadata meta;meta.target=_targetName;meta.session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	meta.slot=slot;meta.description=description.empty()?Common::String::format("Slot %d",slot):description;meta.playtime=getTotalPlayTime();
	meta.mode=_saveMode;
	TimeDate date;_system->getTimeAndDate(date);
	meta.year=date.tm_year+1900;meta.month=date.tm_mon+1;meta.day=date.tm_mday;
	meta.hour=date.tm_hour;meta.minute=date.tm_min;meta.second=MIN(date.tm_sec,59);
	if(!Saves::validDate(meta))return Common::Error(Common::kWritingFailed,"Invalid host save date");
	// Capture this runtime's current world, not the modal GUI overlay. Keep
	// the game aspect ratio with black bars in the standard 160x120 preview.
    if (_nativePlayer) {if (!_nativePlayer->render(_surface)) return Common::Error(Common::kWritingFailed,"Native game frame unavailable");}
    else _runtime->render(_surface,0);
	meta.preview.resize(Saves::kPreviewBytes);memset(meta.preview.data(),0,meta.preview.size());
	const Graphics::PixelFormat previewFormat(2,5,6,5,0,11,5,0,0);
	const uint scaledHeight=Saves::kPreviewWidth*_surface.h/_surface.w;
	const uint top=(Saves::kPreviewHeight-scaledHeight)/2;
	for(uint y=0;y<scaledHeight;++y)for(uint x=0;x<Saves::kPreviewWidth;++x){
		const uint32 pixel=*(const uint32 *)_surface.getBasePtr(x*_surface.w/Saves::kPreviewWidth,y*_surface.h/scaledHeight);
		byte r,g,b;_surface.format.colorToRGB(pixel,r,g,b);
		WRITE_LE_UINT16(meta.preview.data()+((y+top)*Saves::kPreviewWidth+x)*2,previewFormat.RGBToColor(r,g,b));
	}
	if(meta.target.size()>256||meta.session.size()>128)return Common::kWritingFailed;
	Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);Saves::writeMetadata(state,meta);
    if (_nativePlayer) {if (!_nativePlayer->save(state)) return Common::Error(Common::kWritingFailed,"Native game state could not be captured");}
    else if(!_runtime->saveState(state))return Common::Error(Common::kWritingFailed,_runtime->error());
	const Common::String prefix=Saves::slotPrefix(_targetName,meta.session,slot);
	Saves::Entry previous;Common::Array<byte> payload;
	const bool protectedSave=Saves::best(*getSaveFileManager(),prefix,_targetName,previous,payload);
	SnapshotStore store(*getSaveFileManager(),prefix);
	uint target=0;
	if(protectedSave)target=1-previous.generation;
	else {uint32 sequence=0;uint latest=1;store.latest(payload,sequence,latest);target=1-latest;}
	const Common::String targetFile=store.filename(target);
	if(getSaveFileManager()->exists(targetFile+".ce-achievements.json")&&!getSaveFileManager()->removeSavefile(targetFile+".ce-achievements.json"))return Common::kWritingFailed;
	Common::String written;
	struct PartialWriteTest {Common::SaveFileManager *manager;Common::String report;};
	PartialWriteTest interrupt={getSaveFileManager(),Common::String::format("READY\tprotected=%s\tpartial=%s\tsession=%s\tx=%u\ty=%u\n",previous.filename.c_str(),targetFile.c_str(),meta.session.c_str(),_nativePlayer?_nativePlayer->positionX():_runtime->positionX(),_nativePlayer?_nativePlayer->positionY():_runtime->positionY())};
	const bool interruptTest=_targetName=="nox-gameplay-test"&&ConfMan.hasKey("nox_failure_validation")&&ConfMan.getInt("nox_failure_validation")==1;
	auto observePartial=[](void *context){
		PartialWriteTest &test=*static_cast<PartialWriteTest *>(context);
		Common::OutSaveFile *marker=test.manager->openForSaving("nox-partial-write-ready.tsv",false);
		if(marker){marker->write(test.report.c_str(),test.report.size());marker->finalize();delete marker;}
		// The external harness kills only its own child process during this
		// bounded window, while the actual generation file remains open.
		g_system->delayMillis(10000);
	};
	if(!store.write(state.getData(),state.size(),written,protectedSave?int(previous.generation):-1,interruptTest?+observePartial:nullptr,&interrupt))return Common::Error(Common::kWritingFailed,"Could not durably write Nox state");
	if(_targetName=="nox-gameplay-test"&&ConfMan.hasKey("nox_io_validation")&&ConfMan.getInt("nox_io_validation")==3&&!ConfMan.getBool("nox_io_cold")&&!ioWaitForLock(*getSaveFileManager(),written+".ce-achievements.json"))return Common::Error(Common::kWritingFailed,"Commit failure diagnostic lock missing");
	Common::CEAchievements().savePopupStateForSave(written);
	if(Common::CEAchievements().getSavePlaythroughForFile(written)!=meta.session)return Common::Error(Common::kWritingFailed,"Could not commit Play Session metadata");
	Common::CEAchievements().recordSaveCreated(written,slot,meta.description);
	if(Common::CEAchievements().getSavePlaythroughForFile(written)!=meta.session)return Common::Error(Common::kWritingFailed,"Could not verify committed session metadata");
	return Common::kNoError;
}
Common::Error NoxEngine::loadGameState(int slot) {
	const Common::String filename=getSaveStateName(slot);
	if(filename.empty()||(!filename.hasSuffix(".g0")&&!filename.hasSuffix(".g1")))return Common::kReadingFailed;
	Saves::Entry entry;Common::Array<byte> payload;
	if(!Saves::readGeneration(*getSaveFileManager(),filename.substr(0,filename.size()-3),filename.lastChar()-'0',_targetName,entry,payload))return Common::Error(Common::kReadingFailed,"Invalid or incomplete Nox save/session");
	bool knownSession=false;
	for(const auto &record:Common::CEAchievements().listPlaythroughsForTarget(_targetName))
		if(record.clientPlaythroughId==entry.metadata.session&&record.game=="nox-archaist"&&record.variant=="apple2-v119")knownSession=true;

    struct NativeLoadCleanup {NativePlayerSession *host;~NativeLoadCleanup(){if(host)host->discardLoad();}} staged={_nativePlayer};
    Runtime *candidate=nullptr;Common::MemoryReadStream input(payload.data(),payload.size());input.seek(entry.offset);
    if (_nativePlayer) {
        if (!_nativePlayer->stageLoad(input)) return Common::Error(Common::kReadingFailed,"Invalid or unsupported native Nox gameplay state");
    } else {
        candidate=new Runtime();
        if(!candidate->initialize(_resources)||!candidate->loadState(input)){delete candidate;return Common::Error(Common::kReadingFailed,"Invalid Nox gameplay state");}
        configureRuntimeObservation(*candidate);
    }
	Common::String owner=entry.metadata.session;
	if(knownSession){
		const bool trustedHistory=Common::CEAchievements().hasCurrentSaveHistory(filename);
		Common::CEAchievements().recordSaveLoaded(filename,entry.metadata.slot,entry.metadata.description,trustedHistory);
		if(Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=owner){delete candidate;return Common::kReadingFailed;}
		if(trustedHistory)Common::CEAchievements().loadPopupStateForSave(filename);
		else _system->displayMessageOnOSD(Common::U32String("Save history is unavailable or outdated; this Play Session is now Casual."));
	}else{
		// A fully validated committed world survives deletion of its session.
		// Recover into a NEW, permanently Casual owner; never revive deleted
		// history or trust its popup/achievement progress as a fresh clean run.
		if(!Common::CEAchievements().startRecoveredPlaythroughForTarget(_targetName,"nox-archaist","apple2-v119","Recovered: "+entry.metadata.description)){
			delete candidate;return Common::Error(Common::kReadingFailed,"Could not create a recovered Play Session");
		}
		owner=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
		Common::CEAchievements().resetPopupState();
		_system->displayMessageOnOSD(Common::U32String("Continued in a new Casual Play Session because the original session is unavailable."));
	}
    if (_nativePlayer) _nativePlayer->commitLoad();
	delete _runtime;_runtime=candidate;_runtimeSession=owner;_saveMode=entry.metadata.mode;setTotalPlayTime(entry.metadata.playtime);resetAudio();
	return Common::kNoError;
}
Common::Error NoxEngine::run() {
	const uint32 runStarted=_system->getMillis();
	Common::FSNode directory(ConfMan.getPath("path"));
	Common::FSList children;
	if (!directory.getChildren(children, Common::FSNode::kListFilesOnly))
		return Common::Error(Common::kReadingFailed, "Unable to list Nox resources");
	uint matches = 0;
	for (Common::FSList::const_iterator it = children.begin(); it != children.end(); ++it) {
		Common::String name = it->getName();
		name.toLowercase();
		if (!name.hasSuffix(".hdv"))
			continue;
		Common::File stream;
		HdvArchive candidate;
		if (stream.open(*it) && candidate.open(stream) && candidate.isBaseV119()) {
			_resources = candidate;
			++matches;
		}
	}
	if (matches != 1)
		return Common::Error(Common::kNoGameDataFoundError, "Exactly one supported Nox Archaist v119 image is required");
	if (ConfMan.hasKey("nox_session_validation") && ConfMan.getBool("nox_session_validation")) {
		Common::String error;
		return validateSessions(error) ? Common::Error(Common::kNoError) : Common::Error(Common::kUnknownError, error);
	}
	// Internal test driver, enabled only by the isolated resource harness config.
	// Ordinary launch cannot mistake validation for playable gameplay.
	if (ConfMan.hasKey("nox_resource_validation") && ConfMan.getBool("nox_resource_validation")) {
		Common::String error;
		return validateResources(error) ? Common::Error(Common::kNoError) : Common::Error(Common::kUnknownError, error);
	}
	_saveMode=ConfMan.hasKey("nox_classic_saves")&&ConfMan.getBool("nox_classic_saves")?kClassicSaveMode:kModernSaveMode;
	Common::CEAchievements().resetPopupState();
	// Explicit development startup path. It never constructs compatibility
	// Runtime and is not exposed as a complete playable native engine.
	if (ConfMan.hasKey("nox_native_startup") && ConfMan.getBool("nox_native_startup")) return runNativeStartup();
	_runtime = new Runtime();
	if (!_runtime->initialize(_resources))
		return Common::Error(Common::kReadingFailed, _runtime->error());
	configureRuntimeObservation(*_runtime);
	_runtimeSession=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	const Graphics::PixelFormat format(4, 8, 8, 8, 0, 16, 8, 0, 0);
	initGraphics(560, 384, &format);
	_surface.create(560, 384, format);
	resetAudio();
	if(ConfMan.hasKey("nox_state_profile")&&ConfMan.getInt("nox_state_profile")){
		Common::String error;if(!validateStateProfile(error))warning("Nox owner profile validation: %s",error.c_str());
		return Common::kNoError;
	}
	if(ConfMan.hasKey("nox_io_validation")&&ConfMan.getInt("nox_io_validation")){
		Common::String error;if(!validateStorageFailures(error))warning("Nox storage I/O validation failed: %s",error.c_str());return Common::kNoError;
	}
	if(ConfMan.hasKey("nox_history_validation")&&ConfMan.getInt("nox_history_validation")){
		Common::String error;if(!validateSaveHistory(error))warning("Nox save history validation failed: %s",error.c_str());return Common::kNoError;
	}
	if(ConfMan.hasKey("nox_management_validation")&&ConfMan.getInt("nox_management_validation")){
		Common::String error;if(!validateSessionManagement(error))warning("Nox session management validation failed: %s",error.c_str());return Common::kNoError;
	}
	if(ConfMan.hasKey("nox_campaign_reload")&&ConfMan.getBool("nox_campaign_reload")){
		Common::String error;validateCampaignCheckpoint(true,error);return Common::kNoError;
	}
	if(ConfMan.hasKey("nox_dialogue_reload")&&ConfMan.getBool("nox_dialogue_reload")){
		Common::String error;if(!validateDialogueFixture(true,error))warning("Nox cold dialogue validation failed: %s",error.c_str());
		return Common::kNoError; // Diagnostic harness requires a newly written PASS report; avoid an unattended error dialog.
	}
	if(ConfMan.hasKey("save_slot")){
		Common::Error loaded=loadGameState(ConfMan.getInt("save_slot"));
		if(loaded.getCode()!=Common::kNoError)return loaded;
	}
	const bool cold = ConfMan.hasKey("nox_snapshot_validation") && ConfMan.getBool("nox_snapshot_validation");
	const bool online = ConfMan.hasKey("nox_online_validation") && ConfMan.getBool("nox_online_validation");
	const int failureValidation=ConfMan.hasKey("nox_failure_validation")?ConfMan.getInt("nox_failure_validation"):0;
	const bool launcherValidation=ConfMan.hasKey("nox_launcher_validation",Common::ConfigManager::kApplicationDomain)&&ConfMan.getBool("nox_launcher_validation",Common::ConfigManager::kApplicationDomain)&&!cold;
	bool validation = cold || (ConfMan.hasKey("nox_gameplay_validation") && ConfMan.getBool("nox_gameplay_validation"));
	const bool openingReference=validation&&!cold&&_targetName=="nox-gameplay-test"&&ConfMan.hasKey("nox_opening_reference")&&ConfMan.getBool("nox_opening_reference")&&Common::CEAchievements().getLinkToken().empty();
	uint openingReferenceCount=0;
    const bool worldCommandReference=validation&&!cold&&!online&&_targetName=="nox-gameplay-test"&&ConfMan.hasKey("nox_world_command_reference")&&ConfMan.getBool("nox_world_command_reference")&&Common::CEAchievements().getLinkToken().empty();
    bool tutorialReferenceCaptured=false,openReferenceCaptured=false,referenceTactical=false;uint worldReferenceSettleFrames=0;
    const bool worldWalkReference=worldCommandReference&&ConfMan.hasKey("nox_world_walk_reference")&&ConfMan.getBool("nox_world_walk_reference");
    const bool worldContactReference=worldWalkReference&&ConfMan.hasKey("nox_world_contact_reference")&&ConfMan.getBool("nox_world_contact_reference");
    const bool encounterBoundaryReference=worldContactReference&&ConfMan.hasKey("nox_encounter_boundary_reference")&&ConfMan.getBool("nox_encounter_boundary_reference");
    const bool battleCommandReference=worldContactReference&&!encounterBoundaryReference&&ConfMan.hasKey("nox_battle_command_reference")&&ConfMan.getBool("nox_battle_command_reference");
    const bool battleControlReference=battleCommandReference&&ConfMan.hasKey("nox_battle_control_reference")&&ConfMan.getBool("nox_battle_control_reference");
    const int battleControlLimit=ConfMan.hasKey("nox_battle_control_steps")?ConfMan.getInt("nox_battle_control_steps"):4;
    const bool battleAttackReference=battleControlReference&&ConfMan.hasKey("nox_battle_attack_reference")&&ConfMan.getBool("nox_battle_attack_reference");
    uint battleControlStep=0;
    const bool formationReference=battleCommandReference&&ConfMan.hasKey("nox_formation_reference")&&ConfMan.getBool("nox_formation_reference");
    const bool formationChain=formationReference&&ConfMan.hasKey("nox_formation_chain")&&ConfMan.getBool("nox_formation_chain");
    bool formationInputCaptured=false;
    if (encounterBoundaryReference) _runtime->enableEncounterReference();
	const bool hostValidation = cold && ConfMan.hasKey("nox_host_validation") && ConfMan.getBool("nox_host_validation");
	const bool chooserValidation = cold && ConfMan.hasKey("nox_chooser_validation") && ConfMan.getBool("nox_chooser_validation");
	const bool interfaceValidation=ConfMan.hasKey("nox_interface_validation")&&ConfMan.getBool("nox_interface_validation");
	const bool continueValidation=ConfMan.hasKey("nox_continue_validation")&&ConfMan.getBool("nox_continue_validation");
	const bool metadataValidation=cold&&ConfMan.hasKey("nox_metadata_validation")&&ConfMan.getBool("nox_metadata_validation");
	const bool callerValidation=cold&&ConfMan.hasKey("nox_caller_validation")&&ConfMan.getBool("nox_caller_validation");
	const bool gridValidation=cold&&ConfMan.hasKey("nox_grid_validation")&&ConfMan.getBool("nox_grid_validation");
	const bool gridReload=ConfMan.hasKey("nox_grid_reload_validation")&&ConfMan.getBool("nox_grid_reload_validation");
	const bool modeDefaultsValidation=ConfMan.hasKey("nox_mode_defaults_validation")&&ConfMan.getBool("nox_mode_defaults_validation");
	const bool enhancementFixture=ConfMan.hasKey("nox_enhancement_fixture")&&ConfMan.getBool("nox_enhancement_fixture");
	MenuValidationSource menuSource(*this,_runtime,*_eventMan,hostValidation||chooserValidation||interfaceValidation||continueValidation||metadataValidation||gridValidation||gridReload);
	uint interfaceSaves=0,interfaceLoads=0;
	bool interfaceSucceeded=false;
	Common::String interfaceDisk,interfaceSession;
	uint32 interfaceSequence=0;
	PauseToken hostPause;
	uint hostPhase=0;
	uint32 hostStarted=0,hostTime=0,pacedMillis=0;
	uint64 hostCycles=0,hostSamples=0,pacedCycles=0;
	uint phase=0,frames=0,inputCount=0;
	bool preparingSessionB=false;
	uint64 audioSamples=0, nonzeroAudio=0;
	byte startX=0,startY=0;
	ValidationTrace trace = {getSaveFileManager(),Common::String(),validation,cold?"nox-cold-trace.tsv":"nox-gameplay-trace.tsv"};
	auto validationFailure = [&](const Common::String &message) {
		trace.text += "FAIL\t" + message + "\n";
		warning("Nox gameplay validation failed: %s",message.c_str());
		_runtime->render(_surface,_system->getMillis());
		Common::OutSaveFile *image=getSaveFileManager()->openForSaving("nox-gameplay-failure.png",false);
		if(image){Image::writePNG(*image,_surface);image->finalize();delete image;}
		// Test result is the required PASS report, never merely process exit.
		// Avoid an unattended GUI error dialog; normal gameplay errors below
		// retain their actual Common::Error return value.
		return Common::Error(Common::kNoError);
	};
	auto inject = [&](Common::KeyCode code, uint16 ascii) {
		if(code==Common::KEYCODE_INVALID && ascii>='A' && ascii<='Z')code=Common::KeyCode(ascii+32);
		Common::Event event;
		event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(code,ascii);
		_eventMan->pushEvent(event);event.type=Common::EVENT_KEYUP;_eventMan->pushEvent(event);++inputCount;
	};
	if (battleControlReference && (battleControlLimit<1 || battleControlLimit>16)) return validationFailure("Original tactical control reference budget must be1..16");
	auto persistFrame = [&](const char *filename) {
		Common::OutSaveFile *out=getSaveFileManager()->openForSaving(filename,false);
		if(!out)return false;
		const bool encoded=Image::writePNG(*out,_surface);out->finalize();
		const bool ok=encoded&&!out->err();delete out;return ok;
	};
    auto captureWorldReference=[&](const char *label,uint16 site) -> bool {
        Common::MemoryWriteStreamDynamic memory(DisposeAfterUse::YES);_runtime->campaignDiagnosticMemory(memory);
        if (memory.size()!=131072) return false;const byte *m=memory.getData();
        const Common::String prefix=Common::String("nox-original-world-")+label;
        Common::OutSaveFile *out=getSaveFileManager()->openForSaving(prefix+".bin",false);if (!out) return false;
        out->write(m,memory.size());out->finalize();bool ok=!out->err();delete out;if (!ok) return false;
        out=getSaveFileManager()->openForSaving(prefix+".banks",false);if (!out) return false;
        _runtime->campaignDiagnosticBanks(*out);out->finalize();ok=!out->err();delete out;if (!ok) return false;
        out=getSaveFileManager()->openForSaving(prefix+".tsv",false);if (!out) return false;
        out->writeString(Common::String::format("original-world\tscope=compatibility-reference-not-native-acceptance\tpc=%04X\tmap=%u/%u\tposition=%u/%u\tdoor0=%u/%u\tclock=%u/%u/%u\tmain_text_pointer=%04X\taux_text_pointer=%04X\t%s\n",site,uint(m[0x267d]),uint(m[0x2af9]),uint(m[0x6cec]),uint(m[0x6ced]),uint(m[0xbe02]),uint(m[0xbe03]),uint(m[0x2bf8]),uint(m[0x3878]),uint(m[0x3879]),READ_LE_UINT16(m+0xe4),READ_LE_UINT16(m+65536+0xe4),_runtime->diagnostic().c_str()));
        out->finalize();ok=!out->err();delete out;if (!ok) return false;
        _runtime->render(_surface,0);const Common::String png=prefix+".png";return persistFrame(png.c_str());
    };
	if(validation&&online){
		const Common::String endpoint=ConfMan.get("ce_achievements_api_base_url",Common::ConfigManager::kApplicationDomain);
		if((endpoint!="http://hermesbox:8123/api/v1"&&!endpoint.hasPrefix("http://127.0.0.1:"))||Common::CEAchievements().getLinkToken().empty())
			return validationFailure("Online validation requires dedicated loopback/hermesbox configuration");
	}
	auto waitOnline = [&]() {
		if(!online)return true;
		const uint32 deadline=_system->getMillis()+20000;
		while(_system->getMillis()<deadline&&(Common::CEAchievements().isSyncBusy()||Common::CEAchievements().getPendingEventCount())){
			Common::Event event;while(_eventMan->pollEvent(event)){}
			Common::CEAchievements().retryQueuedEvents();_system->delayMillis(10);
		}
		return !Common::CEAchievements().isSyncBusy()&&!Common::CEAchievements().getPendingEventCount();
	};
	if(validation&&!cold&&!launcherValidation&&!continueValidation&&!gridReload){
		if(!Common::CEAchievements().startPlaythroughForTarget(_targetName,"nox-archaist","apple2-v119","M1 session A")||!waitOnline()||!synchronizePlaySession())
			return validationFailure("Cannot initialize session A runtime");
	}
	if(cold){
		SnapshotStore store(*getSaveFileManager(),"nox-gameplay-snapshot-fixture");
		Common::Array<byte> payload;uint32 sequence=0;uint generation=0;
		if(!store.latest(payload,sequence,generation))return validationFailure("No valid cold gameplay snapshot");
		Common::MemoryReadStream input(payload.data(),payload.size());
		if(!_runtime->loadState(input)||!_runtime->atWorldInput())return validationFailure("Cold gameplay state did not restore its command boundary");
		startX=_runtime->positionX();startY=_runtime->positionY();phase=7;
		_runtime->render(_surface,0);
		if(!persistFrame("nox-cold-restored.png"))return validationFailure("Cannot persist cold restored frame");
	}
	uint32 previous = _system->getMillis();
	while (!shouldQuit()) {
		if(!synchronizePlaySession())return Common::Error(Common::kReadingFailed,"Cannot initialize the selected Play Session");
        if (encounterBoundaryReference) _runtime->enableEncounterReference();
        if (battleCommandReference&&phase==42) _runtime->enableBattleValidation();
        if (battleControlReference&&phase==42) _runtime->enableBattleControlObservation();
        if (formationReference&&phase==42) _runtime->enableFormationReference(formationInputCaptured);
		if(hostPhase){
			const uint32 tick=_system->getMillis();
			if(tick-hostStarted>7000)return validationFailure("Normal host lifecycle timed out");
			if(hostPhase==1&&tick-hostTime>=1000){
				pacedMillis=tick-hostTime;pacedCycles=_runtime->cpuClocks()-hostCycles;
				const uint64 expected=uint64(pacedMillis)*(NOX_CPU_HZ/1000);
				if(pacedCycles<expected*7/10||pacedCycles>expected*12/10||audioSamples==hostSamples)
					return validationFailure("Normal host clock/audio pacing differs from elapsed time");
				warning("Nox host test: normal pacing passed; pausing");
				hostPause=pauseEngine();hostTime=tick;hostCycles=_runtime->cpuClocks();hostSamples=audioSamples;hostPhase=2;
			}else if(hostPhase==2&&tick-hostTime>=150){
				if(_runtime->cpuClocks()!=hostCycles||audioSamples!=hostSamples)return validationFailure("Runtime/audio advanced while paused");
				warning("Nox host test: paused clocks/audio stable; resuming");
				hostPause.clear();hostTime=tick;hostPhase=3;
			}else if(hostPhase==3&&tick-hostTime>=300){
				if(_runtime->cpuClocks()<=hostCycles||audioSamples<=hostSamples)return validationFailure("Runtime/audio did not resume");
				warning("Nox host test: resumed; opening global menu");
				menuSource.arm();Common::Event menu;menu.type=Common::EVENT_MAINMENU;_eventMan->pushEvent(menu);hostPhase=4;
			}else if(hostPhase==4&&menuSource.closed){
				if(isPaused()||!menuSource.stable)return validationFailure("Global menu did not pause/resume cleanly");
				warning("Nox host test: global menu closed; requesting session-aware quit");
				menuSource.armQuit();
				Common::Event quit;quit.type=Common::EVENT_QUIT;_eventMan->pushEvent(quit);hostPhase=5;
			}
		}
		if (validation) {
			if (++frames > 20000) return validationFailure(Common::String::format("Timed out, phase %u PC %04X",phase,_runtime->address()));
			const uint16 site=_runtime->address();
            bool referenceInputHandled=false;
            if (formationReference&&phase==42&&_runtime->atFormationReference()) {
                if (!captureWorldReference(formationInputCaptured?"formation-output":"formation-input",site)) return validationFailure("Cannot capture original formation milestone");
                Common::OutSaveFile *context=getSaveFileManager()->openForSaving(formationInputCaptured?"nox-original-formation-output-context.tsv":"nox-original-formation-context.tsv",false);if (!context) return validationFailure("Cannot capture formation input context");
                context->writeString(_runtime->formationReferenceDiagnostic()+"\n");context->finalize();const bool ok=!context->err();delete context;if (!ok) return validationFailure("Cannot persist formation input context");
                if (formationChain&&!formationInputCaptured) { formationInputCaptured=true;_runtime->enableFormationReference(true); }
                else return Common::kNoError;
            }
            if (battleCommandReference&&frames==2000) {
                if (!captureWorldReference("battle-command-stalled",site)) return validationFailure("Cannot preserve stalled reference diagnostics");
                return validationFailure("Battle command reference did not reach its required UI boundary in2000 iterations");
            }
            if (encounterBoundaryReference&&phase==42&&_runtime->atEncounterReference()) {
                if (!captureWorldReference("encounter-handoff",site)) return validationFailure("Cannot capture original encounter handoff");
                return Common::kNoError;
            }
			if(openingReference&&!preparingSessionB&&phase==3&&(site==0x1324||site==0x1371||(site==0xd560&&!_runtime->inWorldScene()))) {
				if(openingReferenceCount>=12)return validationFailure("Original opening reference exceeded its bounded input milestones");
				const Common::String prefix=Common::String::format("nox-original-opening-%02u",openingReferenceCount++);
				Common::MemoryWriteStreamDynamic memory(DisposeAfterUse::YES);_runtime->campaignDiagnosticMemory(memory);
				if(memory.size()!=131072)return validationFailure("Original opening reference memory shape differs");
				Common::OutSaveFile *out=getSaveFileManager()->openForSaving(prefix+".bin",false);
				if(!out)return validationFailure("Cannot write original opening reference");
				out->write(memory.getData(),memory.size());out->finalize();bool ok=!out->err();delete out;
				if(!ok)return validationFailure("Original opening reference write failed");
				const byte *m=memory.getData();
				const Common::String line=Common::String::format("original-opening\tscope=compatibility-reference-not-native-acceptance\tindex=%u\tpc=%04X\tmain_text_pointer=%04X\taux_text_pointer=%04X\tleft=%u\twidth=%u\ttop=%u\tbottom=%u\tcolumn=%u\trow=%u\t%s\n",openingReferenceCount-1,site,READ_LE_UINT16(m+0xe4),READ_LE_UINT16(m+65536+0xe4),m[0x20],m[0x21],m[0x22],m[0x23],m[0x24],m[0x25],_runtime->diagnostic().c_str());
				out=getSaveFileManager()->openForSaving(prefix+".tsv",false);if(!out)return validationFailure("Cannot write original opening boundary metadata");
				out->writeString(line);out->finalize();ok=!out->err();delete out;if(!ok)return validationFailure("Original opening metadata write failed");
				if(site==0xd560) {
					_runtime->render(_surface,0);if(!persistFrame((prefix+".png").c_str()))return validationFailure("Cannot capture original opening milestone");
				}
			}
			if(gridReload&&site==0xa06b&&phase==0){
				Common::String message;
				return validateGridCold(menuSource,message)?Common::Error(Common::kNoError):validationFailure(message);
			}
			trace.text+=Common::String::format("%u\t%u\t%04X\t%u\t%u\t%s\n",frames,phase,site,_runtime->positionX(),_runtime->positionY(),_runtime->diagnostic().c_str());
            if (battleAttackReference && battleControlStep>=9 && _runtime->atWorldInput() && !_runtime->inBattle()) {
                if (_runtime->campaignFinalEnemyHealth() || !_runtime->campaignPartyHealth()) return validationFailure("Original attack chain returned without living-party victory");
                if (!captureWorldReference("battle-return",site)) return validationFailure("Cannot capture actual original battle return");
                Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-original-battle-completion.tsv",false);if (!out) return Common::kWritingFailed;
                out->writeString(Common::String::format("original-battle-completion\tissued_commands=%u\tactual_world_input=1\tenemy_hp=%u\tparty_hp=%u\t%s\n",battleControlStep,_runtime->campaignFinalEnemyHealth(),_runtime->campaignPartyHealth(),_runtime->battleDiagnostic().c_str()));
                out->finalize();const bool ok=!out->err();delete out;return ok?Common::kNoError:Common::kWritingFailed;
            }
            if (worldWalkReference&&phase==41&&site==0xd560&&_runtime->positionY()==36) {
                if (!captureWorldReference("north-notice",site)) return validationFailure("Cannot capture linked second notice");
                if (!worldContactReference) return Common::kNoError;
                inject(Common::KEYCODE_RETURN,13);inject(Common::KEYCODE_UP,0);phase=42;
            }
            if (worldContactReference&&phase==42&&(_runtime->inBattle() || (battleCommandReference&&_runtime->battleInputReady()))) {
                if (!battleCommandReference) {
                    if (!captureWorldReference("contact-battle",site)) return validationFailure("Cannot capture actual linked battle admission");
                    return Common::kNoError;
                }
                if (_runtime->battleInputReady()) {
                    if (_runtime->inBattle()&&_runtime->battlePlayerTurn()&&(site==0xd53f||site==0xfde9)) {
                        if (!battleControlStep) {
                            if (!captureWorldReference("battle-command",site)) return validationFailure("Cannot capture actual opening battle command");
                        } else {
                            // Bounded real input chain: raw state and semantic metadata,
                            // without a screenshot for each directional operation.
                            const Common::String prefix=Common::String::format("nox-original-battle-control-%02u",battleControlStep);
                            Common::MemoryWriteStreamDynamic memory(DisposeAfterUse::YES);_runtime->campaignDiagnosticMemory(memory);
                            if (memory.size()!=131072) return validationFailure("Original tactical control state shape differs");
                            Common::OutSaveFile *out=getSaveFileManager()->openForSaving(prefix+".bin",false);if (!out) return Common::kWritingFailed;
                            out->write(memory.getData(),memory.size());out->finalize();bool ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
                            const byte *m=memory.getData();out=getSaveFileManager()->openForSaving(prefix+".tsv",false);if (!out) return Common::kWritingFailed;
                            out->writeString(Common::String::format("original-battle-control\tstep=%u\tpc=%04X\tparty=%u/%u/%u/%u\tenemy=%u/%u/%u/%u\thp=%u\t%s\n",battleControlStep,site,uint(m[0xbd00]),uint(m[0xbd01]),uint(m[0xbd02]),uint(m[0xbd03]),uint(m[0xbf00]),uint(m[0xbf01]),uint(m[0xbf02]),uint(m[0xbf03]),uint(READ_LE_UINT16(m+65536+0x1602)),_runtime->diagnostic().c_str()));
                            out->writeString(_runtime->battleControlObservation()+"\n");
                            out->writeString(_runtime->battleDiagnostic()+"\n");
                            out->finalize();ok=!out->err();delete out;if (!ok) return Common::kWritingFailed;
                        }
                        if (!battleControlReference || battleControlStep==uint(battleControlLimit)) return Common::kNoError;
                        const Common::KeyCode controls[]={Common::KEYCODE_UP,Common::KEYCODE_RIGHT,Common::KEYCODE_DOWN,Common::KEYCODE_LEFT};
                        if (battleAttackReference && battleControlStep>=8) {
                            // Real physical Attack -> north -> confirmation,
                            // from the naturally reached adjacent source state.
                            inject(Common::KEYCODE_a,'A');inject(Common::KEYCODE_UP,0);inject(Common::KEYCODE_RETURN,13);++battleControlStep;
                        } else inject(controls[(battleControlStep++)&3],0);
                        referenceInputHandled=true;
                    } else {
                        if (site==0x1324&&_runtime->battleAdmissionSeen()&&!referenceTactical) { inject(Common::KEYCODE_t,'T');referenceTactical=true; }
                        else inject(Common::KEYCODE_RETURN,13);
                        referenceInputHandled=true;
                    }
                }
            }
            if (worldCommandReference&&!preparingSessionB&&phase==4&&tutorialReferenceCaptured&&worldReferenceSettleFrames>=64) {
                if (!captureWorldReference("first-east-idle",site)) return validationFailure("Cannot capture original bounded no-input tutorial state");
                inject(Common::KEYCODE_o,'O');inject(Common::KEYCODE_UP,0);phase=5;
            }
			else if(cold&&phase==13){inject(Common::KEYCODE_LEFT,0);phase=14;}
			else if(cold&&phase==14&&_runtime->atWorldInput())phase=15;
			else if(!cold && phase==10){
				PauseToken resetPause=pauseEngine();
				const uint32 oldPlaytime=getTotalPlayTime(),oldQueued=_audio->numQueuedStreams();
				if(!oldPlaytime||!oldQueued)return validationFailure(Common::String::format("Session reset requires actual prior playtime and queued gameplay audio (playtime=%u queued=%u)",oldPlaytime,oldQueued));
				const SaveMode priorMode=_saveMode;
				if(enhancementFixture&&!matchesEnhancementFixture(_runtime->enhancements(),true))return validationFailure("Session A enhancement fixture changed before reset");
				if(modeDefaultsValidation){
					Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES);
					if(!_runtime->saveState(before))return validationFailure("Cannot capture mode-default baseline");
					ConfMan.setBool("nox_classic_saves",priorMode!=kClassicSaveMode);
					if(!synchronizePlaySession()||_saveMode!=priorMode||!_runtime->saveState(after)||before.size()!=after.size()||memcmp(before.getData(),after.getData(),before.size()))return validationFailure("Default change reinterpreted current session mode/world");
				}
				if(!Common::CEAchievements().startPlaythroughForTarget(_targetName,"nox-archaist","apple2-v119","M1 session B")||!waitOnline())return validationFailure("Cannot create session B");
				if(canSaveGameStateCurrently()||saveGameState(1,"Wrong owner").getCode()==Common::kNoError)return validationFailure("Previous world can save under new session ID");
				if(!synchronizePlaySession()||getTotalPlayTime()!=0||_audio->numQueuedStreams()!=0||_runtime->atWorldInput())return validationFailure("New Play Session retained old runtime/audio/playtime");
				if(!_runtime->enhancements().characters().empty()||!_runtime->enhancements().cells().empty()||_runtime->enhancements().sequence())return validationFailure("New session retained previous enhancement state");
				if(modeDefaultsValidation){
					if(_saveMode==priorMode)return validationFailure("New session ignored changed mode default");
					ConfMan.setBool("nox_classic_saves",priorMode==kClassicSaveMode);
					if(!synchronizePlaySession()||_saveMode==priorMode)return validationFailure("Second default change reinterpreted new live session");
					Common::OutSaveFile *modeReport=getSaveFileManager()->openForSaving("nox-mode-defaults.tsv",false);
					if(!modeReport)return validationFailure("Cannot create mode-default report");
					const Common::String report=Common::String::format("PASS\tinitial_mode=%s\tnew_session_mode=%s\tactive_default_changes=2\tcurrent_mode_unchanged=1\tworld_unchanged=1\n",priorMode==kClassicSaveMode?"Classic":"Modern",_saveMode==kClassicSaveMode?"Classic":"Modern");
					modeReport->write(report.c_str(),report.size());modeReport->finalize();const bool ok=!modeReport->err();delete modeReport;
					if(!ok)return validationFailure("Cannot persist mode-default report");
				}
				Common::OutSaveFile *resetReport=getSaveFileManager()->openForSaving("nox-session-reset.tsv",false);
				if(!resetReport)return validationFailure("Cannot create session reset report");
				const Common::String report=Common::String::format("PASS\twrong_owner_rejected=1\tprior_playtime=%u\tprior_audio_queued=%u\tnew_playtime=0\tnew_audio_queued=0\tnew_world_boot=1\n",oldPlaytime,oldQueued);
				resetReport->write(report.c_str(),report.size());resetReport->finalize();const bool resetOk=!resetReport->err();delete resetReport;
				if(!resetOk)return validationFailure("Cannot persist session reset report");
				preparingSessionB=true;phase=0;continue;
			}
			else if(!cold && phase==16){inject(Common::KEYCODE_LEFT,0);phase=12;}
			else if(cold && phase==7){inject(Common::KEYCODE_LEFT,0);phase=8;}
			else if(cold && phase==8 && _runtime->atWorldInput()){
				if(startX==_runtime->positionX()&&startY==_runtime->positionY())return validationFailure("Cold continuation did not move");
				phase=9;
			}
			else if(site==0xa06b&&continueValidation&&(phase==0||phase==31)){
				menuSource.chooser(false,0,phase==0);inject(Common::KEYCODE_RETURN,13);phase=phase==0?30:32;
			}
			else if (site==0xa06b && phase==0) { inject(Common::KEYCODE_DOWN,0);inject(Common::KEYCODE_RETURN,13);phase=1; }
			else if (site==0x1324 && phase==1) {inject(Common::KEYCODE_y,'Y');phase=2;}
			else if (site==0x1371 && phase==2) {
				const char *name="TRACE";
				for (uint i=0;name[i];++i)inject(Common::KEYCODE_INVALID,name[i]);
				inject(Common::KEYCODE_RETURN,13);phase=3;
			} else if (phase==4 && site==0xd560 && (startX!=_runtime->positionX() || startY!=_runtime->positionY())) {
				// The first eastward step opens the game's Open tutorial before
				// returning to the ordinary world loop. Answer through CE events.
                if (worldCommandReference&&!preparingSessionB) {
                    if (!tutorialReferenceCaptured) {
                        if (!captureWorldReference("first-east",site)) return validationFailure("Cannot capture original first-east command milestone");
                        tutorialReferenceCaptured=true;
                    }
                    // D560 is also a bounded animation poll. Observe natural
                    // completion without injecting a key or skipping steps.
                } else {
                    inject(Common::KEYCODE_o,'O');inject(Common::KEYCODE_UP,0);phase=5;
                }
			} else if (_runtime->atWorldInput()) {
				if(phase==3){
                    // Private original-engine oracle at the first actual new-game
                    // world command, before movement. Never a player save or
                    // native initialization input; diagnostics remain local.
                    if(!online && _targetName=="nox-gameplay-test" && Common::CEAchievements().getLinkToken().empty()) {
                        Common::OutSaveFile *reference=getSaveFileManager()->openForSaving(preparingSessionB?"nox-new-game-reference-b.bin":"nox-new-game-reference-a.bin",false);
                        if(!reference)return validationFailure("Cannot open new-game reference");
                        _runtime->campaignDiagnosticMemory(*reference);reference->finalize();const bool ok=!reference->err();delete reference;
                        if(!ok)return validationFailure("Cannot capture new-game reference");
                    }
                    startX=_runtime->positionX();startY=_runtime->positionY();inject(Common::KEYCODE_RIGHT,0);phase=4;
                }
				else if(phase==4){
					if(startX==_runtime->positionX()&&startY==_runtime->positionY())return validationFailure("CE movement did not change player position");
                    if (worldCommandReference&&!preparingSessionB && !captureWorldReference("first-east-settled",site)) return validationFailure("Cannot capture original settled first step");
					inject(Common::KEYCODE_o,'O');inject(Common::KEYCODE_UP,0);phase=5;
                }
                else if(phase==5) {
                    if (worldCommandReference&&!preparingSessionB&&!openReferenceCaptured) {
                        if (!captureWorldReference("open-return",site)) return validationFailure("Cannot capture original Open return milestone");
                        openReferenceCaptured=true;
                    }
                    if (worldWalkReference) { inject(Common::KEYCODE_UP,0);phase=40; }
                    else phase=6;
                }
                else if(worldWalkReference&&phase==40) {
                    if (!captureWorldReference("door-step",site)) return validationFailure("Cannot capture linked door step");
                    inject(Common::KEYCODE_UP,0);phase=41;
                }
                else if(worldWalkReference&&phase==41) {
                    if (!captureWorldReference("north-return",site)) return validationFailure("Cannot capture linked north return");
                    return Common::kNoError;
                }
                else if(worldContactReference&&phase==42) {
                    if (!captureWorldReference("north-advance",site)) return validationFailure("Cannot capture actual north advance");
                    return Common::kNoError;
                }
				else if(phase==12)phase=11;
				else if(phase==20||phase==22){
					menuSource.chooser(true,_saveMode==kClassicSaveMode?0:1,phase==22);
					inject(Common::KEYCODE_q,'Q');++phase;
				}
			} else if(!referenceInputHandled&&((site==0xd560 && !_runtime->inWorldScene()) || (phase>=3 && (site==0x1324 || site==0x1371)))) inject(Common::KEYCODE_RETURN,13);
		}
		Common::Event event;
		while (_eventMan->pollEvent(event)) {
			if (event.type != Common::EVENT_KEYDOWN)
				continue;
			uint value = event.kbd.ascii;
			switch (event.kbd.keycode) {
			case Common::KEYCODE_UP: value=11; break;
			case Common::KEYCODE_DOWN: value=10; break;
			case Common::KEYCODE_LEFT: value=8; break;
			case Common::KEYCODE_RIGHT: value=21; break;
			default: break;
			}
			if (value && value < 128) _runtime->key(value);
		}
		const uint32 now = _system->getMillis();
		const uint32 elapsed = MIN<uint32>(now - previous, 40);
		previous = now;
		if (!isPaused() && (elapsed || validation)) {
            const bool settleWorldReference=worldCommandReference&&!preparingSessionB&&phase==4&&tutorialReferenceCaptured;
            if (settleWorldReference) ++worldReferenceSettleFrames;
			if (!_runtime->advance(validation ? 100000 : elapsed * (NOX_CPU_HZ / 1000), validation,settleWorldReference)) {
				if(validation)return validationFailure(_runtime->error());
				return Common::Error(Common::kUnknownError, _runtime->error());
			}
			const Runtime::InterfaceRequest request=_runtime->takeInterfaceRequest();
			if(request!=Runtime::kNoInterfaceRequest){
				if(request==Runtime::kSaveInterfaceRequest){++interfaceSaves;interfaceSucceeded=saveGameDialog();}
				else {++interfaceLoads;interfaceSucceeded=loadGameDialog();}
				previous=_system->getMillis(); // Modal time is not simulation time.
			}
			Common::Array<int16> &samples = _runtime->audio();
			audioSamples+=samples.size();
			for(uint i=0;i<samples.size();++i)if(samples[i])++nonzeroAudio;
			if (!samples.empty() && _audio->numQueuedStreams() < 8) {
				byte *buffer = (byte *)malloc(samples.size() * 2);
				if (!buffer) return Common::Error(Common::kUnknownError, "Nox audio buffer allocation failed");
				for (uint i=0;i<samples.size();++i) WRITE_LE_UINT16(buffer+i*2,samples[i]);
				_audio->queueBuffer(buffer,samples.size()*2,DisposeAfterUse::YES,Audio::FLAG_16BITS|Audio::FLAG_LITTLE_ENDIAN);
			}
			samples.clear();
		}
		_runtime->render(_surface, validation?0:now);
		_system->copyRectToScreen(_surface.getPixels(), _surface.pitch, 0, 0, 560, 384);
		_system->updateScreen();
		if(validation&&continueValidation&&phase==30&&interfaceLoads==1){
			if(interfaceSucceeded)return validationFailure("Continue cancellation unexpectedly loaded a save");
			phase=31;
		}
		if(validation&&continueValidation&&phase==32&&interfaceLoads==2){
			if(!interfaceSucceeded)return validationFailure("Original Continue Game did not load through CE");
			const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
			Saves::Entry entry;Common::Array<byte> data;
			if(!Saves::best(*getSaveFileManager(),Saves::slotPrefix(_targetName,session,1),_targetName,entry,data)||
			   _saveMode!=entry.metadata.mode||_runtime->positionX()!=(entry.metadata.description=="Session A slot1"?25:24)||_runtime->positionY()!=38)
				return validationFailure("Continue Game loaded inconsistent save/mode/session");
			if(!persistFrame("nox-interface-continue.png"))return validationFailure("Cannot capture original Continue frame");
			Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-interface-continue.tsv",false);
			if(!out)return validationFailure("Cannot create Continue report");
			const Common::String report="PASS\tcontinue_key=1\tcancel=1\tengine_load=1\tsaved_mode=1\tlabel="+entry.metadata.description+"\n";
			out->write(report.c_str(),report.size());out->finalize();const bool ok=!out->err();delete out;
			return ok?Common::Error(Common::kNoError):validationFailure("Cannot persist Continue report");
		}
		if(validation&&interfaceValidation&&(phase==21||phase==23)&&interfaceSaves==(phase==21?1u:2u)){
			Saves::Entry entry;Common::Array<byte> data;
			if(!Saves::best(*getSaveFileManager(),Saves::slotPrefix(_targetName,interfaceSession,1),_targetName,entry,data)||
			   entry.sequence!=interfaceSequence+1||_runtime->diskFingerprint()!=interfaceDisk||
			   Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=interfaceSession||
			   _runtime->positionX()!=24||_runtime->positionY()!=38||interfaceSucceeded!=(phase==21))
				return validationFailure("Q save/cancellation did not preserve durable slot/world/session/private disk");
			if(phase==21){phase=22;continue;}
			Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-interface-save.tsv",false);
			if(!out)return validationFailure("Cannot create Q report");
			const Common::String report="PASS\tq_key=1\tengine_save=1\tcancel=1\tprivate_disk_unchanged=1\tsession_unchanged=1\t"+_runtime->callerObservation()+"\n";
			out->write(report.c_str(),report.size());out->finalize();const bool ok=!out->err();delete out;
			return ok?Common::Error(Common::kNoError):validationFailure("Cannot persist Q report");
		}
		if(validation&&phase==15){
			if(_runtime->positionX()!=24||_runtime->positionY()!=38)return validationFailure("Interrupted overwrite preparation did not move normally");
			saveGameState(1,"Session A slot1");
			return validationFailure("Interrupted-write child was not killed during the partial write window");
		}
		if(validation && phase==9){
			if(!persistFrame("nox-cold-continued.png"))return validationFailure("Cannot persist cold continuation frame");
			const Common::String report=Common::String::format("PASS\tcold_load=1\tcontinuation=1\trestored_x=%u\trestored_y=%u\tx=%u\ty=%u\n",startX,startY,_runtime->positionX(),_runtime->positionY());
			Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-cold.tsv",false);
			if(!out)return validationFailure("Cannot create cold gameplay report");
			out->write(report.c_str(),report.size());out->finalize();const bool ok=!out->err();delete out;
			if(!ok)return validationFailure("Cannot persist cold gameplay report");
			const SaveStateList loadList=getMetaEngine()->listSaves(_targetName.c_str(),false);
			if(loadList.size()!=2)return validationFailure("Cold Engine load list did not retain two session slots");
			Common::String a,b;
			for(const auto &save:loadList){
				const Common::String file=getSaveStateName(save.getSaveSlot());
				Saves::Entry entry;Common::Array<byte> data;
				if(!Saves::readGeneration(*getSaveFileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',_targetName,entry,data))return validationFailure("Cold Engine metadata invalid");
				if(loadGameState(save.getSaveSlot()).getCode()!=Common::kNoError||_audio->numQueuedStreams()!=0)return validationFailure("Cold Engine load/audio reset failed");
				if(_saveMode!=entry.metadata.mode)return validationFailure("Cold Engine load did not restore saved mode");
				const bool isA=entry.metadata.description=="Session A slot1";
				if(enhancementFixture&&!matchesEnhancementFixture(_runtime->enhancements(),isA))return validationFailure("Cold save restored another session's enhancement fixture");
				if(entry.metadata.slot!=1||_runtime->positionX()!=(isA?25:24)||_runtime->positionY()!=38)return validationFailure("Cold Engine loaded another session's position");
				_runtime->render(_surface,0);
				if(!persistFrame(isA?"nox-engine-cold-a.png":"nox-engine-cold-b.png"))return validationFailure("Cannot persist Engine cold frame");
				if(Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=entry.metadata.session)return validationFailure("Cold Engine session identity changed");
				(isA?a:b)=entry.metadata.session;
			}
			if(a.empty()||b.empty()||a==b||Common::CEAchievements().listPlaythroughsForTarget(_targetName).size()!=2)return validationFailure("Cold Engine session registry differs");
			const SaveStateList saveList=getMetaEngine()->listSaves(_targetName.c_str(),true);
			if(saveList.size()!=1||saveList[0].getSaveSlot()!=1)return validationFailure("Engine save chooser exposes another session's logical slots");
			if(!waitOnline())return validationFailure("Online cold-load metadata did not synchronize");
			const Common::String engineReport="PASS\tsessions=2\tlogical_slot=1\tengine_cold_loads=2\taudio_flushes=2\tsaved_mode_restored=1\n";
			out=getSaveFileManager()->openForSaving("nox-engine-cold.tsv",false);
			if(!out)return validationFailure("Cannot create Engine cold report");
			out->write(engineReport.c_str(),engineReport.size());out->finalize();const bool engineOk=!out->err();delete out;
			if(!engineOk)return validationFailure("Cannot persist Engine cold report");
			if(failureValidation){
				const SaveStateList failureList=getMetaEngine()->listSaves(_targetName.c_str(),false);
				bool loadedA=false;
				for(const auto &save:failureList)if(save.getDescription()=="Session A slot1")loadedA=loadGameState(save.getSaveSlot()).getCode()==Common::kNoError;
				if(!loadedA||_runtime->positionX()!=25||_runtime->positionY()!=38)return validationFailure("Previous actual session A generation did not recover");
				if(failureValidation==1){phase=13;continue;}
				const Common::String active=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
				const Common::String prefix=Saves::slotPrefix(_targetName,active,1);
				Saves::Entry valid;Common::Array<byte> payload;
				if(!Saves::best(*getSaveFileManager(),prefix,_targetName,valid,payload))return validationFailure("Recovery generation missing");
				Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES);
				if(!_runtime->saveState(before))return validationFailure("Cannot capture live rollback baseline");
				payload.resize(payload.size()-1);Common::String corrupt;
				SnapshotStore corruptStore(*getSaveFileManager(),prefix);
				if(!corruptStore.write(payload.data(),payload.size(),corrupt,valid.generation))return validationFailure("Cannot prepare checksummed invalid-runtime test save");
				Common::CEAchievements().savePopupStateForSave(corrupt);
				int corruptSlot=-1;
				for(const auto &save:getMetaEngine()->listSaves(_targetName.c_str(),false))if(getSaveStateName(save.getSaveSlot())==corrupt)corruptSlot=save.getSaveSlot();
				if(corruptSlot<0||loadGameState(corruptSlot).getCode()==Common::kNoError)return validationFailure("Corrupt runtime save was not rejected");
				if(!_runtime->saveState(after)||before.size()!=after.size()||memcmp(before.getData(),after.getData(),before.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=active)
					return validationFailure("Rejected load replaced live runtime/session");
				const Common::String oversized(513,'x');
				if(saveGameState(1,oversized).getCode()==Common::kNoError)return validationFailure("Invalid save was reported successful");
				Common::MemoryWriteStreamDynamic afterSave(DisposeAfterUse::YES);
				if(!_runtime->saveState(afterSave)||before.size()!=afterSave.size()||memcmp(before.getData(),afterSave.getData(),before.size())||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=active)
					return validationFailure("Rejected save changed live runtime/session");
				out=getSaveFileManager()->openForSaving("nox-failure.tsv",false);
				if(!out)return validationFailure("Cannot create failure transaction report");
				const Common::String failureReport="PASS\trecovered_x=25\trecovered_y=38\tcorrupt_load_rejected=1\tlive_state_unchanged=1\tsession_unchanged=1\tfailed_save_rejected=1\n";
				out->write(failureReport.c_str(),failureReport.size());out->finalize();const bool failureOk=!out->err();delete out;
				return failureOk?Common::Error(Common::kNoError):validationFailure("Cannot persist failure transaction report");
			}
			if(chooserValidation){
				ConfMan.set("gui_saveload_chooser","list",Common::ConfigManager::kApplicationDomain);
				uint allowed=0;
				for(int slot=0;slot<=kMaximumSaveSlot;++slot)if(getMetaEngine()->isSaveSlotAllowed(_targetName.c_str(),slot))++allowed;
				if(allowed!=(_saveMode==kClassicSaveMode?2u:1000u)||getAutosaveSlot()!=-1||getMetaEngine()->getAutosaveSlot()!=-1||
				   getMetaEngine()->isSaveSlotAllowed(_targetName.c_str(),-1)||getMetaEngine()->isSaveSlotAllowed(_targetName.c_str(),1000))
					return validationFailure("Chooser/engine slot or autosave policy disagrees");
				if(_saveMode==kClassicSaveMode){
					const int denied[]={-1,0,3,999,1000};
					for(uint i=0;i<ARRAYSIZE(denied);++i)if(saveGameState(denied[i],"Rejected slot").getCode()==Common::kNoError||!getMetaEngine()->getSavegameFile(denied[i],_targetName.c_str()).empty())
						return validationFailure("Classic save or filename resolver accepted an invalid logical slot");
				}
				if(saveGameState(1,"Rejected autosave",true).getCode()==Common::kNoError)return validationFailure("Autosave bypassed manual-save policy");
				const Common::String beforeSession=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
				Saves::Entry beforeChooser;Common::Array<byte> beforeData;
				const Common::String beforePrefix=Saves::slotPrefix(_targetName,beforeSession,1);
				if(!Saves::best(*getSaveFileManager(),beforePrefix,_targetName,beforeChooser,beforeData))return validationFailure("Chooser baseline save missing");
				menuSource.chooser(true,_saveMode==kClassicSaveMode?0:1);
				if(!saveGameDialog())return validationFailure("Actual Engine save chooser failed");
				if(!menuSource.chooserCaptured)return validationFailure("Actual save chooser image missing");
				Saves::Entry afterChooser;Common::Array<byte> afterData;
				if(!Saves::best(*getSaveFileManager(),beforePrefix,_targetName,afterChooser,afterData)||
				   afterChooser.sequence<=beforeChooser.sequence||afterChooser.metadata.description!=beforeChooser.metadata.description)
					return validationFailure("Chooser changed the save description or did not overwrite logical slot1");
				menuSource.chooser(false);
				if(!loadGameDialog())return validationFailure("Actual Engine load chooser failed");
				const Common::String active=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
				Saves::Entry selected;Common::Array<byte> data;
				if(!Saves::best(*getSaveFileManager(),Saves::slotPrefix(_targetName,active,1),_targetName,selected,data)||
				   _runtime->positionX()!=(selected.metadata.description=="Session A slot1"?25:24)||_runtime->positionY()!=38)
					return validationFailure("Chooser loaded inconsistent session/gameplay state");
				out=getSaveFileManager()->openForSaving("nox-chooser.tsv",false);
				if(!out)return validationFailure("Cannot create chooser report");
				const Common::String chooserReport=Common::String::format("PASS\tengine_save_dialog=1\tengine_load_dialog=1\tlogical_slot=1\tsession_state=1\tallowed_slots=%u\tpolicy_guards=1\n",allowed);
				out->write(chooserReport.c_str(),chooserReport.size());out->finalize();const bool chooserOk=!out->err();delete out;
				if(!chooserOk)return validationFailure("Cannot persist chooser report");
			}
			if(metadataValidation){
				Common::String message;
				if(!validateSaveManagement(*this,_targetName,message))return validationFailure(message);
#ifndef DISABLE_SAVELOADCHOOSER_GRID
				const bool hadScale=ConfMan.hasKey("gui_scale",Common::ConfigManager::kApplicationDomain);
				const int oldScale=hadScale?ConfMan.getInt("gui_scale",Common::ConfigManager::kApplicationDomain):100;
				const Common::String oldChooser=ConfMan.get("gui_saveload_chooser",Common::ConfigManager::kApplicationDomain);
				// Exercise the supported GUI scaling setting without changing the
				// game's framebuffer or its input/world coordinate system.
				ConfMan.setInt("gui_scale",50,Common::ConfigManager::kApplicationDomain);g_gui.computeScaleFactor();
				ConfMan.set("gui_saveload_chooser","grid",Common::ConfigManager::kApplicationDomain);
				const GUI::SaveLoadChooserType requested=GUI::getRequestedSaveLoadDialog(getMetaEngine(),_targetName.c_str());
				const bool correctType=g_gui.getGUIWidth()>=640&&g_gui.getGUIHeight()>=400&&
					requested==(_saveMode==kClassicSaveMode?GUI::kSaveLoadDialogList:GUI::kSaveLoadDialogGrid);
				menuSource.chooser(true,0,true,"nox-metadata-chooser-ui.png");
				const bool unexpectedlySaved=saveGameDialog();
				ConfMan.set("gui_saveload_chooser",oldChooser,Common::ConfigManager::kApplicationDomain);
				if(hadScale)ConfMan.setInt("gui_scale",oldScale,Common::ConfigManager::kApplicationDomain);
				else ConfMan.removeKey("gui_scale",Common::ConfigManager::kApplicationDomain);
				g_gui.computeScaleFactor();
				if(!correctType||unexpectedlySaved||!menuSource.cancelCaptured)return validationFailure("Actual metadata/grid dialog or Classic list policy failed");
				Common::OutSaveFile *ui=getSaveFileManager()->openForSaving("nox-metadata-ui.tsv",false);
				if(!ui)return validationFailure("Cannot create metadata UI report");
				const Common::String uiReport=Common::String::format("PASS\trequested_grid=1\tactual=%s\tcancelled=1\tcaptured=1\n",requested==GUI::kSaveLoadDialogGrid?"grid":"classic-list");
				ui->write(uiReport.c_str(),uiReport.size());ui->finalize();const bool uiOk=!ui->err();delete ui;
				if(!uiOk)return validationFailure("Cannot persist metadata UI report");
#endif
			}
			if(callerValidation){
				Common::String message;
				if(!validateSaveCallers(message,runStarted))return validationFailure(message);
			}
			if(gridValidation){
				Common::String message;
				if(!validateGridWrites(menuSource,message))return validationFailure(message);
			}
			if(!hostValidation)return Common::kNoError;
			validation=false;hostPhase=1;hostStarted=hostTime=previous=_system->getMillis();
			hostCycles=_runtime->cpuClocks();hostSamples=audioSamples;
		}
		if(validation && phase==11){
			if(_runtime->positionX()!=24||_runtime->positionY()!=38)return validationFailure("Session B preparation did not move normally");
			if(!preparingSessionB||!waitOnline()||
			   saveGameState(1,"Session B slot1").getCode()!=Common::kNoError||saveGameState(1,"Session B slot1").getCode()!=Common::kNoError)return validationFailure("Cannot durably save session B");
			if(!waitOnline())return validationFailure("Online session saves did not synchronize");
			if(!persistFrame("nox-engine-saved-b.png"))return validationFailure("Cannot persist session B frame");
			if(getMetaEngine()->listSaves(_targetName.c_str(),false).size()!=2)return validationFailure("Session slot saves collided");
			if(enhancementFixture){
				if(!matchesEnhancementFixture(_runtime->enhancements(),false))return validationFailure("Session B enhancement fixture changed during save");
				Common::OutSaveFile *fixture=getSaveFileManager()->openForSaving("nox-enhancement-fixture.tsv",false);
				if(!fixture)return validationFailure("Cannot report controlled enhancement fixture");
				const Common::String text="PASS\tprovenance=controlled-owner-fixture\tactual_engine_saves=2\tnew_session_reset=1\tnonempty_journal=1\tobserved_revealed_distinct=1\n";
				fixture->write(text.c_str(),text.size());fixture->finalize();const bool fixtureOk=!fixture->err();delete fixture;
				if(!fixtureOk)return validationFailure("Cannot persist enhancement fixture report");
			}
			const Common::String report="PASS\tsessions=2\tlogical_slot=1\tgeneration_writes=4\n";
			Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-engine-saves.tsv",false);
			if(!out)return validationFailure("Cannot create Engine saves report");
			out->write(report.c_str(),report.size());out->finalize();const bool ok=!out->err();delete out;
			if(interfaceValidation){
				Saves::Entry entry;Common::Array<byte> data;
				interfaceSession=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
				if(!Saves::best(*getSaveFileManager(),Saves::slotPrefix(_targetName,interfaceSession,1),_targetName,entry,data))return validationFailure("Q baseline save unavailable");
				interfaceSequence=entry.sequence;interfaceDisk=_runtime->diskFingerprint();phase=20;continue;
			}
			return ok?Common::Error(Common::kNoError):validationFailure("Cannot persist Engine saves report");
		}
		if(validation && phase==6){
			if(ConfMan.hasKey("nox_campaign_validation")&&ConfMan.getBool("nox_campaign_validation")){
				Common::String error;validateCampaignCheckpoint(false,error);return Common::kNoError;
			}
			if(ConfMan.hasKey("nox_dialogue_validation")&&ConfMan.getBool("nox_dialogue_validation")){
				Common::String error;const bool ok=validateDialogueFixture(false,error);return ok?Common::Error(Common::kNoError):validationFailure(error);
			}
			if(enhancementFixture&&!seedEnhancementFixture(_runtime->enhancements(),!preparingSessionB))return validationFailure("Cannot establish isolated controlled enhancement fixture");
			if(preparingSessionB){phase=16;continue;}
            // Preserve actual queued gameplay audio while unrelated owner,
            // codec and file checks run. Otherwise slow validation bookkeeping
            // can drain the mixer before the unchanged reset precondition.
            PauseToken bookkeepingPause=pauseEngine();
            Common::String ownerReport;
            if(!_runtime->validateOwnerContracts(_resources,ownerReport))return validationFailure(_runtime->error());
            Common::OutSaveFile *ownerOut=getSaveFileManager()->openForSaving("nox-owner-contracts.tsv",false);
            if(!ownerOut)return validationFailure("Cannot create owner report");
            ownerOut->write(ownerReport.c_str(),ownerReport.size());ownerOut->finalize();const bool ownerOk=!ownerOut->err();delete ownerOut;
            if(!ownerOk)return validationFailure("Cannot persist owner report");

			// Verify the complete codec against a real command-boundary state,
			// not a fabricated RAM fixture. Engine save-slot integration is separate.
			Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);
			if(!_runtime->saveState(state))return validationFailure(_runtime->error());
			Runtime *candidate=new Runtime();
			Common::MemoryReadStream input(state.getData(),state.size());
			bool codecOk=candidate->initialize(_resources)&&candidate->loadState(input);
			Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);
			codecOk=codecOk&&candidate->saveState(restored)&&state.size()==restored.size()&&!memcmp(state.getData(),restored.getData(),state.size());
			delete candidate;
			if(!codecOk)return validationFailure("Complete runtime codec round-trip differs");
			candidate=new Runtime();
			Common::MemoryReadStream truncated(state.getData(),state.size()-1);
			codecOk=candidate->initialize(_resources)&&!candidate->loadState(truncated);
			delete candidate;
			if(!codecOk)return validationFailure("Truncated runtime snapshot accepted");
			SnapshotStore store(*getSaveFileManager(),"nox-gameplay-snapshot-fixture");
			Common::String written;
			if(!store.write(state.getData(),state.size(),written))return validationFailure("Cannot persist actual gameplay snapshot fixture");
			Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-gameplay.png",false);
			if(!out)return Common::Error(Common::kWritingFailed,"Cannot create gameplay image");
			bool ok=Image::writePNG(*out,_surface);out->finalize();ok=ok&&!out->err();delete out;
			if(!ok)return Common::Error(Common::kWritingFailed,"Cannot persist gameplay image");
			const Common::String report=Common::String::format("PASS\tnew_game=1\tmovement=1\tmodal_open=1\tframes=%u\tinputs=%u\taudio_samples=%llu\taudio_nonzero=%llu\tnative_sector_calls=%llu\tx=%u\ty=%u\n",frames,inputCount,(unsigned long long)audioSamples,(unsigned long long)nonzeroAudio,(unsigned long long)_runtime->nativeMovementCalls(),_runtime->positionX(),_runtime->positionY());
			out=getSaveFileManager()->openForSaving("nox-gameplay.tsv",false);
			if(!out)return Common::Error(Common::kWritingFailed,"Cannot create gameplay report");
			out->write(report.c_str(),report.size());out->finalize();ok=!out->err();delete out;
			if(!ok)return Common::Error(Common::kWritingFailed,"Cannot persist gameplay report");
			if(!online&&!Common::CEAchievements().getLinkToken().empty())return validationFailure("Offline Engine save driver must not use a CE Link token");
			if(launcherValidation&&(!ConfMan.hasKey("nox_launcher_action_verified",Common::ConfigManager::kTransientDomain)||
			   !ConfMan.getBool("nox_launcher_action_verified",Common::ConfigManager::kTransientDomain)||Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName).empty()))
				return validationFailure("Actual launcher Start/session-name flow was not demonstrated");
			if(!waitOnline()||
			   saveGameState(1,"Session A slot1").getCode()!=Common::kNoError||saveGameState(1,"Session A slot1").getCode()!=Common::kNoError)return validationFailure("Cannot durably save session A");
			if(launcherValidation){
				out=getSaveFileManager()->openForSaving("nox-launcher.tsv",false);
				if(!out)return validationFailure("Cannot create launcher report");
				const Common::String launcherReport="PASS\tlauncher_start=1\tsession_name_start=1\topening_gameplay=1\tdurable_save=1\n";
				out->write(launcherReport.c_str(),launcherReport.size());out->finalize();const bool launcherOk=!out->err();delete out;
				if(!launcherOk)return validationFailure("Cannot persist launcher report");
			}
			phase=10;
		}
		_system->delayMillis(validation?1:4);
	}
	if(hostValidation){
		if(hostPhase!=5)return validationFailure("Host quit did not follow completed lifecycle checks");
		const Common::String report=Common::String::format("PASS\tpaced_ms=%u\tcpu_clocks=%llu\tpause=1\tresume=1\tglobal_menu=1\tquit=1\n",pacedMillis,(unsigned long long)pacedCycles);
		Common::OutSaveFile *out=getSaveFileManager()->openForSaving("nox-host.tsv",false);
		if(!out)return validationFailure("Cannot create host report");
		out->write(report.c_str(),report.size());out->finalize();const bool ok=!out->err();delete out;
		if(!ok)return validationFailure("Cannot persist host report");
	}
	return Common::kNoError;
}
} // namespace Nox
