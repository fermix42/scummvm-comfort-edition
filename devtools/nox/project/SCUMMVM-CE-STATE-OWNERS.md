# Nox CE state ownership — M2

Inspected October 15, 2026. Authority: SCUMMVM-CE-ARCHITECTURE-DECISION.md.
This inventories the actual CE implementation, not the standalone's advertised
features or a claim of campaign coverage. M1 source baseline was reconciled to
its recorded hashes before M2 edits; local archive and digest are recorded in
ce-m2-baseline-verification.json.

## Live CE owners

| Owner / source | State | Persistence and reconstruction | M2 work/evidence needed |
| --- | --- | --- | --- |
| Runtime in engines/nox/runtime.h | mainram, auxram, language-card banks and high banks | Authoritative opaque gameplay/controller bytes; runtime_state.cpp codec2 preserves codec1 field layout and appends mandatory enhancement state; explicit codec1 reader | M2.1d VERIFIED: every byte of five opaque storage domains mutated/restored on disposable clones; original RNG64 and clock124 with BCD oracle; bounded input/retained PCM continuation. Opaque preservation is not semantic interpretation or campaign coverage |
| Runtime | registers, PC/origin, stack in bank memory, mapping flags | Explicit field encoding in runtime codec | Preserve controller continuations and deterministic timing; no original-instruction conversion queue |
| Runtime | private disk array | Full private source-image copy plus runtime block writes; serialized in full, originals read-only | Audit legacy save/load and resource write entry paths; unify persistence before reporting in-game save complete |
| Runtime | video flags | Stored fields; surface rebuilt by render() | Rebuild parity exists for M1 opening; add owner-specific mode/scene cases |
| Runtime | cpu_cycles, total_steps, nativeCalls | Explicit 64-bit fields | cpu_cycles is simulation/synthesis scheduling; do not confuse host frame time with original world time |
| Runtime | speaker and both AY/VIA synthesis instances | Explicit field/finite-value validation, sink/context rebound on successful candidate decode | Retain deterministic continuation; device queue is separate |
| Runtime | music, worldWaiting, key_latch | Encoded flags/latch; save eligibility requires empty input queue and no latched key | Demonstrate idle/IRQ/modal eligibility for both save entry paths; no arbitrary unsafe save claim |
| Runtime | keys, generated samples, pending InterfaceRequest | Transient: queued input must be empty; samples cleared after successful load; Q/Continue host command consumed before dialog/serialization | Do not serialize host events or replay queued input on load; normal-key save/load/cancel and unchanged disk verified in both modes |
| Runtime | rom, font | Immutable constructed host stubs and redistributable font raster; rebuilt by constructor | Resources/fonts must remain compatible; not mutable gameplay state |
| Runtime | failure string | Transient error diagnostic | Never persist diagnostics as authoritative world state |
| Runtime / Enhancements | resource-qualified NPC discovered/used topics and resolved reply history; resource/map-qualified observed versus revealed cells and annotations | Runtime codec2 mandatory NXEH-v1 same-payload chunk; bounded16MiB, candidate decode, explicit empty migration from codec1; derived authored matching and cell index not serialized |23 controlled owner/codec contracts pass on build79930; nonempty actual Engine integration under validation. Actual observed capture still M2.3b, full lifecycle M2.3c/M2.4 pending |
| HdvArchive in NoxEngine | original image/directory/entries/error | Immutable resource owner, parsed on launch; runtime disk is distinct copy | Add explicit saved resource compatibility identity; do not hash mutable original save sectors as sole compatibility rule |
| NoxEngine | surface, audio pointer/handle | Derived host surfaces/device queues; fresh mixer queue on load | M1 actual cold frame and empty queue checks exist; retain during M2 |
| CE popup owner at Nox fresh-world boundaries | seen/progress state and last-sidecar pointer | Existing resetPopupState now called for fresh runtime startup and successful new-session assignment; actual load restores selected CE sidecar | Local lifecycle tests restore scalar11/22/33 independently of permanent Casual A/clean B; full audit-flags/network/unknown-history matrix remains M2.4 |
| Engine / Saves::Metadata | total playtime; target, CE client ID, logical slot, description; Engine save mode; local save timestamp and actual-world preview | NSM2 mandatory resource/mode chunks plus optional DATE and fixed160x120 P565; metadata bounded65536; explicit NSM1 reader defaults v119/Modern | M2.1c and M2.2a/b/c VERIFIED:22 schema cases, both-mode actual date/preview/deletion matrices, UI captures and genuine M1 migration; full mode/session/caller lifecycle still pending |
| SnapshotStore | two generations, sequence, checksum | M1 envelope v1; M2 writer v2 adds terminal NXOK marker with explicit v1 reader | Mixed-version recovery and old actual gameplay fixtures must pass before verification |
| MetaEngine | target, cached Entry list and save-mode flag | Derived chooser mapping; not serialized | Pin operation identity, refresh safely, add complete logical delete/sidecar behavior |
| Common::CEAchievements | session registry, permanent per-session assistance-reason arrays, popup sidecars, signed transport queue | CE registry2 preserves per-session reasons; v1 Casual records explicitly migrate to legacy unknown-assistance marker. Casual activation/acknowledgements are monotonic; Nox does not serialize permanent history into world state | M2.4a VERIFIED for old/new/A/B reason isolation, actual delayed authentic clean replies, same-identity immutable replay and independent server metadata. Archive/delete/unknown-history/offline-link and429 backoff remain M2.4 |

