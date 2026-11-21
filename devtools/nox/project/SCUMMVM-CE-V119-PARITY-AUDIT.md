> Historical parity evidence: hashes, installed-build statements and results
> below describe the dated checks, not the current release. Commands now refer
> to preserved repository helpers; private media and fixtures remain external.
> Later advancement work is documented in `doc/ce-nox/README.md`.

# Apple II v119 parity audit

Reference media is the read-only base v119 HDV, SHA-256
`c42a95c7a356548c78957f98693e92d33c9bae3ff5126d9887c2980e5a490c3c`.
The consolidated production base is `792505122ee`. The starting pinned
Test executable is `24B4587A1044876D79268AB333A57F622AAC5FEE58F5F7685A20B55AFC5A65A6`.
The port changes backing that executable were reconciled from the linked
M4.5 candidate before testing new changes. The consolidated native audio
and build wiring remain in place.

The audit uses short fixtures and explicit input through the CE test CLI.
Historical campaign completion and original references are indexed in
the Nox maintenance guide and the independent project's
`conversion/SCUMMVM-CE-M43-COMPLETION1134.json`. Historical evidence
remains tied to its original executable. New artifacts go under the
workspace's `acceptance-validation-local/nox-v119-parity`.

| Area | Direct evidence on starting Test build | Remaining comparison |
| --- | --- | --- |
| Title and main menu | Fresh title music/effects, title key and New route. All seven menu selection frames and wrap matched retained original instruction-rendered frames at matching animation phase. Root Up/Down and Return/1/2 were traced to LOADER.P $A080–$A0EB. | Full title timing and listening, complete original help/about/acknowledgement page geometry and controls, all numeric-code paths. |
| Creation, prologue and saves | Test CLI New through creation and opening to initial world, then persistent save and cold restore. | Pixel, key and branch comparison for all creation choices and opening pages. CE save/restore and import use the portable CE chooser. The original uses Apple II disk operations. |
| World and exploration | Initial Open/cancel, four map types in reveal regression, and Ruin 31 authored closed door movement/refusal. Exploration assistance is a CE addition. | Original scene/control comparisons across lighting, stairs, transport, projectiles, weather, hidden objects, traps, travel transitions and location/Look/help/quest screens. |
| Party and inventory | Armor and spell info, cancel, successful Magic Missile slot assignment and low-INT refusal. | Original roster, reorder, all Stats and Inventory pages, document/drop flows and exact layout. The original Settings menu includes Quick Combat, Mature Dialog, Change Password and Toggle Font. CE currently routes settings through its host Options screen. |
| Tactical combat, Quick Combat and battle return | Dark Shadow scripted loss in both modes; same battle with assistance enabled; normal and assisted restart gates; Quick victory. Fresh original-vector reports for reticle, Attack, spell damage, area effects, restoration and Quick round behavior. | Tactical UI and all controls, Quick per-character text presentation, other battle modes and victory/flee/defeat branches. |
| Dialogue, commerce and services | Historical source and campaign evidence only at this stage. | Fresh player-path and screen comparisons for voice/input, recruitment, buy/sell, lodging, recovery, training and other services. |
| Story and ending | Historical completed campaign and original script references only. | Focused current-executable checkpoints for story gate branches, rewards, finale and post-ending behavior. |

## Corrections found in this run

- Quick Combat. Original `GAME` $7AEC–$7B12 detects 8 and calls the
  blocking keyboard read at $1324. The next key is consumed. The previous
  Test build stayed paused after Space and treated speed keys as changes
  during the pause. CE now resumes on the next key and consumes it.
  The new ordinary-play test `devtools/nox/test_quick_pause.py` reproduces
  the original behavior during an actual rat encounter.
- Battle-world-return validation. The retained reference stores a v1
  world packet. The expected persisted state now checks the exact original
  fields plus the default v2 entrance anchor and a byte-exact v2 cold round
  trip. The existing source-oracle comparisons of world flags and parent
  actor retirement remain in force.

## Candidate and Test installation

The root clean-environment script completed the consolidated build in
9:30.09 and the oracle follow-up build in 3:02.42, both with zero warnings
and errors. Final executable SHA-256
`96A7B257848FA6A1457AFE0EE69792886FF722BDC5540E0B76BA87788E3929E0`.
The pinned Test executable matched after atomic replacement. The Test
profile and both pin hashes were unchanged. The Release executable
matched its pre-install hash. The original v119 HDV still matched the
source checksum above.

Fresh candidate checks passed for the seven main-menu frames, startup
and cold save, armor and spell Info, the Quick pause fix, Shadow and
restart in both combat modes, four exploration map types and Ruin 31,
training, equipment and spell persistence, and cold arrival at the
earned terminal ending. The world input driver compared original notice
screen bytes and map/quest/clock/facing state, two Open command paths,
32 Wait turns and both cold pending-input continuations. The driver
needed an NWAT codec-aware facing read. The old hardcoded tail offset
read a different field and reported a false difference.

