# SPDX-License-Identifier: GPL-3.0-or-later
"""Original operation retry loop with explicit operation/callback fixtures; no hardware failure claim."""
import argparse,hashlib,json,shutil,struct
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from mohawk_v5_native_io import Native,boot


def cases():
 return [dict(name=name,raw=raw,failures=failures,decision=decision,policy=policy,suppressed=suppressed,attempts=attempts,callbacks=callbacks,expected=status) for name,raw,failures,decision,policy,suppressed,attempts,callbacks,status in [
 ('retry-then-success',21,2,1,1,0,3,2,0),('cancel-first',21,None,0,1,0,1,1,0x284c),('retry-then-cancel',21,None,'once',1,0,2,2,0x284c),('no-callback',21,None,None,1,0,1,0,0x284c),('suppressed',21,None,1,1,-1,1,0,0x284c),('non-retryable',5,None,1,1,0,1,0,0x283c),('success-first',0,0,1,1,0,1,0,0),('policy-retry',21,2,0,0,0,3,2,0)]]


def stub(remote,write,output,case):
 n=Native(remote,write,output);boot(n);obj=remote+0x3000;vt=remote+0x3100;fn=remote+0x3200;callback=remote+0x3400;path=remote+0x3600
 write(path,b'\\\\ProbeServer\\ProbeShare\\Probe.dat\0');write(vt,u32(fn));n.call(0x4563d1,obj);n.s+=b'\xc7\x05'+u32(obj)+u32(vt)
 count=n.slots+7*4;cb_count=n.slots+8*4
 op=b'\xff\x05'+u32(count)+b'\xa1'+u32(count)+b'\xba'+u32(case['raw'])
 if case['failures'] is not None:op+=b'\x83\xf8'+bytes([case['failures']])+b'\x76\x02\x31\xd2'
 op+=b'\x83\xf8\x08\x76\x0f\xba'+u32(5)+b'\xc7\x05'+u32(n.slots+31*4)+u32(1)+b'\x89\x51\x0c\x31\xc0\xc3';write(fn,op)
 cb=b'\xff\x05'+u32(cb_count)+b'\x8b\x54\x24\x04\x8b\x02\xa3'+u32(n.slots+22*4)+b'\x0f\xb7\x42\x04\xa3'+u32(n.slots+24*4)
 if case['decision']=='once':cb+=b'\xa1'+u32(cb_count)+b'\x83\xf8\x01\x0f\x96\xc0\x0f\xb6\xc0'
 else:cb+=b'\xb8'+u32(case['decision'] or 0)
 cb+=b'\xc2\x04\0';write(callback,cb);n.call(0x453ef3,args=[0 if case['decision'] is None else callback])
 if case.get('real_ui'):
  manager=remote+0x4000;title=remote+0x4400;write(title,b'ThinkQuick original comparison probe\0')
  n.call(0x4497cf,manager+4,[title,0xffff]);n.s+=b'\xc7\x05'+u32(0x49afb0)+u32(manager);n.call(0x453ef3,args=[0x42db7e])
 n.s+=b'\x66\xc7\x05'+u32(0x49b0be)+struct.pack('<h',case['policy'])+b'\x66\xc7\x05'+u32(0x49b0bc)+struct.pack('<h',case['suppressed'])
 n.s+=b'\xa1'+u32(0x49b0ac);n.save(25)
 n.call(0x456488,obj,[path]);n.s+=b'\x0f\xb7\xc0\xa3'+u32(output)
 n.s+=b'\xa1'+u32(obj+0xc);n.save(10)
 return n.s

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
 a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
 for case in cases():
  row=run_case(exe,dict(case,output='int32',after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_vm_error=True),10,stub_factory=stub);row.pop('expected',None);row.pop('matches_static_contract',None);w=row.get('native_words',[]);checks={}
  if row['status']=='observed':
   checks=dict(memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,no_vm_error=not row.get('vm_errors'),status=row['actual']==case['expected'],attempts=w[7]==case['attempts'],callbacks=w[8]==case['callbacks'],bounded=w[31]==0,final_raw_status=w[10]==(0 if case['expected']==0 else case['raw']))
   if w[8]:checks.update(category=w[22]==3,passed_policy=w[24]==case['policy'])
   if not all(checks.values()):row['status']='unqualified'
  row.update(name=case['name'],contract_checks=checks);rows.append(row);print(case['name'],row['status'],row.get('actual'),w[7:11],checks,flush=True)
 dependencies={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ['probe_mohawk_v5_retry.py','probe_mohawk_v5_original.py','mohawk_v5_native_io.py','probe_mohawk_v5_native_resources.py']}
 assert sorted(f.name for f in a.root.iterdir())==['CarmenTQ.exe']
 (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),dependencies=dependencies,results=rows),indent=2)+'\n');raise SystemExit(0 if all(r['status']=='observed' for r in rows) else 1)
