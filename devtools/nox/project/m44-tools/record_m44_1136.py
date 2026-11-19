"""Record completed scoped M4.4; historical1134/1135 artifacts remain unchanged."""
from pathlib import Path
import json,hashlib,subprocess,shutil,os
R=Path(os.environ.get('NOX_M44_CONVERSION',str(Path(__file__).resolve().parent)))
CE=Path(r'D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def write(path,data):path.write_text(data,encoding='utf-8')
report=R/'SCUMMVM-CE-M44-1136-RECONCILIATION.json'
d=json.loads(report.read_text());assert d['validation']['status']=='VERIFIED'
commit=subprocess.check_output(['git','log','-1','--format=%H','--','engines/nox/native_battle_loot.h'],cwd=CE,text=True).strip()
d['production_commit']=commit
source=R/'m44-functional1136/source';source.mkdir(exist_ok=True)
for name in ['native_battle_loot.h','native_startup_host.h','native_m44_award_validation.h']:
    shutil.copy2(CE/'engines/nox'/name,source/name)
    assert sha(source/name)==d['validation']['source_sha256']['engines/nox/'+name]
d['changed_source_evidence']={str(p.relative_to(R)):sha(p) for p in source.iterdir() if p.is_file()}
report.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
p=R/'SCUMMVM-CE-M44-1136.md';s=p.read_text(encoding='utf-8-sig')
if 'Production commit:' not in s:s+='\nProduction commit: `'+commit+'`. Exact compiled three-file source copies are retained locally in conversion/m44-functional1136/source.\n'
write(p,s)
summary='''# M4.4 functional equivalence closure1136 — VERIFIED (November18,2026)

The owner M4.4 directive replaces machine-perfect coverage with supported functional equivalence. All16 prior UNKNOWN rows have explicit functional dispositions:4 A,9 B,1 corrected C,2 E. One conditional native loot allocation defect corrected: four grant sites require Applied and counted fields do not commit on physical allocation failure. Three clean native-only builds (initial, diagnostic, test-setup correction), three focused processes (two rejected test setups retained, final six cases PASS); no campaign or unrelated suite. Original selector992 unique cases confirms439 valid packets and19 tails with no additional valid dialogue. Test field133 alias is excluded with affirmative travel/domain evidence. M4.3/completion1134 and1135 historical records remain byte-identical; only two of377 frozen files differ plus one new validator. Physical256 earned reachability is unproved; explicit error/prior-save recovery replaces source fatal behavior, with no silent completed reward. Functional story.08 VERIFIED within M4.4 domain, not arbitrary machine-state proof. Detailed matrix/discrepancies/limits: SCUMMVM-CE-M44-1136.md and -RECONCILIATION.json. Ready for release preparation; none performed. No package/push/tag/publication/deployment.

'''
for name in ['SCUMMVM-CE-PROGRESS.md','SCUMMVM-CE-IMPLEMENTATION-PLAN.md']:
    p=R/name;s=p.read_text(encoding='utf-8-sig')
    if not s.startswith('# M4.4 functional equivalence closure1136'):write(p,summary+s)
p=R/'SCUMMVM-CE-ARCHITECTURE-DECISION.md';s=p.read_text(encoding='utf-8-sig')
if '## M4.4 functional standard1136' not in s:
    write(p,s+'\n## M4.4 functional standard1136 — November18,2026\n\nThe owner directive SCUMMVM-CE-M44-DIRECTIVE.txt is authoritative: functionally equivalent native PC base v119 using original resources, not Apple II machine-state reproduction. Typed ownership/continuations, safe transaction rejection and CE storage are explicit replacements. Original fatal physical-slot failure may abort with prior-save recovery; silent successful reward loss is corrected. Test/debug/imported arbitrary memory is outside supported New/ordinary-play/CE-save domain. No CPU executor/ROM bootstrap/IRQ delivery or new modernization feature is introduced. The functional matrix and accepted differences are in SCUMMVM-CE-M44-1136-RECONCILIATION.json; prior M4.3 checkpoints remain closed.\n')
p=R/'SCUMMVM-CE-ENGINE-COMPLETION.json';matrix=json.loads(p.read_text(encoding='utf-8-sig'))
row=next(r for r in matrix['requirements'] if r['id']=='story.08')
previous=json.loads(json.dumps(row))
if 'm44_functional_closure1136' not in matrix:
    matrix['m44_functional_closure1136']=dict(status='VERIFIED',report=report.name,production_commit=commit,previous_story08_row=previous,
        standard='Owner M4.4 supported functional equivalence; prior1135 exhaustive machine-state judgment remains historical',classifications=d['classifications'])
row['implementation']='IMPLEMENTED'
row['verification']=dict(status='VERIFIED',scope='M4.4 supported base v119 functional-domain source reconciliation, retained evidence and six focused allocation cases; not exhaustive arbitrary machine states or newly replayed campaign.',report=report.name)
row['functional_domain']='Original HDV data; normal New, ordinary commands/turns/authored events and supported CE save/load. Explicit native differences/exclusions in1136 matrix.'
row['production_connection']='Canonical typed owners cover all inventoried functional producer classes; physical allocation failure is explicit rather than silent reward completion. Original CPU/alias/stack/bank details are replaced as documented.'
row['blockers']=[]
row['original_evidence'].append('SCUMMVM-CE-M44-1136-RECONCILIATION.json: functional dispositions of16 prior UNKNOWN rows, original selector992 cases/19tail dispositions; prior reports unchanged.') if not any('M44-1136' in x for x in row['original_evidence']) else None
story=[r for r in matrix['requirements'] if r['subsystem']=='story']
c=matrix['counts']['story'];c['known_required']=sum(r['implementation']!='UNKNOWN' for r in story);c['unknown_required_rows']=sum(r['implementation']=='UNKNOWN' for r in story);c['native_implemented']=sum(r['implementation']=='IMPLEMENTED' for r in story);c['verified']=sum(r['verification']['status']=='VERIFIED' for r in story)
write(p,json.dumps(matrix,indent=2)+'\n')
for name in ['SCUMMVM-CE-ENGINE-BLOCKERS.md','SCUMMVM-CE-ENGINE-FALLBACKS.md']:
    p=R/name;s=p.read_text(encoding='utf-8-sig')
    if not s.startswith('# M4.4 functional closure1136'):
        write(p,'# M4.4 functional closure1136 — VERIFIED\n\nB-SCRIPT is closed for the owner-defined supported functional domain by M44-1136-RECONCILIATION.json. Prior machine-equivalence UNKNOWN records remain historical, not erased. Failed physical allocation is explicit error/prior-save recovery, not a completed reward. No CPU/executor/ROM/disk bootstrap/IRQ dependency or release work introduced. Existing unrelated scoped qualifications below retain their own limits.\n\n'+s)
p=R/'SCUMMVM-CE-M44-WORK1136.json';work=json.loads(p.read_text(encoding='utf-8-sig'));work.update(status='VERIFIED',current='Assignment complete; no release work authorized',builds=3,engine_launches=3,focused_passes=1,campaign_launches=0,production_commit=commit,report=report.name)
by={r['id']:r for r in d['rows']}
for row in work['rows']:row.update(status='VERIFIED',classification=by[row['id']]['classification'],scope='Functional reconciliation; source/runtime qualifications in final report')
write(p,json.dumps(work,indent=2)+'\n')
names=['SCUMMVM-CE-ARCHITECTURE-DECISION.md','SCUMMVM-CE-IMPLEMENTATION-PLAN.md','SCUMMVM-CE-PROGRESS.md','SCUMMVM-CE-ENGINE-COMPLETION.json','SCUMMVM-CE-ENGINE-BLOCKERS.md','SCUMMVM-CE-ENGINE-FALLBACKS.md','SCUMMVM-CE-M44-DIRECTIVE.txt','SCUMMVM-CE-M44-WORK1136.json','SCUMMVM-CE-M44-1136.md','SCUMMVM-CE-M44-1136-RECONCILIATION.json']
dest=CE/'devtools/nox/project'
for name in names:shutil.copyfile(R/name,dest/name);assert sha(R/name)==sha(dest/name)
shutil.copyfile(R.parent/'AGENTS.md',dest/'AGENTS.md');assert sha(R.parent/'AGENTS.md')==sha(dest/'AGENTS.md')
tools=dest/'m44-tools';tools.mkdir(exist_ok=True)
for name in ['analyze_m44_1136.py','check_m44_1136.py','reconcile_m44_1136.py','record_m44_1136.py','native_m44_award_validation.h','m44-1136-build-output.targets']:
    shutil.copyfile(R/name,tools/name)
print('Recorded M4.4 production',commit,'and byte-identical canonical/recovery mirrors; historical checkpoints preserved.')
