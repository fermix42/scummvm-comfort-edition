# Space Quest V: Trash Talkin' Edition 0.2.12

Status (October 23, 2026): focused validation complete for the available
implementation. Intro speech, save/load, synthetic-fixture atlas, visible
portrait lip sync and atlas confirmation animation pass on human observation.
Ending artwork was recovered byte-for-byte from the developer executable, and
the user has now confirmed the ending mouth/blink test passes without smearing.
All focused implementation and human checkpoints are complete. The broader
validation boundaries listed below still apply. Changes are
in the existing SCI engine. No separate SCI implementation, game assets, voice
recordings, external binaries or SoundFonts are included in the source tree.
Unrelated working-tree CE/Nox changes are preserved.

## Provenance and comparison

Input: `D:\Codex Projects\SQ5-Trash-Talkin-Edition-PC-0.2.12`.
`payload/source/BUILD-INFO.json` identifies upstream commit
`96d0d298e4598fbd3472ee29594894de8de9230e`, ScummVM `2026.3.1git`,
w64devkit/GCC 16.2.0, SDL 2.32.10 and an SCI-only Windows x64 build.
The input is a complete base archive plus `sq5-voices.patch`, launcher source
and library notices, rather than a duplicate engine or selected replacement
SCI files. The base archive contains 26,039 entries.

Verified SHA-256:

| Input | SHA-256 |
| --- | --- |
| `payload/source/scummvm-base.tar.gz` | `a36643f40e8ff8b8606a14ed1ae1f20851b661de9241f13b54710caccbc7e60b` |
| `payload/runtime/scummvm.exe` | `69fd17cf3ea43ea0166826dc715487eff3043bef1ff462a6010160587766aab3` |

These agree with BUILD-INFO and BUILD-REPORT. All 32 pre-existing files touched
by the supplied patch are byte-identical between the archive baseline and CE
HEAD `5f84c5cd08c`; the patch adds five files. This makes the fork delta directly
comparable without importing an upstream snapshot. Local evidence is in
`build-sq5-analysis/source-comparison.json`.

**Corresponding-source limitation:** the patch includes two headers that are
absent from both the patch and archive: `engine/flo_ending_mouth.h` and
`engine/ending_blinks.h` under `engines/sci`. The user confirmed no additional
source is available. Consequently the supplied source cannot rebuild the
distributed executable as-is. Matching package hashes establish release
identity, not reproducible binary/source equivalence. No executable reverse
engineering was performed. The user later authorized binary recovery. All five arrays have now been
recovered exactly from named symbols and enabled as external CE data. See
[sq5-trash-talkin-ending-recovery.md](sq5-trash-talkin-ending-recovery.md). The
original archive still omits the headers; their compiled contents are no longer
unavailable.

## What needs integration

| Category | Source evidence | Treatment |
| --- | --- | --- |
| A: resources | BUILD-REPORT and manifest: 32 hash-bound MSG/SCR/HEP/V56 patch recipes; 89 required original files | Reconstruct only from validated owned originals in a new local directory. Modified text, game scripts and view resources need no new resource format. |
| A: audio/data | 2,851 WAV recordings, six SQ5WORD1 timing sidecars, spoken-order text and galaxy-map JSON in `payload/voices` | External local data; existing WAV decoder, mixer and filesystem APIs suffice. |
| B: recorded speech | `tts.cpp`, `message.cpp`, `controls16.cpp`, `paint16.cpp`, `picture.cpp` | Bind displayed SCI messages to tuple recordings, handle graphic-only speech and special log/choice dialogue; preserve TTS for other games. |
| B: audio-resource isolation | `resource/resource_audio.cpp::readWaveAudioPatches` | Exclude tuple-named voice recordings from native SCI numeric WAV patches for this edition. Recursive game-directory search otherwise turns `104-1-0-0-1.wav` into audio resource 104 via `atoi`, replacing the intro soundtrack and cues. Genuine numeric patches and other editions retain their behavior. |
| B: timing/lip sync | `tts.cpp`, `vm.cpp`, `kgraphics.cpp`, `animate.cpp`, mouth-profile table | Analyze recordings in 25 ms RMS bins; render only the active mouth; drive native Narrator deadlines and hash-bound log word timing. |
| B: atlas/gameplay | `dialogs.cpp`, `galaxy_data.h`, graphics/event/sound hooks | Native SCI chart, original story discovery gates and coordinate submission; cloak announcement, maze shortcut/fades and scoped cutscene fixes. Chart art is extracted locally into 18 checked-size byte files. |
| B: saves | `savegame.cpp`, `script.h`, `vm.cpp`, `state.h` | Preserve fork format 47–49 fields and exact script-119 legacy-heap guard; reset recorded-speech state on restore. Ordinary SCI saves still write format 46. |
| C: launcher/platform | external `player.py`, BUILD-INFO, unused `kmisc.cpp` include changes | Do not import launcher/install paths, SDL/toolchain configuration, unrelated source or external music-setting persistence. CE retains native music-device/SoundFont configuration and the existing window/scaling/save/session infrastructure. |

