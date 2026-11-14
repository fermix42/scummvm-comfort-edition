# World information interface contract

Status: IN PROGRESS. Defined before production edits, November14. Parent commit
1445407f0abe2a18e069f055bf5a239bdb6839cf; party ordering is implemented and its
two broader integration qualifications remain deferred. This owner comprises
G location, L Look, ? help, / quest log and their complete read-only modal input,
catalog lookup, presentation and ownership. Tab roster/torch/icon/W are separate
command-interface work after this owner, not silently credited here.

## Source inventory and original dependencies

GAME723F command table; G949F invokes original dynamic message41 from07DF,
L94A7 selects inventory8, ?9495 selects9, /16A9 loads C9C7 quest driver.
Current FixedMessageCatalog41 intentionally represents a different keyed-item
message: do not misuse it for dynamic location or disturb existing lock behavior.

Source analysis `inspect_native_information.py`, local
`ce-native-information-source.json` and proprietary local disassemblies:

* Seven DATA.LOOK resources (0,2,3,4,6,7,8),256 strings each. Loader skips EA
  marker, takes final big-endian decoded length, decodes the intervening backward
  LZ data. Selector8 maps type5 to family6; no DATA.LOOK5 exists.
* All51 quest descriptions and51 four-byte conditions. Each entry requires
  first field/bit set and second field/bit clear (98E4 helper, original39F8
  bit masks). Original descriptions at decoded996E, conditionsA48E, terminatorFF.
  Encoded sourceCA01 length0817; suffix gives0E5B decoded bytes at9700. Native
  code reads the data slices only; it must never execute the decoded code.
* Help text from inventory9+88,388 bytes, immutable original keyboard guide.
  Modern CE additions may be appended accurately; keep Classic source text.
* Look selector8 handles arrow/cancel, frigate two-cell offset, objects with
  control57 (readable documents), source surface-sector regional captions,
  authored map/tile exceptional labels, actor/object/terrain priority and all
 256 generic descriptions. All22 authored named-document IDs use plain text;
  source definition table is the already-owned D4B3 document family.
* Location names are original **retained state**, not simply current tile name.
  Original residentLocation DB60–DC16: ordinary metadata name at23; map0 uses
  original default pointer0218; UM97 searches coordinate/name records or GAME7508
  fallback and preserves its selected name while remaining in that map. This
  state must not change merely because a character walks away from the entrance.
  Three23-byte nameless directory records (TOWN map2,CSTL maps4/5) have no standalone name;
  their original reachability/retention behavior remains UNKNOWN. Padding after
  a valid terminated name must not be included in the caption.76 map records
  are inventoried; this does not make all76 location-caption cases verified.

Prior unqualified Look research read its file prefix as a decoded size; this is
incorrect for the actual EA/suffix packet layout. Do not reuse its output as
accepted evidence. The existing reverse-packet decoder itself is reusable.

## Required inventory (23 known behaviors and3 additional unknown caption cases)

1. G uses retained, semantically owned location name; no keyed-item message alias.
2. Ordinary map captions stop at their authored terminator and exclude padding.
3. Surface0 default location name follows original resident source.
4. UM97 entrance-coordinate caption selection, default and retained lifetime.
5. Preserve location state on normal/cold save and explicit migration of older
   native saves; do not invent unrecorded historical entrance context.
6. Nameless directory original behavior remains UNKNOWN until evidence resolves
   it; no fabricated completed location labels.
7. L starts exclusive direction input, ignores unrelated keys, Escape cancels.
8. Direction cells76/110/94/92 and frigate right/down cells95/127 match1559/961F.
9. Source control57 readable document lookup, full text/paging/acknowledgement.
10. Surface sector155/tile13 special label; regional terrain1/67..73 table scan.
11. Map11/71/58/24 tiles216..219 exceptional labels.
12. Map71 tile8/220, map73 tile144, map69 tiles142/66 exceptional labels.
13. Actor then object (except control55) then terrain generic description choice.
14. Large/compact/extended actors resolve the original base-record tile, not a
    guessed displayed quadrant tile.
15. All7 catalogs,256 descriptions each, including intentional empty strings.
16. ? displays the complete authored help and returns on acknowledgement.
17. / evaluates all51 original conditions against live native quest fields.
18. Only active/incomplete quest descriptions display in original order.
19. Multiple quest pages and end acknowledgement; empty active list is valid.
20. Source-derived wrapping/CR/paging and underlay restoration for every menu.
21. Exclusive modal state blocks movement, other commands, saving and world
    animation; read-only information consumes no turn, random sample or actor AI.
