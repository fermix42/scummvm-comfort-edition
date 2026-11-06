"""Reconcile original MSBuild warning occurrences against historical CE logs."""
import collections
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OLD = ROOT.parent / 'scummvm-private-feature-usable-desktop-window-layout-git'
OUT = ROOT / 'doc/integration-2026.4.0'
BASE = '29e2ceb01611ac18654d82bfe2116db8183bd53d'
UP = '7d2709da5136866589f7602f5f5a7b17de03420e'
pattern = re.compile(r'^\s*.*?scummvm-2026\.4\.0-integration\\(.+?)\((\d+),(\d+)\): warning (C\d+): (.+?) \[(.+?)\]$', re.M)
text = (ROOT / 'build-console.log').read_text(errors='replace').split('Build succeeded.')[0]
rows = []
for m in pattern.finditer(text):
    path, line, col, code, message, project = m.groups()
    if not message.strip() or message.strip().startswith(('with', '[', ']', 'T=')):
        continue
    rows.append(dict(path=path.replace('\\','/'), line=int(line), column=int(col),
                     code=code, message=message, project=Path(project.replace('\\','/')).name))
assert len(rows) == 66, len(rows)
historical = {}
for p in sorted(OLD.glob('*build*.log')):
    if p.stat().st_size > 100_000_000:
        continue
    content = p.read_text(errors='replace')
    for row in rows:
        key=(row['path'],row['code'],row['message'])
        if key in historical:
            continue
        for line in content.splitlines():
            if row['path'].replace('/','\\') in line and f"warning {row['code']}: {row['message']}" in line:
                historical[key] = dict(log=p.name, diagnostic=line.strip())
                break

def blob(ref, path):
    p=subprocess.run(['git','-c',f'safe.directory={ROOT.as_posix()}','-C',str(ROOT),
                      'show',f'{ref}:{path}'],capture_output=True)
    return p.stdout if p.returncode==0 else None

for n,row in enumerate(rows,1):
    key=(row['path'],row['code'],row['message'])
    row['occurrence']=n
    row['source_equal_ce_snapshot']=blob(BASE,row['path'])==blob('HEAD',row['path'])
    row['source_equal_upstream_release']=blob(UP,row['path'])==blob('HEAD',row['path'])
    row['historical_evidence']=historical.get(key)
    row['classification']=('Existing upstream warning' if row['source_equal_upstream_release'] else 'Existing CE warning') if row['historical_evidence'] and row['source_equal_ce_snapshot'] else 'Origin not established'
    if row['classification']=='Origin not established':
        engine=row['path'].split('/')[1]
        repro=ROOT/f'warning-repro-{engine}.log'
        needle=f"{engine}-{Path(row['path']).name}({row['line']},{row['column']}): warning {row['code']}: {row['message']}"
        if repro.exists() and needle in repro.read_text(errors='replace') and ': error ' not in repro.read_text(errors='replace'):
            row['targeted_compilation']=dict(log=repro.name,source_revision=UP if engine=='phoenixvr' else BASE,
                                            scope='Tagged/baseline source; integrated Release headers and compiler flags; no executable linked')
            row['classification']='Existing upstream warning' if row['source_equal_upstream_release'] else 'Existing CE warning'
    row['primary_category']='Toolchain or dependency (preexisting)' if row['code'] in ('C4775','C4506') and row['classification']!='Origin not established' else row['classification']
counts=collections.Counter(row['classification'] for row in rows)
result=dict(original_warning_count=66, occurrences=rows, classification_counts=dict(counts),
            primary_category_counts=dict(collections.Counter(row['primary_category'] for row in rows)),
            method='Count original diagnostics before Build succeeded; exclude repeated summary and template-context lines. Provenance requires matching historical diagnostics with identical CE source, or successful targeted compilation of the recorded baseline/tag source with Release flags. Toolchain/vendor diagnostics also retain their source provenance.')
(OUT/'warning-classification.json').write_text(json.dumps(result,indent=2)+'\n')
groups={}
for row in rows:
    key=(row['path'],row['line'],row['column'],row['code'])
    groups.setdefault(key,[]).append(row)
lines=['# Original Release warning classification','',result['method'],'',
       '| Source location | Code | Occurrences | Classification | Evidence |',
       '| --- | --- | ---: | --- | --- |']
for (path,line,col,code),group in groups.items():
    row=group[0]; proof=row.get('historical_evidence') or row.get('targeted_compilation') or {}
    lines.append(f"| {path}:{line}:{col} | {code} | {len(group)} | {row['classification']} | {proof.get('log','Unestablished')} |")
lines+=['',f'Total: {len(rows)} occurrences across {len(groups)} locations.', '',json.dumps(dict(counts),sort_keys=True)]
(OUT/'WARNING-CLASSIFICATION.md').write_text('\n'.join(lines)+'\n')
print(json.dumps(dict(counts=counts, distinct_locations=len({(x['path'],x['line'],x['column'],x['code']) for x in rows})),indent=2))
