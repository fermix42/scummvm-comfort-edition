# Snatcher in ScummVM Comfort Edition

Snatcher is a normal experimental CE engine. The standard Windows x64 Release
build includes it through ScummVM's existing `--enable-engine=snatcher`
configuration, alongside Nox and the other CE engines. No separate launcher,
installation or distribution is required.

From the workspace root, use the existing build command:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\Codex Projects\scummvm-private\run-build-release-cleanenv.ps1"
```

The ordinary executable is
`scummvm-ce-2026.4.0-consolidated/build-scummvm/Releasex64/scummvm.exe`.
The existing `-UnitTests` workflow runs the CE CxxTests, CE native regressions,
and Snatcher component CTests. There are no Snatcher-specific build switches.

Add original Sega CD game resources using CE's normal Add Game workflow.
Detection retains engine/game ID `snatcher`, platform Sega CD, and preferred
target `snatcher-us-segacd`. Supported inputs include original CHD and supported
single/multi-BIN CUE resources. Optional uncensored data remains an external,
owner-provided resource; the option is off by default.

Detection reports `kUnstableGame`. Engine creation uses ScummVM's existing
unsupported-game warning and its normal preference. The experimental designation
does not establish ordinary-use safety: severe system-wide Windows slowdown,
including reported effects after process exit, remains unresolved. Prior
investigations and evidence are preserved. Automated regression work uses bounded
SDL dummy-device tests; do not run uncontrolled real-device Snatcher tests on
the owner's desktop or claim that this issue is fixed.

Manual saves retain slots 0–99 and existing save stems/schema. Snatcher disables
automatic saving in both engine and metaengine slots, rejects direct automatic
requests, and preserves manual slot 0. Original four-slot compatibility remains
separate and unchanged. Existing FM selection/restoration, input, graphics,
resource loading and strict portable-storage behavior remain intact.

The qualified source was merged from
`b906b0006d4c8ac005504acbdbe0d58d73db152f` without conflicts. The earlier
[integration report](INTEGRATION-20261121.md) is a historical record of the
isolated qualification; its temporary build-switch instructions apply only to
that preserved candidate. Normal CE development now uses this checkout and the
ordinary commands above. Current merge/build evidence is kept under
`validation/snatcher-merge-20261121/` and the ordinary build's owned regression
roots. Existing owner installations and profiles are not updated by a source
merge or a build.