22. Acknowledgement/cancellation returns to the original world frame and imports
    text pages consistently; subsequent native commands remain compatible.
23. Existing Classic/Modern options and CE information enhancements remain;
    information display never grants discoveries, quest flags or assistance.
24. Immutable HDV reads only; no executor, generated opcode dispatcher, scratch
    ABI or placeholder/no-op is a native implementation.

## Native owners, state and invariants

Reuse ReversePacketReader/decodeReversePacket, World/MapResource metadata,
WorldScene typed actor/object lookups, TextWindow, HgrFrame/GlyphFont,
TextGlyphSurface and WorldInput modal exclusion. InformationCatalog owns immutable
read-only text and condition data; InformationInput owns direction/text-window
continuation and frame underlay. World owns retained location identity if needed;
do not store the entire original interpreter overlay or machine caption buffer.

Inputs: current world type/id/coordinate sector, party appearance, scene lookups
and records, live quest fields and actual key events. Outputs: native text/frame
and exclusive modal phase. Gameplay records/RNG/time/session/audio remain unchanged.
Pending information may follow existing no-save transient selector policy; it
must not falsely report saveable. Completed read-only menus require no new save
format; retained location state does. Backward migration must be explicit.

## Blockers and verification budget

B-LOCATION (evidence/integration, owner information/world): prove Surface default
and UM97 retained name lifetime/migration and nameless directory reachability.
Continue independent catalogs/Look/help/quests; do not substitute guessed labels.
All other source branches above are implementation-ready from selector8/quest
and existing scene owners. Original masks and source direction mode are known.

One focused deterministic batch at completion: all1792 generic descriptions,
51 quest conditions including prerequisite/completion changes, all special Look
branches,22 named documents, input cancellation/acknowledgement/paging, frame and
state/RNG invariants, and qualified location/save state. One cleanenv native-only
integration build after coherent owner integration; no exploratory game launch,
campaign traversal, original instruction execution in production or audio suite.
Any remaining evidence blocker is reported explicitly and excluded from completion
claims; two unsuccessful corrective reruns stop a failing test loop.

## In-flight implementation475 (unbuilt/unverified)

InformationCatalog now implements data extraction for all7/256 Look catalogs,
all51 quest conditions/descriptions, help, all exceptional Look labels and named
documents. It does not execute or retain decoded quest instructions. Original
location fallback GAME7508 is the authored string "Depths of Vacous: L1", not a
guessed generic Underworld label. Coordinate sector is the already-qualified
`(y & 240) | (x >> 4)` owner (viewport72 source fixture); special Look uses it.

No new integration build or validation batch has run. Next required code:
location anchor ownership/migration (preserve UM97 arrival name while moving),
G caption resolver, exclusive information direction/window/ack continuation,
WorldInput production dispatch and bounded contract verification. Nameless
directory records remain evidence gaps; do not fabricate names or count them as
completed original behavior. Preserve frozen474b and audio473.

## Integration475 (supersedes the in-flight state above)

Known implementation is integrated:23 known behaviors implemented in native C++,
including1 explicit CE migration.17 have fresh focused verification at this
milestone; six qualifications remain (ordinary/Surface caption breadth, all live
direction combinations, document input/window, live large-actor lookup producers,
legacy UM97/normal Engine cold restoration, active quest multi-page interaction).
Existing text/graphics primitives are reused; no fresh original-raster parity or
campaign claim. Three nameless directory cases remain UNKNOWN, so this owner is
still IN PROGRESS rather than declared complete.

One native-only build475 passed0warnings/0errors in2m46.72s. One focused batch
passed first run:1792 generic descriptions;169 actual original-condition cases;
96 source Look cases,22 named-document bodies; G/L/help/empty-quest modal state,
frame restoration, complete native core/RNG preservation; UM97 name retained
through actual WorldCommand movement; NWLD2 and core round trips; NWLD1 migration.
Receipt: SCUMMVM-CE-INFORMATION475-VERIFICATION.json. All extracted original
program/data fixtures and diagnostics remain local. No audio suite or campaign.

Recorded commands: `python conversion/prepare_information475.py` (offline source
fixtures), cleanenv `-NoxNativeOnly` (one build),
`python conversion/check_information475.py` (one focused batch). Do not rerun
completed evidence absent relevant source drift/defect. Check creates a unique
owned portable output root and intentionally refuses to overwrite it.
