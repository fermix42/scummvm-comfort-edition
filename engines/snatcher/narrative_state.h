/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_NARRATIVE_STATE_H
#define SNATCHER_NARRATIVE_STATE_H
#include "runtime.h"
#include "state_codec.h"
namespace Snatcher {
struct NarrativeWork {Cursor cursor;uint8_t action;uint32_t saved;NarrativeWork(Cursor c=Cursor(),uint8_t a=0,uint32_t s=0):cursor(c),action(a),saved(s){}};
struct NarrativeSnapshot {
 std::string disc;std::vector<uint64_t> scenarioStamps;
 std::vector<uint8_t> packed;ControlSnapshot control;
 std::vector<NarrativeWork> work;std::vector<uint32_t> locals;std::vector<Cursor> returns;
 std::vector<RuntimeRequest> requests;
 Cursor cursor,entry,pending;RuntimeStatus status;bool hasPending,progressionKnown,randomOwnerAttached,menuSelectionResetPending;
 bool bookmarkKnown;uint16_t progressionSection,lastPresentation,bookmarkedPresentation;
 NarrativeSnapshot():status(kRuntimeIdle),hasPending(false),progressionKnown(false),randomOwnerAttached(false),menuSelectionResetPending(false),bookmarkKnown(false),progressionSection(0),lastPresentation(0xffff),bookmarkedPresentation(0xffff){}
};
// Semantic decoded-source stamp; not a cryptographic media identity. The SNSV
// envelope and payload independently bind the caller's verified disc SHA256.
uint64_t narrativeScenarioStamp(const Scenario& scenario);
// Explicit, caller-authenticated variant pair. Pending owners must have exactly
// the same node semantics; no changed instruction is reinterpreted on load.
bool rebindNarrativeScenario(NarrativeSnapshot& state,unsigned index,const Scenario& previous,const Scenario& current,std::string& error);
bool validateNarrativeSnapshot(const NarrativeSnapshot& state,std::string& error);
bool encodeNarrativeState(const NarrativeSnapshot& state,NativeSaveSection& section,std::string& error);
bool decodeNarrativeState(const NativeSaveSection& section,const std::string& expectedDisc,NarrativeSnapshot& state,std::string& error);
}
#endif
