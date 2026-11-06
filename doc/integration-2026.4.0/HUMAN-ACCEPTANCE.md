# Human acceptance checklist

Use this exact local portable candidate:

`D:\Codex Projects\scummvm-private\acceptance-validation-local\package\scummvm-comfort-edition-windows-x64\scummvm.exe`

Keep the entire 182-file package together. Its executable SHA256 is
`7ed5bee7b1f415655930c9054a7974b74d7a2055311239d320be2181952e8d2f`.
The package manifest identifies source/test commit
`bb454edf5f7e6755244fb3fafa39b405cf35ce3e`; later acceptance documentation does
not change application code. This is a local review package, not an installation.

## Isolated setup

Run the following in PowerShell. These commands create only a separate test area.
Add games normally through the launcher so detection supplies their GUI options.
Use copies of saves and configuration; do not point this candidate at normal saves.

```powershell
$exe = 'D:\Codex Projects\scummvm-private\acceptance-validation-local\package\scummvm-comfort-edition-windows-x64\scummvm.exe'
$case = 'D:\Codex Projects\scummvm-private\acceptance-validation-local\human'
New-Item -ItemType Directory -Path "$case\saves" -Force | Out-Null
& $exe "--config=$case\manual.ini" "--savepath=$case\saves" "--extrapath=$(Split-Path $exe)"
```

Leave the CE Link token empty for offline checks. The extra path is explicit so
MI4 can find the redistributable `monkey4-patch.m4b`; moving only the executable
to another directory is insufficient. Keep test credentials out of screenshots,
logs, Git and copied evidence. Record PASS/FAIL plus the executable hash for each
row below. Stop a failing test and retain its isolated files for diagnosis.

## Required before approval

### Achievement hooks, restart and approved staging sync

Data: a legitimate Operation Stealth DOS VGA install (already available), its
CE achievement catalog, two separately named Play Sessions, and an approved
staging account/current CE Link credential for the online half. The historical
Hermesbox credential was revoked; do not reuse it or silently create another.

1. In Global Options > Achievements, enable Challenge Mode. Keep cheats disabled.
2. Create Session A, save before the customs-passport event, then actually get the
   correct passport accepted. Confirm `Passport to Trouble` appears once.
3. Save, quit the application completely, relaunch with the same isolated paths,
   and load. Revisit the event if possible; the same session must not duplicate
   its queued award or repeat the popup with repeat popups disabled.
4. Repeat the event in a distinct Session B. Its award must retain B's identity,
   independently of A. Check a multi-step Deja Vu achievement in a separate run
   if the optional catalog/pack is installed: partial progress must survive a
   save and process restart.
5. In another disposable session, enable a cheat and confirm the warning that
   Challenge Mode and challenge/l33t credit are disabled. Disable the cheat again;
   permanent assisted-session history must remain. Do not mix this with A/B.
6. For the online half, explicitly select the approved staging API in the private
   isolated configuration and configure its current test token through CE Link.
   Test the token, synchronize A/B, and inspect that staging account's records.
   Retry synchronization: no duplicate award/session/save rows may appear.
7. Disconnect the staging service, earn an event, save and restart. The pending
   event must survive; reconnect and retry, retaining the original ownership.

Expected: actual hooks, popups/progress, local persistence, ownership and staging
records agree. Failure: lost progress, cross-session assignment, duplicate credit,
unassisted credit after assistance, or any request sent to production.
Writes: isolated configuration, saves/sidecars/queue, and **approved staging account
records only**. Offline service tests have passed; earned hooks/popups and live
transport are still human acceptance items.

### MI4 Windows and PS2 (check independently)

Data: each legitimate installed edition and at least one compatible older save
from the accepted CE build. Required game directories are under
`D:\Games\ScummVM\Escape from Monkey Island (Multi-Platform)`. Keep separate
Windows and PS2 test-save directories. Use classic `OpenGL` for PS2 as documented
in `doc/ce-monkey4-ps2.md`; the older shader-driver crash workaround remains relevant.

1. Copy the older `.gsv` or `.ps2` into that edition's isolated save directory.
   Load it and verify the actual location, inventory and playable character.
2. Create Sessions A/B. In each, save different playable positions into native
   slots 1 and 2. Native menus must expose the active bank; the launcher must
   select the intended session. Windows has 160 slots per bank; PS2 has four.
3. In list and gallery views, inspect descriptions, native-slot ordering and
   available thumbnails. Missing thumbnails must not hide an otherwise valid save.
4. Quit completely and cold-load both sessions through native and launcher paths.
   Verify actual positions and ownership, then overwrite one disposable slot.
