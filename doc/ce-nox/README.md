# Nox Archaist in ScummVM Comfort Edition

The consolidated ScummVM CE repository owns Nox production source and maintenance.
Make future changes here, including updates to gameplay, detection and regression
tests. The linked M4.5 worktree is a historical release candidate. The
independent Nox Archaist directory holds original media, research and full
acceptance evidence.

## Build and launch

The standard Windows x64 Release build includes Nox, alongside the existing CE
engines. From the consolidated checkout run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run-build-release-cleanenv.ps1 -Clean
```

Omit `-Clean` for an incremental build. The executable and required DLLs are
in `build-scummvm/Releasex64`. The explicitly selected `-WithoutNox` package
configuration remains available for the existing v0.1.7 release recipe.

Supply an original **Nox Archaist base game v119 HDV** in its own directory.
The verified original image SHA-256 is
`c42a95c7a356548c78957f98693e92d33c9bae3ff5126d9887c2980e5a490c3c`.
The detector uses ProDOS resource signatures and requires exactly one matching
base image in the directory. Lord of Storms v137 is outside the supported scope.

In CE, choose **Add Game**, select the directory, confirm **Nox Archaist
(v119/Apple II)** and choose **Start**. Press a key on the crown-and-fire title,
then use Up/Down and Return in the main menu. Start a New Game opens character
creation. Continue Game and Import Saved Game open the CE chooser for
compatible CE saves. In the world, **?** opens command help, **T** talks,
**O** opens, **S** searches, and **Tab** opens the roster. A second Tab opens
Stats & Inventory. **V** cycles Nox sound modes.

## Engine and data ownership

- `engines/nox/detection.cpp` and `resources.h` identify and read the HDV.
  Startup art, glyphs, campaign resources, music and authored text are read
  from the original image at runtime. Keep the HDV separate from the build and
  package. The executable opens it as read-only game data.
- `module.mk` supplies the native engine source list to supported generators.
  `native_engine.cpp` owns CE lifecycle and audio integration.
  `native_startup_host.h` dispatches title, menu, creation, world and combat
  input and presentation. The `native_*.h` subsystem owners handle semantic
  operations and resource decoding. AY synthesis and the speaker resampler
  supply audio; historical CPU and VIA IRQ code is excluded from production.
- `native_player_session.h` and the subsystem codecs serialize typed gameplay
  state. `metaengine.cpp` and `saves.h` expose native save metadata and paired
  generations per CE Play Session. Modern mode uses F5/F7 for save/load and
  leaves Q free of a world action. Classic mode exposes slots 1 and 2 and
  opens its chooser with Q. A restored save keeps its recorded mode.
  Back up the whole portable CE directory while CE is closed to preserve
  configuration, session ownership and all save generations.

Arbitrary Apple II machine snapshots, debug/Test-map content and malformed
images are outside the native workflow. If a distinct reward cannot be placed
because all 256 physical slots are occupied, the operation fails explicitly
and an earlier save is the recovery path. Ordinary capacity refusal retains
its original behavior. Full physical occupancy through ordinary play remains
unproven.

## Acceptance and source reconciliation

The CE baseline was `3fb95c027e2b1482fb9883ada69339c20d5470dc` with Nox
tree `b93bd2781183c5f89eee4eff2a5b0bf14fa444e5`. It already contained
M4.3, the M4.4 correction and functional reconciliation, including M4.4
loot commit `404ce5a611101a7d238ab9261379810b4c406cfd`. CE-only changes
since the common baseline covered AGS detection, the 0.1.7 version and
the optional Windows build without Nox.

The independent research directory has no top-level Git repository. The latest
production source was in the linked CE candidate worktree on
`codex/nox-m45-rc1137`. Its last production revision was
`5d2c5568ae3c87f7c4c3b639f75f347e45a15389` and its receipt HEAD was
`3567ebcf959c7d90856d59f40ac81d7d05d4f0e7`. The engine tree
`23dd0266b0a4fb0624f575f84da3716bee1f6f35` was imported byte-for-byte,
then the standard build selected its existing native branch directly.
The official engine tree after the build change is
`af0d2324070f151b20c42727486ef30e01dfece3`.

RC1138–1149 restored title and music timing, Search and input hints,
save keys, the title-to-menu transition and seven resource-backed menu
entries. They also restored original Quick Combat cadence, panes, cries
and victory, the Stats & Inventory routes, correct NPC typing with mapped
keys, the original exploration and conversation layout, and asset-backed
game feedback. RC1149 starts the splash stream on the crown-and-fire screen
and continues it across the menu transition.

| Milestone | Retained evidence |
| --- | --- |
| M4.3 | [Completion1134](../../devtools/nox/project/SCUMMVM-CE-M43-COMPLETION1134.md) documents the earned New Game to victory, 60 supported campaign nodes and 10 branch obligations. |
| M4.4 | [Functional1136](../../devtools/nox/project/SCUMMVM-CE-M44-1136.md) closes story.08 within the supported domain and dispositions the 16 formerly unknown rows. The correction requires a successful physical-slot grant before clearing pending loot. |
| M4.5 | RC1149 was built and delivered locally with 201 files, 61 DLLs and 139 registered engines. Its receipt covers package contents and title audio sample generation and routing. RC1149 executable SHA-256 is `25c191b88c91e6127df3a378b5ad693216c1eacf52416d1ff3f5d28a96ae3a3e`. |

Canonical private evidence lives under the independent project's `conversion/`
directory. SHA-256 of the canonical JSON records at handoff:

| Record | SHA-256 |
| --- | --- |
| `SCUMMVM-CE-M43-COMPLETION1134.json` | `00895f7051e6fa243798a0971820bcea97f000b7ebd9dee524425cd6c8d8dafc` |
| `SCUMMVM-CE-M44-1136-RECONCILIATION.json` | `fb75ff66af185cc7c5dc2b2b56cf76468e73a957a1987deb3d70a90ddc582461` |
| `SCUMMVM-CE-M45-1149-VERIFICATION.json` | `dfe2fc4d0bed777b5dafbf2a8b22a27bb9706b48e8d272fa05092556a775014b` |

## Focused regression checks

Use a disposable portable copy of the executable and its supporting DLLs. From
the checkout, run the wording guard and the bounded integration smoke:

```text
python devtools/nox/check_original_message_wording.py
python devtools/nox/test_ce_integration.py --exe <portable-copy/scummvm.exe> --image <base-v119.hdv> --artifacts <new-run-directory>
```

The smoke uses the real detector, native startup, player input, rendering and
save implementation through the CE test CLI. It records generated title
music/effects and verifies a cold restore in a second process. The original
image remains unchanged. Personal campaign play, display preference and
listening are ongoing owner validation.
