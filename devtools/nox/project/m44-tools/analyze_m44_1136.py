"""M4.4 functional data/capacity/selector analysis; no gameplay or save edits."""
import collections
import hashlib
import json
import sys
import os
from pathlib import Path

ROOT=Path(os.environ.get('NOX_M44_CONVERSION',str(Path(__file__).resolve().parent)))
sys.path[:0]=[str(ROOT),str(ROOT/'local-python')]
from inspect_session import files
from py65.devices.mpu65c02 import MPU
OUT=ROOT/'m44-functional1136'
R=files(ROOT.parent/'Nox Archaist HDV (v119).HDV')
sha=lambda b:hashlib.sha256(b).hexdigest()

def maps():
    for family in ['SURF','UM','RUIN','TOWN2','TOWN','CSTL','KEEP']:
        raw=R['NA/DATA.MAP.'+family];p=2 if family=='SURF'else 1
        while raw[p]!=255:
            at=int.from_bytes(raw[p+7:p+10],'little')
            yield family,raw[p+1],raw[p:p+(raw[p] or 256)],raw[at:at+1792]
            p+=raw[p]or 256

def capacity():
    inv=R['NA/SRTN.INVENTORY'];producer=collections.defaultdict(set)
    def add(kind,ident,origin):
        if kind<=2:
            producer[(kind,ident)].add(origin)
    initial=R['NA/DATA.SAVE.GAMEN'][0x1800:0x1e00]
    initial_items=[]
    bypass_ordinary=set()
    for at in range(0,1536,6):
        row=initial[at:at+6]
        if row[0]!=254:
            add(row[0]&15,row[1],'initial');initial_items.append((row[0]&15,row[1]))
    at=0x4100
    while at<0x48c9:
        row=inv[at:at+inv[at]]
        for p in range(8,row[3],2):
            if row[p]&128:break
            add(row[p]&191,row[p+1],'merchant')
        at+=row[0]
    for family,ident,header,t in maps():
        for slot in range(0,245,4):
            q=t[1024+slot:1036+slot]
            if q[2]and q[2]!=255 and q[3]&191==53 and q[7]&192==64:
                add(q[4],q[5],'Search:'+family+':'+str(ident))
        for slot in range(0,256,8):
            a=t[1280+slot:1288+slot];q=t[768+slot:780+slot]
            if a[2]and a[3]!=255 and a[5]==126 and q[0]&64:
                add(q[5],q[7],'contact:'+family+':'+str(ident))
    for name,data in R.items():
        if not name.startswith('NA/NPC.JOIN'):continue
        start=int.from_bytes(data[1:3],'little');entries=(start-3)//128
        assert start>=3 and data[start:]and len(data)==start+entries*16
        for at in range(start,len(data),2):
            kind,ident=data[at:at+2]
            if ident:
                add(kind,ident,name)
                bypass_ordinary.add((kind,ident))
    effects=json.loads((ROOT/'ce-native-dialogue-effects500-local.json').read_text())['records']
    ids={r['value']for r in effects if r['kind']==177 and r['field']==3}
    ids.add(R['NA/DATA.SAVE.GAMEN'][0x1703])
    # Deliberate conservative Cartesian overapproximation: even price-low-byte
    # assignments are included as possible item IDs, across all three kinds.
    # High-bit handoff kind assignments are only128/130: kind0 or kind2.
    # Low field4 values0/1 are gold high bytes, never armor handoffs.
    kinds={r['value']&127 for r in effects if r['kind']==177 and r['field']==4 and r['value']&128}
    assert kinds=={0,2}
    for kind in kinds:
        for ident in ids:add(kind,ident,'dialogue-upper-bound')
    scripted_producer={k:set(v) for k,v in producer.items()}
    ranges=inv[0x8191:0x8191+40]
    for kind in range(2):
        for level in range(1,11):
            lower=ranges[kind*20+(0 if level==1 else (level-2)*2)]
            upper=ranges[kind*20+(level-1)*2+1]
            for ident in range(lower,upper+1):
                definition=inv[[0x1000,0x1cc0][kind]+ident*32:][:32]
                if not definition[8]&128:add(kind,ident,'random-loot')
    special=[]
    for (kind,ident),origins in sorted(producer.items()):
        if ident>=[102,122,168][kind]:continue
        definition=inv[[0x1000,0x1cc0,0x2c00][kind]+ident*32:][:32]
        if definition[8]&128:special.append(dict(kind=kind,id=ident,origins=sorted(origins)))
    ordinary_gear=[]
    for kind,ident in sorted(bypass_ordinary):
        if kind>1 or ident>=[102,122][kind]:continue
        d=inv[[0x1000,0x1cc0][kind]+ident*32:][:32]
        if not d[8]&128:ordinary_gear.append([kind,ident])
    return dict(ordinary_limit=170,possible_special_distinct=len(special),physical_bound=170+len(special),special=special,
                bound_is_reachability_proof=False,ordinary_limit_can_be_bypassed_by_recruitment=True,
                recruitment_ordinary_distinct=ordinary_gear,
                initial_duplicate_records=len(initial_items)-len(set(initial_items)),
                producer_items=len(producer),valid_producer_distinct=sum(i<[102,122,168][k] for k,i in producer),dialogue_id_upper_bound=sorted(ids),
                qualification='Conservative producer set including Test records and broad dialogue Cartesian product. The170+special value is NOT a valid global occupancy proof: recruitment grants ordinary gear without that cap. Random ordinary loot rejects special bit128; grantOne merges existing kind/id. No claim that all these items coexist in earned play.')

