"""Conservative, reproducible base-v119 story writer inventory. No gameplay.

The byte-envelope deliberately decodes at EVERY resource byte. It is an upper
bound on static 65C02 candidates, NOT a claim that data is executable. No
reachability, alias or complete-equivalence conclusion follows from this scan.
Proprietary bytes/listings and per-site diagnostics stay in the local output.
"""
from pathlib import Path
import collections
import hashlib
import json
import os
import re
import sys

ROOT = Path(os.environ.get('NOX_STORY08_CONVERSION', str(Path(__file__).resolve().parent))).resolve()
sys.path[:0] = [str(ROOT), str(ROOT/'local-python')]
from inspect_session import files
from py65.devices.mpu65c02 import MPU

CE = Path(r'D:\Codex Projects\scummvm-private\scummvm-ce-2026.4.0-consolidated')
OUT = ROOT/'story08-1135'
EXPECTED_IMAGE = 'c42a95c7a356548c78957f98693e92d33c9bae3ff5126d9887c2980e5a490c3c'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def write(name, value):
    payload = (json.dumps(value, indent=2, sort_keys=True)+'\n').encode()
    path = OUT/name
    # Reproduction can verify identical output; never replace a differing run.
    if path.exists():
        assert path.read_bytes()==payload, 'Differing existing inventory: '+str(path)
    else:
        path.write_bytes(payload)
    return dict(path=str(path.relative_to(ROOT)), sha256=sha(payload))

def metadata(image):
    result=[]
    def directory(key,prefix):
        n=key
        while n:
            block=image[n*512:(n+1)*512]
            for slot in range(13):
                if n==key and not slot:
                    continue
                e=block[4+slot*39:43+slot*39]
                kind=e[0]>>4
                if not kind:
                    continue
                name=prefix+e[1:1+(e[0]&15)].decode()
                if kind==13:
                    directory(int.from_bytes(e[17:19],'little'),name+'/')
                elif kind in (1,2,3):
                    result.append(dict(resource=name,storage_kind=kind,file_type=e[16],
                                       aux_type=int.from_bytes(e[31:33],'little'),
                                       size=int.from_bytes(e[21:24],'little')))
            n=int.from_bytes(block[2:4],'little')
    directory(2,'')
    return result

def writers(resources):
    operations={'STA','STX','STY','STZ','INC','DEC','ASL','LSR','ROL','ROR','TSB','TRB'}
    modesizes={'imp':1,'acc':1,'imm':2,'zpg':2,'zpx':2,'zpy':2,'inx':2,'iny':2,'zpi':2,
               'rel':2,'abs':3,'abx':3,'aby':3,'ind':3,'iax':3,'zpr':3}
    rows=[]; controls=[]; summaries={}; direct=collections.Counter()
    for resource,data in sorted(resources.items()):
        count=collections.Counter()
        for offset,opcode in enumerate(data):
            mnemonic,mode=MPU.disassemble[opcode]
            count['offsets_examined']+=1
            size=modesizes.get(mode)
            assert size is not None, (opcode,mode)
            if offset+size>len(data):
                count['truncated_candidates']+=1
                continue
            if mnemonic in ('JSR','JMP','RTS','RTI','BRK'):
                controls.append(dict(resource=resource,offset=offset,operation=mnemonic,mode=mode,
                                     operand=int.from_bytes(data[offset+1:offset+size],'little') if size>1 else None,
                                     disposition='UNKNOWN', requirement='Establish executable entry, bank/overlay and incoming edges; resolve computed/stack returns before treating this as a real edge.'))
                count['control_candidates']+=1
            writing=mnemonic in operations or mnemonic.startswith(('RMB','SMB'))
            if not writing or mode in ('acc','imp'):
                continue
            operand=int.from_bytes(data[offset+1:offset+size],'little')
            indirect=mode in ('inx','iny','zpi')
            if indirect:
                possible_story=True; family='indirect_store'
            elif mode in ('abs','abx','aby'):
                span=255 if mode in ('abx','aby') else 0
                possible_story=any(0x400<=((operand+i)&65535)<0x500 for i in range(span+1))
                family='absolute_indexed' if span else 'absolute'
            else:
                possible_story=False;family='zero_page'
            row=dict(resource=resource,offset=offset,operation=mnemonic,mode=mode,operand=operand,
                     family=family,may_target_0400_04ff=possible_story,
                     disposition='UNKNOWN',
                     requirement='Resolve code/data and reachable bank/overlay context, then destination/value provenance and complete caller/continuation; raw opcode pattern is not a demonstrated execution.')
            rows.append(row);count[family]+=1
            if possible_story:
                count['story_page_possible']+=1
                if not indirect:
                    direct[operand]+=1
        summaries[resource]=dict(count)
    assert sum(s['offsets_examined']for s in summaries.values())==sum(map(len,resources.values()))
    return rows,controls,summaries,direct

