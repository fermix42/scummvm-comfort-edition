# Complete ordinary NPC navigation recovery518

Status: VERIFIED within declared source-driven contract. Contract before production changes. Parent0ee5cb93765.

Observed input: accepted owned48x48 path data94/normal navigation95 retains
Castle3 geometries15,25->32,15 and reverse with no route. Existing component
returns kNpcNavigationNoRoute correctly, but production WorldInput.prepareClockInput
accepts only Idle/Stored/Searching and returns unsupported for NoRoute/Capacity.
NpcActionDispatcher already owns WaitingForRoute and keeps that NPC still.
This is a demonstrated missing production continuation,not speculative RAM growth.

1. Preserve existing cooperative search,source-terrain legality,RNG consumption,
route store allocation and successful stored-route behavior. Do not rerun1495
oldpathcases,scheduler/animation/audio/campaign suites.
2. A world-facing navigation operation retires only the failed pending queue
entry on NoRoute or Capacity. Release active search/pathBusy and continue other
queued actors. Preserve actor/world positions,quest/party/clock,existing pages,
request identities and RNG actually consumed. Invalid/corrupt context stays
invalid;do not swallow every error or inject movement.
3. Missing-route actors use existing WaitingForRoute semantics;others proceed.
A later ordinary prefetch may retry the same authored route,including after
terrain changes. Do not add a permanent disabled-NPC flag or machine cache.
This is an EXPLICIT CE REPLACEMENT for a fatal/error-dialogue branch:an NPC
waits while the rest of the world stays usable. No new SFX/player UI needed.
4. Preserve native/legacy state formats. Retiring a failed job is quiescent;
active cooperative-search serialization remains unchanged. Verify restored
retired queue does not execute twice and subsequent real queued route works.
5. One focused new batch:both observed Castle3 no-route geometries,successful
queue progress/NPC wait/otherNPCmovement,existing-store capacity refusal without
overwriting live pages,RNG/state/component-codec retention and production caller
connection. Map/queue/actor/RNG starting mutations declared before operations.
No expected outcomes installed after starting operations.
6. One integration build. Include only the UNREACHED weather517 checks with
correct legal HP50 prerequisites as a shared-world regression reason;do not
rerun its already-passed85generator/86codec/12raster/6movement/3gate prefix.
This is a new NPC production-continuation batch,not a restart of517's ended
corrective loop. Keep split source/executable evidence and exact qualifications.
After coherent commit,continue remaining source-driven subsystems.

VERIFIED518b:2knownnoroute/1capacity case,3deferred/3waitingactors,2otheractor
moves,243cooperativestatecodecsets,actualworldinput. Firstnativebatchpasses;
initialcompileadapterprivate-accesscorrectionretained. NPC-RECOVERY518-
VERIFICATION binds372sourcefiles/exe/build/setup/remainingweathertests.