5. Delete only a disposable test save. The other session and original seed copy
   must remain intact. Compare the original input save's SHA256 before/after.
6. Run the existing edition-specific runner using a legitimate compatible seed:

```powershell
$python = 'C:\Users\aaron\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$repo = 'D:\Codex Projects\scummvm-private\scummvm-2026.4.0-integration'
& $python "$repo\devtools\ce\test_mi4_windows_save_banks.py" --exe $exe --game '<Windows game directory>' --save '<original Windows .gsv>'
& $python "$repo\devtools\ce\test_mi4_save_banks.py" --exe $exe --game '<PS2 game directory>' --save '<original PS2 .ps2>'
```

Expected: real restores and bank isolation, consistent metadata/gallery and no
original-save changes. Failure: wrong world/session, missing saves, wrong native
slot, crash or changed original seed. Writes: temporary runner fixtures and
disposable test saves only; no server traffic. Pure helper tests do **not** replace
these engine/seed-dependent checks.

### Essential CE launcher and display behavior

Data: the isolated launcher and at least one installed customized game. The
current desktop is 5120×1440; do not rearrange monitors or change Windows DPI.

1. Add a game through normal detection. Select it, open Game Options and confirm
   the Cheats tab and version-specific patches. Toggle master cheats off/on in a
   disposable session; individual options must follow the master gate.
2. Enable Challenge Mode globally, reopen Cheats, attempt to enable cheats and
   cancel the warning. Challenge Mode must remain on and cheats inactive.
3. Under Backend, select Custom, width 1200, height 900, x=200, y=100. Test titled
   and borderless windows with fullscreen off; quit/restart and inspect geometry.
4. Enter fullscreen, return to windowed, and confirm rendering, usable controls
   and restored window layout. Select another game and reopen its preferences.
5. In a real game, test Fit to window/aspect correction and click or select known
   controls near all four image corners. Confirm cursor actions hit the intended
   game coordinates, including Cine border cropping when enabled.

Expected: settings persist, controls remain reachable and clicks match the image.
Failure: offscreen controls, wrong cursor mapping, lost settings, unexpected
cheat enablement or a stuck fullscreen/window transition. Writes: isolated
configuration and optional test saves. Current single-display startup geometry,
borderless/fullscreen rendering, launcher selection and cheat/challenge dialogs
were observed; cross-monitor/DPI and exhaustive in-game mapping remain unverified.

### Representative customized-engine regression

Use the same candidate and isolated paths. Choose one short check per applicable
customized title rather than replaying whole games:

| Data | Steps | Expected / failure signal | Writes |
| --- | --- | --- | --- |
| Deja Vu Macintosh | Load a copied save; open About, move inventory/container windows, page text, save/reload | Correct title/About, z-order, paging and restored world; failure is a crash, lost text/state or unreachable dialog | Test saves/sidecars |
| Operation Stealth VGA (and Amiga/ST if those variants are used) | Check platform-specific patches, crop/cursor alignment, a copied save and sound; sample the jetski fix at an existing checkpoint | Correct option filtering, playable scene, aligned cursor and audio; failure is changed original data or broken encounter | Test saves/config |
| SQ5 Trash Talkin 0.2.12 | Load copied save; sample speech/lipsync, atlas and Battle Cruiser toggles; save/restart | Voice/art/slot/session behavior retained; failure is missing voice/art, wrong ownership or ineffective cheat gate | Test saves/config |

Indy3 boxing and SCI AVI presentation can be sampled when those features are part
of the intended approval scope. No original game files should change.

### One representative new upstream engine, when data is available

No suitable Phoenix VR, Dungeon Master, Chamber, Harvester, Avalanche, Fool, EEM
or newly supported Wintermute title was found in the known game-data root.
No commercial assets were downloaded. With one legitimately available title:
add its folder through detection, confirm the exact edition, start a new game,
perform one normal interaction, save using its supported save UI, quit/restart
and restore. Expected: correct detection, playable rendering/input and equivalent
restored state. Failure: missing engine/data, crash, incorrect detection or lost
state. Writes: isolated config/saves only. Registration alone is not a runtime pass.

## Recommended, not approval blockers

- Negative-coordinate secondary monitors and another DPI setting on hardware
  already configured that way; do not change the user's desktop to manufacture it.
- Additional supported games, longer sessions, alternate sound/rendering devices.
- More ending/optional-branch coverage and wider controller sampling.
- Further Nox M4 native migration work: separate project, outside this acceptance
  scope. The retained Nox receipts are preserved and do not establish M4 completion.

Approval does not require testing every ScummVM-supported game. It does require
recording the remaining required rows honestly, including any unavailable data.
