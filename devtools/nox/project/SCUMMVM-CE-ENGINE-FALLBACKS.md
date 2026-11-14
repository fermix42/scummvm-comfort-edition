# Original execution and unsupported-behavior inventory

Status: IN PROGRESS. Static source baseline audio473, November14.

| Dependency | Production reachability | Semantics / replacement owner | State |
|---|---|---|---|
| Runtime / original opcode executor | Excluded by module.mk and devtools/nox/native-only.targets native object lists | Native engine/player/session and domain owners; compatibility code retained for standalone/reference builds | Eliminated from selected native target; retain reference code |
| Generated instruction conversion / banked fallback | Canonical conversion/native and compatibility/reference runtime, not selected native startup | Semantic per-domain owners; no rename of generated execution as native completion | Intentionally retained offline/reference; no native release credit |
| CPU ABI, ROM bootstrap, IRQ delivery | native_engine.cpp run unconditionally enters runNativeStartup; selected5 objects exclude runtime/movement interpreter | Native startup, semantic simulation, MusicEngine/CE mixer | Selected target retired these dependencies; retain native link acceptance identity separately |
| Original HDV resources | Native archive/catalog/map/message/music readers | Immutable original data interpreted by typed native owners | Intentionally retained data dependency; not logic fallback |
| World known-command fallthrough | WorldInput handle known string then Ignored | Interface owners; N depends on party identity | Unresolved missing behavior B-COMMAND/B-ORDER; no interpreter fallback masks it |
| Explicit Unsupported/false transaction domains | Native owners listed with exact source lines in completion matrix source_index | Owning subsystem must distinguish valid refusal, malformed data and missing semantics | Unresolved audit classification; each marker is not automatically a defect |
| Quick retained effect/difficulty | Existing Quick source contract; exceptional original scratch producers not fully resolved | Combat/geometry lifetime owner | Unresolved B-QUICK; not completed by interpreter-free compilation |
| Historical machine snapshots | LegacySnapshot data importer, supported scoped historical saves | Native semantic migration / persistence | Intentional import, no runtime instruction execution; broader domain unresolved B-LEGACY |

The selected native production path has no observed interpreter fallback. That
does not establish behavior completeness: explicit refusals and silent dispatch
gaps remain. Native implementation and verification counts are separate. Future
link/build proof applies to its own executable, not automatically to all builds.
No compatibility executor is removed by this audit.