All newly imported SQ5 behavior is gated on the detected edition. The fork
originally gated several palette, message, VM and animation changes on SQ5
alone; CE narrows those to this edition. The otherwise-unused kmisc changes
are omitted. The recorded-speech destructor runs while VM selectors remain
valid. MSVC-specific header/shadowing errors were fixed without changing the
project C++ language standard.

## Identification and local preparation

The existing original SQ5 descriptors are unchanged. A new four-file descriptor
retains the English original resource map/volume/message signatures and adds
`sq5-tte-release.json`: an exact copy of the distribution's 189-byte
`release.json`, MD5 `857b15810cf695c8ad59c0715586836c`. This is a CE installation
marker derived from real supplied bytes, not a guessed original-game hash.
It distinguishes a prepared installation from the unchanged original. The
engine defaults the voice path to the prepared game's `voices` directory.

The preparer checks all payload hashes, all required originals and every
recipe input/output hash before creating a new output directory. It refuses
overlapping source/output paths and existing destinations. It copies only
owned game data, reconstructed patches and voice data; it does not install the
developer's executable, launcher, SoundFonts or source archives. It never
modifies the supplied release or original installation.

Example, from the repository root:

```powershell
python devtools/ce/prepare_sq5_trash_talkin.py --release 'D:\Codex Projects\SQ5-Trash-Talkin-Edition-PC-0.2.12' --original 'D:\Games\ScummVM\Space Quest 5 - The Next Mutation (Floppy DOS)\Space Quest 5 - The Next Mutation (Floppy DOS)' --output '<new separate game directory>'
```

This run's prepared copy is `build-sq5-analysis/game`. Proprietary resources
and generated artwork remain in ignored local build directories. The two
omitted ending headers were subsequently reconstructed from the hash-bound
developer executable. Five recovered arrays now supply the existing ending
renderer; future preparations recover them automatically. The extracted pixels
and reconstructed headers stay local and untracked.

## Validation and progress

- Payload audit: all 2,925 manifest entries, archive and runtime hashes pass.
- Preparation: all 89 original files and all 32 reconstructed patch hashes pass.
- Audio audit: all 2,851 WAV files are complete 16-bit PCM, mono or stereo;
  six timing files match WAV MD5, duration and monotonic word offsets/times.
- Importer/recovery tests: five tests pass, covering valid reconstruction, wrong
  hashes, invalid ranges/operations, path escape, preservation of existing
  sources and rejection of an unrecognized recovery executable.
- Windows Release x64: full CE solution compiles and links. Candidate is
  `build-scummvm/Releasex64/scummvm-sq5-tte.exe`. The generated local vcxproj's
  linker output was changed to this filename; the checked-in build script is
  unchanged. MSBuild's resulting TargetName mismatch warning is expected.
  The generated project was restored to its normal output after building.
  The user's existing `scummvm.exe` was not replaced. Candidate SHA-256:
  `1a0976b7363d2680fc77e8b6b6a76cf17a6cc024596a625b9e09f64836b5daef`.
- Actual candidate detection: prepared game is reported as Trash Talkin'
  Edition v0.2.12; original path remains standard English DOS SQ5.