def tails():
    catalog=json.loads((ROOT/'ce-m3-dialogue-census29-local.json').read_text())
    common=R['NA/SRTN.NPC.TALK'][:0xfd2];results=[];selected={}
    # Execute the original selector through header selection. File service is
    # a bounded immutable resource read/skip; no decoded/outcome data injected.
    for resource in sorted(n for n in R if n.startswith('NA/DATA.TLK.')):
        data=R[resource]
        for actor in range(0,256,8):
            c=MPU();m=c.memory;m[0xac85:0xbc57]=common;m[0x60f]=actor;c.pc=0xafec
            cursor=0;header_offset=None;steps=0;reason=None
            while c.pc not in [0xb074,0xb0db,0xb0df]:
                if c.pc==0x80fd:
                    mode=m[0x6039];count=m[0x6031]|m[0x6032]<<8;dest=m[0x62ee]|m[0x62ef]<<8
                    if mode==0x81:cursor=0
                    if mode==0x90:
                        cursor+=count
                    else:
                        if cursor+count>len(data):reason='source resource exhausted; no complete next header';break
                        assert count==3 and dest==0x67a
                        header_offset=cursor;m[dest:dest+count]=data[cursor:cursor+count];cursor+=count
                    c.pc=(c.stPopWord()+1)&65535
                else:c.step()
                steps+=1;assert steps<4000
            if c.pc==0xb074:
                size=m[0x67b]|m[0x67c]<<8
                valid=size>0 and size<0x1400 and cursor+size<=len(data)
                selected[(resource,actor)]=dict(offset=header_offset,payload=cursor,size=size,valid_extent=valid)
            results.append(dict(resource=resource,actor=actor,selected=selected.get((resource,actor)),steps=steps,stop=reason or hex(c.pc)))
    packet_by_key={(r['resource'],r['actor']):r for r in catalog['records']}
    fresh_selected=[]
    for key,value in selected.items():
        if value['valid_extent']:
            assert key in packet_by_key,(key,value)
            old=packet_by_key[key]
            assert value['payload']==old['packet_offset']and value['size']==old['encoded_length']
            fresh_selected.append(key)
    assert len(fresh_selected)==439
    classified=[]
    for t in catalog['unparsed_tails']:
        raw=R[t['resource']];p=t['offset'];first=raw[p];prefix={r['actor']for r in catalog['records']if r['resource']==t['resource']}
        declared=int.from_bytes(raw[p+1:p+3],'little')if len(raw)-p>=3 else None
        assert first in prefix or first%8!=0
        assert declared is None or declared>len(raw)-p-3
        classified.append(dict(resource=t['resource'],offset=p,length=t['length'],first_actor=first,declared_length=declared,
                               first_match_already_in_prefix=first in prefix,
                               functional_classification='E',
                               disposition='Not an additional valid selectable packet: non-slot actor or prior first match; next declared payload exceeds remaining resource (or header truncated). All valid original selector extents match retained439 packets.',
                               malformed_original_failure_policy='No meaningful fresh dialogue beyond EOF; native bounded refusal is accepted rather than stale-buffer/device-failure emulation.'))
    return dict(original_selector_cases=len(results),valid_selected_packets=len(fresh_selected),tail_bytes=sum(t['length']for t in classified),tails=classified,selectors=results,
                qualification='Original header/record-limit/first-match control executes. EOF data-service stop is analysis, not a claim of original error-PC parity. Legitimate NPC actor selectors are aligned slots; arbitrary imported actor bytes are outside supported operation.')

def alias():
    group_maps=[dict(family=f,map=i)for f,i,h,t in maps()if bytes(h[20:23])==b'001']
    data=json.loads((ROOT/'ce-native-dialogue-effects500-local.json').read_text())
    effect=next(r for r in data['records']if r['field']==133)
    assert effect['resource']=='NA/DATA.TLK.L001'and effect['actor']==48 and effect['value']==129
    return dict(effect=effect,maps=group_maps,keyword='TEST',source_effect='Assign129 to derived lookup field133 (scene lookup index53); no ordinary quest flag, item, reward or permanent actor record is changed.',
                prototype_evidence='The authored response describes a byte-flag test; resource001 belongs to bundled Test maps. Retained497 includes all692 authored travel/portal/inn edges, no outside edge into IDs1..5; supported New starts7 and return/orb only use legitimate owned origins.',
                native_effect='Conversation assigns the canonical field view; scene projection/syncObjectFields regenerates the lookup. The byte is not a durable quest producer.',
                functional_classification='E',scope='Prototype TEST/import/debug activation excluded; no missing supported quest. Native arbitrary machine-memory alias parity is not required.')

def main():
    OUT.mkdir(exist_ok=True)
    result=dict(capacity=capacity(),tails=tails(),alias=alias(),resource_hashes={n:sha(v)for n,v in R.items()})
    (OUT/'priority-source-analysis.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(capacity_bound=result['capacity']['physical_bound'],special_upper_bound=result['capacity']['possible_special_distinct'],alias_maps=result['alias']['maps'],selector_cases=result['tails']['original_selector_cases'],selected_packets=result['tails']['valid_selected_packets'],tails=len(result['tails']['tails'])),indent=2))

if __name__=='__main__':main()
