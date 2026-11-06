# Nox Archaist development instructions

## Approved migration and recovery

The authoritative technical decision is `conversion/SCUMMVM-CE-ARCHITECTURE-DECISION.md`.
Execution order and acceptance criteria are in `conversion/SCUMMVM-CE-IMPLEMENTATION-PLAN.md`;
current evidence and the next action are in `conversion/SCUMMVM-CE-PROGRESS.md`.
Read all three at the beginning of a new session, after this file and any applicable
parent/scoped instructions. Inspect the actual Git working trees and reconcile the
documents against source, build identities and evidence before resuming.

M1 and M2 implementation and testing are authorized. The user's October 15, 2026
"proceed" after M1 completion authorizes M2; it does not authorize M3 or M4.
Prioritize normal player workflows; use bounded fault tests only for an explicit
requirement or observed defect (user clarification October 15, 2026).
Continue through unblocked tasks in the authorized current milestone
within the active execution session: inspect, implement the smallest complete
change, build/test, diagnose/correct, record evidence, then select the next task.
Do not ask for routine compilation/refactoring/test iterations. If blocked, record
the reason and continue independent authorized-milestone work. Do not repeat failed experiments
without new evidence. Stop before implementing M3–M4; planning them is authorized.

Use only NOT STARTED, IN PROGRESS, BLOCKED and VERIFIED as task statuses. VERIFIED
requires actual evidence for every acceptance criterion. Persist task status,
modified files, exact commands/results, failures, dependencies, blockers and next
action after each meaningful unit and before ending a session. Do not silently
weaken criteria. M1 requires actual CE gameplay, sessions and durable cold-process
saving; a library build or title frame is insufficient.

Preserve working native code and the standalone regression build. Never modify
original HDVs. Keep proprietary resources, extracted content, reference generated
programs and diagnostics local; do not automatically copy them into the CE Git
repository. Do not push, deploy website changes, reset/clean/delete repository
state or implement later milestones without explicit authorization. Reuse CE
Play Sessions, achievements and assistance semantics. No placeholder achievements
and no speculative instruction-address conversion queues.

## Workspaces

- Nox source and canonical tracking: `D:\Codex Projects\Nox Archaist`.
- CE production checkout: `D:\Codex Projects\scummvm-private\scummvm-private-feature-usable-desktop-window-layout-git`.
- Read CE's AGENTS.md before editing/building there; use its clean-environment build script.
- The Nox source directory initially has no Git repository. Do not claim it is a
  clean/versioned tree. Mirror the three canonical migration documents and this
  instruction file into CE `devtools/nox/project/` for repository-based recovery;
  keep the copies byte-identical and record both locations in progress. Do not
  initialize or import the resource-heavy Nox directory into Git as a side effect.

## Approved zero-achievement session identity

Use API game `nox-archaist` and variant `apple2-v119` for base v119;
Nox Archaist / Apple II — Base Game v119 are the approved display names.
No placeholder achievements. Website registration may be prepared and tested in
isolated local/staging data. Production deployment/account changes remain outside
authorization. Use dedicated non-admin test users and runtime/private credentials;
never put tokens/passwords in source, committed configuration or reports. Only the
live API criterion may be BLOCKED for unavailable credentials/environment; continue
independent M1 work. CE `devtools/nox/ONLINE-VERIFICATION.md` records the API test
command and remaining end-to-end steps.

## M1 completion checkpoint

When progress records M1 VERIFIED, read `conversion/SCUMMVM-CE-M1-VERIFICATION.json`
for exact executable/source hashes and evidence paths. Historical checkpoints and
corrected failures are retained in `conversion/SCUMMVM-CE-M1-WORK-LOG.md`; their old
pending states do not override current progress. Recovery copies include both.
M2 was subsequently authorized by the user's "proceed" on October 15, 2026.
Preserve M1's historical audit and local source baseline; new M2 source must earn
its own evidence. Stop after M2 verification until M3 is explicitly authorized.

## M2 verified completion checkpoint

M2 is VERIFIED in conversion/SCUMMVM-CE-M2-VERIFICATION.json. Read that audit and current progress before new implementation. The final executable and all required local/live evidence are hash-bound there; historical failures remain in progress. Stop: M3/M4 implementation requires explicit new authorization. General original-instruction interpretation and private image compatibility remain temporary dependencies, not completed M4 work. Do not rerun completed tasks without source drift or new evidence.

## M3 authorization and testing clarification

The user's October 15, 2026 "M3 Testing Strategy Clarification" explicitly
authorizes autonomous M3 implementation/testing and supersedes earlier M3 stop
instructions and exhaustive earned-run prerequisites. M4 remains unauthorized.
Use source-supported controlled campaign checkpoints, linked real gameplay,
deterministic CE input/state inspection, and selective visual evidence. Never
inject expected outcomes after starting a tested operation. Record all setup
mutations and permanent assistance semantics. Test prerequisites, success,
relevant refusal, resulting state, cold persistence and subsequent compatibility
for every required gate. Demonstrate actual ending behavior/presentation in CE.
Keep controlled checkpoint, linked gameplay, earned continuous progression,
full new-game-to-ending and untested/blocked coverage separate. A continuous
unassisted full run is desirable; its absence must be disclosed and must not
block other campaign validation. Continue unblocked M3 work; stop before M4.

## M4 authorization - October17,2026

The user's explicit "Proceed with M4 autonomously until completion" authorizes
M4 implementation and verification and supersedes earlier M4 stop statements.
Continue unblocked M4 tasks; preserve the accepted M3 executable/source/evidence.
Retain the architecture's native-only release criteria: no general opcode or
generated instruction executor, required CPU ABI escape, ROM/disk bootstrap or
CPU IRQ delivery. Do not rename compatibility code as a semantic replacement.
Controlled M3 checkpoints remain regression evidence, not an earned native-only
campaign. M4.3 earned campaign and supported branch/save/audio/session acceptance
remain required. Publication, push and deployment remain unauthorized.
