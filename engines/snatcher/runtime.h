/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_RUNTIME_H
#define SNATCHER_RUNTIME_H
#include "scenario.h"
#include "control.h"
#include "events.h"
namespace Snatcher {
struct NarrativeSnapshot;
enum RuntimeStatus {kRuntimeIdle,kRuntimeRunning,kRuntimePassFinished,kRuntimeChoices,kRuntimeWaiting,kRuntimeUnsupported,kRuntimeError};
enum RequestKind {kRequestDialogue,kRequestPcm,kRequestEffect,kRequestAudioWait,kRequestPresentation,kRequestFm,kRequestActorAction,kRequestCommand,kRequestCoordinatorControl,kRequestInteractionMode,kRequestPresentationBookmark,kRequestSemanticSetting,kRequestGameSave};
// Native9/10/11 requests retain their original scalar in first; they are
// semantic owner operations, not invented command-queue identifiers.
struct GameSaveContext {
 bool known=false,progressionKnown=false;uint16_t entry=0,progression=0;
 std::vector<uint8_t> packed;std::vector<uint16_t> history;
};
struct RuntimeRequest {RequestKind kind;Cursor owner;uint16_t first,second;EventToken token;uint16_t queue;std::vector<uint16_t> operands;GameSaveContext gameSave;RuntimeRequest():kind(kRequestCommand),first(0),second(0),queue(0){} };
// Shared game RNG ownership is external: narrative does not invent a seed or
// replay draws owned by actors/shooting. One supplied draw is consumed per3E.
class NarrativeRandomSource {
public:virtual ~NarrativeRandomSource(){};virtual bool next(uint16_t& draw,std::string& error)=0;
};
// Typed externally owned expression values. Source-address decoding is confined
// to the original resource adapter; callers never receive a guest-memory API.
// The neutral counter/result names retain unresolved higher-level meanings.
enum NarrativeQuery {
 kQueryResponseStatus,kQueryResponseWord0,kQueryResponseWord1,kQueryResponseWord2,kQueryResponseWord3,
 kQueryShootingShots,kQueryShootingTypeClearHits,kQueryShootingTypeSetHits,kQueryShootingMisses,
 kQueryShootingCounter,kQueryShootingResult,kQueryShootingInputToggle,kQueryAlternateShootingInput,
 kQueryMenuContext,kQueryStorageStatus,kNarrativeQueryCount
};
bool decodeNarrativeQuery(uint16_t originalField,NarrativeQuery& query);
// Source native6 assignments have separate semantic owners; no arbitrary
// address or native-memory write is exposed to the engine.
enum NarrativeSetting {kSettingTextEntrySession,kSettingTextEntryLimit,kSettingDialogueTop,kSettingPhoneEntryPolicy,kSettingShootingParameter,kSettingAmbientPresentation,kSettingQuietPcm,kNarrativeSettingCount};
bool decodeNarrativeSetting(uint16_t originalField,NarrativeSetting& setting);
class NarrativeQuerySource {
public:virtual ~NarrativeQuerySource(){};virtual bool read(NarrativeQuery query,uint16_t& value,std::string& error)=0;
};
// Executes only specified callable contracts. Constructors/data are never
// mistaken for actions; unsupported nodes stop with exact resource/node identity.
class NarrativeRuntime {
public:
 NarrativeRuntime(const std::vector<Scenario>& scenarios,EventScheduler& events,SemanticTrace& trace);
 bool start(Cursor entry,const std::vector<uint8_t>& packed,std::string& error);
 bool startGameCheckpoint(Cursor entry,const std::vector<uint8_t>& packed,uint16_t progression,const std::vector<uint16_t>& history,uint16_t presentation,std::string& error);
 RuntimeStatus run(size_t budget=10000);
 void setRandomSource(NarrativeRandomSource *source){_random=source;if(source)_randomRebindRequired=false;}
 void setQuerySource(NarrativeQuerySource *source){_queries=source;}
 bool consumeMenuSelectionReset(){bool pending=_menuSelectionResetPending;_menuSelectionResetPending=false;return pending;}
 bool menuSelectionResetPending()const{return _menuSelectionResetPending;}
 bool randomOwnerNeedsRebind()const{return _randomRebindRequired;}
 bool snapshot(const std::string& disc,NarrativeSnapshot& state,std::string& error)const;
 // All validation and scheduler rebinding stage before live mutation. Pending
 // playback/presentation owners must independently cancel and reissue their jobs.
 bool restore(const NarrativeSnapshot& state,const std::string& expectedDisc,const EventSnapshot& savedEvents,std::vector<EventRebind>& rebind,std::string& error);
 bool choose(unsigned ordinal,std::string& error);bool cancelChoice(std::string& error);
 // UI command identities are original text references, never screen positions.
 // Ambiguous duplicate references are rejected rather than selecting a route.
 bool chooseReference(uint16_t reference,std::string& error);
 bool resumeCompleted(std::string& error);
 // Source16922 empty-pass restart is a later coordinator traversal, not game end.
 bool continueTraversal(std::string& error);
 const std::vector<RuntimeRequest>& requests()const{return _requests;}
 const std::vector<uint8_t>& packed()const{return _packed;}
 const ControlState& control()const{return _control;}
 uint16_t lastPresentation()const{return _lastPresentation;}
 bool progressionSection(uint16_t& value)const{value=_progressionSection;return _progressionKnown;}
 // The actual fresh-game initializer DE86 clears the source progression owner.
 // Generic controlled runtime starts retain unknown ownership until supplied.
 void establishFreshProgression(){_progressionSection=0;_progressionKnown=true;}
 Cursor cursor()const{return _cursor;}RuntimeStatus status()const{return _status;}const std::string& diagnostic()const{return _diagnostic;}
 bool logicalSkip(Cursor start,uint32_t& end);
 bool logicalList(Cursor start,std::vector<Cursor>& items){return listItems(start,items);}
private:
 struct Work {Cursor cursor;uint8_t action;uint32_t saved;Work(Cursor c,uint8_t a=0,uint32_t s=0):cursor(c),action(a),saved(s){}};
 const std::vector<Scenario>& _scenarios;EventScheduler& _events;SemanticTrace& _trace;
 std::vector<uint8_t> _packed;ControlState _control;std::vector<Work> _work;std::vector<uint32_t> _locals;
 std::vector<Cursor> _returns;std::vector<RuntimeRequest> _requests;
 NarrativeRandomSource *_random;NarrativeQuerySource *_queries;uint16_t _progressionSection,_lastPresentation,_bookmarkedPresentation;bool _bookmarkKnown,_progressionKnown,_randomRebindRequired;
 Cursor _cursor,_entry,_pending;bool _hasPending,_menuSelectionResetPending;RuntimeStatus _status;std::string _diagnostic;
 const Scenario *scenario(uint16_t id)const;const Node *node(Cursor c)const;
 bool push(Cursor c,uint8_t action=0,uint32_t saved=0);bool fail(const char *reason,bool unsupported=false);
 bool request(RequestKind kind,const Node& n,uint16_t first,uint16_t second=0);
 bool command(const Node& n,uint16_t queue,const std::vector<uint16_t>& operands);
 bool nativeAction(const Node& n);
 bool branch(const Node& n);bool finishPass();bool restart(std::string& error);
 bool listItems(Cursor list,std::vector<Cursor>& items);
 static bool resolveExpression(void *runtime,const Node& node,uint16_t& value,std::string& error);
 static void observeRead(void *runtime,uint32_t node,uint16_t selector,uint16_t value);
};
}
#endif
