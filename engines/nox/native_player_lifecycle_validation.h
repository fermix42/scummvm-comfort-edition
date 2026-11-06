/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PLAYER_LIFECYCLE_VALIDATION_H
#define NOX_NATIVE_PLAYER_LIFECYCLE_VALIDATION_H
#include "nox/native_player_session.h"
#include "nox/saves.h"
#include "nox/host_validation.h"
#include "common/ptr.h"
namespace Nox {
// Isolated offline integration driver. It uses real Engine save/load APIs and
// actual Q/character-creation/story input, never patches an operation's result.
class NativePlayerLifecycleProbe {
public:
    NativePlayerLifecycleProbe(NoxEngine &engine,Native::PlayerSessionState &player,int phase) : _engine(engine),_player(player),_phase(phase),_stage(0),_nameIndex(0),_generated(0),_qStable(false),
        _menu(engine,engine._runtime,*engine._eventMan,phase!=0),_initialMode(engine.saveMode()) {}
    bool active() const {return _phase!=0;}
    bool finished() const {return _stage==4;}
    bool start(Common::String &error) {
        if (!active()) return true;
        Common::CEAchievementService &ce=Common::CEAchievements();
        if (_engine._targetName!="nox-native-player-test" || !ce.getLinkToken().empty() || _engine._runtime || _engine._nativePlayer!=&_player || _phase<1 || _phase>8) return reject(error,"Native lifecycle probe requires its isolated offline target");
        if (_phase>=5) return startEnding(error);
        if (_phase==2 || _phase==4) {
            // Generic startup save_slot already invoked the actual Engine load.
            if (!_player.canSave() || !sameExpected("nox-native-player-expected-A.bin") || _engine._runtime) return reject(error,"Native cold startup did not restore the complete saved player state");
            bool assisted=false;for (const auto &record:ce.listPlaythroughsForTarget(_engine._targetName)) if (record.clientPlaythroughId==_engine._runtimeSession) assisted=record.casual;
            if (!assisted || !writeReport(_phase==4?"recovered-cold":"cold",-1)) return reject(error,"Native cold session assistance or report mismatch");_stage=4;return true;
        }
        if (_phase==3) {
            if (!_player.canSave() || !sameExpected("nox-native-player-expected-A.bin")) return reject(error,"Native recovery baseline mismatch");
            _ownerA=_engine._runtimeSession;Saves::Entry original;Common::Array<byte> payload;
            if (!Saves::best(*_engine.getSaveFileManager(),Saves::slotPrefix(_engine._targetName,_ownerA,1),_engine._targetName,original,payload) ||
                !ce.deletePlaythroughForTarget(_engine._targetName,_ownerA)) return reject(error,"Isolated native recovery owner could not be removed");
            const int handle=findHandle("Native lifecycle A");
            if (handle<0 || _engine.loadGameState(handle).getCode()!=Common::kNoError || _engine._runtimeSession.empty() || _engine._runtimeSession==_ownerA ||
                !sameExpected("nox-native-player-expected-A.bin") || _engine.saveGameState(1,"Native recovered continuation").getCode()!=Common::kNoError) return reject(error,"Native orphaned save did not recover into a new owner");
            for (const auto &record:ce.listPlaythroughsForTarget(_engine._targetName)) if (record.clientPlaythroughId==_ownerA) return reject(error,"Native recovery recreated the deleted owner");
            Saves::Entry retained;Common::Array<byte> after;
            if (!Saves::best(*_engine.getSaveFileManager(),Saves::slotPrefix(_engine._targetName,_ownerA,1),_engine._targetName,retained,after) || !equal(payload,after) ||
                !writeReport("recovery",findHandle("Native recovered continuation"))) return reject(error,"Native recovery changed the original save or report");
            _stage=4;return true;
        }
        if (!_player.canSave() || !ce.startPlaythroughForTarget(_engine._targetName,"nox-archaist","apple2-v119","Native lifecycle A")) return reject(error,"Native lifecycle session A could not start");
        _ownerA=ce.getCurrentPlaythroughForTarget(_engine._targetName);_engine._runtimeSession=_ownerA;
        ce.noteDisqualifyingAssistance("nox_native_controlled_checkpoint");
        // Declared controlled cue before the tested world/save operations.
        // Actual source cue placement remains a separate integration criterion.
        int16 audio[4800];
        if (!_player.music.splash(_engine._resources) || !_player.music.render(audio,4800) || !_player.music.nonzero() || !_engine.queueNativeMusic(audio,4800)) return reject(error,"Native authored music did not reach the Engine mixer service");
        // Exercise the private linked-checkpoint envelope with live audio,
        // independently of the normal NPLS player-save envelope below.
        Common::MemoryWriteStreamDynamic checkpoint(DisposeAfterUse::YES);
        if (!Native::ExplorationSave::save(checkpoint,_player.game,_player.window,_player.prepared,_player.turns,_player.routes,_player.random,_player.text,
                _player.frame,_player.visibility,_player.light,_player.worldInput,_player.animationState,_player.encounterState,&_player.music)) return reject(error,"Native audio checkpoint could not be saved");
        Common::ScopedPtr<Native::PlayerSessionState> restored(new Native::PlayerSessionState(_engine._resources));Common::MemoryReadStream checkpointInput(checkpoint.getData(),checkpoint.size());
        if (!restored->stageLoad(checkpointInput)) return reject(error,"Native audio checkpoint could not be staged");restored->commitLoad();
        Common::MemoryWriteStreamDynamic beforeAudio(DisposeAfterUse::YES),afterAudio(DisposeAfterUse::YES);
        if (!_player.music.save(beforeAudio) || !restored->music.save(afterAudio) || beforeAudio.size()!=afterAudio.size() || memcmp(beforeAudio.getData(),afterAudio.getData(),beforeAudio.size())) return reject(error,"Native checkpoint lost audio state");
        Native::MusicPlayback expected=_player.music;int16 expectedPcm[4800],actualPcm[4800];
        if (!expected.render(expectedPcm,4800) || !restored->music.render(actualPcm,4800) || memcmp(expectedPcm,actualPcm,sizeof(expectedPcm))) return reject(error,"Native checkpoint audio continuation differed");
        if (!_engine.canSaveGameStateCurrently() || _engine.saveGameState(1,"Native lifecycle A").getCode()!=Common::kNoError ||
            _engine.saveGameState(1,"Forbidden automatic",true).getCode()==Common::kNoError) return reject(error,"Native initial Engine save or autosave policy failed");
        if (_initialMode==kClassicSaveMode && (_engine.saveGameState(0,"Forbidden classic0").getCode()==Common::kNoError || _engine.saveGameState(999,"Forbidden classic999").getCode()==Common::kNoError)) return reject(error,"Native Classic slot policy bypassed");
        if (!capture(_beforeWait)) return reject(error,"Native lifecycle baseline could not be captured");
        _stage=1;return true;
    }
    // Called only after the declared ordinary replay keys have drained.
    bool next(Common::Event &event,bool &generated,Common::String &error) {
        generated=false;if (!active() || finished()) return true;
        if (_phase>=5) return nextEnding(event,generated,error);
        if (_stage==1) {
            Common::Array<byte> after;if (!capture(after) || equal(after,_beforeWait)) return reject(error,"Native lifecycle ordinary Wait did not change state");
            _menu.chooser(true,1,true,"nox-native-player-Q-cancel.png");
            event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_q,'Q');generated=true;_stage=2;++_generated;return true;
        }
        if (_stage==2) {
            if (!_qStable || !_menu.cancelCaptured || !capture(_expectedA) || !writeExpected("nox-native-player-expected-A.bin",_expectedA)) return reject(error,"Native Q cancellation or capture failed");
            _menu.arm();_engine.openMainMenuDialog();
            if (!_menu.closed || !_menu.stable || !sameExpected("nox-native-player-expected-A.bin")) return reject(error,"Native actual global menu changed gameplay/audio state");
            ConfMan.setInt("music_volume",93);ConfMan.setInt("sfx_volume",141);ConfMan.setBool("mute",true);_engine.syncSoundSettings();
            if (!_engine._mixer->isSoundTypeMuted(Audio::Mixer::kMusicSoundType)) return reject(error,"Native CE music mute setting not applied");
            ConfMan.setBool("mute",false);_engine.syncSoundSettings();
            if (_engine._mixer->getVolumeForSoundType(Audio::Mixer::kMusicSoundType)!=93 || _engine._mixer->getVolumeForSoundType(Audio::Mixer::kSFXSoundType)!=141 ||
                _engine._mixer->isSoundTypeMuted(Audio::Mixer::kMusicSoundType) || !sameExpected("nox-native-player-expected-A.bin")) return reject(error,"Native independent volume settings changed player state or were not applied");
            if (_engine.saveMode()==kClassicSaveMode) _menu.chooser(true,0);
            else _menu.grid(true,1,"Native lifecycle A","nox-native-player-save-grid");
            if (!_engine.saveGameDialog() || !_menu.chooserCaptured) return reject(error,"Native actual Engine save chooser failed");
            Saves::Entry a;Common::Array<byte> payload;
            if (!Saves::best(*_engine.getSaveFileManager(),Saves::slotPrefix(_engine._targetName,_ownerA,1),_engine._targetName,a,payload) || a.sequence!=2) return reject(error,"Native generations did not advance exactly twice");
            _savedA=payload;_generationA=a.generation;_sequenceA=a.sequence;
            ConfMan.setBool("nox_classic_saves",_initialMode!=kClassicSaveMode);
            if (!_engine.synchronizePlaySession() || _engine.saveMode()!=_initialMode) return reject(error,"Native active mode changed with the new-session default");
            Common::CEAchievementService &ce=Common::CEAchievements();
            if (!ce.startPlaythroughForTarget(_engine._targetName,"nox-archaist","apple2-v119","Native lifecycle B")) return reject(error,"Native session B could not start");
            _ownerB=ce.getCurrentPlaythroughForTarget(_engine._targetName);
            if (_ownerA==_ownerB || _engine.canSaveGameStateCurrently() || _engine.saveGameState(1,"Wrong world owner").getCode()==Common::kNoError ||
                !_engine.synchronizePlaySession() || _player.game.ready() || _player.sceneReady || _player.canSave() || _player.music.ready() || _engine.getTotalPlayTime()>100 || _engine.saveMode()==_initialMode || _engine._runtime) return reject(error,"Native new-session world isolation failed");
            _stage=3;
        }
        if (_stage==3 && !_player.sceneReady) {
            if (++_generated>160) return reject(error,"Native fresh character/story input exceeded its bound");
            event.type=Common::EVENT_KEYDOWN;
            const char name[]="NATIVE B";
            if (_player.creation.stage()==Native::CharacterCreation::kName && _nameIndex<sizeof(name)-1) {
                const byte c=name[_nameIndex++];event.kbd=Common::KeyState(Common::KeyCode(c>='A' && c<='Z'?c+32:c),c);
            } else event.kbd=Common::KeyState(Common::KEYCODE_RETURN,13);
            generated=true;return true;
        }
        if (_stage==3) {
            Common::Array<byte> b;if (!_player.canSave() || !capture(b) || equal(b,_expectedA) ||
                _engine.saveGameState(1,"Native lifecycle B").getCode()!=Common::kNoError) return reject(error,"Fresh native session B could not save independently");
            Saves::Entry a;Common::Array<byte> payload;
            if (!Saves::best(*_engine.getSaveFileManager(),Saves::slotPrefix(_engine._targetName,_ownerA,1),_engine._targetName,a,payload) || !equal(payload,_savedA) || a.generation!=_generationA || a.sequence!=_sequenceA) return reject(error,"Session B changed A's committed save");
            const int handle=findHandle("Native lifecycle A");uint sessionIndex=0;bool found=false;
            for (const auto &record:Common::CEAchievements().listPlaythroughsForTarget(_engine._targetName)) {if (record.clientPlaythroughId==_ownerA) {found=true;break;}++sessionIndex;}
            if (handle<0 || !found) return reject(error,"Native saved session A missing from chooser");
            _menu.chooser(false);_menu.selectSession(sessionIndex);_menu.loadRow(0);
            if (!_engine.loadGameDialog() || _engine._runtimeSession!=_ownerA || _engine.saveMode()!=_initialMode || !sameExpected("nox-native-player-expected-A.bin") || _engine._runtime) return reject(error,"Native actual load chooser did not restore A/mode/history");
            const int coldHandle=findHandle("Native lifecycle A");
            if (!writeReport("warm",coldHandle)) return reject(error,"Native lifecycle report could not persist");_stage=4;
        }
        return true;
    }
    bool beforeQ(Common::String &error) {return !active() || _stage!=2 || capture(_beforeQ) || reject(error,"Native pre-Q capture failed");}
    bool afterQ(bool saved,Common::String &error) {
        if (!active() || _stage!=2) return true;Common::Array<byte> after;
        if (saved || !capture(after) || !equal(after,_beforeQ)) return reject(error,"Actual Q chooser cancellation changed native gameplay");_qStable=true;return true;
    }
private:
    bool endingReady() const {return _player.encounterState.presented && _player.encounterState.resultPhase==4 && _player.canSave() && !_engine._runtime;}
    bool startEnding(Common::String &error) {
        Common::CEAchievementService &ce=Common::CEAchievements();
        if (!endingReady()) return reject(error,"Ending lifecycle requires actual native terminal presentation");
        if (_phase==5) {
            if (!ce.startPlaythroughForTarget(_engine._targetName,"nox-archaist","apple2-v119","Native ending A")) return reject(error,"Ending session could not start");
            _engine._runtimeSession=ce.getCurrentPlaythroughForTarget(_engine._targetName);_ownerA=_engine._runtimeSession;ce.noteDisqualifyingAssistance("nox_native_controlled_checkpoint");
            if (!capture(_expectedA) || !writeExpected("nox-native-ending-expected.bin",_expectedA) || _engine.saveGameState(1,"Native ending A").getCode()!=Common::kNoError) return reject(error,"Native ending initial Engine save failed");
            _stage=1;return true;
        }
        if (!sameExpected("nox-native-ending-expected.bin")) return reject(error,"Native ending cold state differs");
        if (_phase==7) {
            _ownerA=_engine._runtimeSession;const int handle=findHandle("Native ending A");
            if (handle<0 || !ce.deletePlaythroughForTarget(_engine._targetName,_ownerA) || _engine.loadGameState(handle).getCode()!=Common::kNoError ||
                _engine._runtimeSession==_ownerA || !endingReady() || !sameExpected("nox-native-ending-expected.bin") || _engine.saveGameState(1,"Recovered native ending").getCode()!=Common::kNoError) return reject(error,"Native ending orphaned-save recovery failed");
        }
        const char *phase=_phase==6?"cold":_phase==7?"recovery":"recovered-cold";
        const int handle=_phase==7?findHandle("Recovered native ending"):-1;
        if (!writeEndingReport(phase,handle)) return reject(error,"Native ending cold/recovery receipt failed");_stage=4;return true;
    }
    bool nextEnding(Common::Event &event,bool &generated,Common::String &error) {
        if (_stage==1) {
            _menu.chooser(true,1,true,"nox-native-ending-Q-cancel.png");event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_q,'Q');generated=true;_stage=2;return true;
        }
        if (_stage!=2 || !_qStable || !_menu.cancelCaptured || !sameExpected("nox-native-ending-expected.bin")) return reject(error,"Native ending Q cancellation changed terminal state");
        _menu.arm();_engine.openMainMenuDialog();
        if (!_menu.closed || !_menu.stable || !sameExpected("nox-native-ending-expected.bin")) return reject(error,"Native ending global menu changed terminal state");
        if (_engine.saveMode()==kClassicSaveMode) _menu.chooser(true,0);else _menu.grid(true,1,"Native ending A","nox-native-ending-save-grid");
        if (!_engine.saveGameDialog() || !_menu.chooserCaptured || !sameExpected("nox-native-ending-expected.bin")) return reject(error,"Native ending actual save chooser failed");
        const int handle=findHandle("Native ending A");
        if (handle<0 || _engine.loadGameState(handle).getCode()!=Common::kNoError || !endingReady() || !sameExpected("nox-native-ending-expected.bin") || !writeEndingReport("warm",handle)) return reject(error,"Native ending Engine load failed");
        _stage=4;return true;
    }
    bool writeEndingReport(const char *phase,int handle) {
        bool assisted=false;
        for (const auto &record:Common::CEAchievements().listPlaythroughsForTarget(_engine._targetName)) if (record.clientPlaythroughId==_engine._runtimeSession) assisted=record.casual;
        if (!endingReady() || !assisted) return false;
        const Common::String text=Common::String::format("PASS\tphase=%s\tactual_native_ending=1\tcomplete_native_state_exact=1\tEngine_save_load=1\tQ_and_global_menu=%u\tcold_load_handle=%d\tmode=%u\tpermanent_assistance=1\tcompatibility_runtime_present=0\n",phase,uint(_phase==5),handle,uint(_engine.saveMode()));
        const Common::String name=Common::String::format("nox-native-ending-player-%s.tsv",phase);Common::OutSaveFile *out=_engine.getSaveFileManager()->openForSaving(name,false);if (!out) return false;
        out->writeString(text);out->finalize();const bool ok=!out->err();delete out;return ok;
    }
    static bool equal(const Common::Array<byte> &a,const Common::Array<byte> &b) {return a.size()==b.size() && (!a.size() || !memcmp(a.data(),b.data(),a.size()));}
    static bool reject(Common::String &error,const char *message) {error=message;return false;}
    bool capture(Common::Array<byte> &bytes) const {
        Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);if (!_player.save(out)) return false;
        bytes.resize(out.size());memcpy(bytes.data(),out.getData(),out.size());return true;
    }
    bool writeExpected(const char *name,const Common::Array<byte> &bytes) const {
        Common::OutSaveFile *out=_engine.getSaveFileManager()->openForSaving(name,false);if (!out) return false;
        const bool wrote=out->write(bytes.data(),bytes.size())==bytes.size();out->finalize();const bool ok=wrote&&!out->err();delete out;return ok;
    }
    bool sameExpected(const char *name) const {
        Common::InSaveFile *in=_engine.getSaveFileManager()->openForLoading(name);if (!in || in->size()<1 || in->size()>1048576) {delete in;return false;}
        Common::Array<byte> expected,current;expected.resize(in->size());const bool read=in->read(expected.data(),expected.size())==expected.size();delete in;
        return read && capture(current) && equal(current,expected);
    }
    int findHandle(const Common::String &description) {
        _engine.getMetaEngine()->listSaves(_engine._targetName.c_str());
        const Common::Array<Saves::Entry> entries=Saves::entries(*_engine.getSaveFileManager(),_engine._targetName);
        for (uint i=0;i<entries.size();++i) if (entries[i].metadata.description==description) return int(i);return -1;
    }
    bool writeReport(const char *phase,int coldHandle) {
        const Common::Array<Common::CEPlaythroughRecord> records=Common::CEAchievements().listPlaythroughsForTarget(_engine._targetName);
        if (records.size()!=2) return false;
        bool assisted=false,clean=false;
        for (const auto &record:records) {if (record.clientPlaythroughId==_engine._runtimeSession) assisted=record.casual;else clean=!record.casual;}
        if (!assisted || !clean) return false;
        const Common::String value=Common::String::format("PASS\tphase=%s\towned_native_snapshot_exact=1\tEngine_save_load=1\tsessions=2\tQ_cancel=%u\tactual_save_load_choosers=%u\tcold_load_handle_A=%d\tmode=%u\tcompatibility_runtime_present=%u\tlink_token_absent=1\tcontrolled_A=1\tfresh_B_creation_and_story=%u\tassistance_isolated=1\tnative_audio_acceptance=0\n",phase,uint(_qStable),uint(_phase==1),coldHandle,uint(_engine.saveMode()),uint(_engine._runtime!=nullptr),uint(_phase==1));
        const Common::String name=Common::String::format("nox-native-player-%s.tsv",phase);
        Common::OutSaveFile *out=_engine.getSaveFileManager()->openForSaving(name,false);if (!out) return false;out->writeString(value);out->finalize();const bool ok=!out->err();delete out;return ok;
    }
    NoxEngine &_engine;Native::PlayerSessionState &_player;int _phase;uint _stage,_nameIndex,_generated;bool _qStable;
    MenuValidationSource _menu;SaveMode _initialMode;Common::String _ownerA,_ownerB;Common::Array<byte> _beforeWait,_beforeQ,_expectedA,_savedA;uint _generationA=0;uint32 _sequenceA=0;
};
}
#endif