- Both editions remain running through a 20-second headless startup check,
  with no logged fatal/VM error. The enhanced edition creates its recorded-voice
  capture; the original does not. No speech playback events occur during this
  initial title-screen smoke. Evidence: `build-sq5-analysis/validation/validation.json`
  and adjacent logs. A process remaining alive alone is not proof of speech,
  rendering, a successful save round trip or full CE session regression.
- All 89 original source-file hashes were checked again after preparation and
  runtime tests and remain intact. `git diff --check -- engines/sci` passes.
- Human checkpoint 1 failed: user reported wrong/repeated speech during the
  outer-space intro. The speech trace contained no `#play` events. A resource
  debug run then confirmed 2,850 tuple recordings were mistakenly registered
  as native SCI audio patches (`build-sq5-analysis/collision-before.log`).
  The edition-scoped scanner fix addresses this independently of dialogue
  selection/timing. Rebuilt candidate passes the scanner regression: zero
  tuple-WAV audio patches; genuine `65000.wav` fixture still loads. Enhanced
  and original SQ5 both pass detection and an eight-second startup smoke,
  and recorded-voice initialization remains edition-specific. Evidence:
  `build-sq5-analysis/validation-audio-fix/validation.json` and adjacent logs.
  User subsequently confirmed: intro plays correctly, speech matches, and
  lines no longer repeat. Startup/intro speech checkpoint passes on human
  observation. User subsequently
  confirmed checkpoint 2: saving, restoring the saved position and speech after
  loading all pass. Play Session isolation was not separately established.
  User subsequently confirmed checkpoint 3: atlas display, cancel, planet
  narration and course confirmation all pass. Gangularis is reached by first
  selecting its star system, Viridia. This validates the isolated synthetic
  bridge fixture, not natural story progression or all destination gates.
- Final visual checkpoint: user confirmed lip sync follows speech without
  facial glitches, and atlas confirmation animation looks correct. The female
  character's mouth showed good lip sync. The user noted Droole has no visible
  mouth in the observed appearance; no missing-mouth defect is reported on
  that basis. These are targeted human observations, not all-portrait coverage.

Reproduction:

```powershell
python devtools/ce/test_sq5_trash_talkin.py -v
python devtools/ce/prepare_sq5_trash_talkin.py --release 'D:\Codex Projects\SQ5-Trash-Talkin-Edition-PC-0.2.12' --audit-only
python devtools/ce/validate_sq5_trash_talkin.py --exe build-scummvm/Releasex64/scummvm-sq5-tte.exe --game build-sq5-analysis/game --original 'D:\Games\ScummVM\Space Quest 5 - The Next Mutation (Floppy DOS)\Space Quest 5 - The Next Mutation (Floppy DOS)' --evidence build-sq5-analysis/validation
```

## Human checkpoints

Final outcome: all five focused checkpoints passed (the fifth is recorded in
the ending-recovery report). The steps below are retained for reproduction.

Stop and collect the observed result after each checkpoint. Do not mark this
integration complete until the applicable observations pass.

1. Run `build-sq5-analysis/Launch SQ5 CE candidate.cmd`. It uses the isolated
   `candidate.ini`, `human-saves` folder and `human-startup.log`; no personal
   CE account configuration is imported. Start a fresh game, let the opening
   play and observe a spoken simulator/portrait shot. Expect audible dialogue,
   intact text, mouth movement during speech with neutral pauses, and normal
   progression without an error or stall. Report startup, speech, mouth and
   animation as separate pass/fail observations, with the line/scene of a fault.
2. After checkpoint 1 passes, perform one CE save/load round trip and verify
   the same scene/state returns and speech continues. Verify the save belongs
   to the intended local Play Session if supported by the installed CE catalog.
   This is required because fork fields
   and legacy save restoration changed; it has not been established by a build.
   Use a controllable scene, save as `TTE checkpoint`, note Roger's position,
   then move him before restoring. Quit the candidate normally and relaunch
   using the isolated launcher before loading that save. Expect the original
   scene/position, working controls and the next Look/Talk response to speak
   correctly once. Report save listed, state restored and subsequent speech
   separately. The isolated save directory was empty before this checkpoint;
   the config has `originalsaveload=false` and no account/session credentials.
