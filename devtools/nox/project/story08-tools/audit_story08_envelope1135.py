"""Independent limits/completeness audit of inventory_story08_1135.

Scans rounded allocated file content and raw disk blocks as additional upper
bounds. These are private source diagnostics, never a claimed execution census.
"""
import collections
import json
import math
from inventory_story08_1135 import ROOT, OUT, EXPECTED_IMAGE, MPU, sha, write, writers

def allocated_files(image):
    output={}; layout=[]
    def block(n):
        assert 0<=n<len(image)//512
        return image[n*512:(n+1)*512]
    def pointers(n):
        raw=block(n) if n else bytes(512)
        return [raw[i]|raw[i+256]<<8 for i in range(256)]
    def visit(key,prefix):
        n=key
        while n:
            raw=block(n)
            for slot in range(13):
                if n==key and not slot:
                    continue
                e=raw[4+slot*39:43+slot*39];kind=e[0]>>4
                if not kind:
                    continue
                name=prefix+e[1:1+(e[0]&15)].decode();ptr=int.from_bytes(e[17:19],'little')
                if kind==13:
                    visit(ptr,name+'/');continue
                assert kind in (1,2,3)
                size=int.from_bytes(e[21:24],'little');count=math.ceil(size/512)
                ids=[ptr] if kind==1 else pointers(ptr) if kind==2 else [p for index in pointers(ptr) for p in pointers(index)]
                assert count<=len(ids)
                body=b''.join(block(i) if i else bytes(512) for i in ids[:count])
                output[name]=body
                layout.append(dict(resource=name,logical_size=size,rounded_size=len(body),tail_bytes=len(body)-size,
                                   payload_sha256=sha(body[:size]),rounded_sha256=sha(body),data_blocks=ids[:count]))
            n=int.from_bytes(raw[2:4],'little')
    visit(2,'')
    return output,layout

def independent_opcode_check():
    # Explicit documented 65C02 memory-writing opcode set. This is independent
    # of mnemonic string matching in the primary inventory.
    groups={
        'STA':{0x81,0x85,0x8d,0x91,0x92,0x95,0x99,0x9d},
        'STX':{0x86,0x8e,0x96},'STY':{0x84,0x8c,0x94},'STZ':{0x64,0x74,0x9c,0x9e},
        'INC':{0xe6,0xee,0xf6,0xfe},'DEC':{0xc6,0xce,0xd6,0xde},
        'ASL':{0x06,0x0e,0x16,0x1e},'LSR':{0x46,0x4e,0x56,0x5e},
        'ROL':{0x26,0x2e,0x36,0x3e},'ROR':{0x66,0x6e,0x76,0x7e},
        'TSB':{0x04,0x0c},'TRB':{0x14,0x1c},
        'RMB':set(range(0x07,0x80,0x10)),'SMB':set(range(0x87,0x100,0x10))}
    manual=set.union(*groups.values())
    derived={op for op,(mn,mode)in enumerate(MPU.disassemble)
             if (mn in set(groups)-{'RMB','SMB'} or mn.startswith(('RMB','SMB'))) and mode not in ('imp','acc')}
    assert manual==derived,(manual-derived,derived-manual)
    assert all(MPU.disassemble[op][0]==family or MPU.disassemble[op][0].startswith(family)for family,ops in groups.items()for op in ops)
    # Boundary/wrap audit for indexed writes: include bases below0400 and
    # exclude zero-page wrap from story-page aliases. No current flag list used.
    probes={0x300:False,0x301:True,0x3ff:True,0x400:True,0x4ff:True,0x500:False,0xffff:False}
    assert all(any(0x400<=((base+i)&65535)<0x500 for i in range(256))==expected for base,expected in probes.items())
    return dict(documented_memory_write_opcodes=len(manual),opcode_set_exact=True,indexed_boundaries_pass=True,
                limitation='65C02 documented static bytes only; not65816/native-mode or generated-byte reachability.')

def main():
    image=(ROOT.parent/'Nox Archaist HDV (v119).HDV').read_bytes()
    assert sha(image)==EXPECTED_IMAGE
    payload_summary=json.loads((OUT/'inventory-summary.json').read_text())
    content,layout=allocated_files(image)
    payload_meta=json.loads((OUT/'resources.local.json').read_text())
    original={row['resource']:row for row in payload_meta}
    assert set(content)==set(original)
    assert all(row['payload_sha256']==original[row['resource']]['sha256']for row in layout)
    rows,controls,summary,_=writers(content)
    physical_rows,physical_controls,_,_=writers({'RAW_BASE_HDV':image})
    artifacts={
        'allocated_layout':write('allocated-layout.local.json',layout),
        'allocated_writes':write('allocated-writers.local.json',rows),
        'allocated_controls':write('allocated-controls.local.json',controls),
        'raw_image_writes':write('raw-image-writers.local.json',physical_rows),
        'raw_image_controls':write('raw-image-controls.local.json',physical_controls)}
    result=dict(status='VERIFIED',scope='Audit of static envelope completeness/limitations, not exhaustive reachable-path coverage.',
                decoder=independent_opcode_check(),source_image_sha256=sha(image),
                resource_count=len(content),logical_payload_bytes=payload_summary['resource_bytes'],
                rounded_file_bytes=sum(map(len,content.values())),allocated_tail_bytes=sum(r['tail_bytes']for r in layout),
                raw_image_bytes=len(image),rounded_candidate_writes=len(rows),rounded_candidate_controls=len(controls),
                physical_candidate_writes=len(physical_rows),physical_candidate_controls=len(physical_controls),
                artifacts=artifacts,
                identified_gaps=[
                    dict(id='U1',finding='Payload-only scanning omits allocated EOF padding.',disposition='Static source envelope expanded to rounded files and raw disk; executable reachability still UNKNOWN.'),
                    dict(id='U2',finding='All-byte decoding treats data as instructions.',disposition='All candidates remain UNKNOWN until a bank/overlay-specific code/data certificate exists; no204604-path claim.'),
                    dict(id='U3',finding='Call/return and indirect-store targets depend on incoming machine state.',disposition='Need complete per-context entry and alias/value summaries, including ordinary0400 fields and nonpage story consequences.'),
                    dict(id='U4',finding='Self-modified/copied/generated instructions can differ from immutable resource bytes.',disposition='Raw disk scan cannot cover generated values; enumerate code-writing stores and prove closure of their value sets before promotion.'),
                    dict(id='U5',finding='Conditional65816 firmware, ROM vectors, bank/overlay loads and interrupt contexts differ from ordinary65C02 data readers.',disposition='Need separate original context boundaries and proof of no additional story effect, or an explicit native replacement disposition.'),
                    dict(id='U6',finding='Reference C, trace coverage and finite known flags do not independently certify a complete executable universe.',disposition='Reference6686 sites are supporting coordinates only; completeness cannot be inferred from generated case count or M4.3.')])
    write('completeness-audit.json',result)
    print(json.dumps({k:v for k,v in result.items()if k not in ['artifacts','identified_gaps']},indent=2))

if __name__=='__main__':
    main()
