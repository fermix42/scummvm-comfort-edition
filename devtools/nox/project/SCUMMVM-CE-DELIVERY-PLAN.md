# Current methodology - November 14, 2026

The source-driven subsystem directive supersedes dated reactive/campaign queues and the prior audio stop. Current execution plan: SCUMMVM-CE-SUBSYSTEM-PLAN.md. Matrix: SCUMMVM-CE-ENGINE-COMPLETION.json. No gameplay discovery or campaign acceptance is authorized now. Audio473 remains accepted; D3-D5 remain unfinished.

# User stop boundary - November 14, 2026

After the audio subsystem is complete and verified, present an audio completion
report and stop until the user gives further instructions. This supersedes the
standing goal's automatic continuation into graphics/input/campaign/release work.
Finish the current full sound/music batch and its connected acceptance first;
do not treat this instruction as a request to stop unfinished audio work now.

# Sound/music batch execution - November 14, 2026

The user explicitly rejected repeated incremental builds. Let the already-started
470 build finish. Implement all remaining required sound/music integration before
the next combined build, then run focused connected acceptance. Earlier builds
require a concrete dependency that prevents further implementation; a few completed
cues are not a build boundary. Keep one concise result for the complete batch.

# Infrastructure priority - November 14, 2026

User clarification: graphics, music, sound and inputs are basic infrastructure
and must exist as complete connected systems. Finish the in-flight ship-departure
change, then prioritize closing these infrastructure gaps before sustained earned
campaign progression. Use controlled/linked gameplay to verify infrastructure
callers; preserve earned127 and the later observed stranded128 diagnostic.
Do not switch to the next campaign obstacle after each narrow sound check.
D3-D5 scope and full earned ending/release acceptance remain unchanged.

Immediate execution order: (1) finish/check pending boat-departure464;
(2) complete authored sound generators and their gameplay callers as one batch;
(3) reconcile and close graphics/music/input gaps against actual runtime and
existing acceptance evidence; (4) resume sustained earned campaign/release work.
Existing working infrastructure is retained; only missing or defective behavior
needs implementation. Do not restart completed broad matrices or build a generic
test-platform project. Full subsystem claims require connected runtime evidence.

# Standing completion goal � November 13, 2026

The user authorized continued autonomous work through remaining D3-D5 overnight.
Milestones are internal checkpoints, not permission/idle stops. Continue the next
unblocked necessary task after each package. The approximately6:30AM Mountain
check-in is a status checkpoint, not a scheduled stop. D1/D2 evidence remains
preserved; earned campaign/native-only full release criteria remain unchanged.
No push/publication or unrelated features. Begin D3 now, reconcile stale work
records against actual source, then continue D4/D5. Detailed goal is held by the
active Codex goal; the finite delivery queue below remains authoritative.

# Delivery plan — November 13, 2026

Status: IN PROGRESS. This is the current execution queue, superseding dated
ordering and next-action instructions in the implementation/M4 plans and logs.
The user requested an actual-engine review and a shorter path to completion.
This request resumes review/planning after the October18 pause; it does not claim
new gameplay acceptance. Existing M4 implementation authorization remains valid.
Architecture and release acceptance are unchanged. No publication is authorized.

## Delivery priority clarification

The user clarified during this review: finish ASAP; do not preserve irrelevant
scaffolding. Preserve original assets, player saves and accepted reference evidence,
but freely replace/remove obstructive implementation and redundant validation code
within authorized M4 work. Do not maintain every old diagnostic command or codec
solely for historical convenience. Required supported player imports and release
criteria still need explicit implementation or a user-approved scope change.
Avoid a general cleanup/rewrite: remove scaffolding when it blocks the next delivery.

## Findings from actual source

- `nox.cpp:238` selects native startup only through `nox_native_startup`;
  ordinary launch constructs `Runtime`. `module.mk` still links runtime,
  CPU/audio compatibility and validation objects. `runtime.cpp` includes
  `cpu.inc` and performs the original loader/music bootstrap. Native-only release
  is an outstanding architectural deliverable, not a counter reaching zero.
- Native systems are substantial: `PlayerSessionState` owns game, party, world,
  input, encounter, presentation and music state; normal saves use NPLS/NPXS.
  Its staging/commit load path is separate from machine-image restoration.
  Preserve these systems; a rewrite would discard the principal investment.
- `native_startup_host.h` is a 1,020-line inline host combining gameplay,
  oracle runners and controlled fixtures; 142 of 307 engine files have validation
  in their name. These counts show coupling, not a measured percentage of wasted
  work. Split only the dependencies needed to build the real native release.
- Temple failure has a concrete source-supported cause: `WorldInput::travel`
  constructs its scene before applying NPC scheduling; `beginTempleConversation`
  requires NPC8 in that scene. The shared ordering is intentional for ordinary
  travel. Refresh the Temple continuation after scheduling, without globally
  reversing travel order. This is a proposed fix, not verified gameplay.
- The plans already prescribed workflow batches and limited regressions. More
  prose prescribing the same behavior is insufficient. The queue below changes
  delivery order and defines investigation exit rules.

## Current identity and limits

CE checkout: `D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated`.
Inspected Git HEAD: `d35a5e955daf19251c23e31e9d588cd4860e7f7c`.
Git status has four existing modifications: root/project AGENTS and the two
clean-environment build scripts; no engine modifications were reported.
Canonical Nox source remains unversioned. No repository state was reset.

