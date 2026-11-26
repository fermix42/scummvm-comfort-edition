# SPDX-License-Identifier: GPL-3.0-or-later
"""Original filesystem error mapping and callback policy controls; no UI claim."""
import argparse,hashlib,json,shutil,struct
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from mohawk_v5_native_io import Native,boot
MAP={2:0x2845,3:0x2847,5:0x283c,19:0x284d,21:0x284c,32:0x2848,33:0x2848,38:0x283f,39:0x2849,112:0x2849,80:0x283e,183:0x283e,12345:0x2846,34:0x284c,107:0x284c,108:0x284a,15:0x2870,16:0x283d}

def cases():
 rows=[dict(name='map-'+str(raw),mode='map',raw=raw,expected=out) for raw,out in MAP.items()]
 for name,callback,policy,suppressed,result,count in [('no-callback-stop',None,1,0,0,0),('no-callback-continue',None,0,0,1,0),('callback-stop',0,1,0,0,1),('callback-continue',1,1,0,1,1),('policy-continue',0,0,0,1,1),('suppressed',1,0,-1,0,0)]:
  rows.append(dict(name=name,mode='adapter',callback=callback,policy=policy,suppressed=suppressed,expected=result,expected_count=count))
 for raw in [2,3,5,19,21,32,38,112,183,34,108,12345]:
  rows.append(dict(name='category-'+str(raw),mode='category',raw=raw,expected=1 if raw in [19,21,34,108,12345] else 0,expected_count=1 if raw in [19,21,34,108,12345] else 0))
 rows.append(dict(name='application-suppressed',mode='application',expected=0,expected_count=0))
 return rows


def stub(remote,write,output,case):
 n=Native(remote,write,output);boot(n);data=remote+0x3000;callback=remote+0x3200
 write(data,struct.pack('<5I',1,0,0,0,0));write(remote+0x3400,b'Probe\0')
 if case['mode']=='map':n.call(0x454991,args=[case['raw']],cdecl=True)
 elif case['mode']=='application':
  write(0x49ac38,b'\x01\0');n.call(0x42db7e,args=[data])
 else:
  ret=case.get('callback',1);count=n.slots+7*4
  code=b'\xff\x05'+u32(count)+b'\x8b\x44\x24\x04\x8b\x10\x89\x15'+u32(n.slots+22*4)+b'\x0f\xb7\x50\x04\x89\x15'+u32(n.slots+24*4)+b'\xb8'+u32(ret or 0)+b'\xc2\x04\0'
  write(callback,code);n.call(0x453ef3,args=[0 if case.get('callback',1) is None else callback]);n.save(6)
  n.s+=b'\x66\xc7\x05'+u32(0x49b0be)+struct.pack('<h',case.get('policy',1))+b'\x66\xc7\x05'+u32(0x49b0bc)+struct.pack('<h',case.get('suppressed',0))
  if case['mode']=='category':n.call(0x454102,args=[remote+0x3400,case['raw']],cdecl=True)
  else:n.call(0x4548ef,args=[data],cdecl=True)
 n.s+=b'\x0f\xb7\xc0\xa3'+u32(output)
 n.s+=b'\x0f\xb7\x05'+u32(0x49b098);n.save(8)
 return n.s

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
 a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
 for case in cases():
  row=run_case(exe,dict(case,output='int32',after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_vm_error=True),10,stub_factory=stub);row.pop('expected',None);row.pop('matches_static_contract',None);row['name']=case['name'];w=row.get('native_words',[]);checks={}
  if row['status']=='observed':
   checks=dict(value=row['actual']==case['expected'],memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,no_vm_error=not row.get('vm_errors'))
   if case['mode']!='map':checks['callback_count']=w[7]==case['expected_count']
   if case['mode']=='adapter' and w[7]:checks.update(category=w[22]==1,passed_policy=w[24]==case['policy'])
   if case['mode']=='category':
    checks['native_status']=w[8]==(0 if case['expected'] else MAP[case['raw']])
    if w[7]:checks.update(category=w[22]=={19:4,21:3,34:3,108:2,12345:1}[case['raw']],passed_policy=w[24]==1)
   if not all(checks.values()):row['status']='unqualified'
  row['contract_checks']=checks;rows.append(row);print(case['name'],row['status'],row.get('actual'),w[7:9],flush=True)
 dependencies={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ['probe_mohawk_v5_os_errors.py','mohawk_v5_native_io.py','probe_mohawk_v5_native_resources.py','probe_mohawk_v5_original.py']}
 assert sorted(f.name for f in a.root.iterdir())==['CarmenTQ.exe']
 (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),dependencies=dependencies,results=rows),indent=2)+'\n')
 raise SystemExit(0 if all(r['status']=='observed' for r in rows) else 1)
