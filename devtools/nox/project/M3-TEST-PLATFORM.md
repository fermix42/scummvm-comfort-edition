# M3 test platform

Run commands from `D:/Codex Projects/Nox Archaist`.

```
python conversion/m3_platform.py reconcile
python conversion/m3_platform.py select --changed engines/nox/vortex_validation.h
python conversion/m3_platform.py checkpoint --report <relative-verification.json> --slot 1
python conversion/m3_platform.py run-case --report <relative-predecessor-verification.json> --case <relative-case.json> --artifacts <new-artifact-directory>
python conversion/test_m3_platform.py
```

`reconcile` generates the scenario manifest and dashboard from canonical coverage, checks declared audit/report/artifact hashes and preserved executable/source archives, and exits nonzero for integrity failures. Historical pending/investigation claims are retained separately from integrity. It never upgrades gameplay criteria, declares a route complete, or counts a controlled setup as earned progress. Hashes are cached within one invocation with file identity/size/time checks; no stale cross-run cache is accepted.

The generated dashboard is the current evidence-count summary. Update coverage only after source/oracle review; rerun reconciliation after accepting evidence. Existing progress prose remains a milestone narrative, not an independent live test counter.

`select` uses recorded source dependencies. Existing reports often list the entire engine source directory, so selection is deliberately conservative and may select most suites. Unknown dependencies require review, not a silent skip. Narrowing these declarations requires evidence about actual test dependencies; do not claim fine-grained regression acceleration yet.

`checkpoint` validates report artifacts and the actual NXSV version/length/checksum before selecting a save generation. It writes no game state, renames no save descriptions and grants no progression credit. `run-case` verifies the explicit predecessor and delegates to the existing CE driver. Cases must declare source anchors and controlled initialization. Existing predecessor-label contracts remain enforced. This removes custom wrapper scripts for compatible cases without inventing a general state-conversion mechanism.

The engine-level shared conversation controller remains the tested build40 implementation; no additional input-controller rewrite is needed for this platform work. No new screenshots, game delays, desktop automation or game-logic changes were introduced.

Validation: three integrity tests (tampered artifact, escaping path, unverified predecessor); all72 declared evidence references pass integrity; real CE Father WRONG refusal warm/cold through run-case passes. Logs and receipt stay local in conversion. M3 source review resumed with SCUMMVM-CE-M3-STORY-GRAPH.json: all15 Lord British advice branches mapped, while complete-route C1/C2/C3 acceptance remains open.

Completed-build guard: run `python conversion/m3_platform.py accept-build --log ce-m3-<successful-build>.log` after build has exited successfully. It records x64 PE and executable/source hashes in BUILD-RECEIPT. run-case rejects active compilers, newer source, missing receipt or changed/partially linked executable; timestamps alone are insufficient. Four integrity tests include substituted executable/source rejection.

## Accepted M3 platform closeout

Use an exact completed build receipt before gameplay. test_late_npc.py accepts a separately declared controlled-setup manifest; preparation is never a fabricated VERIFIED gameplay report. m3_prepare_checkpoint.py changes only the currently loaded original map tables/cache and explicitly named prior ordinary deaths BEFORE a new operation. CHECKPOINT71 independently proves quest/inventory/HP preservation. Actual expected gate/reward results always come from subsequent CE operations. Native tasklist guards compiler activity; unknown inspection failures are not mislabeled active builds. Reuse reports only when case, parent, setup, source and executable identities all match. Hashes do not promote gameplay acceptance. For this unstable host the final suites use own-process inherited E-core scope0xffff0000; root cause remains unconfirmed. Four negative integrity/build tests pass. No OS/BIOS affinity changes or M4 work.
