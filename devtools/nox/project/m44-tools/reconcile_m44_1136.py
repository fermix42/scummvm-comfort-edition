"""Reconcile M4.4 functional findings without rewriting historical acceptance."""
from pathlib import Path
import hashlib,json,collections,os
R=Path(os.environ.get('NOX_M44_CONVERSION',str(Path(__file__).resolve().parent)))
CE=Path(r'D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
old=json.loads((R/'SCUMMVM-CE-STORY08-1135-RECONCILIATION.json').read_text())
analysis=json.loads((R/'m44-functional1136/priority-source-analysis.json').read_text())
validation=json.loads((R/'m44-functional1136/focused3/verification.json').read_text())
rows=[]
findings={
'S03':('E','L001 actor48 TEST response writes129 to field133, the derived lookup cell53. Resource001 is attached only to Test Town1/2. All692 authored travel/inn/portal edges and native New/Temple/QuickExit/owned-return entries exclude outside entry into Test IDs1..5. Debug/import activation is explicitly excluded.',
 'No supported quest/item/identity change. Original indexed alias changes a temporary lookup; next scene projection replaces it. Native syncObjectFields derives lookup from canonical scene; it is not a durable quest flag.',
 'priority-source-analysis.json alias; SERVICE-ADMISSION497; SCRIPT-SERVICE-OWNERS500'),
'S06':('B','Directional Talk selects original packet; response fields3/4 encode gold or item,16 recruitment/award,17 payment/service/battle,18/19 price,20 hourly state. Complete552 authored mutations plus service offer/teacher/request producers are retained. Conversation.finish handles exit once; WorldConversation stages4/5..30 own party, payment and services.',
 'Gold uses original wrapped16-bit addition/cap; item grant merges; full/poor/cancel/repeat keep existing contracts. Physical-slot exhaustion returns NoSpace and exits through explicit engine error, not a completed reward. Original shared884C..88A3 fails fatally; native stages/rolls back instead of preserving unsaveable partial RAM. Recovery is reload of the prior CE save, not a new retry UI. Party/gold/inventory and pending service state have canonical codecs.',
 'SCRIPT-SERVICE-OWNERS500; INTEGRATION527-I2; native_inventory.h describeDialogueHandoff/finishDialogueHandoff; native_world_conversation.h stage4; focused1136 Search/handoff boundary'),
'S10':('B','Syntactic extended Search object state53/op64 -> lock/trap eligibility -> searchQuestReward -> inventory grant -> SearchInput acknowledgement. Original Search96EC and shared884C check first-match inventory then empty slot;88A3 invokes14DD when neither exists.',
 'Normal award/repeat/ordinary-capacity ordering remains unchanged: searchable marker consumed, quest bit and counted-item field committed even on ordinary capacity refusal. Retained270 original/native cases include24 capacity refusals. Physical exhaustion is distinct: native refuses atomically and engine returns an error; original writes fields then fails before a normal save boundary. Neither grants a duplicate/retry advantage or successfully saves a lost reward. Prior-save recovery replaces original fatal RAM details. Reachability of simultaneous256 earned types is not proved or assumed impossible.',
 'M4-SEARCH-REWARD36-UNIT; M43-PAGES501; native/progression_shared.inc884C..88A3; native_search.h; focused1136'),
'S11':('A','Original item definition byte8bit32 selects counted acquisition/removal. Only misc68/69 carry it, targeting torch32/tool33. Search, scripted loot, commerce, Drop and consumption reach explicit native owners.',
 'Acquisition adds one; Drop/sale retain source commerce quantity/arithmetic semantics rather than substituting removed stack size. Protected/readied/cancel do not remove. Counters, retained commerce context and inventory serialize in core. No arbitrary imported ID/CPU decimal-flag state is admitted as a new gameplay rule.',
 'DATA-OWNERSHIP509; M43-DROP1091; native_inventory_actions.h; native_search.h; native_battle_loot.h'),
'S12':('A','Normal U/C/Readied-book -> InventoryActions/WorldCast or battle spell owner -> resource-derived cost/effect/navigation. Initial inventory, shops, Search, contacts, dialogue item assignments and NPC gear enumerate item producer grammars; random loot is weapon/armor only.',
 'Typed owners charge mana/consumables, retain Orb origin and transport, apply effects and travel, preserve cancellation/refusal and codec continuation. Obsolete zero-cost/power book45 has no producer in these item sources (dialogue field3 values also exclude45); no invented spell. Invalid/imported definitions remain outside supported operation.',
 'SOURCE-PRODUCERS494; subsystem magic519 contract; INTEGRATION527-I3; priority-source-analysis.json capacity dialogue IDs; native_world_cast.h/native_inventory_actions.h'),
'S13':('A','Dialogue recruitment request/NPC role34..65 -> Recruitment; inn P/L -> InnMembership; N command -> PartyOrder. Original128-byte members and16-byte equipment are copied/exchanged with matching inventory owner masks.',
 'Native owned record exchange/left compaction produces same record order without machine overlap. Inn leave stores128 meaningful bytes, clears equipment effects, shifts owner bits and food cap; pickup charges fee, restores retained record and HP/MP without changing retained dead status. Original pickup scratch129th byte is outside the128-byte character; no additional character field is lost. Full/poor/last/no-storage checks precede commits. Core/inn cold chains retained.',
 'SCRIPT-SERVICE-OWNERS500; INTEGRATION527-I2; native/party_membership.inc988A/9968; native_inn_membership.h; copy_callers below'),
'S15':('C','Actor/map/category and protection predicates -> EncounterPreparation -> battle rules -> victory/escape/defeat -> BattleAftermath -> BattleWorldReturn effects -> advancement/loot -> retirement. All17 fixed predicates/15 role126 producers,9 awards remain included, including zero-coordinate templates.',
 'Protection suppresses effects; escape has no quest reward/retirement; ending/revelation precedes normal loot. Defect1136: four loot grant sites treated NoSpace as success. Corrected all four to require Applied. Counted scripted increment now commits only after successful physical allocation, while ordinary-capacity refusal still increments as source. Failed award leaves loot pending, inventory/fields/RNG unchanged; prior-save recovery matches source fatal outcome without silent completion. No claim that the full physical inventory is earned/reachable.',
 'SCRIPT-SERVICE-OWNERS500 contact502; M43-COMPLETION1134; native/progression_shared.inc88A3; native_battle_loot.h; focused1136 six cases'),
'S21':('B','World entry/time/action -> scheduler -> population/actor pass -> patches/contact predicates. Source76 maps, identity routes and11 patch records retain their producer owners; zero-coordinate records are not dismissed as padding.',
 'Scheduler alters route/actor identity, not a new service role. Typed World/Window capture changed records before travel/save; patches use canonical flags. NoRoute/Capacity wait/defer replaces original machine fatal/frozen navigation rather than inventing quest outcomes; record remains available for later turns. No new interleaving feature is implied by CPU bank timing.',
 'WORLD-POPULATION517; NPC-RECOVERY518; SERVICE-ADMISSION497 minute/role closure; INTEGRATION527-I3; native_npc_schedule.h/native_world_actor_turn.h'),
'S23':('B','Committed world action, wait, Talk/service -> explicit clock/counter owner -> hourly20/tool33/torch32/request retirement and next actor turn. Source caller timing is represented by paid/free native continuations.',
 'Byte counters and fields persist; completed Talk synchronizes field20 to Clock. Typed request lifetimes replace reused RAM. Documented CE free/cancel/dialogue-encounter clock policies remain intentional differences, not missing IRQ writes. No arbitrary inherited BCD flag is required to execute an ordinary command.',
 'DIALOGUE-CLOCK515; WORLD-COMMAND524 and ENGINE-FALLBACKS policies; native_clock.h/native_world_wait.h/native_world_conversation.h'),
'U01':('B','Supported roots are New, supported CE save/load, ordinary player commands, autonomous world turns and authored event/service transitions. The74 immutable resource inventory supplies data; raw writer-pattern candidates are not treated as thousands of features.',
 'Native startup/core/resource catalogs and subsystem owners implement those roots. Boot, original executable residency, arbitrary original-PC entry and debug/imported RAM are replaced/excluded by the owner directive. This is a supported functional-domain reconciliation, NOT a complete code/data certificate or proof that every byte of the HDV is unreachable code. No concrete omitted supported root was identified.',
 'M44 directive sections1/3/5; original1135 inventory; HARDCODED-STORY525-REVIEW; native_engine.cpp/module.mk/native_startup_host.h; M43 supported-domain evidence'),
'U02':('B','Actual1441/1473 callers below cover character/menu initialization, inn membership, battle/summon/record scratch, encounter loading, schedule reset and menu reset. Native equivalents own typed records/buffers rather than an arbitrary pointer store service.',
 'Inclusive forward copy/exclusive fill are preserved in original probes; native substitutions use required record lengths and explicit initialization. Leftward record compaction reads unchanged later records and agrees with forward copy; swaps use owned temporaries. Scratch/menu reset endpoints and incidental adjacent RAM bytes are accepted native ownership differences. No demonstrated gameplay difference remains in these callers. This disposition does not certify every raw byte-pattern call candidate as executable or require an emulated memory-copy layer.',
 'story08-1135/original-helper-probes.json16 cases; copy_callers below; DATA-OWNERSHIP509; INTEGRATION527-I2; native_party.h/native_inn_membership.h/native_vortex_cast.h'),
'U03':('B','Four movement pointer setters select ED5B/EE20/EEDC/EF91; six loader menu selectors choose literal branches1..6. Modal/combat/nonlocal original continuations correspond to typed WorldInput, WorldConversation and BattleAftermath stages.',
 'The260 retained original selector cases establish these dispatch effects. Banks/stack return bytes are not serialized as gameplay state; canonical continuation codecs preserve pending decisions. Global unbanked-PC collisions are not missing features. Arbitrary original-PC/stack entry is explicitly outside supported roots.',
 'story08-1135/original-dispatch-probes.json and trace-summary.json; native_world_input.h/native_world_conversation.h/native_battle_aftermath.h; INTEGRATION527'),
'U04':('B','Known movement operand patching selects directions; scene patches select raster reads/writes; disk driver opcode patches select transfer direction. Native movement/render/archive/save owners implement their observable operations.',
 'Native executes no generated original instruction. Resource-derived directions, raster output and transfer results are explicit native operations, not campaign-outcome shortcuts. Original code residency/modification itself has no additional required player effect. A fixed point over hypothetical generated opcodes is outside this functional objective, not evidence of missing quests.',
 'disassembly/hdv-and-banks.md; original1135 movement setters; native_world_move.h/native_graphics.h/native_engine.cpp; HARDCODED-STORY525-REVIEW'),
'U05':('B','Original boot/65816/ROM/disk/IRQ contexts select platform execution, graphics/audio/input/storage. Native engine/Mixer/event/core owners replace them. Raw Open53 and254 contexts reside in Test Town2/Town1, the excluded island.',
 'Audio473 accepted sequencing is independent of CPU IRQ; original resources remain authoritative. CE save/session authority replaces original disk writes and machine snapshots. Inherited bytes/EOF garbage are not imported as new story rules. Original Test raw-table qualifications remain historical and are explicitly outside ordinary New/travel domain, not silently rewritten.',
 'ARCHITECTURE-DECISION; M44 directive; audio473; SERVICE-ADMISSION497; original1135 raw Open context; native_engine.cpp/native_new_game.h'),
'U06':('A','Producer grammar includes actual New/save ownership,76 map initial states,11 typed map patches,552 dialogue mutations,callbacks,17 fixed/15 contact predicates,9 contact awards,49 offers,NPC gear,recruitment,scheduling and the implemented Use/cast/world/combat actions.',
 'Consumers use canonical field/item/actor/map identities. Random loot produces only nonspecial weapons/armor; grants merge kind/id, not fresh duplicate records. Recruitment bypasses ordinary170 capacity, so occupancy cannot be proved by170+special arithmetic. That uncertainty is preserved; full-slot error policy is now explicit and no award may silently complete. Test/debug/import entry is excluded affirmatively, not by absence of a known caller. No additional dynamic opcode interpretation is needed for this typed producer grammar.',
 'SOURCE-PRODUCERS494; SERVICE-ADMISSION497; SCRIPT-SERVICE-OWNERS500/502; priority-source-analysis.json; corrected native_battle_loot.h; focused1136'),
'U07':('E','All31 TLK files x32 legitimate aligned actor selectors execute original AFEC..B074 header/skip/first-match/32-record control using immutable file read/skip service.992 cases yield exactly439 valid extents matching the retained catalog.',
 'For each19 tail, first actor is non-slot-aligned or already selected earlier; every complete tail header claims more bytes than remain, or header is truncated. Thus no additional valid selectable authored packet/effect is omitted. Native bounded refusal replaces stale-buffer/device-error behavior beyond EOF (B subdisposition). This is not a claim of original EOF error-PC parity or a claim that all4502 bytes are padding.',
 'priority-source-analysis.json tails (19 individual dispositions); NA/SRTN.NPC.TALK commonAC85 selectorAFEC..B0DF; native_dialogue.h selectDialoguePacket')}
for r in old['mechanism_rows']:
    if r['disposition']!='UNKNOWN':continue
    c,trigger,effect,evidence=findings[r['id']]
    rows.append(dict(id=r['id'],mechanism=r['mechanism'],classification=c,trigger_and_original_mechanism=trigger,
        player_visible_order_persistence_failure=effect,native_owners=r['native_owners'],
        original_references=r['original_references'],retained_evidence=r['retained_evidence'],new_support=evidence,
        remaining_risk='No identified unresolved native progression discrepancy. Full physical-slot earned reachability is not proved; error/reload policy is explicit, not silent reward completion.' if r['id'] in ['S06','S10','S15','U06'] else 'No specific additional supported player-visible risk identified; evidence remains scoped, not universal machine-state proof.',
        correction='native_battle_loot.h four failed-grant checks and counted-field commit ordering; focused1136 PASS' if r['id']=='S15' else 'None required'))
assert len(rows)==16
baseline=json.loads((R/'ce-native-acceptance528/guardian885b/corrected-source.json').read_text())
drift=[p for p,h in baseline.items() if sha(CE/p)!=h]
assert set(drift)=={'engines/nox/native_battle_loot.h','engines/nox/native_startup_host.h'},drift
for f,h in old['accepted_m43_hashes'].items():assert sha(R/f)==h,f
assert sha(R.parent/'Nox Archaist HDV (v119).HDV')==old['source_image_sha256']
accepted=json.loads((R/'SCUMMVM-CE-M43-COMPLETION1134.json').read_text())
assert sha(Path(accepted['executable_path']))==accepted['executable_sha256']
assert sha(R/'ce-native-acceptance528/guardian885b/corrected-source.zip')==accepted['source_archive_sha256']
callers=[
 ('battle_services.inc:8A2C','source D2/D3..+63 ->0200,64-byte battle presentation/record scratch','native battle records/render owners; nonoverlap scratch'),
 ('battle_summon.inc:9FD6','source D2/D3..+63 ->0200,64-byte summon scratch','native summon record owner; nonoverlap scratch'),
 ('combat_turns.inc:EB71','source668/669..+671 inclusive ->0200','typed selected combat record/effect scratch'),
 ('combat_turns.inc:F065','sourceD2/D3..+63 ->0200','typed combat record/effect scratch'),
 ('combat_overlay.inc:96A7','fillBD00..<BFFF with0','BattleRecords initialized owned storage; active records loaded explicitly'),
 ('combat_overlay.inc:9B08','fill9600..<97FF withFF','typed battle layout/occupancy initialization'),
 ('inventory_encounters.inc:9ACE','050F..0710 inclusive ->D000,514-byte encounter/load transfer','EncounterPreparation/EncounterState read authored definitions, no shared executable scratch'),
 ('inventory_schedule.inc:9A01/1C/37/52','fillB500..<B8FF zero; AA00..<B4FF AA; A900..<A9FF AA; B900..<B9FF AA','NpcSchedule/WorldWindow initialize actor/object/route arrays explicitly; no RAM sentinel persistence'),
 ('menu_controller.inc:9F62','fill0400..<0800 zero','host New/startup initializes canonical fields and scratch; CE load uses staged core, not accidental RAM clearing'),
 ('menu_initialize.inc:97D0/97EB/9806','AE60..AEBF->7043 (96),AEC0..AECE->6CDA(15),AECF..AEEE->050B(32)','NewGame imports real source initial records; host menus own filenames/configuration'),
 ('party_membership.inc:988A','member source..+128 inclusive ->067F (129 scratch bytes)','InnMembership pickup meaningful128-byte member; extra scratch byte not a character field'),
 ('party_membership.inc:9968','067F..06FE inclusive ->member (128)','InnMembership leave stores128; typed record/owner compaction and food clamp')]
result=dict(status='VERIFIED',assignment='M4.4 functional equivalence closure1136',
    standard='Supported functional domain under owner directive; not exhaustive Apple II machine-state equivalence.',
    m43='VERIFIED; closed; preserved',story08_functional='VERIFIED within supported base v119 functional domain',
    exhaustive_machine_equivalence=False,previous1135_report_preserved=True,rows=rows,
    classifications=dict(collections.Counter(r['classification'] for r in rows)),
    carried_prior_rows=[r for r in old['mechanism_rows'] if r['disposition']!='UNKNOWN'],
    copy_callers=[dict(original=a,range_or_operation=b,native_effect=c) for a,b,c in callers],
    discrepancy=dict(id='M44-01',status='VERIFIED',defect='Conditional allocation failure could silently complete native loot; physical-full earned reachability not demonstrated.',
        corrected='Require Applied at four grant sites; commit counted field after successful physical allocation, keeping source ordinary-capacity refusal semantics.',
        production_files=['engines/nox/native_battle_loot.h'],validation=validation['report']),
    accepted_difference='Original fatal physical-slot path versus native transaction rejection/engine error and reload prior save. No new retry/drop UI or item storage expansion.',
    residual_risks=[
      'No proof of the maximum simultaneously earned distinct inventory types. Recruitment can bypass170 ordinary capacity; therefore physical256 cannot be dismissed as impossible. Native full-slot failure is explicit and no longer silently completes a reward. Both original and native require prior-save recovery at this failure; do not advertise seamless recovery.',
      'Original EOF/device/stale-buffer error details, arbitrary imported/debug memory and Test content are outside supported functional domain. The selector proof covers real aligned actor slots and complete resource extents.',
      'Source/retained evidence supports functional reconciliation; no new all-feature campaign, listening audit, full opcode/code-data certificate or every-PC execution proof is claimed. Existing old scoped failures/qualifications remain intact.'],
    ready_for_release_preparation=True,release_preparation_performed=False,
    new_builds=3,new_focused_processes=3,new_campaign_processes=0,
    failures=['First analysis script syntax error corrected before source probe execution.',
              'First focused process rejected a generic assertion; second diagnostic process located it at the failed-award comparison. Test setup had assigned ordinary item metadata before begin(), which clears it. Setup corrected after begin; no second production correction. Both rejected batches and their binaries/logs remain preserved.'],
    source_drift_from377=drift,new_validation_file='engines/nox/native_m44_award_validation.h',
    accepted_hashes=old['accepted_m43_hashes'],source_image_sha256=old['source_image_sha256'],
    prior_reconciliation_sha256=sha(R/'SCUMMVM-CE-STORY08-1135-RECONCILIATION.json'),
    original_selector_cases=analysis['tails']['original_selector_cases'],original_selector_executions=2976,analysis_passes=3,valid_packets=439,tail_rows=19,
    validation=validation,
    commands=['python conversion/analyze_m44_1136.py','Official clean native-only script with m44-1136-build-output.targets','python conversion/check_m44_1136.py focused3','python conversion/reconcile_m44_1136.py'],
    reproduction_note='Analysis is read-only over original resources; focused runner creates a fresh isolated portable directory and intentionally refuses an existing one. Rename/archive that test directory or adapt output destination for a justified reproduction. No replay needed to reproduce source classification.',
    artifact_sha256={str(p.relative_to(R)):sha(p) for p in [R/'analyze_m44_1136.py',R/'check_m44_1136.py',R/'patch_m44_1136.py',R/'native_m44_award_validation.h',R/'m44-functional1136/priority-source-analysis.json',R/'m44-functional1136/build.log',R/'m44-functional1136/build-diagnostic.log',R/'m44-functional1136/build-fixture-corrected.log',R/'m44-functional1136/focused/native-validation-rejected.json',R/'m44-functional1136/focused2/native-validation-rejected.json',R/'m44-functional1136/focused3/verification.json']})
checked={}
for group in ['reports_sha256','immutable_observations_sha256']:
    for name,h in accepted[group].items():
        assert sha(R/name)==h,name
        checked[name]=h
assert len(checked)==56
proof=dict(status='VERIFIED',accepted_reports_and_observations=56,sha256=checked,
    accepted_executable_sha256=accepted['executable_sha256'],source_archive_sha256=accepted['source_archive_sha256'],
    preserved_previous1135_sha256=result['prior_reconciliation_sha256'],live_saves_written=False)
(R/'m44-functional1136/preservation.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
result['preservation_check']=dict(report='m44-functional1136/preservation.json',sha256=sha(R/'m44-functional1136/preservation.json'),accepted_reports_and_observations=56)
result['build_validation']=[dict(command="$env:ForceImportAfterCppTargets='D:\\Codex Projects\\Nox Archaist\\conversion\\m44-1136-build-output.targets'; & '.\\run-build-release-cleanenv.ps1' -NoxNativeOnly",
    cwd=str(CE),log=log,exit=0,warnings=0,errors=0,elapsed=elapsed) for log,elapsed in [('build.log','00:03:12.82'),('build-diagnostic.log','00:02:55.78'),('build-fixture-corrected.log','00:02:48.94')]]
result['recording_failure']='Initial tracking writer used Windows cp1252 and rejected an existing UTF-8 document before its write; corrected explicit UTF-8 handling. No additional build or game process.'
(R/'SCUMMVM-CE-M44-1136-RECONCILIATION.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
md=['# M4.4 functional equivalence closure1136','',
'Status: VERIFIED within the supported functional domain. M4.3/completion1134 remains closed and unchanged. This is a separate corrected candidate; it does not replace the accepted executable or rewrite its evidence.','',
'One conditional native defect was found and corrected: loot accepted a failed physical-slot allocation as success. Four grant sites now require an Applied result. Counted scripted fields commit only after successful allocation; original ordinary-capacity refusal semantics stay intact. Full physical-slot earned reachability was not demonstrated and is not asserted impossible.','',
'The native failure policy remains an explicit engine error followed by loading the previous save. It does not add a retry/drop UI. This is an accepted replacement for the original fatal path, not silent quest-reward loss.','',
'## Functional matrix','',
'The JSON companion contains original sources, call chains, triggers, order/persistence/failure effects, native owners, retained evidence, corrections and residual risks for every row. All16 prior UNKNOWN rows have explicit A–F dispositions; the prior16 bounded equivalent/difference rows are carried with their exact historical qualifications. C below means corrected defect, not an outstanding defect.','',
'| Row | Class | Functional disposition |','|---|---|---|']
for r in rows:md.append('|'+r['id']+'|'+r['classification']+'|'+r['player_visible_order_persistence_failure'].replace('|','/')+'|')
md+=['','## Priority evidence','',
'All31 TLK resources ×32 aligned actor selectors execute original selector control (992 cases). All439 valid selected extents agree exactly with the retained packet catalog. Each of the19 tails/4502 bytes has an individual source disposition in local priority-source-analysis.json: non-slot selector or earlier first match, then oversized/truncated next payload. No additional valid selectable dialogue is omitted. Native EOF refusal is intentionally bounded; original error-PC/stale-buffer parity is not claimed.','',
'The only derived field133 assignment is L001 actor48 TEST, attached to Test Town1/2. Existing692 travel/inn/portal edges and explicit New/Temple/QuickExit/owned-return entry semantics exclude normal entry into Test IDs1..5. Debug/import/Test activation is explicitly excluded; the original data and existing handlers are preserved.','',
'Search retains its270-case source/native contract (111 awards/135 repeats/24 ordinary capacity refusals). Physical-slot failure is separate from the ordinary170 limit. The fresh producer overapproximation includes86 special types, and recruitment can add16 ordinary gear types without that limit: **170+special is not a proof of an occupancy ceiling**. No exclusion is based on that faulty argument. Source884C..88A3 first finds kind/id, then an empty six-byte record, then calls fatal14DD. Native Search/handoff fail atomically; corrected loot does likewise.','',
'## Actual copy/fill callers','',
'Original1441 copies forward through an inclusive endpoint;1473 fills to an exclusive endpoint. Prior16 original probes remain unchanged. The functional review uses actual callers, not raw-byte candidates.','',
'| Original caller | Actual transfer | Native disposition |','|---|---|---|']
for a,b,c in callers:md.append('|'+a+'|'+b+'|'+c+'|')
md+=['','Typed record copies preserve meaningful128-byte member/16-byte equipment/32-byte enemy records. Scratch resets, incidental adjacent bytes, filenames/menu setup and banked executable buffers have native owners rather than a shared emulated RAM API. Party compaction does not require reverse-copy propagation: leftward destinations cannot overwrite not-yet-read later records. Ownership-mask exchange and retained inn dead status remain unchanged.','',
'## Correction and validation','',
'Production rule: `engines/nox/native_battle_loot.h`. Test entry: `native_startup_host.h`; focused test: `native_m44_award_validation.h`. Three clean native-only builds and three isolated focused processes: initial correction, diagnostic-only assertion build, and diagnosed test-setup correction. The first two batches rejected the new test setup because begin() clears ordinary offer metadata; the final test assigns it at the actual pending-offer boundary. No second production correction, campaign, old audio suite or unrelated regression rerun. All rejected reports/binaries/logs remain preserved. Exact final executable/source hashes, command, exit and build log hashes are in the JSON.','',
'Focused result: `'+validation['report'].strip()+'`','',
'The allocation-boundary inventory is constructed by real catalog grants in an isolated rule test. It is not an earned game state or evidence of reachable inventory fullness. No live save, quest flag, map, battle outcome or original resource was changed to make a scenario pass. New/merge awards, duplicate commit refusal, pending-state codecs, unchanged inventory/RNG/fields, Search and dialogue failure boundaries are checked. Ordinary-capacity source tests are retained, not newly executed. Source analysis ran three times while refining the producer bound:2976 selector executions,992 unique cases; repetition adds no coverage.','',
'## Completeness and residual risk','',
'The functional inventory is organized by producer mechanism and native owner, not a finite quest-flag list or successful campaign path: New/load, dialogue assignments/bit operations, shared callbacks/handoffs, Search, item/count/commerce operations, spells/navigation, party/inn identity, combat consequences, schedule/population/patches, clocks and host continuations. Known dynamic records remain included. Hardware-only root/bank/patch/IRQ mechanisms are explicitly replaced under the owner directive; arbitrary CPU/memory entry is excluded. Raw opcode patterns are not counted as gameplay paths.','',
'No identified supported feature remains missing and no identified native progression discrepancy remains unresolved after the failed-award correction. The practical supported base v119 port is ready for **release preparation**, which this assignment does not perform. This does not mean error-free software or a proof of all original machine states.','',
'Residuals: maximum simultaneously earned inventory occupancy is not established; full physical-slot exhaustion retains explicit error/prior-save recovery. Malformed EOF/stale RAM, arbitrary imported/debug states and Test-only dialogue are excluded. New verification is focused component execution; broader play/audio/save evidence remains the qualified M4.3 baseline, not falsely re-executed on this candidate.','',
'The previous1135 exhaustive-machine judgment remains historical. Current story.08 functional classification is VERIFIED under the explicit M4.4 standard, with the accepted differences/exclusions above; exhaustive machine-state equivalence remains unproved and is not a release requirement.','',
'## Reproduction and preservation','',
'Run the analysis and focused commands in the JSON from the canonical Nox directory. Proprietary derived diagnostics stay local under conversion/m44-functional1136; sanitized matrix/report/method sources may be mirrored into CE. All377 frozen source hashes are checked: only the documented loot rule and startup test entry differ; one validator is new. Accepted completion1134 records and original HDV hashes are checked unchanged. No accepted save, audio473 artifact or baseline executable was overwritten. No push/tag/package/publication/deployment.','']
(R/'SCUMMVM-CE-M44-1136.md').write_text('\n'.join(md),encoding='utf-8')
print('M4.4 matrix:',result['classifications'],'baseline drift:',drift,'focused:',validation['report'].strip())
