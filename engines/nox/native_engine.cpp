/* SPDX-License-Identifier: GPL-3.0-or-later */
// Native-only Engine host. The compatibility/reference host remains in nox.cpp.
#ifndef NOX_NATIVE_ONLY
#error native_engine.cpp requires the native-only build target
#endif
#include "nox/nox.h"
#include "engines/util.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/events.h"
#include "common/system.h"
#include "common/savefile.h"
#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "image/png.h"
#include "nox/snapshot.h"
#include "nox/saves.h"
#include "engines/metaengine.h"
#include "gui/saveload-dialog.h"
#include "nox/native_speaker.h"
#include "nox/native_cwspeech.h"
#include "nox/native_startup_host.h"
namespace Nox {
NoxEngine::~NoxEngine() {
	_mixer->stopHandle(_sound);
    _mixer->stopHandle(_nativeMusicSound);
	_surface.free();
}
void NoxEngine::resetAudio() {
	_mixer->stopHandle(_sound);
    _nativeEffectAudio=nullptr;
    _mixer->stopHandle(_nativeMusicSound);
    _nativeMusicAudio=Audio::makeQueuingAudioStream(NOX_AUDIO_HZ,false);
    _mixer->playStream(Audio::Mixer::kMusicSoundType,&_nativeMusicSound,_nativeMusicAudio);
    _mixer->setChannelVolume(_nativeMusicSound,(_nativeSoundMode==1 || _nativeSoundMode==2)?0:Audio::Mixer::kMaxChannelVolume);
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
bool NoxEngine::playNativeRefusal() {return playNativeSpeaker(Native::kSpeakerRefusal);}
bool NoxEngine::playNativeCast() {return playNativeSpeaker(Native::kSpeakerSelection);}
bool NoxEngine::playNativeSpeaker(byte cue) {
    if (_nativeSoundMode==1 || _nativeSoundMode==3) return true;
    Common::Array<int16> samples;uint toggles;
    if (cue&Native::kCwSpeechFirst) {
        if (!Native::CwSpeechEffect::render(_resources,byte(cue&127),samples,toggles)) return false;
    } else if (cue>Native::kSpeakerPartyHit || !Native::SpeakerEffect::render(_resources,Native::SpeakerCue(cue),samples,toggles)) return false;
    byte *buffer=static_cast<byte *>(malloc(samples.size()*2));if (!buffer) return false;
    for (uint i=0;i<samples.size();++i) WRITE_LE_UINT16(buffer+i*2,uint16(samples[i]));
    if (!_mixer->isSoundHandleActive(_sound)) {
        _nativeEffectAudio=Audio::makeQueuingAudioStream(NOX_AUDIO_HZ,false);
        _mixer->playStream(Audio::Mixer::kSFXSoundType,&_sound,_nativeEffectAudio);
    }
    _nativeEffectAudio->queueBuffer(buffer,samples.size()*2,DisposeAfterUse::YES,Audio::FLAG_16BITS|Audio::FLAG_LITTLE_ENDIAN);
    ++_nativeEffectsPlayed;_nativeEffectSamples+=samples.size();_nativeEffectToggles+=toggles;return true;
}
Common::String NoxEngine::getSaveStateName(int slot) const { return getMetaEngine()->getSavegameFile(slot,_targetName.c_str()); }
bool NoxEngine::synchronizePlaySession() {
	const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	if(session==_runtimeSession)return true;
    if (_nativePlayer) {
        if (!_nativePlayer->reset()) return false;
        Common::CEAchievements().resetPopupState();_runtimeSession=session;
        _saveMode=ConfMan.hasKey("nox_classic_saves")&&ConfMan.getBool("nox_classic_saves")?kClassicSaveMode:kModernSaveMode;
        setTotalPlayTime(0);resetAudio();return true;
    }
	return false; // Native owner is registered by runNativeStartup.

}
bool NoxEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	const bool ready=(_nativePlayer && _nativePlayer->canSave())&&!session.empty()&&session==_runtimeSession;
	if(!ready&&msg)*msg=Common::U32String("Save at an idle world command or the final ending screen in a Play Session.");
	return ready;
}
bool NoxEngine::canLoadGameStateCurrently(Common::U32String *) { return _nativePlayer != nullptr; }
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
    else return Common::kWritingFailed;
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
    else return Common::kWritingFailed;
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
	PartialWriteTest interrupt={getSaveFileManager(),Common::String::format("READY\tprotected=%s\tpartial=%s\tsession=%s\tx=%u\ty=%u\n",previous.filename.c_str(),targetFile.c_str(),meta.session.c_str(),_nativePlayer->positionX(),_nativePlayer->positionY())};
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
    Common::MemoryReadStream input(payload.data(),payload.size());input.seek(entry.offset);
    if (!_nativePlayer || !_nativePlayer->stageLoad(input))
        return Common::Error(Common::kReadingFailed,"Invalid or unsupported native Nox gameplay state");
	Common::String owner=entry.metadata.session;
	if(knownSession && !_nativePlayer->stagedLegacy()){
		const bool trustedHistory=Common::CEAchievements().hasCurrentSaveHistory(filename);
		Common::CEAchievements().recordSaveLoaded(filename,entry.metadata.slot,entry.metadata.description,trustedHistory);
		if(Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=owner){return Common::kReadingFailed;}
		if(trustedHistory)Common::CEAchievements().loadPopupStateForSave(filename);
		else _system->displayMessageOnOSD(Common::U32String("Save history is unavailable or outdated; this Play Session is now Casual."));
	}else{
		// A fully validated committed world survives deletion of its session.
		// Recover into a NEW, permanently Casual owner; never revive deleted
		// history or trust its popup/achievement progress as a fresh clean run.
		if(!Common::CEAchievements().startRecoveredPlaythroughForTarget(_targetName,"nox-archaist","apple2-v119","Recovered: "+entry.metadata.description)){
			return Common::Error(Common::kReadingFailed,"Could not create a recovered Play Session");
		}
		owner=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
		Common::CEAchievements().resetPopupState();
		_system->displayMessageOnOSD(Common::U32String("Continued in a new Casual Play Session because the original session is unavailable."));
	}
    if (_nativePlayer) {_nativePlayer->commitLoad();_nativeStartupMenu=false;}
	_runtimeSession=owner;_saveMode=entry.metadata.mode;setTotalPlayTime(entry.metadata.playtime);resetAudio();
	return Common::kNoError;
}
Common::Error NoxEngine::run() {
    const int mode=ConfMan.hasKey("nox_sound_mode")?ConfMan.getInt("nox_sound_mode"):0;_nativeSoundMode=mode>=0 && mode<4?byte(mode):0;
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
    _saveMode=ConfMan.hasKey("nox_classic_saves")&&ConfMan.getBool("nox_classic_saves")?kClassicSaveMode:kModernSaveMode;
    Common::CEAchievements().resetPopupState();
    // Native-only builds have no compatibility fallback or opt-in requirement.
    return runNativeStartup();
}
} // namespace Nox

#include "nox/native_test_cli.h"
