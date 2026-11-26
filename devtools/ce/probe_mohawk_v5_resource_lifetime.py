# SPDX-License-Identifier: GPL-3.0-or-later
"""Original resource-unload control flow with traced fixture callbacks, not real cache ownership."""
import argparse, hashlib, json, shutil, struct
from pathlib import Path
from probe_mohawk_v5_original import run_case, u32


def stub(remote,write,output,case):
    owner=remote+0x2000;item=remote+0x2200;second=remote+0x2400;vtable=remote+0x2600
    count=output+64;records=remote+0x3000;callback=remote+0x4000;release=remote+0x4100
    active=case['name']!='unload-inactive';handle=0 if case['name']=='unload-no-handle' else 0x1234
    head=0 if case['name']=='unload-empty' else item
    write(owner,struct.pack('<6I',int(active),head,0,123,456,handle))
    write(item,u32(vtable)+u32(second));write(second,u32(vtable)+u32(0));write(vtable+4,u32(callback))
    # Trace kind, live handle and whether owned items remain. No item destructor
    # is substituted silently: these are deliberately fixture event callbacks.
    prefix=b'\x50\x52\xa1'+u32(count)+b'\x6b\xc0\x0c\x05'+u32(records)
    snapshot=b'\x8b\x15'+u32(owner+0x14)+b'\x89\x50\x04\x8b\x15'+u32(owner+4)+b'\x89\x50\x08\xff\x05'+u32(count)
    write(callback,prefix+b'\x8b\x54\x24\x0c\x89\x10'+snapshot+b'\x8b\x51\x04\x89\x15'+u32(owner+4)+b'\x5a\x58\xb8'+u32(1)+b'\xc2\x0c\x00')
    write(release,prefix+b'\xc7\x00'+u32(99)+snapshot+b'\x5a\x58\xc2\x08\x00')
    write(0x45a2c3,b'\xe9'+u32(release-0x45a2c3-5))
    s=bytearray()
    for _ in range(2 if case['name']=='unload-repeat' else 1):s+=b'\xb9'+u32(owner)+b'\xb8'+u32(0x4268d3)+b'\xff\xd0'
    s+=b'\xa1'+u32(count)+b'\xa3'+u32(output)
    return s


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
    if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized original executable')
    a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
    for name in ['unload-items','unload-no-handle','unload-empty','unload-inactive','unload-repeat']:
        case=dict(name=name,output='int32',expected=0,after_loader=True,allocation_size=0x10000,observe_resource_unload=True)
        row=run_case(exe,case,5,stub_factory=stub);row.pop('expected',None);row.pop('matches_static_contract',None);expected={'unload-items':[7,7,99],'unload-no-handle':[7,7],'unload-empty':[99],'unload-inactive':[],'unload-repeat':[7,7,99]}[name]
        observed=row.get('resource_unload',{});row['contract_checks']=dict(order=[x['kind'] for x in observed.get('events',[])]==expected,owner_state_correct=observed.get('owner_words')==[0]*6 if name!='unload-inactive' else len(observed.get('owner_words',[]))==6 and observed['owner_words'][0]==0 and observed['owner_words'][1]!=0 and observed['owner_words'][2:]==[0,123,456,0x1234],release_after_items=all(not x['head_present'] for x in observed.get('events',[]) if x['kind']==99))
        if not all(row['contract_checks'].values()):row['status']='unqualified'
        rows.append(dict(name=name,**row));print(name,row,flush=True)
    receipt=dict(kind='original unload routine control-flow observations; fixture event-7 and release callbacks; no real destructor/cache or CE comparison',original_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),transport_sha256=hashlib.sha256(Path(__file__).with_name('probe_mohawk_v5_original.py').read_bytes()).hexdigest(),results=rows)
    (a.root/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    raise SystemExit(0 if all(x['status']=='observed' for x in rows) else 1)
