/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_ENGINE_H
#define SNATCHER_ENGINE_H
#include <memory>
#include "common/events.h"
#include "engines/engine.h"
#include "snatcher/scenario.h"
#include "snatcher/resource.h"
#include "snatcher/runtime.h"
#include "snatcher/presentation.h"
#include "snatcher/audio_runtime.h"
#include "snatcher/cd_audio.h"
#include "snatcher/fm_audio.h"
#include "snatcher/audio_output.h"
#include "snatcher/scene_assets.h"
#include "snatcher/scene_runtime.h"
#include "snatcher/game_clock.h"
#include "snatcher/frame_publication.h"
#include "snatcher/delay.h"
#include "snatcher/command_input.h"
#include "snatcher/startup_presentation.h"
#include "snatcher/shooting.h"
#include "snatcher/text_entry.h"
#include "snatcher/audio_script.h"
#include "snatcher/game_service.h"
#include "snatcher/interaction.h"
#include "snatcher/credits.h"
#include "snatcher/mouse_input.h"
#include "snatcher/presentation_bookmark.h"
namespace Graphics { struct Surface; }
namespace Snatcher {
FmCore configuredFmCore();
struct NativeRandomOwner:public NarrativeRandomSource {
 RandomWords state;uint64_t draws;NativeRandomOwner():state(0,0),draws(0){}
 bool next(uint16_t& value,std::string& error)override{error.clear();if(draws==UINT64_MAX){error="native random draw identity exhausted";return false;}value=drawRandom(state);++draws;return true;}
};
class SnatcherEngine : public Engine, public NarrativeQuerySource {
public:
 explicit SnatcherEngine(OSystem *system):Engine(system),_ready(false),_code(0),_text(0),_unknown(0),_runtime(nullptr),_sceneAssetsReady(false),_output(*_mixer),_audioBackend(_output.mixer()),_audio(_audioBackend),_streamOwner(_audio,_activity,&_music),_effectOwner(_audio,_activity,&_streamOwner),_pcmMono(false),_fm(_output.mixer(),false,configuredFmCore()),_fmLoaded(false),_fmControlTicked(false),_cdSource(_resources),_cdBackend(_output.mixer()),_cd(_cdSource,_cdBackend),_selection(0),_rootSelection(0),_fixture(false),_dialogueReady(false),_confirmHeld(false),_lastTickMillis(0),_canvasWidth(640),_canvasHeight(240),_sceneVisible(false),_presentationFixture(false) {noteTestInstanceCreated();}
 ~SnatcherEngine() override {if(_inputPause.isActive())_inputPause.clear();delete _runtime;noteTestInstanceDestruction();}
 Common::Error run() override;
 bool hasFeature(EngineFeature f) const override {return f==kSupportsReturnToLauncher||f==kSupportsSavingDuringRuntime||f==kSupportsLoadingDuringRuntime;}
 Common::Error saveGameState(int slot,const Common::String& description,bool autosave=false) override;
 // Manual slots 0-99 are never reserved for framework autosaving.
 int getAutosaveSlot() const override { return -1; }
 bool canSaveAutosaveCurrently() override { return false; }
 Common::Error loadGameState(int slot) override;
 bool canSaveGameStateCurrently(Common::U32String* message=nullptr) override;
 bool canLoadGameStateCurrently(Common::U32String* message=nullptr) override;
 Common::JSONValue *testCLIState() override;
 Common::JSONValue *testCLICommand(Common::JSONValue &request) override;
private:
 friend class EngineStateCodec;
 // Opt-in CLI lifecycle diagnostics, never serialized or used by gameplay.
 uint64 _testInstanceSerial=0;bool _testRecreationQueued=false;
 void noteTestInstanceCreated();void noteTestInstanceDestruction();
 MouseInput _mouseInput;
 FramePublication _framePublication;
 uint32_t _hostUpdatesLast=0;uint64_t _hostSuspendedUpdates=0;
 PauseToken _inputPause;
 bool _pauseKeyHeld=false;
 uint64_t _mouseContextIdentity=0,_mouseContextGeneration=0;
 bool _mouseAimSelected=false,_mousePositionKnown=false;
 int _mouseX=0,_mouseY=0;
 bool _entryMousePending=false;
 uint8_t _entryMousePressed=0;
 bool _mouseCommandPending=false;
 CommandButton _mouseCommandButton=kCommandConfirm;
 unsigned _mouseCommandIndex=0;
 uint16_t _mouseCommandReference=0;
 uint64_t _mouseCommandContext=0;
 CommandResponse _closingCommand;bool _closingCommandRoot=false;
 bool consumeMouseCommand(std::string& error);
 bool _mouseDialoguePending=false;uint64_t _mouseDialogueContext=0;size_t _mouseDialogueCursor=0;
 bool consumeMouseDialogue(std::string& error);
 bool _mouseStartupPending=false,_mouseStartupSelect=false;
 unsigned _mouseStartupTarget=0;
 uint8_t _mouseStartupMask=0;
 uint64_t _mouseStartupContext=0;
 bool consumeMouseStartup(std::string& error);
 bool moveMouseExamination(int x,int y,bool publish,std::string& error);
 NativePointerRepeat _pointerRepeat;
 int _entryMouseX=0,_entryMouseY=0;
 PointerDirectView _directView;
 bool mouseEnabled()const;
 MouseContext mouseContext(std::string& error);
 bool processMouseEvent(const Common::Event& event,std::string& error);
 void releaseInput();
 void drawMouse(Graphics::Surface& screen);
 // Test-only measurement state. Never serialized or consulted by gameplay.
 struct VisualWorkerCapture {
  bool firstKnown=false,captured=false;uint8_t targetMode=0,visualMode=0,phoneMode=0,paletteMode=0;
  uint64_t firstTick=0,captureTick=0,rngDraws=0;uint16_t counter=0,phonePhase=0,resource=0,sceneResource=0,rngFirst=0,rngSecond=0;
  uint32_t node=0;unsigned sceneIndex=0;size_t width=0,height=0;std::vector<uint8_t> rgb;
 };
 struct VisualWorkerObserver {
  bool active=false,done=false;uint64_t serial=0,startSerial=0,workers=0;unsigned maxWorkers=18000;
  std::string error;VisualWorkerCapture captures[2];
 } _visualObserver;
 void resetVisualObserver();void enterVisualObserver(uint16_t counter);void finishVisualObserver(uint16_t counter);
 Common::JSONValue *visualCaptureMetadata(const VisualWorkerCapture& capture,const char* filename=nullptr)const;
 bool saveBoundary(std::string& error)const;bool loadFmAssets(std::string& error);
 Common::JSONValue* saveAnywhereProbe(int slot);
 Common::JSONValue* saveOwnerContracts();
 std::string _saveError;
 GameResource _resources;std::vector<Scenario> _scenarios;
 bool _ready;size_t _code,_text,_unknown;
 NativeRandomOwner _nativeRandom;
 EventScheduler _events;SemanticTrace _trace;NarrativeRuntime *_runtime;OriginalFont _font;
 uint64_t _renderFrames=0,_renderDiagnosticFrames=0,_renderPendingSceneFrames=0,_renderRetainedFrames=0;
 SceneAssets _sceneAssets;bool _sceneAssetsReady;SceneRuntime _scene;GameClock _clock;NarrativeDelay _delay;EventToken _delayToken;AudioActivity _activity;AudioActivityWait _audioWait;
 CeAudioOutputOwner _output;CeAudioBackend _audioBackend;AudioController _audio;MusicControl _music;PcmStreamOwner _streamOwner;PcmEffectOwner _effectOwner;bool _pcmMono;std::vector<PcmCatalogEntry> _catalog;std::vector<PcmEffect> _effectCatalog;std::vector<uint8_t> _drumData;
 CeFmAudioOwner _fm;bool _fmLoaded,_fmControlTicked;
 bool startFm(uint16_t command,std::string& error,bool resetFade=true);bool postUiClick(std::string& error);bool updateFm(std::string& error);
 ResourceCdFrameSource _cdSource;CeCdQueueBackend _cdBackend;CdAudioPump _cd;
 std::vector<uint8_t> _pcmData,_pixels;std::vector<uint16_t> _textColors;uint16_t _residentBankId=0xffff;std::vector<EventToken> _handled;
 unsigned _selection,_rootSelection;CommandInput _commandInput;bool _fixture,_dialogueReady;EventToken _dialogueToken;std::string _blocker;
 TextReader _textReader;DialogueWindow _dialogueWindow;int _textX,_textY;bool _confirmHeld;
 uint32_t _lastTickMillis;unsigned _canvasWidth,_canvasHeight;bool _sceneVisible,_presentationFixture;std::string _renderError,_audioError;struct PendingEffect {uint16_t id,bank;uint32_t generation;Cursor owner;};std::vector<PendingEffect> _pendingEffects;
 std::string _startupError;StartupPresenter _startup;bool _startupActive=false,_startupCompleted=false;uint8_t _startupHeld=0,_startupPressed=0;
 bool startStartup(std::string& error,bool returnedFromGame=false);bool stepStartup(uint16_t counter);
 CreditsTimeline _credits;std::vector<uint8_t> _creditsData;EventToken _creditsToken;
 unsigned _creditsPhase=0;int _creditsScene=-1;bool _creditsClockKnown=false;uint32_t _creditsClock=0;
 uint64_t _creditsClockOrigin=0,_creditsCompletionSerial=0;
 // Read-only first final-Start-gate capture, test host only, never serialized.
 std::vector<uint8_t> _creditsGateRgb;uint64_t _creditsGateTick=0;
 void resetCredits();bool startCredits(const RuntimeRequest& request,std::string& error);
 bool updateCredits(std::string& error);bool installCreditsScene(unsigned index,std::string& error);
 bool stepNative(uint16_t counter);bool coordinateNative(uint8_t counter);void updateNative();void drawNative();bool startNative(Cursor cursor,bool fixture,std::string& error);bool showDialogue(const RuntimeRequest& request,std::string& error);
 bool _textPromptUpdated=false;
 bool _commonGraphicsInitPending=false;
 Cursor _commonGraphicsEntry;
 bool coordinateText(std::string& error);
 bool advanceText(uint8_t counter,std::string& error);bool acknowledgeText(std::string& error);
 bool publishDialogue(DialogueWindow& window,std::string& error);
 bool _startPressed=false,_workerStartPressed=false;
 uint16_t _coordinatorMode=2,_interactionMode=0,_textEntrySession=0,_textEntryLimit=0,_phoneEntryPolicy=0,_ambientMode=0;bool _quietPcm=false;bool _pointerAdjusting=false;
 PointerInteraction _pointerInteraction;
 NativePresentationBookmark _bookmark;EventToken _bookmarkToken;unsigned _bookmarkPhase=0;SceneResourcePreparation _bookmarkPreparation;bool _bookmarkNeedsLoad=false;std::vector<uint8_t> _bookmarkData;uint16_t _lastPcmCommand=0xffff;bool _lastPcmKnown=true;
 bool updateBookmark(const RuntimeRequest& request,std::string& error);
 EventToken _presentationToken;SceneResourcePreparation _presentationPreparation;
 unsigned _presentationLoadPhase=0;bool _presentationNeedsLoad=false;std::vector<uint8_t> _presentationData;
 bool updateQueuedPresentation(const RuntimeRequest& request,std::string& error);
 bool applySemanticOwner(const RuntimeRequest& request,std::string& error);
 std::shared_ptr<ResidentPcmStaging> _residentStaging;uint16_t _residentStreamPending=0;
 bool finishPcmStreamStaging(std::string& error);
 bool loadResidentBank(uint16_t bank,std::string& error);
 bool updatePcmRequest(const RuntimeRequest& request,std::string& error);
 EventToken _pcmRequestToken;unsigned _pcmRequestPhase=0;uint16_t _pcmRequestId=0;std::vector<uint8_t> _pcmVisualTable;
 bool capturePresentationBookmark(uint16_t presentation,NativePresentationBookmark& bookmark,std::string& error)const;
 bool restoreBookmarkPolicies(std::string& error);
 uint64_t _gameElapsedOrigin=0;GameSaveRecord _startupRecord;bool _startupRecordReady=false,_originalResumePending=false;unsigned _startupRecordSlot=0;RuntimeRequest _originalResumeJob;
 GameServiceState _gameServices;bool _gameStorageScanned=false;
 // Opt-in test transport only; bounded owner bytes never applied to gameplay.
 std::vector<uint8_t> _compatibilityProbeInput;
 StartupSettings _originalInputSettings;
 struct GameSaveCapture {EventToken token;NativePresentationBookmark bookmark;GameSaveElapsed elapsed;StartupSettings inputSettings;uint8_t controllerMapping=0;bool mappingKnown=false,sourceContextSupported=false,mono=false;};
 std::vector<GameSaveCapture> _gameSaveCaptures;
 bool ensureGameStorage(std::string& error);
 bool captureOriginalGameSlot(const RuntimeRequest& request,std::string& error);
 bool writeOriginalGameSlot(const RuntimeRequest& request,std::string& error);
 bool startupStorage(StartupStorage& storage,std::string& error);
 bool readStartupGameSlot(unsigned slot,std::string& error);
 bool readCompatibleOriginalSlot(unsigned slot,GameSaveRecord& record,std::string& error);
 bool beginOriginalGameResume(const GameSaveRecord& record,const StartupSettings& settings,std::string& error);
 bool updateOriginalGameResume(std::string& error);
 EventToken _returnTitleToken;unsigned _returnTitlePhase=0,_returnTitleRemaining=0;bool _returnTitleReady=false;
 bool updateReturnTitle(const RuntimeRequest& request,std::string& error);
 ShootingCounterSettlement _counterSettlement;EventToken _counterToken;
 bool updateCounterSettlement(const RuntimeRequest& request,uint8_t counter,std::string& error);
 bool read(NarrativeQuery query,uint16_t& value,std::string& error)override;
 ShootingEncounterData _shootingData;ShootingAimLayout _shootingAim;ShootingFireLayout _shootingFire;ShootingCoordinatorState _shooting;EventToken _shootingToken;
 uint8_t _shootingHeld=0,_shootingPressed=0,_entryGlobalPressed=0,_entryFramePressed=0;uint16_t _pointerX=112,_pointerY=56;bool _shootingProfileReady=false;
 bool updateShooting(const RuntimeRequest& request,std::string& error);
 bool dispatchNativeCommand(const RuntimeRequest& request,std::string& error);
 bool dispatchSceneEffect(const PendingEffect& effect,std::string& error);
 bool dispatchNarrativeEffect(const RuntimeRequest& request,std::string& error);
 ScriptAudioWait _scriptAudioWait;EventToken _scriptAudioToken;uint8_t _primaryMusicCommand=0;bool _primaryMusicKnown=true;uint64_t _primaryCompletionObserved=0,_secondaryCompletionObserved=0,_completionPublicationObserved=0;uint8_t _completionSerialObserved=0;
 bool consumeFmCompletions(std::string& error);
 int _entryCloseView=-1;TextEntry _textEntry;bool _entryOpen=false,_entryPending=false,_entryKeySaved=false;int16_t _entryKeyX=0,_entryKeyY=0;
 bool updateTextEntry(uint8_t counter,std::string& error);
 bool applyTextEntryEffects(TextEntryEffects& effects,std::string& error);
 bool showMenu(bool opening,std::string& error);
};
}
#endif
