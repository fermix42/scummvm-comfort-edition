"""Publish a scoped source reconciliation, retaining UNKNOWN global closure.

Private per-site originals and payloads stay under story08-1135. The public
report contains references, dispositions and limits, not extracted resources.
"""
import collections
import json
import re
import zipfile
from inventory_story08_1135 import ROOT, CE, OUT, sha, write

E='VERIFIED equivalent'
D='VERIFIED intentionally different'
U='UNKNOWN'
ROWS=[]

def row(ident,mechanism,status,original,native,chain,conditions,ordering,persistence,failure,remaining,evidence):
    ROWS.append(dict(id=ident,mechanism=mechanism,disposition=status,original_references=original.split(),
                     native_owners=native.split(),known_call_chain=chain,conditions=conditions,ordering_and_side_effects=ordering,
                     persistence=persistence,failure_behavior=failure,unresolved_evidence_requirement=remaining,
                     retained_evidence=evidence.split()))

row('S01','Decoded dialogue assignment and bit operators',E,
    'native/dialogue.cpp native/dialogue_controller.inc ce-native-dialogue-effects500-local.json',
    'native_dialogue_rules.h native_conversation.h native_world_conversation.h native_world.h',
    'WorldInput Talk -> WorldConversation -> Conversation -> selectDialogueResponse -> commitDialogueFields -> acknowledged handoff -> World.acceptConversation.',
    '439 decoded packets/3227 headers/552 complete effects;551 target the ordinary0..79 field region. Conditions include ordered keyword/opening matching, exact/bit fields, voice, adult and time.',
    'Read conditions before effects; up to2 assignments precede up to3 ordered set/clear operations; response commit is separate from later item/payment/battle callbacks.',
    'Conversation owns pending fields and codecs; World.acceptConversation copies256 fields; World/NewGame/core codecs retain committed state.',
    'Ineligible response selects the next response; invalid native effect shape returns refusal/invalid before field writes.',
    'Bounded operator equivalence only. All original incoming contexts and the one derived-lookup effect are not included in this row.',
    'SCUMMVM-CE-SCRIPT-SERVICE-OWNERS500.md SCUMMVM-CE-M4-CONVERSATION10-UNIT.json SCUMMVM-CE-INTEGRATION527-I2-VERIFICATION.json')
row('S02','Malformed parameter tails',D,
    'ce-native-dialogue-effects500-local.json native/dialogue.cpp',
    'native_dialogue_rules.h',
    'Packet header scan -> parameter boundary validation before DialogueFieldPair creation.',
    'Two shipped JOB headers end in a bare parameter tag in prototype packets.',
    'Native stops at header/body separator instead of interpreting separator/body bytes as a field/value pair.',
    'No invented mutation is committed for the missing pair; normal earlier effects retain their order.',
    'Bounded malformed-data handling is explicit; not original out-of-header memory alias parity.',
    'No claim of whole prototype activation equivalence; that remains S03/U03.',
    'SCUMMVM-CE-DIALOGUE-BOUNDARY486-VERIFICATION.json SCUMMVM-CE-SCRIPT-SERVICE-OWNERS500.md')
row('S03','Derived lookup alias and prototype response context',U,
    'ce-native-dialogue-effects500-local.json native/actor_world.inc native/scene_actor_frame.inc native/scene_vertical.inc',
    'native_world.h native_world_actor_turn.h native_world_presentation.h native_dialogue_rules.h',
    'Original indexed0400+field aliases lookup450+cell; native scene lookup -> syncObjectFields -> Conversation field view -> later scene refresh.',
    'One authored complete mutation targets derived field133; byte-indexed field space aliases only the first176 lookup bytes, not every scene cell.',
    'Copying a lookup byte into canonical fields is not proof that all later projection/refresh and prototype re-entry orders agree.',
    'Native saves owned fields/scene state; original transient lookup lifetime differs from ordinary persistent flags.',
    'No successful normal-world activation or out-of-header alias parity is inferred from the packet.',
    'Establish exact original/native pre-response lookup, mutation, next redraw/consumer, save and imported/debug activation. Preserve the prototype incoming-edge qualification.',
    'SCUMMVM-CE-SERVICE-ADMISSION497.md SCUMMVM-CE-SCRIPT-SERVICE-OWNERS500.md')
row('S04','Known dialogue world callbacks',E,
    'native/dialogue_effects.inc',
    'native_conversation_callbacks.h native_world.h native_world_conversation.h native_world_input.h',
    'Conversation marks worldPending -> World.applyConversationCallbacks -> ConversationCallbacks.apply -> retire request -> input refresh.',
    'Callback request bits and map68 portal creation use the canonical field state and authored12-byte template.',
    'Consume field12 request; portal template then ordered42/46 object operations and bit retirement; callback completion precedes response display/exit.',
    'Canonical changed object records and completed fields persist; pending callback state has a separate modal owner.',
    'Invalid/missing template rejects the staged transaction; no repeated completed callback on ordinary refresh.',
    'Known callback families only; complete arbitrary original callers, main/aux disagreement and imported states remain U03/U04.',
    'SCUMMVM-CE-CALLBACKS479-VERIFICATION.json SCUMMVM-CE-INTEGRATION527-I2-VERIFICATION.json')
row('S05','Canonical map ownership instead of main/aux copies',D,
    'native/dialogue_effects.inc native/resident_location.inc',
    'native_world.h native_conversation_callbacks.h native_world_story.h',
    'Original mainBE/aux57 record copies -> one canonical World map plus explicit scene projection.',
    'Original callbacks contain distinct main/aux writes and loader scratch; CE owns one logical changed-map record set.',
    'Native refreshes projections from canonical committed records; it does not preserve arbitrary divergent machine caches.',
    'Changed maps persist directly in World/core, without modifying the HDV.',
    'Original stale-cache or arbitrary import states are not silently declared equivalent.',
    'Replacement itself is established; every special original cache/context still needs its own effect boundary.',
    'SCUMMVM-CE-ARCHITECTURE-DECISION.md SCUMMVM-CE-CALLBACKS479-VERIFICATION.json SCUMMVM-CE-DATA-OWNERSHIP509.md')
