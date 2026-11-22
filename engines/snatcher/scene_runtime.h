/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_SCENE_RUNTIME_H
#define SNATCHER_SCENE_RUNTIME_H
#include "scene_renderer.h"
#include "events.h"
#include "line_scroll.h"
#include "phone_visual.h"
#include "dialogue_window.h"
#include "environment_visual.h"
#include <memory>
#include <array>
namespace Snatcher {
class NarrativeRandomSource;
class TextEntry;
struct SceneMusicCommand {uint8_t command;bool resetFade;SceneMusicCommand(uint8_t c=0,bool reset=false):command(c),resetFade(reset){}bool operator==(const SceneMusicCommand& other)const{return command==other.command&&resetFade==other.resetFade;}};

struct SceneActorView {
 bool owned,active,markersKnown,phaseKnown,cleared;uint8_t phase,flags,completionFlags,markers[8];uint16_t record,remaining,resource;size_t sequence;int16_t x,y;
 SceneActorView():owned(false),active(false),markersKnown(false),phaseKnown(false),cleared(false),phase(0),flags(0),completionFlags(0),record(0xffff),remaining(0xffff),resource(0xffff),sequence(0),x(0),y(0){for(unsigned i=0;i<8;++i)markers[i]=0;}
};
struct SceneActorPatch {
 enum Field {kFlags=1,kRecord=2,kRemaining=4,kX=8,kY=16,kActive=32,kPhase=64};unsigned fields;bool active;uint8_t flags,phase;uint16_t record,remaining;int16_t x,y;
 SceneActorPatch():fields(0),active(false),flags(0),phase(0),record(0),remaining(0),x(0),y(0){}
};
// Queue0F has a palette-only retry before replacement, then acknowledges the
// completed replacement on its next admitted visit. Its caller owns the job.
struct SceneResourcePreparation {
 enum Stage {kPrepare,kWaitForReplacement,kPrepared};Stage stage;
 SceneResourcePreparation():stage(kPrepare){}
};
// Owns game objects/resources and their semantic updates, not hardware state.
// Installation, decoded allocation commit and animation completion are distinct.
class SceneRuntime {
public:
 SceneRuntime();
 void reset();
 // Source module/application dispatch clears actors and policies, but not
 // the application-lifetime submission history used by environmental draws.
 void resetForModule();
 // Normal game boot supplies a cleared submission allocation. Establish
 // that recovered initial semantic state once, never for module changes or
 // legacy archives whose retained history is explicitly unknown.
 bool establishBootSubmissionHistory(std::string& error);
 const EnvironmentVisualState& submissionHistory()const{return _environmentState;}
 bool validateModuleEnvironmentHistory(uint16_t mode,std::string& error)const;
 // Source application F560 + EEB6, distinct from ordinary F5E8 scene changes.
 bool resetApplicationPresentation(std::string& error);
 bool beginCreditsPresentation(std::string& error);
 bool recoverCreditsPalette(bool& repaired,std::string& error);
 bool configure(const SceneLayout& layout,std::string& error);
 bool loadEnvironmentTables(const uint8_t* sub,size_t size,std::string& error){return readEnvironmentVisualTables(sub,size,_environmentTables,error);}
 bool setEnvironmentMode(uint16_t mode,std::string& error);
 uint16_t environmentMode()const{return _environmentMode;}
 bool commitEnvironmentFrame(uint16_t counter,std::string& error);
 void establishFreshModuleDialogueContext(){_dialogueLayout=freshModuleDialogueLayout();_fontModeKnown=true;_fontFixedWidth=false;_visualCommand=0;_visualKnown=true;}
 bool setDialogueTop(uint8_t top,std::string& error){error.clear();if(top>9||!_dialogueLayout.established){error="dialogue top requires bounded native layout owner";return false;}_dialogueLayout.top=top;return true;}
 bool dialogueLayout(DialogueLayout& out,std::string& error)const;
 bool setControllerMapping(uint8_t mapping,std::string& error);
 bool applyDialogueTransfers(const std::vector<GraphicsTransfer>& transfers,std::string& error);
 bool prepareDialogue(DialogueWindow& window,std::string& error)const;
 bool stageDialogue(DialogueWindow& window,std::string& error);
 bool prepareTextEntry(TextEntry& entry,std::string& error)const;
 bool stageTextEntry(TextEntry& entry,std::string& error);
 size_t pendingGraphicsCopies()const{return _graphicsCopies.size();}
 // Stable source Main FF393C/FF0EFA mode9; transition ownership is separate.
 bool establishDialogueView(unsigned mode,std::string& error);
 bool requestPalettePublication(uint8_t command,std::string& error);
 bool palettePublicationPending()const{return _pendingPalettePublication!=0;}
 uint8_t palettePublicationMode()const{return _palettePublicationMode;}
 bool setPlaneAllocation(unsigned plane,unsigned allocation,std::string& error);
 int visualCountdown()const{return _visualCountdown;}
 const SceneLayout& presentationLayout()const{return _baseLayout;}
 bool requestVisualCommand(uint16_t command,std::string& error);
 bool requestDialogueView(uint16_t speakerWord,std::string& error);
 // Sub F3F8 posts the primary mailbox; Main3334 accepts it with the next
 // canvas publication. Do not expose reused text patterns in the Sub visit.
 bool postDialogueView(uint16_t command,std::string& error);
 bool requestMenuView(std::string& error);
 bool requestMenuClose(std::string& error);
 bool visualBusy()const{return _visualBusy||_workerVisualBusy||_pendingPhoneKnown||_pendingDialogueView;}
 bool visualControllerBusy()const{return _visualBusy||_pendingPhoneKnown;}
 // SUB139D4 clears only the first actor word for the ten caption-role slots.
 void deactivateCaptionActors();
 bool setMenuCursorPieces(const std::vector<PlacedPiece>& pieces,std::string& error);
 void clearMenuCursor(){_menuCursorPieces.clear();_menuCursorPendingPieces.clear();_menuCursorOwned=_menuCursorPending=false;}
 void deferMenuCursorClear(){_menuCursorPendingPieces.clear();_menuCursorPending=true;}
 bool visualCommand(uint8_t& command)const{command=_visualCommand;return _visualKnown;}
 const PhoneVisualState& phoneVisualState()const{return _phoneVisual;}
 const PhoneFramePublication& phoneFramePublication()const{return _phoneFrame;}
 bool pendingPhoneRequest(uint8_t& command)const{command=_pendingPhoneRequest;return _pendingPhoneKnown;}
 SceneLayout framePresentationLayout()const{return currentRenderLayout();}
 bool establishFreshPalette(std::string& error);
 bool setPalette(const std::vector<uint16_t>& colors,unsigned bank,std::string& error);
 bool establishSceneReset(SceneLayoutPreset preset,std::string& error);
 // Main2BB8 square reset belongs to the completed license owner before handoff.
 void inheritLicenseRowHistory();
 bool requestGameLayout(uint16_t word,std::string& error);
 bool loadGameLayoutPresets(const uint8_t* mainIp,size_t size,std::string& error);
 // Main0652 initializes the alternate pointing palette inactive; changing it
 // requires the actual typed input owner and supplied original palette.
 bool setAlternatePalette(bool active,const std::vector<uint16_t>& colors,std::string& error);
 const std::vector<uint16_t>& displayedPaletteColors()const{return _displayedColors;}
 bool displayedPaletteKnown()const{return _displayedHistoryKnown;}
 void establishBlankAssets(){_renderer.establishBlankAssets();}
 bool applyTransfer(const GraphicsTransfer& transfer,std::string& error){return _renderer.applyTransfer(transfer,error);}
 bool fillPlaneCells(unsigned plane,unsigned column,unsigned row,unsigned columns,unsigned rows,uint16_t attributes,std::string& error){return _renderer.fillPlaneCells(plane,column,row,columns,rows,attributes,error);}
 void establishSceneFlags(uint8_t flags){_sceneFlags=flags;_sceneFlagsKnown=true;}
 bool sceneFlags(uint8_t& value)const{value=_sceneFlags;return _sceneFlagsKnown;}
 bool setTextPalette(const std::vector<uint16_t>& colors,std::string& error);
 bool prepareResourceChange(std::string& error);
 bool advanceResourcePreparation(SceneResourcePreparation& job,bool& complete,std::string& error);
 // Direct F5E8 retains displayed names now, then requests the source E34E/
 // Main clear unless flag bit1 suppresses it. Queue0F has its own frontend.
 bool preparePresentationReplacement(std::string& error);
 bool resourceChangePending()const{return _resourceChangePending;}
 bool install(uint16_t resource,const std::vector<uint8_t>& data,unsigned index,std::string& error);
 bool installOverlay(uint16_t resource,const std::vector<uint8_t>& bytes,unsigned index,std::string& error);
 bool pcmVisualStartReady(const std::vector<ActorInitializer>& actors)const;
 // Repair only the byte-recognized erroneous common overlay in old hospital saves.
 bool recoverHospitalCombatMap(unsigned healthRemaining,const std::vector<uint8_t>& subcode,bool& repaired,std::string& error);
 bool installSystemPresentation(const std::vector<uint8_t>& subcode,unsigned index,unsigned position,std::string& error);
 bool commitTransfers(std::string& error);
 bool selectActorRecord(unsigned actor,unsigned record,uint8_t flags,std::string& error);
 bool inspectActor(unsigned actor,SceneActorView& view,std::string& error)const;
 bool actorPieces(unsigned actor,std::vector<PlacedPiece>& pieces,std::string& error,int record=-1)const;
 bool patchActor(unsigned actor,const SceneActorPatch& patch,std::string& error);
 bool reserveActor(unsigned actor,bool reserved,std::string& error){return _actors.reserve(actor,reserved,error);}
 void captureActorBookmark(std::array<uint8_t,64>& summary)const;
 bool restoreActorBookmark(const std::array<uint8_t,64>& summary,std::string& error);
 bool initializeBookmarkedActors(const std::array<uint8_t,64>& summary,std::string& error);
 bool copyActorTemplate(unsigned actor,unsigned templateActor,std::string& error);
 bool setActorFlags(unsigned actor,uint8_t flags,std::string& error);
 bool queueScrollPosition(unsigned plane,ScrollAxis axis,int16_t position,std::string& error){return _scroll.queuePosition(plane,axis,position,error);}
 // Secondary absolute plane publication preserves the primary visual owner.
 bool queueAbsolutePlanePosition(unsigned plane,int16_t x,int16_t y,std::string& error);
 bool loadSharedGraphicsEffects(const uint8_t* subcode,size_t size,std::string& error);
 bool queueGraphicsTasks(const std::vector<SceneGraphicsTask>& tasks,std::string& error);
 // Source EC5A packet: application clear and the two common decoded assets.
 // The cursor actor is represented by the existing semantic menu owner.
 bool queueScenarioGraphicsInitialization(const uint8_t* subcode,size_t size,std::string& error,bool resumedGame=false);
 void cancelPaletteEffects(){_palettes.cancelEffects();}
 bool queuePaletteEffect(const PaletteEffect& effect,std::string& error);
 bool beginApplicationFade(const PaletteEffect& effect,std::string& error);
 // Main publishes the prior palette buffers before queue/coordinator work.
 // The caller then consumes only events already present from earlier updates.
 bool beginFrame(bool paused,std::string& error,int presentationCounter=-1);
 // Source D6F6: action queue/coordinator precedes F7F0 object updates. Markers
 // produced here are available to the following coordinator phase, not a
 // coordinator that has already run in this logical frame.
 bool updateObjects(bool paused,std::string& error,unsigned elapsedUpdates=1);
 // Convenience for owners without an intervening coordinator (native fixtures).
 bool tick(bool paused,std::string& error);
 bool render(SceneColorEncoding encoding,SceneImage& image,std::string& error,unsigned hiddenActor=64);
 ActorActionResult actorAction(unsigned actor,unsigned action,std::string& error);
 const ActorControlState* actor(unsigned index)const{return _actors.state(index);}
 const AnimationState* animation(unsigned index)const;
 bool transferBusy()const{return !_pendingTransfers.empty()||_transferCompletionPending;}
 bool blankingPending()const{return _nameBlankPhase!=0||_mainNameBlankPending;}
 bool graphicsServiceBusy()const{return transferBusy()||blankingPending();}
 bool fontPublicationPending()const;
 size_t pendingGraphicsTasks()const{return _graphicsTasks.pending();}
 bool resourceIdentity(uint16_t& resource,unsigned& index)const{resource=_saveResource;index=_saveScene;return resource!=0xffff;}
 bool systemActor(unsigned actor)const{return actor<64&&_slots[actor].owned&&_slots[actor].systemSource;}
 const SceneAssets& assets()const{return _assets;}
 const std::vector<uint16_t>& paletteColors()const{return _palettes.colors();}
 const std::vector<uint16_t>& computedPaletteColors()const{return _palettes.colors();}
 const std::vector<uint16_t>& pendingPaletteColors()const{return _pendingColors;}
 const std::vector<uint16_t>& visiblePaletteColors()const{return _visibleColors;}
 const std::vector<bool>& computedPaletteKnown()const{return _knownColors;}
 const std::vector<bool>& pendingPaletteKnown()const{return _pendingKnownColors;}
 const std::vector<bool>& visiblePaletteKnown()const{return _visibleKnownColors;}
 const ScrollSnapshot& scroll()const{return _scroll.state();}
 // Current committed plane position, not the pending next worker request.
 bool publishedPlanePosition(unsigned plane,int16_t& x,int16_t& y,std::string& error)const;
 bool secondaryScrollFrozen()const{return _secondaryScrollFrozen;}
 bool setTransferChannel(unsigned channel,std::string& error);
 void setPaletteSuppressed(bool value){_paletteSuppressed=value;_paletteSuppressionKnown=true;}
 void setAnimationCadenceMask(bool known,uint8_t mask){_cadenceKnown=known;_cadenceMask=mask;}
 void setCdClock(bool known,uint32_t packed){_cdClockKnown=known;_cdClock=packed;}
 void establishFreshMusicContext(){_musicSuppressed=false;_musicSuppressionKnown=true;_lastMusicCommand=_lastSecondaryMusicCommand=0;_lastMusicKnown=_lastSecondaryMusicKnown=true;}
 void setMusicSuppressed(bool suppressed){_musicSuppressed=suppressed;_musicSuppressionKnown=true;}
 bool requestCommandMusic(uint8_t command,std::string& error);
 void clearSecondaryMusicCache(){_lastMusicCommand=0;_lastMusicKnown=true;}
 bool musicSuppressed()const{return _musicSuppressed;}
 bool lastAnimationMusic(uint8_t& command)const{command=_lastMusicCommand;return _lastMusicKnown;}
 bool lastSecondaryAnimationMusic(uint8_t& command)const{command=_lastSecondaryMusicCommand;return _lastSecondaryMusicKnown;}
 std::vector<SceneMusicCommand> takeMusicCommands(){std::vector<SceneMusicCommand> out;out.swap(_musicCommands);return out;}
 void setEffectsSuppressed(bool value){_effectsSuppressed=value;}
 bool effectsSuppressed()const{return _effectsSuppressed;}
 bool cdClock(uint32_t& packed)const{packed=_cdClock;return _cdClockKnown;}
 void clearSoundContext(){_soundKnown=false;}
 void setSoundContext(bool effectsSuppressed,uint8_t activity){_effectsSuppressed=effectsSuppressed;_audioActivity=activity;_soundKnown=true;}
 void establishFreshAnimationClock(){_animationClock=AnimationClockCapture();_animationClock.clockKnown=_animationClock.captureKnown=true;_cadenceKnown=true;_cadenceMask=0;_cdClockKnown=true;_cdClock=0;}
 void setAnimationClock(uint16_t clock){_animationClock.sourceTicks=clock;_animationClock.clockKnown=true;}
 // D6EE/D6F6 admits E046 only after dependent/blanking ownership clears.
 // A waiting caller can distinguish a held visit from an admitted one.
 bool advanceCoordinatorClock(std::string& error,bool* admitted=nullptr);
 const AnimationClockCapture& animationClock()const{return _animationClock;}
 void setRandomSource(NarrativeRandomSource *source){_random=source;}
 void setTrace(SemanticTrace *trace){_trace=trace;}
 std::vector<uint16_t> takeEffects(){std::vector<uint16_t> result;result.swap(_effects);return result;}
 bool textPaletteRequested()const{return _textPaletteRequested;}
 bool palettePosted()const{return _palettePosted;}
private:
 uint8_t _pendingDialogueView=0;
 bool prepareReplacement(bool resourceHandshake,std::string& error);
 bool clearPresentationPalette(std::string& error);
 friend class SceneStateCodec;
 struct ActorSlot {
  bool owned,frameKnown,systemSource,catchUp,clearedKnown;uint32_t cdDeadline;uint8_t cadenceMask;uint16_t resource;unsigned frame;ActorInitializer initializer;
  std::shared_ptr<const std::vector<uint8_t> > data;AnimationPlayer player;int32_t baseVelocity[2],effectiveVelocity[2];
  ActorSlot():owned(false),frameKnown(false),systemSource(false),catchUp(false),clearedKnown(false),cdDeadline(0),cadenceMask(0),resource(0),frame(0),initializer(){baseVelocity[0]=baseVelocity[1]=effectiveVelocity[0]=effectiveVelocity[1]=0;}
 };
 uint64_t _saveStamp=0;uint16_t _saveResource=0xffff;unsigned _saveScene=0;
 NativeLineScroll _lineScroll;bool _fullPlaneScrollOverride=false,_legacyLineComposition=false,_rowSamplingKnown=false,_frameLineSampling=false,_pendingSamplingReset=false,_allRowsKnown=false,_legacyUnknownRows=false;uint8_t _pendingSecondaryMode=0,_tailRowsKnown=0;int16_t _rowX[2][256]={};bool _secondaryScrollFrozen=false;uint8_t _secondaryScrollMemo=0;PhoneVisualState _phoneVisual;PhoneFramePublication _phoneFrame;uint8_t _pendingPhoneRequest=0;bool _pendingPhoneKnown=false;
 AnimationClockCapture _animationClock;ActorController _actors;ActorSlot _slots[64];PaletteController _palettes;ScrollState _scroll;SceneRenderer _renderer;SceneGraphicsQueue _graphicsTasks;
 // F5E8 requests; E34E posts/observes; Main32F0 publishes the name clears.
 // Stable legacy saves exclude these stages; active-owner saves retain them.
 uint8_t _nameBlankPhase=0;bool _mainNameBlankPending=false;
 // Main3CC6 publishes vertical sampling before the next Sub scroll worker.
 bool _fullPlanePublicationEnabled=true; // Main E076, independent of Sub scroll positions.
 bool _scrollPublicationKnown=false;int16_t _frameScrollY[2]={};
 std::vector<bool> _knownColors,_pendingKnownColors,_visibleKnownColors;
 std::vector<uint16_t> _pendingColors,_visibleColors;
 std::vector<uint16_t> _displayedColors,_frameDisplayColors,_frameLowerColors,_alternateColors;
 bool _displayedHistoryKnown,_displayedDirty,_alternatePaletteActive,_computedPrimaryPosted,_pendingPrimaryPosted,_gameLayoutsKnown;
 SceneLayout _tallGameLayout,_wideGameLayout;
 uint16_t _pendingGameLayout;
 EnvironmentVisualTables _environmentTables;EnvironmentVisualState _environmentState;uint16_t _environmentMode;bool _environmentSubmissionKnown;std::vector<PlacedPiece> _environmentSubmission;
 // Main publishes the last completed game submission before the next actor
 // worker runs. Computed geometry and displayed geometry have distinct ages.
 bool _displayedSubmissionKnown=false;
 std::vector<PlacedPiece> _displayedSubmission;
 SceneLayout currentRenderLayout()const;
 bool collectPieces(const SceneLayout& layout,std::vector<PlacedPiece>& prefix,std::vector<PlacedPiece>& suffix,std::string& error)const;
 std::vector<PlacedPiece> _menuCursorPieces;bool _menuCursorOwned,_visualBusy;int _visualThreshold,_visualOrigin,_visualCountdown;
 // Main completion reaches the worker's report separately. The menu task
 // changes its actor only after that worker has emitted its current list.
 bool _workerVisualBusy=false,_menuCursorPending=false;
 std::vector<PlacedPiece> _menuCursorPendingPieces;
 std::vector<SceneGraphicsCopy> _graphicsCopies;
 bool _fontClearPending=false;
 // Source9728 is independent of the legacy inherited layout metadata.
 bool _fontModeKnown=false,_fontFixedWidth=false;
 bool _transferCompletionPending=false;unsigned _transferCopyOffset=0;
 uint8_t _visualCommand,_palettePublicationMode,_pendingPalettePublication;bool _visualKnown,_palettePublicationDirty;DialogueLayout _dialogueLayout;SceneLayout _baseLayout;SceneAssets _assets;std::vector<GraphicsTransfer> _pendingTransfers;std::vector<uint16_t> _effects;std::vector<SceneMusicCommand> _musicCommands;bool _musicSuppressed,_musicSuppressionKnown,_lastMusicKnown,_lastSecondaryMusicKnown;uint8_t _lastMusicCommand,_lastSecondaryMusicCommand;
 unsigned _transferChannel,_pendingTransferChannel;bool _transferChannelKnown,_pendingTransferChannelKnown;
 bool _scrollEnabled,_cdClockKnown,_cadenceKnown;uint8_t _cadenceMask;uint32_t _cdClock;
 bool _paletteKnown,_paletteSuppressed,_paletteSuppressionKnown,_effectsSuppressed,_soundKnown,_textPaletteRequested,_palettePosted;
 uint8_t _sceneFlags;bool _sceneFlagsKnown,_palettePostedKnown,_palettePolicyKnown,_freshPaletteRequested,_keepSecondaryPalette,_textPaletteKnown,_resourceChangePending;
 std::vector<uint16_t> _textPalette;
 unsigned _paletteBank,_currentActor;uint8_t _audioActivity;NarrativeRandomSource *_random;SemanticTrace *_trace;
 bool updateVisual(std::string& error);
 bool publishDisplayedPalette(bool primaryChanged,std::string& error);
 bool applyGameLayout(GameLayoutRequest request,std::string& error);
 bool applyLowerWindow(unsigned first,int origin,bool both,std::string& error);
 bool applyCinematicShutter(unsigned amplitude,std::string& error);
 bool applyPhoneView(std::string& error);
 bool applyEntryView(bool closing,std::string& error);
 bool installStaged(uint16_t resource,const std::shared_ptr<const std::vector<uint8_t> >& data,const SceneAssets& assets,std::string& error,bool systemSource=false);
 bool paletteRangeKnown(const PaletteEffect& effect,std::string& error)const;
 bool initializeActor(const ActorInitializer& initial,uint16_t resource,const std::shared_ptr<const std::vector<uint8_t> >& data,std::string& error,bool systemSource=false);
 bool descendants(unsigned actor,std::vector<unsigned>& result,std::string& error)const;
 bool updateActorObjects(unsigned elapsedUpdates,std::string& error);
 bool retireActor(unsigned actor,std::string& error);
 bool updateVelocity(unsigned actor,unsigned axis,std::string& error);
 bool restartActor(unsigned actor,std::string& error);
 void record(unsigned actor,const char *category,uint32_t value=0);
 bool failActor(unsigned actor,const std::string& reason,std::string& error)const;
 AnimationCommandResult command(const AnimationCommand& command,std::string& error);
 static AnimationCommandResult handle(void *owner,const AnimationCommand& command,std::string& error);
 static bool randomDuration(void *owner,uint16_t lower,uint16_t span,uint16_t& duration,std::string& error);
};
}
#endif
