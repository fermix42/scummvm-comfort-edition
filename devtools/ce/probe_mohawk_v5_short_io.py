# SPDX-License-Identifier: GPL-3.0-or-later
"""Original exact-I/O wrappers over owned three-byte files; real native I/O."""
import argparse,hashlib,json,shutil
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from mohawk_v5_native_io import Native,boot
CASES=[('read-exact',3,0,3),('read-short',8,0x283f,3),('read-zero',0,0,0),('read-eof',1,0x283f,0),('read-invalid',3,0x286f,0),('write-exact',3,0,3),('write-readonly',3,0x283c,0)]


def stub(remote,write,output,case):
 n=Native(remote,write,output);boot(n);path=remote+0x2000;text=remote+0x2200;count=remote+0x3200;callback=remote+0x3400;buf=remote+0x3600
 write(text,case['file'].encode('ascii')+b'\0');write(count,u32(case['requested']));write(buf,b'XYZ'+b'\xcc'*13 if case['name'].startswith('write-') else b'\xcc'*16)
 n.call(0x4531df,path,[text]);n.call(0x45529d,args=[path,0x43 if case['name']=='write-exact' else 0x41]);n.save(3)
 write(callback,b'\xff\x05'+u32(n.slots+8*4)+b'\xb8'+u32(1)+b'\xc2\x04\0');n.call(0x453ef3,args=[callback])
 if case['name']=='read-eof':
  n.s+=b'\xc7\x05'+u32(count)+u32(3);n.call(0x45559b,args=[n.mem(3),buf,count]);n.s+=b'\x0f\xb7\xc0';n.save(11)
  n.s+=b'\xa1'+u32(count);n.save(12);n.s+=b'\xc7\x05'+u32(count)+u32(1)
 fn=0x4554b8 if case['name'].startswith('write-') else 0x45559b
 n.call(fn,args=[0 if case['name']=='read-invalid' else n.mem(3),buf,count]);n.s+=b'\x0f\xb7\xc0\xa3'+u32(output)
 n.s+=b'\xa1'+u32(count);n.save(7)
 n.call(0x455369,args=[n.mem(3),1]);n.s+=b'\x0f\xb7\xc0';n.save(9)
 n.s+=b'\xbe'+u32(buf)+b'\xbf'+u32(output+0x6000)+b'\xb9'+u32(8)+b'\xf3\xa4'
 return n.s

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
 a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
 for name,requested,status,count in CASES:
  file=a.root/(name+'.dat');file.write_bytes(b'ABC');case=dict(name=name,requested=requested,file=str(file),expected=status)
  row=run_case(exe,dict(case,output='int32',after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_native_blob_size=8,native_blob_offset=0x6000,observe_vm_error=True),10,stub_factory=stub);row.pop('expected',None);row.pop('matches_static_contract',None);w=row.get('native_words',[]);checks={}
  if row['status']=='observed':
   checks=dict(memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,opened=w[3]!=0,no_vm_error=not row.get('vm_errors'),status=row['actual']==status,actual_count=w[7]==count,no_callback=w[8]==0,closed=w[9]==0,exact_disk=file.read_bytes()==(b'XYZ' if name=='write-exact' else b'ABC'))
   if name in ['read-exact','read-short','read-eof']:checks['exact_read']=bytes.fromhex(row['native_blobs'][0])==b'ABC'+b'\xcc'*5
   if name=='read-eof':checks['first_exact_read']=w[11]==0 and w[12]==3
   if not all(checks.values()):row['status']='unqualified'
  row.update(name=name,file_hex=file.read_bytes().hex(),contract_checks=checks);rows.append(row);print(name,row['status'],row.get('actual'),w[7:10],checks,flush=True)
 dependencies={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ['probe_mohawk_v5_original.py', 'probe_mohawk_v5_native_resources.py', 'mohawk_v5_native_io.py', 'probe_mohawk_v5_short_io.py']}
 files={str(f.relative_to(a.root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in a.root.rglob('*') if f.is_file()}
 (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),dependencies=dependencies,owned_files_before_receipt=files,results=rows),indent=2)+'\n');raise SystemExit(0 if all(r['status']=='observed' for r in rows) else 1)