row('S06','Dialogue item/gold/recruit/payment handoffs',U,
    'native/dialogue_effects.inc native/progression_handoff.inc native/quest_tavern.inc native/quest_training.inc',
    'native_inventory.h native_recruitment.h native_world_conversation.h native_conversation.h',
    'Completed Conversation -> describeDialogueHandoff -> inventory/gold/recruit/payment owner -> request retirement -> world commit.',
    'Field16 request state, encoded item kind/id, gold and request17 price/skill/battle/service markers.',
    'Known staged once-only handoffs exist; cross-caller capacity, payment-cancel and service-lifetime equivalence is not proved by the field header census.',
    'Party/inventory/gold and pending modal state have native core codecs.',
    'Normal poor/cancel/full/repeated cases have retained coverage; every original shared-helper call and failure side effect is not closed.',
    'Map every original request producer and exit caller, including service re-entry and full physical-slot failure, to exact native order/state/result.',
    'SCUMMVM-CE-SCRIPT-SERVICE-OWNERS500.md SCUMMVM-CE-INTEGRATION527-I2-VERIFICATION.json')
row('S07','Authored map-patch effect grammar',E,
    'NA/BS.ROUTINES.BK2@0x28f2 native/resident_world.inc@FD00 ce-native-producers494-local.json',
    'native_story_patches.h native_world_story.h',
    'World input/viewport/recenter -> WorldStory.apply -> StoryMapPatches.apply -> canonical objects/actors and owned terrain/spawns.',
    'All11 original variable-length rules; map/neighbor sector plus polarity/mask; kinds0/1/3/4 only.',
    'Original rule order; one terrain byte, object/actor/spawn four-byte prefixes; actor effect additionally writes role127. No quest field is written by this grammar.',
    'Window state serializes terrain/objects/actors/spawns; canonical changed map records retained separately.',
    'Out-of-range native offsets and unknown kind reject before state assignment; all authored rows satisfy the admitted shapes.',
    'Bounded complete grammar/operation, not all loader/recenter/interleaving contexts; those remain U03/U04.',
    'SCUMMVM-CE-M4-STORY-PATCHES64-UNIT.json SCUMMVM-CE-M4-WORLD-STORY65-UNIT.json SCUMMVM-CE-SOURCE-PRODUCERS494.md')
row('S08','Key/compound door and reversible control operators',E,
    'native/progression_open.inc ce-m3-keydoor-census31-local.json',
    'native_gates.h native_world_open.h native_world.h',
    'WorldInput Open -> WorldOpen.run -> keyDoor/compoundDoor or paired control -> record commit -> object settling.',
    '38 keyed and5 compound authored candidates; any-mask key predicate versus full contiguous compound bits; control requires its paired record.',
    'Refusal leaves door unchanged; allowed key opening changes marker/picture; control changes matching wall/control states and is reversible.',
    'Owned World map tables retain changes; object countdown/settle is a separate phase.',
    'Malformed pair/extent and unsupported state remain explicit; no bit predicate is silently treated as a successful unlock.',
    'Only the operator and qualified record shapes are closed; arbitrary dynamically written records and all incoming callers remain UNKNOWN.',
    'SCUMMVM-CE-M4-GATES1-UNIT.json SCUMMVM-CE-M43-LATE-GATES956-VERIFICATION.json')
row('S09','Pressure plate/current-cell story flag operator',E,
    'native/resident_world.inc@EA35 ce-native-scripted-contacts502-local.json',
    'native_gates.h native_world.h native_world_input.h',
    'Completed move/stationary world tail -> current-cell operation54 -> World.activatePlate -> pressurePlate.',
    '14 initial plate candidates; source field/mask from the extended object, including the phylactery-style flag operation.',
    'OR the authored mask, preserve unrelated bits, then refresh/continue; repeat is idempotent.',
    'World fields/core plus object/scene state are owned; normal earned phylactery and four-plate persistence retained.',
    'Invalid record/slot returns typed failure; no flag write on rejected shape.',
    'Complete code entry/continuation contexts and original slot-overlap producer provenance remain U03/U04.',
    'SCUMMVM-CE-M4-GATES1-UNIT.json SCUMMVM-CE-M43-PHYL715-VERIFICATION.json SCUMMVM-CE-M43-CULT1118-VERIFICATION.json')
row('S10','Search quest flag and reward transaction',U,
    'native/progression_search.inc@96EC native/progression_handoff.inc@9C9B',
    'native_search.h native_search_input.h native_world.h native_inventory.h',
    'Search target/lock/trap -> World.search -> searchQuestReward -> inventory handoff -> status/ack/tail.',
    'Source extended object state53, quest field/mask, kind/id, prior-claim state and capacity.',
    'Known source ordering consumes searchable marker on repeat/capacity; normal award sets field and relevant counter, then returns to input.',
    'Committed object/field/party/inventory/RNG values and pending search state have codecs.',
    'Native search explicitly leaves original full physical-slot fatal path unqualified; original flag writes can precede that failure while native staging may not commit.',
    'Establish exact original full physical inventory/fatal side effects, defined caller domain and justified native failure policy. Audit all dynamic object/item producer bounds, not just successful pages.',
    'SCUMMVM-CE-M4-SEARCH-REWARD36-UNIT.json SCUMMVM-CE-M43-PAGES501-VERIFICATION.json')