## Standalone owners and CE adaptation boundary

`conversion/native/bridge.cpp` holds process-global map caches, WorldEvents,
ConversationJournal and a pending conversation event. Its resource initialization
clears them, but those globals are not linked into the CE engine's module.
Do not claim that the CE runtime serializes those native C++ owners merely because
it serializes the original game's RAM and private disk.
M2.3a now adapts journal/discovery durable semantics into a separate per-Runtime
Enhancements owner. M2.3b adds runtime_observers.cpp: actual visited-cell capture
is verified for the normal opening; actual completed NPC response capture and
recruited naming pass both modes using explicitly assisted authored-exit/adjacency
setup. Standalone globals and event-file logging are not linked.

- ConversationJournal already distinguishes discovered/used topics, actor and
  resource/record identities, observed responses and a consumable event queue.
  CE's per-runtime C++11 owner now preserves discovered/used/history semantics
  with bounded restore/reset. Standalone event-file logging is not linked into CE;
  actual observed response acceptance now covers9 original replies,6 used topics,
  intermediate discovery-before-use, alias/default behavior and authoritative
  recruited naming. See ce-m2-dialogue-capture-verification.json for exact bounded
  fixture provenance. Hidden authored topics stay private; no all-NPC claim.
- WorldEvents describes committed turn/interaction/location boundaries. Adapt
  verified completion boundaries; decoding a resource is not a discovery event.
- Native map/catalog caches are derived resource views. CE currently renders
  directly from machine banks. Durable observed/revealed cells and annotations
  now have a resource-qualified same-payload owner. Actual opening visited cells
  are captured only at the code-qualified committed world boundary, with original
  filename/family/archive identity checks. Active map identity is derived from
  saved RAM on load. This is visited-cell foundation, not complete visibility/FOV
  or transition coverage. Do not implement final map/dialogue UI in M2.

## Format layers and migration boundary

1. SaveFileManager filename hashes target/session/logical slot and selects a
   physical generation. Payload still validates full identity, not only a hash.
2. SnapshotStore envelope verifies version, monotonic generation, bounds and
   checksum. New v2 completion marker is part of checksum; explicit v1 reader
   preserves M1 envelopes. Reader rejects unknown mandatory envelope versions.
3. NSM2 metadata records target/session/description/slot/playtime plus mandatory
   resource-fingerprint and mode chunks. Bounded optional unknown chunks can be
   skipped; unknown mandatory chunks/versions, duplicates and incompatible
   identity/mode/slot combinations are rejected. NSM1 migration explicitly
   defaults to v119/Modern. M2.1c VERIFIED on executable d5452fde (full SHA256 and reports in progress).
4. Runtime codec2 retains all opaque execution fields and appends a mandatory
   NXEH-v1 enhancement chunk in the same payload. Codec1 explicitly migrates to
   empty enhancement state. Unknown enhancement versions, malformed lengths,
   duplicate identities or noncanonical booleans reject the candidate. Matching
   data is rebuilt on the next qualified dialogue packet; cell lookup is rebuilt
   during decode. Saves remain restricted to idle world commands, so pending
   response observers are transient and must finish before a save boundary.
   Device/input events and CE account/history remain external. The16MiB chunk
   limit and20MiB total envelope bound fail explicitly rather than dropping history.
