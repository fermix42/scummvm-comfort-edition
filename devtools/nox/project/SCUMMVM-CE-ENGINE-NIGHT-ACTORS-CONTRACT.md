# Map46 night actors504

Status: VERIFIED within focused contract;broader integration remains IN PROGRESS.
Contract defined before production changes. Parent4f36ba08553.

Original resident Faf0..Fb0b, called by map entry DE9C and daylight transition
FAD4, writes exactly24 compact actor bytes on map46. Light mode>=2 copies the
immutable BS.ROUTINES.BK2 F992 table (file2992); light mode<2 writes zeros.
Three eight-byte fixed records use sprite195/state6 and field39 masks1/2/4.
Actor admission already suppresses records whose earned predicate is set.
The initial map46 table has zero compact records, so omitting this producer
silently removes these normal night encounters. No native equivalent found.

Required complete native behavior:

1. A typed resource owner loads the24 authored bytes; no instruction execution.
2. Shared actual map entry applies them after choosing the destination light
   mode and before the first viewport. Other map records stay unchanged.
3. Ordinary input's dawn/dusk category change and explicit W-hour category
   change run the same producer and rebuild the affected scene/lookups/raster.
   No extra world turn, actor AI pass, clock/RNG draw or audio event is added.
4. Never reinstall actors on every ordinary input, refresh or load. An actor
   moved/killed during the same night retains its current record. Source quest
   predicates survive both transition and reentry and suppress earned deaths.
5. Existing World/core saves retain the exact installed/removed/current records.
   Loading does not rerun map entry; schema stays unchanged. Unrelated compact
   records after byte23, NPCs, objects and quest fields remain owned unchanged
   except existing viewport admission's documented predicate/facing behavior.
6. One focused batch covers helper byte policy, actual authored47->46 travel day/
   night, dawn/dusk input, W crossing, no repeated reinstallation, completed
   predicate suppression, core/window/input codecs and subsequent command.
   Declare all checkpoint positions/clock/book/orb/flags before tested input.
   One integration build after the coherent implementation. No campaign,
   earned encounter/victory, normal Engine cold/session or old/audio suites.

The adjacent map97 entry copy was examined separately: original DE8A projects
three retained wall bytes from fields77..79 after reloading the map. Native World
already retains the changed map record itself across travel/save. Do not add an
entry reset that overwrites a changed native wall with stale compatibility fields.
Broader historical snapshot reconstruction remains the separate legacy domain.