row('S11','Counted item increment/decrement',U,
    'native/inventory_lists.inc@A267 native/progression_loot.inc@9DF9 native/progression_search.inc@982D',
    'native_inventory_actions.h native_search.h native_battle_loot.h native_new_game.h',
    'Source misc definition -> acquisition/commerce/Drop context -> quantity/counter operation -> native retained commerceCounter and core.',
    'Only two of168 authored misc definitions have the tracked-counter bit; field index comes from definition, not a fixed quest list.',
    'Acquisition increments; sale/Drop path uses caller-retained quantity and arithmetic mode, which need not equal the removed stack count.',
    'Native commerce counter context and ordinary fields persist with current core.',
    'Protected/readied/cancel and tracked Drop contexts have earned proof; arbitrary inherited quantity/BCD/helper-entry contexts are not closed.',
    'Enumerate all original helper callers and admissible arithmetic/quantity states; distinguish ordinary definitions from dynamically generated/imported IDs.',
    'SCUMMVM-CE-M43-DROP1091-VERIFICATION.json SCUMMVM-CE-DATA-OWNERSHIP509.md')
row('S12','Item Use, spells and navigation producers',U,
    'native/inventory_lists.inc native/battle_services.inc native/battle_spells.inc',
    'native_inventory_actions.h native_world_cast.h native_world_input.h native_new_game.h',
    'Inventory Use/Readied book -> world casting or battle cast -> typed effect/navigation -> next world continuation.',
    'Known spell/item domains and initial/shop/chest producers are enumerated in494/519; this does not bound every dynamic original producer.',
    'Mana, inventory consumption, origin capture, map transition and deferred completion are caller-specific.',
    'Book, inventory, navigation and modal/core owners preserve typed state.',
    'Normal outside-battle refusal/cancel and selected Orb/QuickExit scenarios are retained; not all generated or imported definitions.',
    'Close dialogue/scripted/dynamic item availability and all indirect Use/cast selectors, including obsolete book45, without assuming table presence proves reachability.',
    'SCUMMVM-CE-SOURCE-PRODUCERS494.md SCUMMVM-CE-INTEGRATION527-I3-VERIFICATION.json')
row('S13','Recruitment and party/inn identity mutation',U,
    'native/party_recruitment.inc@9B27 native/party_membership.inc native/party_reordering.inc',
    'native_recruitment.h native_party.h native_inventory.h native_new_game.h native_world_conversation.h',
    'Dialogue/NPC identity -> Recruitment or inn membership/reorder -> party records/owner masks -> actor retirement and core.',
    'Source identities34..65 map into recruitment fields; party capacity/last-member/equipment ownership gates apply.',
    'Known record exchange/compaction, equipment removal/recalculation and flags are typed transactions; caller copy order still requires a complete provenance proof.',
    'Party, inventory and inn storage are independently serialized; ordinary cold behavior retained.',
    'Full/poor/no-storage/refusal and dead-status policies are explicit; do not equate inn pickup with resurrection.',
    'Trace all original copy-helper callers and dynamic recruitment/retirement contexts, including overlapping buffers and imported identity values.',
    'SCUMMVM-CE-SCRIPT-SERVICE-OWNERS500.md SCUMMVM-CE-INTEGRATION527-I2-VERIFICATION.json')
row('S14','Initial compact and role126 contact encodings',E,
    'native/combat_results.inc@987D native/combat_results.inc@98DE ce-native-scripted-contacts502-local.json',
    'native_actors.h native_battle_world_return.h native_battle_aftermath.h native_battle_loot.h',
    'Actor admission/contact -> battle -> victory effects -> advancement/loot -> delayed parent retirement -> world.',
    '17 compact predicates and15 active initial role126 records across76 map records;9 authored rewards.',
    'Set authored flag before growth/loot; preserve marker2 when bit128 is absent; retire parent only after results; flee skips completed-contact effects.',
    'Canonical actor/field and pending battle/result/loot state use their native owners/codecs.',
    'Invalid reward shape rejects; no expected victory/award is initialized. Original wrapped slot and marker contexts are not generally included.',
    'Verified initial encoding and typed operator only. All dynamically created definitions, arbitrary slot contexts and full-failure effects remain UNKNOWN separately.',
    'SCUMMVM-CE-SCRIPT-SERVICE-OWNERS500.md SCUMMVM-CE-M43-FIRE886-VERIFICATION.json SCUMMVM-CE-M43-AIR892-VERIFICATION.json')
row('S15','Combat protection, defeat/flee and consequences',U,
    'native/actor_suppression.inc native/actor_removal.inc native/combat_results.inc',
    'native_encounter_admission.h native_battle_round.h native_battle_aftermath.h native_battle_world_return.h',
    'Original actor/template context -> native encounter protection/no-flee -> tactical/Quick effects -> victory/escape/death -> world consequence.',
    'Map/category, parent bank/slot, quest condition, party/enemy side, spell and mode affect the result.',
    'Normal protected hit suppresses mutation; no reward/quest retirement on escape; source ending/revelation branches precede normal advancement/loot.',
    'Battle records and parent world effects have separate commit/retirement boundaries and codecs.',
    'Actual earned losses/flee/guardian victories and scoped protected289 evidence are retained; not all exceptional raw metadata contexts.',
    'Resolve all original actor/template producers, removal helpers, mode/slot special cases and caller preconditions, including malformed or wrapping definitions.',
    'SCUMMVM-CE-M43-COMPLETION1134.json SCUMMVM-CE-M4-GENERAL-ADMISSION-WORK.json')
