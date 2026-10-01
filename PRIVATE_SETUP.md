# Private ScummVM workspace

This is Aaron's private ScummVM workspace, imported from https://github.com/scummvm/scummvm.

## Source import

- Upstream branch: `master`.
- Upstream revision: `b72dad4a9c8e91139935cb83d234722199bc25c1` (2026-09-30, `I18N: Update translations templates`).
- All 179,211 commits reachable from that revision are retained with original authorship. Upstream files, including licenses, are unchanged.
- A merge joins upstream history to bootstrap commit `49a53743aacc0275cf12213631fbf123b17585bf`; no history rewriting or force push is required.
- The repository remains private. No upstream PR or release is part of this setup.

## Baseline verification (2026-10-01)

Environment: Linux x86_64, GCC 14.2.0. Builds were performed outside the source tree.

- Default `configure`: blocked because SDL development packages are absent (neither SDL1/SDL2 configuration tools nor SDL3 pkg-config metadata were found).
- `configure --backend=null --disable-all-engines`: passed.
- `make -j5`: passed for that null-backend configuration.
- `make -j5 test`: passed, 406 tests, following `test/README`.
- `./scummvm --version`: passed; reported `2026.3.1gitdirty` because the import merge was still staged during the build.
- Full desktop/all-engine builds, engine-specific tests, graphical/audio runtime, gameplay, and other platform builds were not run.

The null configuration also reported unavailable optional libraries: SDL_Net, Ogg, Vorbis, Tremor, FLAC, MAD, ALSA, JPEG, PNG, GIF, FAAD, sndio, TiMidity, MPEG2, A52, OpenMPT, MPC, FluidSynth, FluidLite, Sonivox, readline, libunity, GTK, FreeType2, OpenGL, FriBidi, Discord RPC, TTS, and the OPL hardware libraries. This reduced build does not establish a playable desktop baseline.

Local logs for this setup session are in `/workspace/scummvm-baseline/default/configure.log` and `/workspace/scummvm-baseline/null/{configure,build,test}.log`; they are not repository files and will not persist into a fresh environment.

## Future environments

Check out `main` from `fermix42/scummvm-private`. Git remotes are local configuration; when needed in a fresh clone:

```sh
git remote add upstream https://github.com/scummvm/scummvm.git
git config remote.upstream.pushurl DISABLED
git config remote.pushDefault origin
git fetch --no-tags upstream master
```

Read `AI-GUIDELINES.md`, `CONTRIBUTING.md`, and relevant platform instructions before development. No `AGENTS.md` or `.agents/skills` was present in the imported revision. The linked wiki build and commit-guideline pages denied automated access during setup; local configure help and `test/README` supplied the baseline commands.

Choose the intended target platform and engine/game before adding platform dependencies or making feature changes. This import does not configure account-level Codex Cloud settings or install a persistent dependency setup script.
