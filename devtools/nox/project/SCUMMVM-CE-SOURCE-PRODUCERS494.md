# Authored producer closure494

Status: VERIFIED for this static census only. Parent108f1204597.
Command: python conversion/census_native_producers494.py (exit0).
Local source-derived output: ce-native-producers494-local.json; proprietary
records stay local. No build or executable run was needed for this unit.

Across76 map records, all49 commerce offer keys match their initial NPC roles:
22 merchants plus27 food/boat/armory/mount offers. The existing native catalogs
select the same resource slices and role/map/slot keys; no missing offer parser
or new service implementation is indicated. Forty-one additional initial role
records lack offers, largely inactive/non-service schedule identities; they
are not silently classified as missing merchants. Town1 is authored Test.
UM77's five unmatched role templates start at0,0 and its type2 caller skips
local scheduling. Their physical activation remains qualified. Native service
role5 has no initial producer; schedule row0 supplies route identity in actor4,
not service role in actor5. Story actor patches set role127, not a new shop.

All11 authored map-patch records use the four implemented effect kinds:
one terrain change, eleven actor-prefix changes, one spawn and one object
prefix change. Every actor-prefix index is aligned to an eight-byte actor.
StoryMapPatches handles these data effects; WorldStory/WorldViewport and
WorldInput retain application/projection ownership. This is complete grammar
coverage of that table, not fresh workflow or campaign validation.

Every named book definition has a native world/battle family except obsolete
duplicate Mage Armor book45, whose spell cost/power are zero. Book45 is absent
from initial inventory, all merchant stocks and syntactic chest candidates.
This does not prove its absence from every dialogue/scripted/imported context.
No additional magic effect is invented from its name. Random ordinary loot
generates weapons/armor, not books; explicit scripted rewards remain separate.
Reserved ## book definitions remain data placeholders, not missing spells.

Unresolved producer scope is retained separately from implementation. No row
is promoted to full gameplay/cold/session/earned acceptance by this census.
Accepted source/executables and all old/audio suites remain untouched.