row('S16','Keep65 one-shot reveal',E,
    'native/resident_world.inc SCUMMVM-CE-HARDCODED-STORY525-REVIEW.md',
    'native_world_story.h native_world_input.h',
    'WorldInput.pollWorldEvents -> WorldStory.revealKeep -> spawn metadata and NPC sprite -> refresh.',
    'Map65, source position rectangle and high-bit spawn marker.',
    'Shift one-shot marker and set authored sprite; keep both owned window and canonical table bytes consistent.',
    'World/window codecs retain the actual marker and actor record.',
    'Out-of-region or already-used state produces no new reveal.',
    'Bounded direct producer only; all original loaders and nonstandard map/type callers still need U03.',
    'SCUMMVM-CE-M4-WORLD-STORY65-UNIT.json SCUMMVM-CE-HARDCODED-STORY525-REVIEW.md')
row('S17','Shadow approach producer',E,
    'NA/BS.ROUTINES.BK2@0x2a16 native/resident_world.inc@FA16',
    'native_world_story_events.h native_world.h native_world_input.h',
    'Ordinary world poll/input -> World.discoverShadowApproach -> WorldStoryEvents.shadowApproach -> field59 bit1.',
    'Type<3, map0, sector169, Temple prerequisite56 bit64 and not already discovered.',
    'Set only once; preserve other fields and RNG; typed later encounter admission consumes the state.',
    'Existing normal core/session save retains it; original private disk checkpoint is separate S18.',
    'Wrong prerequisite/map/sector or repeated input makes no mutation.',
    'Eight retained original branches and two native production hooks cover this bounded producer; not every indirect original writer.',
    'SCUMMVM-CE-WORLD-STORY525-VERIFICATION.json SCUMMVM-CE-INTEGRATION527-I4-VERIFICATION.json')
row('S18','Private machine checkpoint and durable saving',D,
    'native/resident_world.inc@FA39 native/shared_helpers.inc',
    'native_new_game.h native_world.h native_exploration_save.h native_player_session.h',
    'Original private8E0D/disk bootstrap checkpoint -> approved native CE core/player-save/session ownership.',
    'Physical HDV/CPU checkpoint has no release-runtime authority.',
    'Persist semantic world/party/inventory/continuations through CE; no CPU PC/stack/IRQ or HDV mutation is introduced.',
    'Unified durable native saves and owner/assistance semantics deliberately replace original disk serialization.',
    'Staged invalid native restore fails without partial owner adoption; original device failures are not hardware-emulated.',
    'Replacement is approved; do not infer equivalence of every old save/import or arbitrary original memory alias.',
    'SCUMMVM-CE-ARCHITECTURE-DECISION.md SCUMMVM-CE-WORLD-STORY525-VERIFICATION.json SCUMMVM-CE-PERSISTENCE520-VERIFICATION.json')
row('S19','Praevoc movement/dialogue phase writer',E,
    'native/resident_world.inc@FC58 native/resident_world.inc@FCA0 native/dialogue_effects.inc@B2F9',
    'native_praevoc.h native_world_move.h native_world_conversation.h',
    'Selected Keep69 floor or dialogue cue -> PraevocSpeechRules.trigger/baseCue -> source phase/cursor and ordinary completion.',
    'Map/context, admitted notice condition, effect preference, field12 phase bit and cursor0..5.',
    'Trigger toggles phase; enabled effects advance/wrap cursor and emit corresponding cues; dialogue base cue has a distinct no-phase-advance contract.',
    'Canonical field and actor-turn cursor have explicit codecs; audio remains473-owned.',
    'Invalid cursor rejects; disabled sound preserves source phase behavior without invented audio.',
    'Bounded505/506 contracts only; every original nonlocal stack-return caller remains U03/U04.',
    'SCUMMVM-CE-PRAEVOC505-VERIFICATION.json SCUMMVM-CE-DIALOGUE-SPEECH506-VERIFICATION.json')
row('S20','Ending and revelation state boundary',E,
    'native/combat_results.inc SCUMMVM-CE-M43-ENDING1082-VERIFICATION.json',
    'native_ending.h native_battle_aftermath.h native_startup_host.h',
    'Actual selected parent victory -> BattleAftermath ending/revelation branch -> EndingPresentation -> acknowledgement/terminal continuation.',
    'Original parent map/contact identity and real victory; no guessed quest flag or ordinary last-enemy event substitutes for the ending.',
    'Branch precedes ordinary advancement/loot; acknowledgment and terminal phase remain explicit.',
    'Native result phase and legal post-ending core/cold restore retain terminal state.',
    'Ordinary victory and defeat do not become endings; modal/CLI timeout failures remain historical.',
    'This bounded real ending is verified; it does not certify every original special scripted context.',
    'SCUMMVM-CE-M43-ENDING1082-VERIFICATION.json')
row('S21','Schedule, movement, population and indirect map consequences',U,
    'native/inventory_schedule.inc native/actor_world.inc native/resident_location.inc native/resident_world.inc',
    'native_npc_schedule.h native_world_actor_turn.h native_world_story.h native_world_input.h',
    'World enter/clock/input -> scheduler/population/actor pass -> mutable actor/object/spawn records -> script predicates and future events.',
    'Map family/type, clock, route, sight/occupancy, freeze flags and dynamic actor identity determine writes.',
    'Known ordinary scheduling/turn/patch order has dedicated native owners; one complete global original writer/interleaving proof does not exist.',
    'World/window/actor turn/scheduler state are serialized; changed maps survive travel.',
    'NoRoute/Capacity native wait/defer policy is an explicit replacement; invalid contexts remain failures.',
    'Prove every dynamic record producer and its predicate/side-effect consumers, including zero-coordinate templates, partial-map windows and special actor phases.',
    'SCUMMVM-CE-WORLD-POPULATION517-VERIFICATION.json SCUMMVM-CE-NPC-RECOVERY518-VERIFICATION.json')
