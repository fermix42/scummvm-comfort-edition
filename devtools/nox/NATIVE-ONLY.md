# Native-only development build

Build from the consolidated checkout with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\run-build-release-cleanenv.ps1 -NoxNativeOnly
```

This selects `native_engine.cpp`, `metaengine.cpp`, `mockingboard.cpp`, `enhancements.cpp` and `speaker_audio.cpp` for
Nox through `native-only.targets`. `NOX_NATIVE_ONLY` removes the VIA timer/IRQ
bus functions; AY tone synthesis remains. The library excludes the compatibility
host, CPU interpreter, machine-state serializer, bootstrap, observers and
CPU-driven speaker scheduling. The speaker resampler renders semantic tone schedules. Native gameplay starts directly, without `nox_native_startup`.
Native player saves retain CE's normal generations/session ownership and NPLS
state. A machine-image compatibility save is not a native save.

The overlay works with the existing generated consolidated solution and is
reapplied by the build script. It does not execute a different generator or
change Smart App Control. Future project regeneration still requires a permitted
project generator. `module.mk` also records the conditional native source list.

The output remains `build-scummvm/Releasex64/scummvm.exe`; copy it with its DLLs
to a disposable portable installation before testing. Switching build mode can
replace that output. The normal script without `-NoxNativeOnly` retains the
compatibility/reference build. Do not mix receipts between modes.

The native link writes `scummvm-nox-native.map`. Inspect actual `nox.lib` archive
members and public symbols to confirm the dependency cut; merely setting the
macro is insufficient. Compile success and the bounded new-game/movement/normal
save/cold-load/next-input smoke establish this development target, not complete
campaign, all supported branches, legacy imports or release readiness.
