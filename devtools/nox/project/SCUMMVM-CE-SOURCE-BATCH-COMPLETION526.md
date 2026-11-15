# Source-driven batch completion526

Status: VERIFIED for the enumerated implementation batches below.
Whole port/M4 status: IN PROGRESS. This is not full engine or campaign acceptance.

| Complete scoped work | Implementation and evidence |
|---|---|
| Weather/population517 | Catalog selection, generation/RNG ordering, storm movement/footprints/damage/protection, turn scheduling, travel cleanup and saved state. Commit0ee5cb93765; WORLD-POPULATION517-VERIFICATION combines the preserved517c prefix with518b remaining checks. |
| Ordinary NPC route failure518 | NoRoute/Capacity defer the failed job, preserve actor/world state and let other work/input continue. Commit249037bdfc0; NPC-RECOVERY518-VERIFICATION. |
| Known magic implementation519 | Existing owners cover all eight enumerated magic implementation rows and the previously enumerated spell domain. Static closure; no new magic rewrite/build/runtime promotion. |
| Shared normal persistence520 | Eight bounded processes pass Modern/Classic normal Engine saving, generations/session isolation, exact cold state and following input on518b. PERSISTENCE520-VERIFICATION; no fresh525 cold claim. |
| Production bootstrap522 | Normal New Game obtains engine entropy; explicit replay stays deterministic; loaded RNG ownership retained. Commit2ec8f803eb4; BOOTSTRAP522-VERIFICATION. |
| Shared lock retry523 | Y/Return retry; other acknowledgements close after the existing native attempt handoff. Commit14e71152380; SEARCH-RETRY523-VERIFICATION. Native per-attempt versus original deferred scheduling is explicit. |
| Normal command domain524 | All28 ordinary entries have production owners. Hidden legacy ! unlock/restricted3 alias deliberately excluded; normal A retained. Commit2616ca2f1aa; COMMAND-DOMAIN524, static scope only. |
| Hardcoded world approach525 | Missing Temple-prerequisite/surface-sector approach flag now produced by normal idle/input paths and retained by existing core saves. Commit1176db1f4c8; WORLD-STORY525-VERIFICATION. Original private disk checkpoint excluded. |

Latest native candidate: world-story525. SHA256:
f9e8a81c5c29ecd699dcc86a52f04b0753bea724d6a5304d653a2e6066acb716.
Frozen377 engine files and source archive are bound in its receipt. Build:
0 warnings,0 errors,2m41.48s. First focused525 batch passes8 original predicates,
2 production hooks,2 repeat/core restores and2 subsequent typed admission queries;
one core flag byte changes, full RNG unchanged. No actual boss battle performed.
The523 batch also passed its first native run; initial522 comparison-reference
mistake and its no-rerun correction remain explicitly recorded.

Accepted audio473 and its seven implementation files remain unchanged. No audio
suite rerun. Original HDVs, standalone/reference code and accepted historical
executables/evidence are preserved. No push, publication, release or account change.

## Remaining qualifications and next phase

The completion matrix remains IN PROGRESS. PARTIAL/UNKNOWN rows are not all
promoted merely because a known owner exists. In particular:

* story.08 remains UNKNOWN beyond the reviewed authored data and direct hardcoded
  selector contexts.525 is not a proof about every indirect original writer.
* Current joined controls/items/services/combat-return/presentation workflows
  have wider normal Engine cold/session requirements than component codecs.
* Full earned native-only campaign/ending and supported branches remain required
  by M4.3. Controlled M3/checkpoints or predicate queries do not replace them.
* Historical caption, Quick scratch, Search scheduling, prototype/debug and
  legacy-import qualifications retain their explicit native policies and scopes.

No further concrete missing owner was demonstrated in the reviewed normal
command/direct-location producer scope. Do not invent speculative behavior,
repeat completed censuses/suites or treat unproven global coverage as completion.

The prepared next execution phase is SCUMMVM-CE-INTEGRATION-PLAN.md I1-I5;
its unexecuted batches remain NOT STARTED. The current source-driven plan says
to prepare that phase without running it, and AGENTS explicitly says:
"Do not run campaign traversal/acceptance or exploratory gameplay."
This report does not override that boundary or waive any remaining port criterion.
No background build/test/campaign run is left running at this checkpoint.

Canonical tracking lives in D:\Codex Projects\Nox Archaist\conversion and is
mirrored byte-identically under the production CE checkout's devtools/nox/project.