row('S22','NPC route-failure continuation policy',D,
    'native/actor_world.inc',
    'native_world_actor_turn.h native_npc_actor_actions.h',
    'Failed ordinary route job -> retire/defer that job -> let other actors/input continue.',
    'Source route exhaustion or queue-capacity condition in admitted ordinary NPC work.',
    'CE avoids the original fatal/hang continuation; does not permanently erase NPCs, rewind RNG or fabricate a path.',
    'Existing waiting/route state and codecs retain deferred work.',
    'Invalid source context is still rejected; ordinary route failure is recoverable.',
    'Approved narrow replacement; not a declaration that every invalid original context is supported.',
    'SCUMMVM-CE-NPC-RECOVERY518-VERIFICATION.json SCUMMVM-CE-ENGINE-FALLBACKS.md')
row('S23','Clock, tool/torch counters and retained service temporaries',U,
    'native/resident_world.inc@FE41 native/quest_tavern.inc native/progression_search.inc native/progression_open.inc',
    'native_clock.h native_world_wait.h native_world_conversation.h native_world.h',
    'World action/wait/dialogue/service -> source clock/counter update -> actor/story predicate consumers.',
    'Source0400 page includes ordinary counters and request temporaries, not only quest bits; byte overflow, BCD and free/paid action context matter.',
    'Known hourly20/tool33/torch32 and service17..21 handling is implemented; all original alias/interleaving lifetimes are not proven.',
    'Clock, fields and pending action/turn/modal state have separate native codecs.',
    'Known cancel/free/refusal clock policies are retained and some are approved CE changes; not every helper entry has a lifetime proof.',
    'Bound all original callers, arithmetic flags, pointer aliases and post-dialogue/world-turn ordering for these shared temporaries.',
    'SCUMMVM-CE-DIALOGUE-CLOCK515-VERIFICATION.json SCUMMVM-CE-ENGINE-FALLBACKS.md')
row('U01','Complete executable/data and root universe',U,
    'story08-1135/resources.local.json story08-1135/allocated-layout.local.json disassembly/analysis.json',
    'module.mk native_engine.cpp',
    'Original file/overlay loader -> selected bank/root -> executed instructions -> mutation. Native has no arbitrary original executor.',
    'All74 base resources, logical/allocated bytes and raw image scanned; their byte patterns do not establish code boundaries.',
    '204604 logical-payload writer candidates include deliberate data false positives. Do not label them real paths or missing implementations.',
    'Original memory residency/code generation and native logical-state persistence are distinct universes.',
    'No absence-of-opcode or successful-campaign argument can prove global completeness.',
    'Provide a resource-offset/bank/overlay code/data certificate, complete entry roots and load/relocation extents, with explicit dispositions for every excluded region.',
    'SCUMMVM-CE-HARDCODED-STORY525-REVIEW.md')
row('U02','Caller-derived indirect copy/fill and store destinations',U,
    'NA/LWR.MAIN.RTNS@0x841 NA/LWR.MAIN.RTNS@0x873 native/shared_helpers.inc story08-1135/reference-call-index-v2.local.json',
    'native_new_game.h native_inventory.h native_party.h native_world.h',
    'Original caller supplies C2/C4/C6 or EB/end/value ->1441/1473 -> actual destination writes; native counterparts depend on caller, not a general emulated memory API.',
    '16 new original probes; inclusive forward copy versus exclusive fill, page boundaries and both overlap directions. Pointer value sets are not established by known quest flags.',
    '5690 original instructions/1030 story-page writes confirm the mechanism. Forward copy can propagate newly written bytes under overlap.',
    'Different callers cover creation/load, party membership, combat copies, graphics and menu buffers.',
    'No native bug is inferred merely because no arbitrary-pointer copy API exists; each legitimate caller needs a typed equivalence summary.',
    'For every direct/indirect caller, resolve bank, source/destination/end/value ranges, alias/overlap, full write set, return order and native operation.27 helper-related raw call candidates and137 pointer-store reference sites are indexed, not all certified code.',
    'story08-1135/original-helper-probes.json story08-1135/trace-summary.json')
row('U03','Physical bank, overlay and computed/stack call closure',U,
    'disassembly/hdv-and-banks.md native/bank_services.cpp story08-1135/reference-pc-collisions.local.json',
    'native_world_input.h native_world_conversation.h native_battle_aftermath.h',
    'Original bank/zero-page switch + overlay load + computed call/modified return -> story owner; native typed continuations replace that ABI.',
    '3668 reference call candidates after excluding4 helper definitions;2 computed expressions resolve to bounded selectors in new probes, not a full original graph.9105 PC values occur at multiple reference locations. Unbanked PC equality is not a callee identity.',
    'Original stack-discard/nonlocal-return and bank switches can change which later writer executes.',
    'Native continuation codecs need per-context relations, not raw stack-byte reproduction.',
    'Unresolved edges are not removed by assuming no runtime fallback in the native target.',
    'Resolve each reachable computed jump/return/overlay edge and summarize entry state and resulting continuation; prove roots/caller set complete rather than matching case labels.',
    'disassembly/hdv-and-banks.md SCUMMVM-CE-HARDCODED-STORY525-REVIEW.md')