def reference_writers():
    rows=[];cases=collections.defaultdict(list);sources={}
    paths=sorted((ROOT/'native').glob('*.inc'))+sorted((ROOT/'native').glob('*.cpp'))
    mutation=re.compile(r'\b(store|write|turnIncrement|std::copy_n|memcpy|memset)\s*\(')
    for path in paths:
        text=path.read_text(encoding='utf-8-sig');sources[str(path.relative_to(ROOT))]=sha(path.read_bytes())
        pc=None
        for number,line in enumerate(text.splitlines(),1):
            m=re.match(r'\s*case\s+(0x[0-9a-f]+)\s*:',line)
            if m:
                pc=int(m[1],16);cases[pc].append(dict(path=str(path.relative_to(ROOT)),line=number))
            m=mutation.search(line)
            raw_assignment=bool(re.search(r'\b(?:m|h|aux|zero)\[[^]]+\]\s*(?:[|&^+\-]|)=(?!=)',line))
            if not m and not raw_assignment:
                continue
            kind=m[1] if m else 'raw_assignment'
            literal=re.search(r'\b(?:store|write|turnIncrement)\s*\(\s*(0x[0-9a-f]+)',line)
            value=int(literal[1],16) if literal else None
            rows.append(dict(path=str(path.relative_to(ROOT)),line=number,case_pc=pc,operation=kind,
                             source=line.strip(),literal_destination=value,
                             direct_story_page=value is not None and 0x400<=value<0x500,
                             disposition='UNKNOWN',
                             requirement='Reference C is a partial semantic reconstruction; establish exact original-resource bytes/bank/callers and native effect before assigning equivalence.'))
    return rows,dict(cases),sources

def main():
    OUT.mkdir(exist_ok=True)
    image=(ROOT.parent/'Nox Archaist HDV (v119).HDV').read_bytes()
    assert sha(image)==EXPECTED_IMAGE
    resources=files(ROOT.parent/'Nox Archaist HDV (v119).HDV')
    meta=metadata(image)
    assert {m['resource']for m in meta}==set(resources)
    assert all(len(resources[m['resource']])==m['size']for m in meta)
    for row in meta:
        row['sha256']=sha(resources[row['resource']])
    source=json.loads((ROOT/'ce-native-acceptance528/guardian885b/corrected-source.json').read_text())
    source_drift=[name for name,h in source.items()if sha((CE/name).read_bytes())!=h]
    assert not source_drift,source_drift
    rows,controls,summaries,direct=writers(resources)
    references,cases,refhashes=reference_writers()
    outputs={}
    outputs['resources']=write('resources.local.json',meta)
    outputs['writer_envelope']=write('writer-envelope.local.json',rows)
    outputs['control_envelope']=write('control-envelope.local.json',controls)
    outputs['reference_writers']=write('reference-writers.local.json',references)
    outputs['reference_cases']=write('reference-cases.local.json',cases)
    outputs['reference_source_hashes']=write('reference-source-hashes.local.json',refhashes)
    snapshot_names=['SCUMMVM-CE-M43-COMPLETION1134.json','SCUMMVM-CE-M43-COMPLETION1134.md',
                    'SCUMMVM-CE-M43-ACCEPTANCE528.json','SCUMMVM-CE-COMPLETION1134-INTEGRITY.json']
    baseline={n:sha((ROOT/n).read_bytes())for n in snapshot_names}
    summary=dict(status='VERIFIED',scope='Complete static all-byte candidate envelope, NOT exhaustive reachable story equivalence.',
                 image_sha256=sha(image),resource_count=len(resources),resource_bytes=sum(map(len,resources.values())),
                 candidate_writes=len(rows),candidate_controls=len(controls),
                 candidate_indirect_stores=sum(r['family']=='indirect_store'for r in rows),
                 candidate_story_page_writes=sum(r['may_target_0400_04ff']for r in rows),
                 reference_mutation_sites=len(references),reference_files=len(refhashes),reference_case_addresses=len(cases),
                 frozen_native_files=len(source),source_drift=source_drift,accepted_m43_hashes=baseline,
                 resource_summary=summaries,outputs=outputs,
                 completeness_limits=[
                     'Every byte offset of all74 base resource payloads is examined; data bytes create intentional false positives.',
                     'Writes outside0400..04ff remain in the envelope because story consequences include map/actor/inventory/party state and later alias/copy effects.',
                     'Static byte coverage does not establish instruction boundaries, complete roots, bank/overlay reachability, indirect effective addresses or self-modified bytes.',
                     '65816 native-mode firmware and ROM/driver/generated code require separate context disposition; 65C02 decoding alone cannot close them.',
                     'Reference native C and historical traces are indexed as supporting evidence only; neither is a complete original program proof.',
                     'Parsed authored data grammars and native context equivalence require separate reconciliation; no story.08 promotion follows.'
                 ])
    write('inventory-summary.json',summary)
    print(json.dumps({k:v for k,v in summary.items()if k not in ['resource_summary','outputs','accepted_m43_hashes','completeness_limits']},indent=2))

if __name__=='__main__':
    main()