Current consolidated executable SHA256:
`b35fca1622f0ba5370105efd33f14c31460420472fe625d61e762d243a386d8b`.
It is not historical384 or focused387. Against387's 307-file receipt, 116 raw
hashes differ: 114 match after newline normalization, and `native_summon_pool.h`
and `native_summon_validation.h` have other differences. Against384, 120 differ,
110 solely by line endings. Full lists are in
`SCUMMVM-CE-DELIVERY-REVIEW-20261113.json`. Do not infer behavioral changes from
raw hash counts or credit historical tests to the consolidated binary.
M1/M2/M3 verification records were inspected; their accepted scopes remain
historical.384 remains the latest accepted M4 integration checkpoint in the
canonical records. No build or gameplay test was run during this review.

## Finite delivery queue

| Order | Status | Deliverable and exit condition |
|---|---|---|
| D1 | VERIFIED | Build a native-only development target early using CE's clean environment. Remove compatibility references from its host/save/session/validation link path; preserve the separate oracle build. Prove new game, real movement and ordinary save/cold load without Runtime/CPU/bootstrap/IRQ objects. This begins M4.2 engineering now; it does not verify M4.2 before complete supported acceptance. |
| D2 | VERIFIED | Finish Temple Talk -> full admission -> revelation/results -> journal -> travel -> Archus -> next operation. Apply the scoped post-schedule refresh; run one focused continuation with pending-revelation cold split and affected ordinary travel/conversation/ending checks. Keep declared final-blow controls separate from earned combat. |
| D3 | IN PROGRESS | Close normal-player integration: native default startup/Continue/Q/menus, required legacy imports, music return/SFX/settings, discovery/events, sessions and assistance. Reuse existing370/376/382 owners and evidence. Register concrete missing operations in the existing W6 work records; close them as connected workflows. Test both save modes and session isolation at the boundary. No new achievement catalog or generic test-platform project. |
| D4 | IN PROGRESS | Close supported branches against the existing campaign matrix and combat contract: forced-Quick retained context, tracked Drop contexts, Castle11 and exceptional admissions/projectiles. First reconcile each old open item with current source; do not reimplement completed behavior. Each remaining item must name a reachable supported caller, missing behavior and completion check. Unknown reachability stays explicit; it cannot be silently waived. |
| D5 | IN PROGRESS | Freeze native-only release candidate and complete M4.2 dependency audit/default-launch acceptance. Begin an earned native new-game-to-ending campaign as soon as its route can execute; checkpoint ordinary saves and resume that lineage. Finish required branch/save/audio/session acceptance and full-health final battle/ending on the release identity to verify M4.3. Controlled fixtures cannot substitute for earned progression. |

D1 precedes further broad behavior conversion so hidden architectural dependencies
surface now. If D1 exposes a genuine D2/D3 dependency, record the exact link/runtime
dependency and resolve it there; do not wait for every hypothetical branch before
attempting native-only compilation. D3 work can proceed when a D2/D4 source question
is blocked. Start the earned route during D3/D4 when playable, rather than delaying
all integration feedback until the last subsystem is declared complete. Final
acceptance remains on the frozen release identity; changes require affected
validation and a compatible earned lineage, not automatic replay of every old run.

## Execution rules that prevent another testing loop

1. Each work batch must deliver one row's player capability or remove its named
   dependency. Report blockers closed and remaining, not build/checkpoint counts.
2. During coding: incremental/focused clean-environment build plus changed-owner
   checks. Do not rebuild for an external driver edit. Batch related code changes.
3. At the workflow boundary: one linked gameplay chain, relevant refusal, resulting
   state, fresh-process persistence and next action. Use existing immutable setup
   and reference results. Expand only for changed shared contracts or actual failures.
   Broad acceptance runs at integration/release boundaries, not each helper edit.
4. Bound a source investigation to one focused work batch (normally at most one
   hour). It must yield a caller/behavior, an implementation decision, or a named
   unresolved dependency. Do not repeat the zero-group385 experiment without new
   caller evidence. Two harness corrections without gameplay progress trigger a
   simpler driver/operation boundary before another engine patch. No automatic
   deferral of a required reachable branch merely because its budget expired.
5. Prefer the existing native Test CLI where Nox capabilities suffice; otherwise
   retain deterministic Nox key replay. Do not build a new adapter/platform merely
   for uniformity. Use selective images for presentation defects and release QA.
6. Keep one current queue (this file), one session progress entry, and raw commands/
   results. Update machine progress at meaningful boundaries and sync once. Do not
   prepend duplicate narratives to every contract or create a source archive for
   each helper pass. Preserve old evidence unchanged; new acceptance binds its
   actual source/executable/resources. No manual mass re-hashing of old reports.

## Ghidra and understanding the original

Ghidra is installed under the user's local Programs directory, with headless
launchers and a6502 processor module. It was not launched in this review.
Use it only for a concrete caller/data-flow question that existing disassembly,
resource readers and qualified original execution cannot answer cheaply—first
candidate: forced-Quick context provenance; second: zero-count group5 producer.
Load the particular resource with its correct origin and bank/overlay identity;
validate any65C02 extensions against bytes and existing interpreter behavior.
Decompiler output is evidence to check, not a game specification. Keep projects,
raw resources and extracted text local. No whole-image decompilation prerequisite.

We have a strong recovered model and qualified reference behavior, not exhaustive
understanding of every original branch. The campaign dependency matrix, native
owners and targeted original comparisons are enough to drive bounded delivery;
the earned run remains the required integration proof. No defensible completion
percentage or time estimate follows from current records.

D1 is VERIFIED by SCUMMVM-CE-NATIVE-ONLY-VERIFICATION.json on391.
D2 is VERIFIED by SCUMMVM-CE-TEMPLE-WORKFLOW-VERIFICATION.json on394: both modes,
normal pending-save cold/uninterrupted equality and subsequent gameplay.
The user turned Smart App Control Off; no current launch blocker remains.
The Temple goal is complete. The new standing goal continues through D3-D5;
full M4.2 release and M4.3 campaign acceptance remain outstanding.