row('U04','Copied, patched and generated instruction contexts',U,
    'disassembly/hdv-and-banks.md native/scene_tiles.inc native/bank_services.cpp',
    'native_world_story.h native_world_input.h native_world_conversation.h',
    'Original code-writing store/copy/patch -> later fetch of modified instruction -> potential story mutation/continuation.',
    'Documented driver patch changes an indirect read opcode into a store. Static immutable65C02 scanning does not include every produced opcode value.',
    'Must follow writes to future instruction operands/opcodes, including cross-bank/shared helper effects.',
    'Native does not execute generated original bytes; equivalence requires semantic effect and caller relation.',
    'A bounded code-pointer guard in a reference program is not a proof that all dynamic versions are admitted or irrelevant.',
    'Enumerate all reachable code-writing ranges and values to a fixed point, then map each version/context or prove its story-state noninterference.',
    'disassembly/hdv-and-banks.md')
row('U05','Special CPU/ROM/interrupt and inherited-memory contexts',U,
    'disassembly/NOXARCH.MAIN.asm disassembly/NOXARCH.SYSTEM.hdv-driver-D000.asm disassembly/hdv-and-banks.md',
    'native_engine.cpp native_new_game.h native_world.h',
    'Conditional machine/65816 routine, firmware vector, IRQ or beyond-EOF/inherited byte -> later shared state -> story consumer.',
    'Loader has conditional65816 mode and copies beyond EOF; rounded file/raw image envelope adds16723 padding bytes but cannot enumerate inherited/generated RAM.',
    'Most platform mechanisms have native replacements, but whole-context story noninterference is not automatically established by replacing the host.',
    'CE saves semantic owners; original machine snapshots and arbitrary imported garbage are not native logical state.',
    'Raw Open53/254 inherited-table contexts remain separately source-qualified; this assignment does not silently waive their state consequences.',
    'Provide entry/exit/write-set invariants for each special context and inherited-memory read; prove no additional story effects or document a specific justified difference.',
    'SCUMMVM-CE-ARCHITECTURE-DECISION.md SCUMMVM-CE-M43-COMPLETION1134.json')
row('U06','Dynamic authored-data producers and reachability',U,
    'ce-native-producers494-local.json ce-native-scripted-contacts502-local.json native/resident_world.inc',
    'native_story_patches.h native_world_conversation.h native_world_actor_turn.h native_inventory.h',
    'Original code/callback/schedule/copy creates or changes a record -> later dialogue/contact/Search/Use/gate operator.',
    'Initial data closure does not prove all later records, item IDs, service identities, map references or prototype/import entry contexts.',
    'A complete operator grammar can coexist with an unclassified producer that changes its operands or activation order.',
    'New/loaded/changed maps and modal transactions must maintain the original admissible record invariants across save/travel.',
    'Zero coordinates, missing actor-specific packets or a reserved definition name do not by themselves prove unreachable behavior.',
    'Trace every code-derived producer and consumer, including all legitimate entry/copy/spawn/patch mechanisms; establish induction invariants for reachable record values.',
    'SCUMMVM-CE-SOURCE-PRODUCERS494.md SCUMMVM-CE-SCRIPT-SERVICE-OWNERS500.md SCUMMVM-CE-SERVICE-ADMISSION497.md')

row('S24','Four-setter patched movement dispatch',D,
    'NA/GAME@0x3289 native/command_actions.inc@92D8 story08-1135/original-dispatch-probes.json',
    'native_world_input.h native_world_move.h',
    'Original four direction setters patch928F/9290 ->9289 repeat dispatcher ->928E JSR; native key/Direction selection calls the corresponding typed movement owner.',
    'Known setters produce exactly ED5B/EE20/EEDC/EF91; new probes execute all4 setters through the actual call. All static absolute patch-store candidates in the base payloads are the8 GAME stores.',
    'CE uses typed cardinal selection and transport/move continuations, not writable original call operands. Selector probes leave the story page unchanged.',
    'Persist logical movement/transport/continuation state, not generated instruction bytes.',
    'Other hypothetical indirect patch writers are not excluded by this bounded selector proof.',
    'Typed-dispatch replacement is established for known cardinal entries; original repeat-count/descendant and global operand-alias closure remain U02/U03/U04.',
    'story08-1135/original-dispatch-probes.json SCUMMVM-CE-COMMAND-DOMAIN524.md SCUMMVM-CE-ARCHITECTURE-DECISION.md')
row('S25','Fixed loader menu selection and native host replacement',D,
    'NA/LOADER.P@0xa9f native/menu_controller.inc@A09F native/menu_extras.inc',
    'native_engine.cpp native_startup_host.h',
    'Original loader A09F six-value branch sequence -> fixed menu destination; native CE host owns New/load/save/options lifecycle.',
    'New probes cover every A byte0..255: values1..6 select the six literal branches;250 other values reach original14A7. entry[3] is a statically enumerated reference expression, not an arbitrary pointer.',
    'Native host menus deliberately replace original loader/physical disk operations and machine return topology; source selector itself does not write the story page.',
    'Native Engine save/session/configuration owns persistence; no original loader image is resumed.',
    'Native host validation replaces original raw invalid-selector BRK behavior; no new hidden debug entry is authorized.',
    'Approved host replacement only; every descendant story effect or unusual original entry still requires its own source relation. A3B2 has distinct loader and inventory meanings.',
    'story08-1135/original-dispatch-probes.json SCUMMVM-CE-ARCHITECTURE-DECISION.md SCUMMVM-CE-M43-COMPLETION1134.json')

