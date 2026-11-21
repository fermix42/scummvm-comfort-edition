# Private Nox v119 parity checks

These helpers preserve the reusable scripts previously kept only in
`acceptance-validation-local/nox-v119-parity`. They do not contain game media,
save fixtures, screenshots or historical executables.

Run from the consolidated checkout with Python 3. All six accept four positional
arguments: checkout root, disposable portable executable, private game directory,
and a new artifact directory. Keep artifacts beneath the disposable executable's
directory. The executable needs its runtime DLLs and theme resources.

```text
python devtools/nox/check_menu.py <checkout> <exe> <game> <artifacts>
python devtools/nox/check_ending.py <checkout> <exe> <game> <artifacts>
python devtools/nox/check_harbor.py <checkout> <exe> <game> <artifacts>
python devtools/nox/check_dialogue_story.py <checkout> <exe> <game> <artifacts>
python devtools/nox/check_story_branches.py <checkout> <exe> <game> <artifacts>
python devtools/nox/combat_oracles_direct.py <checkout> <exe> <game> <artifacts>
```

The private directory must retain the base v119 HDV and the `conversion` fixtures
named explicitly in each script. Story checks also require the extracted v119
`Nox Archaist Extracted/NOXARCHAIST/NA/GAME`. Menu checks require the original
seven menu PNGs under `conversion/startup1148`, including animation phases 1-3;
the PNG decoder is now included in the helper, with no external script execution.
Ending, harbor and story checks use historical or controlled checkpoints; their
success is scoped to those paths and is not a full campaign replay.

The scripts import the checked-in native CLI client and Nox test helpers. They
fail if required fixtures are unavailable. The historical audit at
`project/SCUMMVM-CE-V119-PARITY-AUDIT.md` records prior runs and their hashes;
those results are not fresh tests of a different executable.

`combat_oracles.py` in the local evidence directory is a retained superseded
attempt; use `combat_oracles_direct.py` for the production native oracle route.
