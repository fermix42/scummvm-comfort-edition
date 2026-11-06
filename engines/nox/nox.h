/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_ENGINE_H
#define NOX_ENGINE_H
#include "engines/engine.h"
#include "nox/resources.h"
#include "nox/save_policy.h"
#include "audio/mixer.h"
#include "graphics/surface.h"
namespace Audio { class QueuingAudioStream; }
namespace Nox {
class Runtime;
class MenuValidationSource;
class NativePlayerLifecycleProbe;
// Native gameplay owns its state; the Engine owns durable saves and CE sessions.
// Load is staged before session/history changes and committed only afterward.
class NativePlayerSession {
public:
    virtual ~NativePlayerSession() {}
    virtual bool canSave() const = 0;
    virtual bool save(Common::WriteStream &out) const = 0;
    virtual bool stageLoad(Common::SeekableReadStream &in) = 0;
    virtual void commitLoad() = 0;
    virtual void discardLoad() = 0;
    virtual void resetPresentationClock() = 0;
    virtual bool reset() = 0;
    virtual bool render(Graphics::Surface &surface) const = 0;
    virtual uint positionX() const = 0;
    virtual uint positionY() const = 0;
};
class NoxEngine : public Engine {
    friend class NativePlayerLifecycleProbe;
public:
	explicit NoxEngine(OSystem *system) : Engine(system) {}
	~NoxEngine() override;
	Common::Error run() override;
	bool hasFeature(EngineFeature feature) const override { return feature == kSupportsReturnToLauncher || feature == kSupportsSavingDuringRuntime || feature == kSupportsLoadingDuringRuntime; }
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error saveGameState(int slot,const Common::String &description,bool autosave=false) override;
	Common::Error loadGameState(int slot) override;
	Common::String getSaveStateName(int slot) const override;
	int getAutosaveSlot() const override { return -1; }
	bool canSaveAutosaveCurrently() override { return false; }
	SaveMode saveMode() const { return _saveMode; }
private:
	SaveMode _saveMode=kModernSaveMode;
	Common::String _runtimeSession;
	HdvArchive _resources;
	Runtime *_runtime = nullptr;
    NativePlayerSession *_nativePlayer = nullptr; // Scoped owner in native host.
	Graphics::Surface _surface;
	Audio::QueuingAudioStream *_audio = nullptr;
	Audio::SoundHandle _sound;
    Audio::QueuingAudioStream *_nativeMusicAudio = nullptr;
    Audio::SoundHandle _nativeMusicSound;
    bool queueNativeMusic(const int16 *samples,uint count);
    void pauseEngineIntern(bool pause) override;
	bool validateResources(Common::String &error);
	Common::Error runNativeStartup();
	bool validateSessions(Common::String &error);
	bool validateStateProfile(Common::String &error);
	bool validateSaveCallers(Common::String &error,uint32 runStarted);
	bool validateGridWrites(MenuValidationSource &input,Common::String &error);
	bool validateGridCold(MenuValidationSource &input,Common::String &error);
	void resetAudio();
	bool synchronizePlaySession();
	void configureRuntimeObservation(Runtime &runtime);
	bool validateVortexChain(bool cold, Common::String &error);
	bool validateSpellReady(bool cold, Common::String &error);
	bool validateDarkKey(bool cold, Common::String &error);
	bool validateDarkKeyGate(bool cold, Common::String &error);
	bool validateCultist(bool cold, Common::String &error);
	bool finishQuestBattle(byte map,byte actor,Common::String &trace,Common::String &error);
	bool validateShadowCheckpoint(bool cold, Common::String &error);
	bool validateShadowConfrontation(bool cold, Common::String &error);
	bool validateCampaignPage(bool cold, Common::String &error);
	bool validateCampaignPagesLinked(bool cold, Common::String &error);
	bool validateCampaignCheckpoint(bool cold, Common::String &error);
	bool validateDialogueFixture(bool cold, Common::String &error);
	bool validateSessionManagement(Common::String &error);
	bool validateSaveHistory(Common::String &error);
	bool validateStorageFailures(Common::String &error);
};
} // namespace Nox
#endif