row('U07','Dialogue resource tails and selector termination',U,
    'ce-m3-dialogue-census29-local.json native/dialogue_loader.cpp native/dialogue_packets.cpp story08-1135/retained-dialogue-tails.local.json',
    'native_dialogue.h native_world_conversation.h',
    'Original resource/actor selector and loaded-buffer lifetime -> packet boundary/decompress -> response effects; native bounded resource selector -> typed packet or refusal.',
    'Fresh immutable-image revalidation exactly reproduces439 valid packets/3227 headers/552 effects, but the retained catalog also has19 unparsed resource tails totaling4502 bytes.',
    'Native validates header/payload extents before accepting a matching actor. A parser stopping at malformed/trailing bytes does not establish how original selectors and retained RAM behave.',
    'Selected valid packet/modal state is owned; bytes outside logical payload and original load-buffer reuse require a context-specific relation.',
    'Do not label these tails additional scripts, missing quests or harmless padding without proving their original entry/termination behavior.',
    'For each indexed tail and every legitimate actor selector, establish original scan termination, record limit, buffer/read bounds and any possible mutation/exception; prove unselectability or map actual behavior to the native refusal/consumer.',
    'story08-1135/retained-input-validation.json SCUMMVM-CE-M3-CORPUS74-CLOSURE.json')

def normalized(raw):
    return raw.decode().replace('\r','')

def source_binding():
    bindings={}
    for owner in sorted({n for r in ROWS for n in r['native_owners']}):
        path=CE/'engines/nox'/owner
        assert path.is_file(),owner
        text=path.read_text()
        # Line anchors are useful review coordinates, not an AST call proof.
        anchors=[dict(line=i,text=line.strip()[:170])for i,line in enumerate(text.splitlines(),1)
                 if re.search(r'\b(?:class|struct)\s|\b(?:commitDialogueFields|dialogueEligible|searchQuestReward|pressurePlate|keyDoor|compoundDoor|portal|syncObjectFields|discoverShadowApproach)\s*\(',line)]
        bindings[owner]=dict(sha256=sha(path.read_bytes()),anchors=anchors)
    return bindings

def compatibility():
    path=ROOT/'ce-m4-story-patches64-source.zip';archive=zipfile.ZipFile(path)
    old=normalized(archive.read(next(n for n in archive.namelist()if n.endswith('native_story_patches.h'))))
    new=normalized((CE/'engines/nox/native_story_patches.h').read_bytes())
    assert old==new
    path2=ROOT/'ce-m4-conversation10-source.zip';archive2=zipfile.ZipFile(path2)
    old2=normalized(archive2.read(next(n for n in archive2.namelist()if n.endswith('native_dialogue_rules.h'))))
    new2=normalized((CE/'engines/nox/native_dialogue_rules.h').read_bytes())
    tails=(old2[old2.index('inline DialogueCommitResult commitDialogueFields'):],new2[new2.index('inline DialogueCommitResult commitDialogueFields'):])
    eligible=(old2[old2.index('inline bool dialogueEligible'):old2.index('inline bool selectDialogueResponse')],new2[new2.index('inline bool dialogueEligible'):new2.index('inline bool selectDialogueResponse')])
    assert tails[0]==tails[1] and eligible[0]==eligible[1]
    return dict(map_patch64_normalized_source_exact=True,dialogue_commit10_normalized_tail_exact=True,
                dialogue_eligibility10_normalized_section_exact=True,
                archives={path.name:sha(path.read_bytes()),path2.name:sha(path2.read_bytes())},
                qualification='Line endings normalized for semantic source comparison; frozen current source still uses exact byte hashes. Does not turn older whole-workflow qualifications into fresh current execution.')