Eight retained combat datasets passed on the candidate, including 80
battle-world-return cases, 20 reticle frames, 12 complete Attack cases,
24,576 terrain cases, 672 spell impacts, 108 area spell cases, 288
restoration cases and three Quick battles with 55 actions. These
datasets compare rules and selected render output. The ordinary inputs
tested separately include Quick pause, live encounter admission,
restart, victory and scripted loss. Quick pause and the startup/cold
save smoke were rerun using the installed Test executable.

The original component corpora for services and transport have a separate
legacy resource runner. That runner's config key is implemented only in
the unlinked compatibility engine, so its invocation cannot validate the
production native binary. Live training did pass in the normal player
host. The other service and transport UI paths remain open in the audit.

Commands used from the consolidated checkout unless specified. Relative
fixture locations refer to the artifacts or the independent game's
`conversion` tree.

```powershell
# Workspace root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run-build-release-cleanenv.ps1
# Consolidated checkout
$g = 'D:/Codex Projects/Nox Archaist'
$c = "$g/conversion"
$p = '../acceptance-validation-local/nox-v119-parity'
$exe = 'build-scummvm/Releasex64/scummvm.exe'
$test = 'D:/Codex Projects/scummvm-private/ScummVM CE Test/ScummVM CE Test.exe'
$extra = Split-Path $test
python devtools/nox/test_quick_pause.py --exe $test --game $g --extra $extra --artifacts "$p/quick-installed"
python devtools/nox/test_ce_integration.py --exe $test --image "$g/Nox Archaist HDV (v119).HDV" --artifacts "$p/installed-startup"
python devtools/nox/check_menu.py . $exe $g "$p/candidate-menu"
python devtools/nox/combat_oracles_direct.py . $exe $g "$p/candidate-combat-oracles"
python devtools/nox/check_ending.py . $exe $g "$p/candidate-ending"
python devtools/nox/test_armor_spell_information.py --exe $exe --game $g --extra $extra --artifacts "$p/candidate-inventory"
python devtools/nox/test_encounter_restart.py --exe $exe --game $g --extra $extra --artifacts "$p/candidate-encounters"
python devtools/nox/test_native_world_input.py --exe $exe --game $g --artifacts build-scummvm/Releasex64/nox-v119-parity-world --reference "$c/ce-m4-battle231-controls-original/saves" --wait-reference "$c/ce-m4-open-wait194-reference"
python devtools/nox/test_party_invulnerability.py --exe $exe --assets $g --extra $extra --progression-only --artifacts build-scummvm/Releasex64/nox-v119-parity-progression
python devtools/nox/test_exploration_doors.py --exe $exe --game $g --extra $extra --artifacts "$p/candidate-doors"
$d = '../scummvm-ce-2026.4.0-nox-m45-rc1137/consolidation-validation-local/party-invulnerability/final/cannon-terrain/underworld-footer-first-True/saves/nox-native-controlled-world-before-input.bin'
python devtools/nox/test_exploration_map.py --exe $exe --game $g --extra $extra --dungeon $d --artifacts "$p/candidate-maps"
python devtools/nox/check_original_message_wording.py
git diff --check
```

## Settings, Quick text and targeted branch follow-up

The selected Settings row now loads the v119 Quick Combat label and its
OFF, ON, and ASK values directly from `NA/SRTN.INVENTORY` at file offsets
$643E and $64BB/$64BF/$64C3. Return cycles the three original states and
the next encounter reads the live preference. The other original Settings
rows, Mature Dialog, Change Password, and Toggle Font, remain open.
CE save and audio navigation instructions are host text.

Quick Combat renders the existing authored cries and victory stream from
`NA/BS.AUX.RTN.BK1` as the original per-character delay elapses, including
carriage returns. The test showed a blank pane at the start of the first cry,
then a visible partial word from the same cry, and confirmed that pause
freezes the text and the next key is consumed. Existing Quick victory,
defeat, restart, and eight combat reference datasets passed. This verifies
the paced presentation, controls and tested outcomes. It does not compare
every intermediate v119 frame or every combat text context.

The installed Test build rendered the authored harbor shop, sold a Skiff for
55 gold from the preserved purchase-menu checkpoint, and exercised actual
B boarding, sailing and X disembark in separate retained checkpoints.
The original armed/story test passed eight predicates and two production
admission hooks, with only the expected flag byte changed and RNG preserved.
The previously earned Cold Ending fixture passed on the installed binary.
Three native dialogue references failed identically on the previous and new
executables at Talk admission, scheduled modal field 174, and the book-ready
shortcut. These failures remain unclassified for gameplay parity. Shop,
transport, dialogue and story coverage is targeted and incomplete.

