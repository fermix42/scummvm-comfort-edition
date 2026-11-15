# Magic dispatch and NPC producers499

Status: VERIFIED for static dispatch/producer classification only. Parent
d3ceb3f0e65. No build, executable run, old suite or campaign traversal.

Read-only command: `python conversion/census_magic_dispatch499.py`.
Result: functional_named=39 named_NULL=45 native_family_coverage=39/39
NPC_descriptor_records=141 overrides=14 ally_nibbles=16 uncovered=0 runtime=0.
Local resource hashes and full per-book dispatch census are retained in
ce-native-magic-dispatch499-local.json; extracted resources remain local.

The embedded Combat program at file44B3 loads at9C01. Its A260 four-byte
descriptor table selects loader/mode/handler; source9DCC shifts the spell by
two and9DD0 reads A262/A263 before dispatch. All39 named books with nonzero
handlers belong to existing native battle/world spell families. Book45's
duplicate Mage Armor name has a zero handler, not an original missing effect.
Its zero cost/power and absent initial/merchant/chest producer were independently
recorded494. Reserved/noncombat test names are not ordinary supported books.
Do not invent Book45 functionality or restore a NULL jump.

The native battle family covers34 spell IDs and world family9, overlapping
restoration49..52. WorldCast owns restoration/light/coordinates/QuickExit/orbs;
BattleCast owns directed/area/summon/restoration/armor/engagement. This is family
coverage, not a claim that every UI/context/persistence criterion is verified.

EncounterAdmissionRules reads141 four-byte descriptors at Combat.E82 and14
five-byte overrides atE3C. Their spell fields masked63, plus every ally record
nibble0..15 from BattleRound.npcCast, fit existing directed/area/summon owners.
CastRules.prepareNpc's map-specific remaps (11/139 to3 on map32 and12 to10
on map8) remain within those families. This conservative domain includes
inactive descriptors; no new missing NPC restoration/world spell handler is
implied by the complete player catalog.

BattleSpellDamage already owns resistance255 immunity, percentage resistance,
instant-kill power comparison, protected targets, death removal and reward
credit. Directed/area callers pass protection and share that transaction.
This does not by itself enumerate every status producer or prove magic.06's
entire scope. Matrix implementation/verification classifications remain
unchanged; the finding closes the specific Book45/NPC-family question.

Continue remaining service/membership/temple/modal and lifecycle/script
connections from source. Preserve audio473 and498b. No campaign acceptance,
normal cold/session promotion, compatibility expansion or release action.