def main():
    inv=json.loads((OUT/'inventory-summary.json').read_text())
    audit=json.loads((OUT/'completeness-audit.json').read_text())
    trace=json.loads((OUT/'trace-summary-v2.json').read_text())
    probe=json.loads((OUT/'original-helper-probes.json').read_text())
    dispatch=json.loads((OUT/'original-dispatch-probes.json').read_text())
    input_validation=json.loads((OUT/'retained-input-validation.json').read_text())
    context_audit=json.loads((OUT/'stack-mmio-audit.json').read_text())
    assert all(sha((ROOT/name).read_bytes())==h for name,h in inv['accepted_m43_hashes'].items())
    retained={}
    for r in ROWS:
        for name in r['retained_evidence']:
            path=ROOT/name
            if not path.exists():
                path=ROOT.parent/name
            assert path.is_file(),name
            retained[name]=sha(path.read_bytes())
    result=dict(status='IN PROGRESS',assignment='story.08 focused technical investigation1135',
                milestone_m43='VERIFIED; closed and unchanged',story08_implementation='UNKNOWN',story08_verification='IN PROGRESS',
                exhaustive_reachable_equivalence_demonstrated=False,
                judgment='Exhaustive static candidate envelopes and bounded source reconciliation are delivered. Exhaustive original-to-native reachable mutation coverage is NOT demonstrated; story.08 is not promoted.',
                requirements=dict(original_scope='Base v119 only; all original story-state mutation mechanisms and execution contexts, not just selected campaign or finite known flags.',
                                  task_authority='Owner explicit story.08 source investigation/correction assignment; no campaign replay, release preparation, packaging or publication.'),
                source_image_sha256=inv['image_sha256'],native_source_commit='d52b130c5df',native_source_files=377,native_source_drift=[],
                accepted_m43_hashes=inv['accepted_m43_hashes'],source_bindings=source_binding(),compatibility=compatibility(),
                mechanism_rows=ROWS,disposition_counts=dict(collections.Counter(r['disposition']for r in ROWS)),
                private_inventory=dict(logical_resources=74,logical_bytes=960173,logical_writer_candidates=inv['candidate_writes'],
                                       logical_indirect_store_candidates=inv['candidate_indirect_stores'],rounded_bytes=audit['rounded_file_bytes'],
                                       extra_allocated_tail_bytes=audit['allocated_tail_bytes'],raw_disk_bytes=audit['raw_image_bytes'],
                                       authored_instance_rows=trace['authored_instances'],bounded_instance_dispositions=trace['instance_dispositions'],
                                       reference_calls=trace['call_sites'],unbanked_pc_collisions=trace['colliding_unbanked_pc_values'],
                                       unparsed_dialogue_resource_tails=input_validation['unparsed_resource_tails'],
                                       unparsed_dialogue_tail_bytes=input_validation['unparsed_resource_tail_bytes'],
                                       stack_mmio_context_candidate_rows=context_audit['candidate_rows'],
                                       warning='Candidate counts include data false positives and overlap; they are not counts of actual story paths, missing code or completed requirements.'),
                new_focused_checks=dict(original_helper_cases=16,original_dispatch_cases=260,original_cases_total=276,
                                        original_instruction_steps=probe['original_instruction_steps']+dispatch['original_instruction_steps'],actual_story_page_writes=1030,
                                        coverage_counts='Unique cases/steps in one complete probe pass; immutable reproduction does not add coverage.',
                                        complete_probe_passes=2,actual_original_case_executions_in_assignment=552,total_original_instruction_steps_in_assignment=18040,
                                        independent_explicit_addressed_write_opcodes=62,implicit_stack_write_opcodes=6,documented_memory_write_opcode_total=68,decoder_set_and_index_wrap_audit='PASS',
                                        native_execution_checks=0,engine_launches=0,builds=0,
                                        qualification='Original helper probes and source-integrity/compatibility checks only; no new native execution or campaign claim.'),
                reproduction=dict(status='VERIFIED',result='The original seven-command method reproduced exit0 and immutable outputs; the added stack/MMIO supplement reproduced separately without repeating the original probes.',
                                  reason='Required reproducibility/completeness audit; no unrelated suite or gameplay repetition.'),
                demonstrated_native_defects=[],native_corrections=[],
                genuine_discrepancies=[
                    dict(id='A1',domain='Inventory method',finding='Logical EOF omits16723 bytes in rounded allocated source files.',disposition='Corrected source envelope; raw whole-image supplement retained, inherited RAM still UNKNOWN.'),
                    dict(id='A2',domain='Inventory method',finding='Lexical reference index omitted52 increment/decrement/fill spellings.',disposition='Additional source coordinates retained; reference index remains supporting evidence, not the completeness certificate.'),
                    dict(id='A3',domain='Call-chain method',finding='9105 case PC values are nonunique across reference locations; initial lexical call scan also counted4 helper definitions.',disposition='Revision2 excludes the4 definitions, retains revision1 and records3668 call candidates/2 computed sites. No unbanked-PC matching accepted as equivalence; bank/overlay proof remains required.'),
                    dict(id='A4',domain='Original mechanism',finding='Copy is inclusive/forward sequential; fill is exclusive-end. Substituting memmove or a shared length rule without caller proof is unjustified.',disposition='16 bounded original probes pass; native discrepancy is not inferred without a concrete legitimate caller mismatch.'),
                    dict(id='A5',domain='Retained qualifications',finding='Full physical-slot Search failure, prototype lookup alias, dynamic records, generated instructions and special machine contexts lack complete effect relations.',disposition='Explicit UNKNOWN rows; not waived by M4.3 or relabeled as newly discovered production defects.'),
                    dict(id='A6',domain='Authored input completeness',finding='Revalidated439 valid packets and552 effects, but retained19 unparsed tails/4502 bytes cannot be declared unreachable solely from the census parser stopping.',disposition='Exact tail coordinates retained privately; U07 requires original selector/termination and buffer proof, not exploratory gameplay.'),
                    dict(id='A7',domain='Instruction/context completeness',finding='The initial62-opcode wording omitted implicit stack writers and memory-read side effects on bank/IO context.',disposition='Corrected to62 explicit addressed writes/RMW plus6 implicit stack-writing opcodes; stack coordinates and MMIO touch candidates supplemented. Asynchronous and generated contexts remain UNKNOWN, not asserted covered by static decoding.')],
                residual_risk='A source-reachable mutation or context-specific ordering/failure effect may remain unmapped despite bounded operators and accepted M4.3. No global noninterference or complete original executable/alias/caller proof exists. Native fallback absence and campaign success do not discharge that risk.',
                remaining_work=[dict(id=r['id'],requirement=r['unresolved_evidence_requirement'])for r in ROWS if r['disposition']==U],
                reproducible_commands=['python conversion/inventory_story08_1135.py','python conversion/audit_story08_envelope1135.py','python conversion/probe_story08_helpers1135.py','python conversion/trace_story08_1135.py','python conversion/probe_story08_dispatch1135.py','python conversion/validate_story08_inputs1135.py','python conversion/audit_story08_contexts1135.py','python conversion/reconcile_story08_1135.py'],
                method_source_sha256={n:sha((ROOT/n).read_bytes())for n in ['inventory_story08_1135.py','audit_story08_envelope1135.py','probe_story08_helpers1135.py','trace_story08_1135.py','probe_story08_dispatch1135.py','validate_story08_inputs1135.py','audit_story08_contexts1135.py','reconcile_story08_1135.py']},
                validation_dependency_sha256=input_validation['dependencies_sha256'],
                artifact_hashes={p.name:sha(p.read_bytes())for p in sorted(OUT.glob('*.json'))},retained_evidence_sha256=retained,
                stopping_judgment='Technical investigation delivered with an explicit unresolved result, as permitted by the assignment. This is not a research-plan-only report and not a claim of exhaustive closure; exact remaining proof obligations and per-site candidate matrices are retained.')
    public=ROOT/'SCUMMVM-CE-STORY08-1135-RECONCILIATION.json'
    public.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(story08='UNKNOWN',mechanisms=len(ROWS),dispositions=result['disposition_counts'],original_cases=276,new_native_defects=0,builds=0,engine_launches=0),indent=2))

if __name__=='__main__':
    main()
