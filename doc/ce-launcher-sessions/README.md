# Launcher game sessions

The launcher session selector belongs to the single highlighted game. It lists
unarchived sessions, newest creation time first. With no saved preference it
selects the newest session. An explicit selection is stored as
`ce_launcher_session` in that game's INI domain and survives restarts. A deleted
or archived preference falls back to the newest visible session. Multiple game
selection and add-ons disable session controls.

The closed selector shows the session title only. Open dropdown entries have
a bold title on the first line and a creation date/time from registry metadata
on the second line. Existing names are preserved; new default names never
include timestamps. Creation timestamps are converted from UTC metadata to the host local timezone
and displayed as `Created Nov 19, 2026 - 10:30 AM`.

- **Start** activates the selected session and launches the game normally,
  without a startup save slot or session-name prompt. For a game with no visible
  sessions, it automatically creates a session with a numbered default title (`Session 1`,
  `Session 2`, etc.).
- **Load...** retains its existing label and save chooser. The chooser initially
  filters to the launcher's selected session; its existing session and archive
  controls remain available. Returning without loading refreshes the launcher
  selector from the registry, including deletions, archives and selection fallback.
- **New Session...** asks for a name, creates and selects a session, and remains
  in the launcher. Cancel leaves the selection and registry unchanged.

Selecting or normally launching an existing session does not add a registry
entry or change its lifecycle status. Completed sessions remain completed.
Quitting or returning to the launcher does not prompt to complete the session
or change its lifecycle status. The standard exit confirmation follows the
existing `confirm_exit` setting and engine override.
Archived sessions remain available through the Load dialog's existing controls.
The CLI's active-session-only selection contract remains unchanged.

Theme layouts cover the landscape sidebar, portrait and low-resolution list,
grid launcher and built-in classic theme. Rebuild external theme ZIPs with
`python scummtheme.py makeall` from `gui/themes` when changing those layouts.

Regression coverage: `testLauncherSessionPreferenceAndNormalActivation` in
`test/common/ce_acceptance.h` verifies creation-time defaulting, persisted older
selection, activation without registry growth, target isolation, terminal session
activation and deleted/archived fallback. Build with the workspace's approved
clean-environment script, using `-UnitTests` for regression tests.
