# Orb navigation492

Status: IN PROGRESS. Contract precedes production changes.

WorldNavigation.bind accepts Surface/underworld coordinates including X=0.
Authored usable miscellaneous items135..143 select the nine four-byte records;
InventoryWindow Use binds them, WorldCast63 chooses them, and WorldInput adopts
the resulting travel through the shared WorldTravel owner. Spell63 costs zero.
Current choose scans forward over zero bytes and reads four bytes starting at
that advanced index: a valid bound X=0 corrupts the destination; the last orb
can return Unsupported at the end of the array. This source indexing behavior
is an explicit CE_REPLACEMENT, not useful gameplay semantics to preserve.

Select exactly the chosen aligned four-byte record (X,Y,map,type). Four zero
bytes retain the existing unbound sentinel. Do not change binding admission,
the nine shortcut numbers, existing NNAV1 format, travel/cargo owners or mana.
The all-zero Surface origin remains indistinguishable from an unbound record;
adding an occupancy bitmap/save migration is outside this correction.

One focused batch covers all nine aligned records with X=0 and ordinary X,
all-zero unbound, rebinding, record isolation, binding refusal, NNAV1 codecs,
and actual InventoryWindow Use -> WorldCast digit/caster/orb selection with
modal/core roundtrips and unchanged inventory/party/clock. Controlled setup
declares one authored orb and spell63 before input; no outcomes injected after
an operation begins. Destination selection is distinct from a full map-travel,
normal Engine cold save/session or campaign claim. No old navigation/audio
suite rerun: old shifted-zero expectations remain historical source parity.

The previous proposed local exit extension492 is withdrawn: static geometry
inspection did not establish an ordinary outward route to the physical edge.
NpcScheduler writes actor[4] route identity, actor[6] behavior and actor[7]
schedule data; it does not write actor[5] service role. Schedule row[0] counts
are not service-role counts. StoryMapPatches' actor writer sets role127.