3. After checkpoint 2 passes, check one bridge navigation interaction when
   accessible (an existing safe test save is preferable). Expect the chart,
   story-limited destinations, cancel and one valid course submission. Do not
   walk through the entire game to reach this check.

   The user's normal test save was still at Academy. A separate `atlas.ini` and `atlas-saves`
   directory contain a copy of the test save; the original Academy checkpoint
   remains untouched (SHA-256
   `577b66014fc3e3cf0966560c4994d2155401dc65a8ccbf3a64cde8ebd69ee433`).
   `build-sq5-analysis/Launch SQ5 atlas test.cmd` loads that isolated copy.
   After it loads, use Ctrl+Alt+D and enter each SCI debugger command:

   ```text
   gameflags_init 183
   sf 31
   room 201
   exit
   ```

   `sf 31` grants initial pickup orders without overwriting other flags;
   `room 201` sets the pending room-change global through EngineState's normal
   API. Computer Use could not submit debugger Enter, so the user performed
   the shortcut. The resulting atlas display, cancel, planet narration and
   course confirmation all passed. On the overview choose Viridia, then
   Gangularis inside the system. This remains a synthetic fixture, not a
   story-progression validation. No travel order was required.

4. Final visual checkpoint passed: the user confirmed speech-following lip
   sync with no facial glitches, good visible mouth animation on the female
   character, and correct atlas confirmation animation. Droole's observed
   appearance has no visible mouth; the user identifies this as his appearance,
   not an integration fault.

## Handoff

Final result: all focused checkpoints pass, including the recovered ending
mouth and blink overlays. No known missing ending artwork remains in the CE
installation. The final user observation was: Flo's mouth and both characters'
blinking look correct, with no smearing. No build or executable change was
needed after artwork recovery; the verified executable already contained the
source-backed renderer.

The focused test sequence is finished. Continue normal testing with
`build-sq5-analysis/Launch SQ5 CE candidate.cmd`; it uses `candidate.ini` and
the ordinary `human-saves` directory. The separate
`build-sq5-analysis/Launch SQ5 atlas test.cmd` uses `atlas.ini` and `atlas-saves`.
Its debugger shortcut is a synthetic test setup; keep those saves separate
from an ordinary playthrough. Both launch the verified candidate executable
whose SHA-256 is recorded above.

Repository delivery is scoped to the SQ5 files listed below; commit and push
results are reported in the final handoff. No publication, production
installation or replacement of the regular CE executable was performed.
Pre-existing CE/Nox changes remain outside this integration's scope. The focused source scope is `engines/sci`, the three
`devtools/ce/*sq5_trash_talkin.py` tools, `devtools/ce/sq5_tte_ending_art.py`,
the ending-recovery report and this report plus the adjacent
`sq5-trash-talkin-verification.json`. Original distribution/game data and local
test assets remain outside the tracked source deliverables.

Known limitations and acceptance boundaries:

- `flo_ending_mouth.h` and `ending_blinks.h` were not supplied, but their five
  compiled pixel arrays have now been recovered exactly. Reference and CE
  ending playback reached the closing bridge scene; final human verification
  of the restored ending mouth/blinks now passes: the user confirmed Flo's
  mouth and both characters' blinking look correct, with no smearing.
- Atlas behavior passed in a debugger-prepared bridge fixture. Natural story
  progression and every destination's unlock/refusal state were not validated.
- Native CE save/load passed. Account-linked Play Sessions, session-scoped
  achievement delivery and live achievement API behavior were not exercised;
  the relevant CE framework changes were not part of this port.
- Original SQ5 detection/startup and numeric WAV patch loading passed. This
  does not establish a full original-SQ5 or all-SCI-game regression pass.
- The fork's exact legacy-save compatibility path, full-game completion and
  cross-interpreter save portability were not separately exercised.

The additional ending checkpoint is complete on human observation. The
separate ending test launcher remains available for reproduction and reaches
the closing scene automatically using a disposable fixture. No exhaustive
gameplay or further human checkpoint is required for this focused handoff.

Full playthrough, live achievement API regression, all other SCI titles,
and cross-interpreter save portability are not claimed as validated.
