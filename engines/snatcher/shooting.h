/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_SHOOTING_H
#define SNATCHER_SHOOTING_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "state_codec.h"
namespace Snatcher {
// Explicit algorithm state. No default seed, automatic cadence or encounter
// ownership is inferred. The coordinator and other systems also consume draws.
struct RandomWords {uint16_t first,second;RandomWords(uint16_t a,uint16_t b):first(a),second(b){}};
uint16_t drawRandom(RandomWords& state);
uint16_t drawRandomRange(RandomWords& state,uint16_t lower,uint16_t span);
bool quantizeShootingAim(unsigned normalizedX,unsigned normalizedY,unsigned& cell,std::string& error);
// Original accepted-shot regions. This selects a hit owner at fire launch;
// it does not quantize, snap or change the displayed absolute aim position.
bool shootingHitRegionAt(int16_t x,int16_t y,unsigned& region,std::string& error);
// Source digital-aim table is asset data from SUBCODE. Direction bits are the
// game's held flags (up1/down2/left4/right8), supplied by the input owner.
struct ShootingAimLayout {uint16_t x[3],y[3];};
bool readShootingAimLayout(const uint8_t* data,size_t size,ShootingAimLayout& layout,std::string& error);
bool digitalShootingAim(uint8_t held,const ShootingAimLayout& layout,uint16_t& x,uint16_t& y,unsigned& cell,std::string& error);
bool targetCellFromDraw(uint16_t draw,const uint8_t *distribution,size_t count,unsigned& cell,std::string& error);
enum TargetHitClass {kTargetPhaseIneligible,kTargetTypeBitClear,kTargetTypeBitSet};
// The caller must already have selected a valid target slot and accepted the
// fire action. This is only the specified target phase/type transformation.
TargetHitClass markTargetHit(uint8_t& phaseAndType);
uint16_t decreaseTimingAdjustment(uint16_t current);
uint16_t increaseTimingAdjustment(uint16_t current,uint16_t mode,bool alternateInputContext);
uint16_t advanceMissCounter(uint16_t current);
// Data is supplied by the player's original SUBCODE, never embedded tables.
struct ShootingModeProfile {
 uint8_t hitTypeSetSound,fireSound,initialPhase,initialSpawnDelay,initialTiming,policy;
};
bool readShootingModeProfiles(const uint8_t *data,size_t size,std::vector<ShootingModeProfile>& profiles,std::string& error);
struct ShootingShotState {
 uint8_t targetPhaseAndType;
 uint16_t timingAdjustment,typeClearHits,typeSetHits,misses;
 ShootingShotState():targetPhaseAndType(0),timingAdjustment(0),typeClearHits(0),typeSetHits(0),misses(0){}
};
struct ShootingShotEffects {
 TargetHitClass hitClass;
 bool soundRequested,reactTypeSetActor,requestModeNineRefresh,releaseFireActor;
 uint8_t sound;
 ShootingShotEffects():hitClass(kTargetPhaseIneligible),soundRequested(false),reactTypeSetActor(false),requestModeNineRefresh(false),releaseFireActor(false),sound(0){}
};
// Resolve an already accepted shot against an already selected target. This
// does not infer fire gating, aim mapping, actor cadence or encounter outcome.
// Mode 10's additional grid/damage consumer remains explicitly unsupported.
bool resolveShootingShot(unsigned mode,const std::vector<ShootingModeProfile>& profiles,bool alternateInputContext,ShootingShotState& state,ShootingShotEffects& effects,std::string& error);
// Source-defined accepted-fire gate. Button bits are caller-supplied edge flags,
// not a light-gun protocol or desktop/input mapping. Names retain ownership gaps.
struct ShootingFireContext {
 bool actorActive,alternateInput;uint8_t actorTarget,primaryEdges,alternateEdges;
 uint16_t encounterPhase,aimPulse;
 ShootingFireContext():actorActive(false),alternateInput(false),actorTarget(0xff),primaryEdges(0),alternateEdges(0),encounterPhase(0),aimPulse(0){}
};
struct ShootingFireState {
 uint16_t phase,shots,aimUpdateCounter,modeFourteenLatch;
 ShootingFireState():phase(0),shots(0),aimUpdateCounter(0),modeFourteenLatch(0){}
};
// Returns accepted=false without mutation when an original gate rejects. Unknown
// modes/entry phases fail; accepted shots are resolved by the separate hit owner.
bool acceptShootingFire(unsigned mode,const ShootingFireContext& context,ShootingFireState& state,bool& accepted,std::string& error);
struct ShootingFireCompletion {bool completed,soundRequested,encounterTransition;uint8_t sound;uint16_t nextEncounterPhase,clearedEncounterSubphase,typeClearHits;ShootingFireCompletion():completed(false),soundRequested(false),encounterTransition(false),sound(0),nextEncounterPhase(0),clearedEncounterSubphase(0),typeClearHits(0){} };
// Mode14 returns an encounter transition; its owner must apply that transition
// before polling this phase again. This helper does not own event deduplication.
bool completeShootingFire(unsigned mode,uint16_t actorFrame,ShootingFireState& state,ShootingFireCompletion& effects,std::string& error);
struct ShootingFireLayout {uint16_t x[9],y[9];};
bool readShootingFireLayout(const uint8_t* sub,size_t size,ShootingFireLayout& layout,std::string& error);
struct ShootingFireLaunch {bool launched,soundRequested;uint8_t cell,sound;uint16_t x,y;ShootingFireLaunch():launched(false),soundRequested(false),cell(0),sound(0),x(0),y(0){} };
// Shared source fire phases1/4/5. Launch is a semantic actor/audio request; the
// caller owns animation advancement and subsequently calls completeFire.
bool advanceShootingFire(unsigned mode,uint16_t timing,unsigned aimCell,const ShootingFireLayout& layout,const std::vector<ShootingModeProfile>& profiles,ShootingFireState& state,ShootingFireLaunch& effects,std::string& error);
// Snapshot of already specified shooting helpers, not a whole encounter save.
// Target spawning, nine-actor timelines and outcome ownership are not invented.
struct ShootingSnapshot {
 uint32_t profileIdentity;uint8_t mode,aimCell,aimX,aimY;bool alternateInput;
 RandomWords randomState;ShootingFireState fire;ShootingShotState shot;
 ShootingSnapshot():profileIdentity(0),mode(0),aimCell(0),aimX(0),aimY(0),alternateInput(false),randomState(0,0){}
};
uint32_t shootingProfileIdentity(const std::vector<ShootingModeProfile>& profiles);
bool encodeShootingState(const ShootingSnapshot& state,NativeSaveSection& section,std::string& error);
bool decodeShootingState(const NativeSaveSection& section,uint32_t expectedProfileIdentity,ShootingSnapshot& state,std::string& error);

class NarrativeRandomSource;
struct ShootingEncounterRule {uint8_t attackSound,aimSound,hitGoal,alternateHitGoal,spawnLimit,simultaneous,alternateSimultaneous;};
struct ShootingEncounterData {
 std::vector<ShootingModeProfile> profiles;std::vector<ShootingEncounterRule> rules;uint8_t cellDistribution[32],typeDistribution[9],gridTypes[9];int16_t healthDestinations[6];bool loaded;
 ShootingEncounterData():loaded(false){}
};
bool readShootingEncounterData(const uint8_t* subcode,size_t size,ShootingEncounterData& data,std::string& error);
// Marker bytes are data at the start of each original target sequence, supplied
// by the real actor/template owner. They are not guessed animation durations.
struct ShootingTargetState {
 bool active,markersKnown,cleared;uint8_t phaseAndType,flags,markers[8];uint16_t record,remaining,timer;int16_t templateIndex;
 ShootingTargetState():active(false),markersKnown(false),cleared(false),phaseAndType(0),flags(0),record(0),remaining(0),timer(0),templateIndex(-1){for(unsigned i=0;i<8;++i)markers[i]=0;}
};
struct ShootingTemplateState {uint8_t markers[8];uint16_t remaining;bool known;ShootingTemplateState():remaining(0),known(false){for(unsigned i=0;i<8;++i)markers[i]=0;}};
struct ShootingEncounterState {
 bool initialized,alternateInput;uint8_t mode;uint16_t phase,timing,spawnDelay,spawnPhase,selectedCell,spawned,specialTargets,variant;
 uint16_t typeClearHits,typeSetHits,damage,activeTargets,attackPhase,attackCell,cooldown,firePhase,inputGate,inputToggle,specialTimer,modeFourteenLatch;
 ShootingTargetState targets[9];
 ShootingEncounterState():initialized(false),alternateInput(false),mode(0),phase(0),timing(0),spawnDelay(0),spawnPhase(0),selectedCell(0),spawned(0),specialTargets(0),variant(0),typeClearHits(0),typeSetHits(0),damage(0),activeTargets(0),attackPhase(0),attackCell(0),cooldown(0),firePhase(0),inputGate(0),inputToggle(0),specialTimer(0),modeFourteenLatch(0){}
};
struct ShootingTemplateRequest {unsigned cell,index;ShootingTemplateRequest(unsigned c,unsigned i):cell(c),index(i){}};
struct ShootingEncounterEffects {
 std::vector<ShootingTemplateRequest> templates;std::vector<uint8_t> musicCommands;bool modeCounterActorIncrement,forcedActorPosition,countdownActorsSetup,countdownActorsRetire;unsigned forcedActor;uint16_t forcedX,forcedY;
 ShootingEncounterEffects():modeCounterActorIncrement(false),forcedActorPosition(false),countdownActorsSetup(false),countdownActorsRetire(false),forcedActor(0),forcedX(0),forcedY(0){}
};
// Post-presentation initialization: the caller has completed the real encounter
// prelude and supplied existing actor/template ownership. Does not fabricate it.
bool initializeShootingEncounter(const ShootingEncounterData& data,unsigned mode,bool alternateInput,uint16_t priorTiming,ShootingEncounterState& state,std::string& error);
// Caller synchronizes target record/remaining/flags with actual actor animation
// before each source update, and applies returned semantic effects before next.
// All nine targets update in original slot order, using one shared RNG provider.
bool tickShootingTargets(const ShootingEncounterData& data,NarrativeRandomSource& randomSource,ShootingEncounterState& state,std::string& error);
bool tickShootingSpawner(const ShootingEncounterData& data,const std::vector<ShootingTemplateState>& templates,NarrativeRandomSource& randomSource,ShootingEncounterState& state,ShootingEncounterEffects& effects,std::string& error);
enum ShootingOutcome {kShootingOutcomePending,kShootingOutcomeReady,kShootingOutcomeFailure};
// Ready/failure requests source cleanup; it never clears the owning narrative
// encounter token or substitutes a timeout for real presentation/audio cleanup.
bool evaluateShootingOutcome(const ShootingEncounterData& data,const ShootingEncounterState& state,ShootingOutcome& outcome,std::string& error);
bool tickShootingCountdown(ShootingEncounterState& state,ShootingEncounterEffects& effects,std::string& error);
bool tickShootingTimedMode(ShootingEncounterState& state,ShootingEncounterEffects& effects,bool& cleanupRequested,std::string& error);

struct ShootingCounterSettlement {bool active;uint16_t phase;ShootingCounterSettlement():active(false),phase(0){}};
struct ShootingCounterEffects {bool writeRecords,hideCounterActors,clearCountdownRecords;ShootingCounterEffects():writeRecords(false),hideCounterActors(false),clearCountdownRecords(false){}};
bool startShootingCounterSettlement(uint16_t initialPhase,ShootingCounterSettlement& state,std::string& error);
// Queue31's original even-source-update gate and actual two actor record values.
// Hide/reset effects refer to the independently owned shooting counter actors;
// completion is delivered once and never inferred from elapsed host time.
bool tickShootingCounterSettlement(bool evenSourceUpdate,uint16_t& primaryRecord,uint16_t& secondaryRecord,ShootingCounterSettlement& state,ShootingCounterEffects& effects,bool& completed,std::string& error);


// Complete game-level coordinator. Actual scene animation is observed before a
// coordinator update; effects are applied before the following actor update.
struct ShootingActorView {
 bool owned,active,markersKnown,phaseKnown,cleared;uint8_t flags,phase,markers[8];uint16_t record,remaining;int16_t x,y;
 ShootingActorView():owned(false),active(false),markersKnown(false),phaseKnown(false),cleared(false),flags(0),phase(0),record(0xffff),remaining(0xffff),x(0),y(0){for(unsigned i=0;i<8;++i)markers[i]=0;}
};
struct ShootingActorMutation {
 enum Field {kFlags=1,kRecord=2,kRemaining=4,kX=8,kY=16,kActive=32};
 unsigned actor,fields;bool active;uint8_t flags;uint16_t record,remaining;int16_t x,y;
 ShootingActorMutation(unsigned i=0):actor(i),fields(0),active(false),flags(0),record(0),remaining(0),x(0),y(0){}
};
struct ShootingResourceAction {
 enum Kind {kPalette,kGraphics,kCancelPalettes,kProbeInput};Kind kind;size_t dataOffset;bool overrideValue;int16_t value;
 ShootingResourceAction(Kind k,size_t at=0):kind(k),dataOffset(at),overrideValue(false),value(0){}
};
struct ShootingCoordinatorInput {
 bool profileReady,effectBusy,absoluteAim,infiniteHealth;uint8_t held,pressed;uint16_t pointerX,pointerY;int16_t scrollX,scrollY,mouseX,mouseY;
 std::vector<ShootingActorView> actors;
 ShootingCoordinatorInput():profileReady(false),effectBusy(false),absoluteAim(false),infiniteHealth(false),held(0),pressed(0),pointerX(0),pointerY(0),scrollX(0),scrollY(0),mouseX(0),mouseY(0){}
};
struct ShootingCoordinatorState {
 bool active,completed,aimEnabled,inputLocked,allSpecialHit,absoluteAim;ShootingEncounterState encounter;ShootingFireState fire;
 uint16_t subphase,timer,misses,shots,pointerDelay,fireTarget,aimCell,aimX,aimY,aimToggle,healthRemaining;int16_t capturedScrollX,capturedScrollY;
 ShootingCoordinatorState():active(false),completed(false),aimEnabled(false),inputLocked(false),allSpecialHit(false),absoluteAim(false),subphase(0),timer(0),misses(0),shots(0),pointerDelay(0),fireTarget(255),aimCell(4),aimX(112),aimY(56),aimToggle(0),healthRemaining(6),capturedScrollX(0),capturedScrollY(0){}
};
struct ShootingCoordinatorEffects {
 std::vector<ShootingActorMutation> actors;std::vector<ShootingTemplateRequest> templates;std::vector<ShootingResourceAction> resources;std::vector<uint8_t> music,effects;
 bool completed,failed;ShootingCoordinatorEffects():completed(false),failed(false){}
};
bool startShootingCoordinator(const ShootingEncounterData& data,unsigned mode,bool alternateInput,uint16_t inheritedTiming,ShootingCoordinatorState& state,std::string& error);
bool tickShootingCoordinator(const ShootingEncounterData& data,const ShootingAimLayout& aim,const ShootingFireLayout& fireLayout,const ShootingCoordinatorInput& input,NarrativeRandomSource& randomSource,ShootingCoordinatorState& state,ShootingCoordinatorEffects& effects,std::string& error);

}
#endif
