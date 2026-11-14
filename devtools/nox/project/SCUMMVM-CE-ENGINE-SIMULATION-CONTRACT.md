# World/simulation/NPC source closure477

Status: IN PROGRESS. Parent controls commit f4320130b25. No campaign traversal.

The existing map, travel, transport, ordered actor, hazards, clock, schedule,
route-search/follow/recovery and dialogue owners are retained. PARTIAL matrix
rows do not justify rewriting working code or rerunning old suites. Static
producer inventory covers immutable authored records separately from dynamic
script producers and imported state; those latter domains remain explicit.

## Demonstrated missing daylight producer

Original GAME71C9 calls resident F9AA before normal input. F9AA-FA06 starts
dawn/dusk transitions through light mode387D, level2A7D and tick2A7F. Native
VisibilityMask already implements these transitions, but no native ordinary
input caller starts them. FB0C instant light selection is a distinct map-entry/
rest operation and cannot supply gradual ordinary transitions.

1. Type2 and permanent-dark map list GAME70B1 suppress ordinary transitions.
2. At05:30 set mode0, level0, ticks0 (dawn).
3. At20:30 set mode2, level4, ticks0 (dusk).
4. At05:31/20:31, transport speed367E>=4 repeats the respective transition.
   This is WorldTransportState.speed, not illumination level or difficulty.
5. Other valid times leave the entire light state unchanged. No RNG/clock turn.
6. Connect at the normal world-command boundary before existing schedule service;
   modal continuations/free selectors remain exclusive. Subsequent existing
   viewport/actor owners consume the new LightState; no synthetic turns/delays.
7. Persist through existing LightState serialization, without adding CPU state.
8. Existing fixed day/night selection remains the map-entry/rest policy.

Bounded source proof executes F9AA to FA07 with declared pre-operation contexts;
source/map bytes and results remain local. One focused batch will combine changed
simulation owners, state codec checks and actual native input connections.
One integration build after the coherent source audit/implementation batch.

## Source audit findings not treated as missing code

WorldStory requests local-map exit at X8/40,Y7/39 before raw coordinate bounds;
the WorldMove raw outside-map guard therefore is not evidence of a missing normal
exit. Actor projectile phase requires parent255, excluded by ordinary native map
families; tactical rendering is separate. No speculative fix to those guards.
Authored map census finds1377 initial live actor records and zero tile225 actors;
tile225 dynamic producer/lifetime remains unclassified, not declared unreachable
merely from that census. World A Attack and exceptional combat aliases remain
combat-owned. Nameless captions remain B-LOCATION.