The clean build passed with zero errors or warnings in 2:53.19. Executable
SHA-256 `CF9ED72FA4D58367577F05891EE1984F033EF2848B6B2A3C3C7B271B073B9013`
matches the existing pinned Test installation. Release, the Test profile
and both pins retained their previous checksums. Original disks were read
only. The settings screenshots, harbor runs and promotion receipt are in
the parity validation directory.

```powershell
# Workspace root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run-build-release-cleanenv.ps1
# Consolidated checkout and variables above
python devtools/nox/test_quick_pause.py --exe $test --game $g --extra $extra --artifacts "$p/settings-quick-installed"
python devtools/nox/check_harbor.py . $test $g "$p/harbor-installed"
python devtools/nox/check_ending.py . $test $g "$p/settings-quick-ending-installed"
python devtools/nox/check_dialogue_story.py . $exe $g build-scummvm/Releasex64/nox-dialogue-story-parity-retry
python devtools/nox/combat_oracles_direct.py . $exe $g "$p/settings-quick-combat-oracles"
python devtools/nox/test_encounter_restart.py --exe $exe --game $g --extra $extra --artifacts "$p/settings-quick-encounters"
python devtools/nox/test_party_invulnerability.py --exe $exe --assets $g --extra $extra --progression-only --artifacts build-scummvm/Releasex64/settings-quick-progression
python devtools/nox/check_original_message_wording.py
git diff --check
```

## Original Settings and dialogue diagnostic follow-up

Mature Dialog, Change Password, and Toggle Font now appear with labels read
from the original inventory resource. Normal inventory Return and arrows
select the rows. The player sees the disk-backed password, confirmation and
reboot prompts. Wrong passwords and mismatched confirmation leave the
current settings intact. The password and mature option use the portable
target profile. The native conversation admission passes the mature setting
to the original authored eligibility rules. A focused check selects the
two different BYE bodies in L001 actor 8. Cold launch selects the same
disk-backed FONT1 or FONT2 as the approved preference. Captures of the same
Settings screen with the two fonts differ, and both were inspected.
The first build incorrectly initialized the font on session reset. The
startup initialization corrects that and the test now checks rendered output
in addition to persisted configuration.

The three previous dialogue failures traced to stale checks. The voice
fixture expected CE's superseded synthetic footer, so it now checks the
actual v119 prompt and voice labels. The spell shortcut fixture now opens
the Spells tab and gives its controlled character enough INT before making
the attempt. The scheduled-modal fixture compared the final field 174 to
a hardcoded pre-setup value. It now compares the value immediately before
interaction against the final one, while the original direct packet checks
still use and preserve the explicit 90 sentinel. All three pass on the
installed-binary copy. Coverage includes 768 original RNG cases, three
actual voice modes, paid ordinary NPC workflows, and ten spell shortcuts.

The portal comparison had expected a clock charge for a successful
movement portal. Original F6A8 bypasses F134 on that branch. The corrected
comparison requires unchanged clock phase on a successful portal and
advancement for the locked portal. The fixture passes all 236 policies,
three actual direction cases and the existing CE stationary return check.
The installed binary also passed ordinary movement and cold continuation
for portal success/refusal, damaging/protected barrier and the pressure
plate, plus the Dark Key gate from an earned approach. The refusal test
changes only the original required quest flag in a disposable copy.
Dig, authored callbacks, story predicates, harbor purchase/travel and
cold ending checks passed in this batch. These results supplement the
earlier branch inventory. They do not convert historical campaign evidence
into fresh runtime verification of every original screen or quest path.

Clean build passed in 2:51.72 with zero warnings and errors. The pinned
Test executable matches
`09F620D7E0EC1D4AB2DB711D523D2F9C8A6B261F904835F03153D694961DC205`.
Release, the Test profile, both pins, and the original v119 HDV retained
their previous hashes. Test runs used isolated profiles and checkpoint
copies. No campaign replay was performed.

```powershell
# Workspace root
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run-build-release-cleanenv.ps1
# Consolidated checkout and variables above
python devtools/nox/test_original_settings.py --exe $test --game $g --extra $extra --artifacts "$p/original-settings-installed"
python devtools/nox/check_story_branches.py . $test $g "$p/story-branches-installed"
python devtools/nox/check_dialogue_story.py . "$p/installed-dialogue-stage/scummvm.exe" $g "$p/installed-dialogue-stage/checks"
python devtools/nox/test_quick_pause.py --exe $exe --game $g --extra $extra --artifacts "$p/settings-quick-regression"
python devtools/nox/check_harbor.py . $exe $g "$p/settings-harbor-candidate"
python devtools/nox/test_ce_integration.py --exe $test --image "$g/Nox Archaist HDV (v119).HDV" --artifacts "$p/original-settings-startup-installed"
python devtools/nox/check_ending.py . $test $g "$p/original-settings-ending-installed"
python devtools/nox/check_original_message_wording.py
git diff --check
```