5. Shared CE sidecars remain the session commit/provenance boundary. Missing
   sidecars need conservative CE-owned recovery/untracked handling, not automatic
   trusted history inferred from an unsigned payload.

The inventory itself is VERIFIED by source inspection. M2.1 bounded serialization/owner contracts are VERIFIED by ce-m2-state-contracts-verification.json; full M2 lifecycle remains IN PROGRESS. Early/mid/late campaign claims
require their own provenance; controlled state fixtures must be labeled as such.

## Live Runtime session ownership (M2.2d3a under validation)

NoxEngine now records `_runtimeSession` independently of CE's current selection.
Saving requires a nonempty matching ID; a selection change cannot relabel the
previous world through saveGameState. Before the next game-loop advance,
synchronizePlaySession builds a new Runtime from read-only resources, replaces
the prior runtime only after initialization succeeds, clears the audio queue,
resets playtime and adopts the configured new-session save mode. Full validated
loads assign the saved owner/mode after the candidate succeeds. CE-owned history
is not part of this runtime replacement. Enhancement owners still require M2.3
and complete lifecycle/default-change acceptance remains pending; source intent
does not establish those tests.

## Confirmed enhancement capture sources for M2.3

The existing standalone `conversation_observer.inc` records only resolved response
returns and resource-qualified dialogue/recruitment code. It supplies packet
identity, record/actor, submitted input, resolved selected label and actually
displayed response to `native/conversation_journal.cpp`. That journal keeps
authored labels private; only spoken or successfully used terms enter discoveries,
with separate used/history state. CE must adapt these existing semantics to a
per-Runtime owner and retain the return/stack qualification; packet loading alone
is not a discovered topic. The observer's file logging/test packet dumps are
standalone diagnostics and do not belong in ordinary CE gameplay.

`live_events.inc` supplies committed movement/interaction/location boundaries to
`native/world_events.cpp`; it tracks active map resource identity separately from
coordinates. CE Runtime currently lacks that active resource owner. Establish
resource-qualified location identity before recording discovery, rather than
using a coordinate or a shared code address as the global map key. Pending
observation state needs an explicit save-boundary contract alongside durable
discovery/history and derived private resource matching data.

This is source investigation, not implemented or tested CE enhancement capture.

## M2.4 save history content boundary

New Nox CE sidecars carry save_hash for their exact physical snapshot generation.
The CE service compares that hash before Nox restores save-associated CE popup
progress. This detects stale generation metadata, not malicious user edits.
Missing/corrupt/mismatched commit sidecars are refused before live replacement.
An otherwise valid legacy or stale same-session sidecar keeps the saved session
identity/world/mode, adds permanent unknown_save_history Casual classification,
and clears uncertain CE popup progress. Original files are not rewritten merely
by loading uncertain history. A missing/deleted registry owner instead recovers
the complete validated world into a NEW Casual session, retaining original files.
Modern/Classic missing-history runtime evidence belongs in the progress document;
complete offline/link/deferred audit and I/O failures remain separate gates.

## Original save callers and compatibility image boundary

M2.2d4 source/oracle and actual normal-key audit is recorded in ce-m2-original-caller-verification.json. Original New Game prepares both private slot images through FE95/9600; actual trace records X49 then50, map8, matching GAME70EE initialization. These writes remain per-Runtime compatibility bytes and become durable only as part of a complete CE snapshot. Internal source helper GAME8E0D uses the same private image layer; natural campaign triggers are not claimed tested. User Q and opening Continue use the unified Engine serializer, with actual cancellation/durable load evidence both modes. Distinct original overlays also execute at9600, so no global address suppression is valid. All original HDVs remain immutable; general instruction interpretation is retained until M4.

## Final M2 reconciliation

M2 VERIFIED on final audit SCUMMVM-CE-M2-VERIFICATION.json. All live mutable owners and external CE history are accounted for by current-codec/resource/owner/lifecycle/failure/live tests. Opaque original fields are preserved without asserting semantic campaign coverage. Journal/discovery/annotations persist in the same world payload; permanent history remains CE-owned and monotonic, including signed same-owner audit repair after registry write denial. M3 earned campaign stages and M4 bridge removal remain unimplemented. Historical pending rows above retain their investigation provenance; final source/report audit controls completion.
