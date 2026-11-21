# F12 slowdown investigation capture

During normal Snatcher play, press **F12 once** when the slowdown starts.
Wait for the persistent completion dialog:

> Capture complete - safe to restart.

That success message appears only after the diagnostic report, recovery save
and completion record have been finalized, and their stored bytes checked.
The dialog stays open until dismissed. If the recovery save fails, the dialog
explicitly says **SAVE FAILED**; a diagnostic report can still be available.
Do not interpret a progress message or an incomplete/error message as success.

Evidence goes into the installation's managed `saves` folder, with a shared
`<target>-snatcher-investigation-<date>-<time>-<counter>` prefix:

- `.json`: engine state, recent semantic trace, selected graphics/audio settings,
  event counts and recent performance history, written before attempting a save.
- `.save`: full native recovery snapshot, separate from numbered gameplay saves.
- `.result.json`: whether snapshot finalization and read-back validation succeeded.

Each press creates a new capture. Existing manual saves, autosaves, prior
captures and Play Session ownership are preserved. The snapshot can be copied
to an unused manual slot for investigation; it is not automatically inserted
into or loaded through the player's save list.

Normal play retains at most 1,800 performance samples in memory, generally one
per second plus scene-resource changes, and 256 semantic trace entries. Samples
include polling/update/draw/present timing maxima, input/mouse counts, narrative
location and pending events. Windows samples also include this process's working
set, private bytes, handle count, cumulative CPU time and available physical RAM.
There is no network upload, other-process enumeration or full configuration dump.

F12 runs on the game's main thread. It can capture a responsive slowdown, but
cannot execute while that thread is completely stuck inside a driver or other
blocking call. This is an investigation tool, not a fix for the reported slowdown.
An abrupt process-termination test checks that completed evidence survives loss
of the process; it does not simulate sudden power loss or failing storage.
