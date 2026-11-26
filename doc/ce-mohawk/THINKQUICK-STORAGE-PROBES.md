# Original owned datafile/profile/history persistence

16 original-only cases qualify twice using generated files beneath a fresh owned
validation root. Each case uses a new original process, withheld application
entry, selected original CRT/filesystem/cache boot and the documented synchronous
I/O fallback. The original symbol table and VM constructors initialize the shared
VM at 0x49a6a8 before datafile construction; datafile.getVar evaluates serialized
values using that actual VM. Normal game startup is not run.

| Flow | Warm observation | Fresh-process / disk observation |
| --- | --- | --- |
| Default write-through | set answer=42, close/reopen/get ->42 | Exact `[Probe]\r\nanswer=42;\r\n`; cold get ->42 |
| Buffered then close | Close/reopen/get ->42 from shared profile cache | File remains empty; cold get ->0 with native missing-key 0x296c |
| Buffered then writeThruOn | Same-process get ->42 | Explicit mode switch flushes; exact saved bytes and cold get ->42 |
| Buffered then real component destructor | Last setVar result remains42 | Actual destructor 0x446491 runs; file remains empty |
| Buffered, close, profile cleanup | Actual cleanup 0x458570 runs | File remains empty; no implicit flush inferred |
| openReadOnly existing file, set answer=99 | Close returns short0 | Existing file is actually rewritten with answer=99; the API name does not enforce write protection |
| open missing parent | VM return short0 | Open flag remains0; filesystem status0x2847; no file or parent is created |
| History add7, add9, add7, save | Newest duplicate moves to front: [7,9] | Exact `[Level1]\r\nDeckA=[7,9]\r\n`; fresh user/profile load returns [7,9] |
| History add7, add9 without save | In-memory list [9,7] | Created file remains empty; fresh load returns empty list, native missing-key0x296c |
| String value a"b | getVar returns exact string | Exact `text="a""b";` serialized value; fresh getVar unescapes to a"b |

The first three rows each include independent writer/warm and cold cases;
history and string rows likewise include separate cold processes. Native value
tags, list length/elements, string text, constructor vtables, open flags, helper
errors, VM errors and exact file bytes are guarded. Every process is terminated
and drained before the next case. Cold reads therefore cannot inherit original
profile-cache allocations from the writer. A warm close/reopen alone demonstrably
does not prove persistence.

There is no replacement file/profile/value/method callback. Direct component
destruction does not free the fixture's caller-owned object shell; it executes
the real destructor. Profile cleanup is called explicitly, not through a claimed
normal application shutdown. Filesystem initialization resolves the copied
executable's directory as the default context; all datafile/profile names are
absolute owned paths. Original opens can change their own process cwd. Exactly
the copied executable and eight named generated datafiles exist before the
harness writes its receipt; unexpected files fail the run. Owner profiles/saves
and game assets are not opened as write targets.

Reproduce with probe_mohawk_v5_storage.py --original <pinned CarmenTQ.exe>
--root <new owned directory>. Runtime source/executable identities, actual and
normalized fixture hashes, full native observations and final owned-file hashes
are recorded in THINKQUICK-STORAGE-OBSERVATIONS.json. Both repeated batches
qualify. No CE persistence equivalence is claimed by these original-only cases;
the separate CE component-constructor suite already identifies unsupported
history construction. Applicable datafile CE observations remain required.

Step 3 remains active. Database/index/selector graph behavior, OS-error retry/UI,
remaining VM/packed/lifecycle cases, applicable CE outcomes and aggregate replay
are still open. No engine implementation, build or accepted-installation update.
Stop before Step 4; keep PR #295 a draft.
