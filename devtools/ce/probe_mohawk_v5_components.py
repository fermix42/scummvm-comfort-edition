# SPDX-License-Identifier: GPL-3.0-or-later
"""Isolated original native component dispatch probes; no normal app startup."""
import argparse,hashlib,json,shutil
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from mohawk_v5_native_io import Native,boot

COMPONENTS={
 'aaDatabase':(0x43aded,0x43b06c,0x488bb8),
 'aaDirectory':(0x43fa3a,0x43feca,0x488bc4),
 'aaQSelector':(0x4404ba,0x4406f1,0x488bd4),
 'aaUserHistory':(0x44538a,0x4455bd,0x488bf8),
 'aaStringLib':(0x444dca,0x444eb3,0x488bec),
 'datafile':(0x4463da,0x44652b,0x488c04),
 'encryptor':(0x447dba,0x447e73,0x488c10),
 'font':(0x44873a,0x448897,0x488c1c),
 'clock':(0x44943a,0x449523,0x488c34)}

def cases():
 rows=[]
 for family in COMPONENTS:
  for method,label in [(-3,'refcount'),(-4,'component-id'),(999,'unknown-positive')]:
   rows.append(dict(name=family+'-'+label,family=family,method=method,arguments=[]))
 for method,label,args in [(1,'upper',['aBc 19!']),(2,'lower',['aBc 19!']),(1,'wrong-arity',[])]:
  rows.append(dict(name='string-'+label,family='aaStringLib',method=method,arguments=args,text=bool(args)))
 for method,label in [(2,'default-key'),(999,'unknown')]:rows.append(dict(name='encryptor-'+label,family='encryptor',method=method,arguments=[]))
 rows.append(dict(name='font-default',family='font',method=2,arguments=[]))
 for method in [1,4,5,6,7,8,9]:rows.append(dict(name='clock-field-'+str(method),family='clock',method=method,arguments=[]))
 rows.extend([
  dict(name='database-owned-paths',family='aaDatabase',steps=[(11,['@root','@root'])]),
  dict(name='directory-owned-path',family='aaDirectory',steps=[(1,['@root'])]),
  dict(name='selector-location-count',family='aaQSelector',steps=[(3,[4])]),
  dict(name='history-level',family='aaUserHistory',steps=[(4,[3]),(5,[])]),
  dict(name='datafile-close-empty',family='datafile',steps=[(2,[])]),
  dict(name='datafile-open-wrong-arity',family='datafile',steps=[(1,[])]),
  dict(name='encryptor-key',family='encryptor',steps=[(1,[123]),(2,[])]),
  dict(name='encryptor-roundtrip',family='encryptor',steps=[(1,[123]),(3,[123456]),(4,[{'previous':True}])]),
  dict(name='font-set-wrong-arity',family='font',steps=[(1,[])])])
 return rows


def stub(remote,write,output,case):
 n=Native(remote,write,output);boot(n);obj=remote+0x2800;result=remote+0x3100;lst=remote+0x3200
 ctor,dispatch,vt=COMPONENTS[case['family']]
 n.call(ctor,obj,[0,17]);n.s+=b'\xa1'+u32(obj);n.save(19)
 n.call(0x401ed0,result)
 for step,(method,arguments) in enumerate(case.get('steps',[(case.get('method'),case.get('arguments',[]))])):
  n.call(0x4162b3,lst,[0x80])
  for i,arg in enumerate(arguments):
   cell=remote+0x3400+i*0x20;n.call(0x401ed0,cell)
   if isinstance(arg,dict):n.call(0x401f60,cell,[result])
   elif isinstance(arg,str):
    if arg=='@root':arg=case['owned_path']
    text=remote+0x8000+step*0x400+i*0x100;ss=remote+0x3700+i*8;write(text,arg.encode('ascii')+b'\0')
    n.call(0x4497cf,ss,[text,0xffff]);n.call(0x40ee1c,cell,[ss])
   else:n.call(0x405b10,cell,[arg])
   n.call(0x4166ad,lst,[cell])
  n.call(0x401f00,result);n.call(0x401ed0,result)
  n.call(dispatch,obj,[result,method,lst]);n.call(0x40ffa9,result);n.save(2+step)
  n.call(0x4165c3,lst)
 n.call(0x40ffa9,result);n.s+=b'\xa3'+u32(output)
 n.s+=b'\x0f\xb6\x05'+u32(result);n.save(20)
 if case.get('text'):n.s+=b'\xa1'+u32(result+8);n.save(22)
 n.s+=b'\xbe'+u32(result)+b'\xbf'+u32(output+0x6000)+b'\xb9'+u32(16)+b'\xf3\xa4'
 return n.s

def qualify(case,row):
 w=row.get('native_words',[])
 if row['status']!='observed':return {}
 name=case['name'];method=case.get('method');family=case['family']
 checks=dict(memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,
             constructor_vtable=w[19]==COMPONENTS[family][2],no_vm_errors=not row.get('vm_errors'))
 if name.startswith('clock-field-'):
  lo,hi={1:(0,60),4:(1,31),5:(1,12),6:(1900,9999),7:(1,7),8:(1,366),9:(-1,1)}[method]
  checks.update(calendar_range=lo<=row['actual']<=hi,result_tag=w[20]==3)
 elif case.get('text'):
  expected='ABC 19!' if name=='string-upper' else 'abc 19!'
  checks.update(result_tag=w[20]==7,text=row.get('native_text')==[expected,None])
 else:
  expected=17 if method==-4 else (-1 if method==999 and family=='clock' else 0)
  expected={'selector-location-count':4,'history-level':3,'datafile-open-wrong-arity':-1,'encryptor-key':123,'encryptor-roundtrip':123456}.get(name,expected)
  checks.update(value=row['actual']==expected,result_tag=w[20] in [2,3])
  if name=='history-level':checks['set_and_get']=w[2]==w[3]==3
  if name=='encryptor-key':checks['set_and_get']=w[2]==w[3]==123
  if name=='encryptor-roundtrip':checks['nonidentity_ciphertext']=w[3]!=123456 and w[4]==123456
 return checks


if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
 a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
 for case in cases():
  fixture_hash=hashlib.sha256(json.dumps(case,sort_keys=True).encode()).hexdigest()
  case['owned_path']=str(a.root)+'\\'
  config=dict(case,output='int32',expected=0,after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_native_text=case.get('text',False),observe_native_blob_size=16,native_blob_offset=0x6000,observe_vm_error=True)
  row=run_case(exe,config,10,stub_factory=stub);row.pop('expected',None);row.pop('matches_static_contract',None);row.update(case);checks=qualify(case,row);row['contract_checks']=checks;row['fixture_sha256']=fixture_hash
  if row['status']=='observed' and not all(checks.values()):row['status']='unqualified'
  rows.append(row)
  print(case['name'],row['status'],row.get('actual'),row.get('native_text'),row.get('native_words',[])[19:21],flush=True)
 dependencies={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ['probe_mohawk_v5_components.py','probe_mohawk_v5_original.py','mohawk_v5_native_io.py','probe_mohawk_v5_native_resources.py']}
 files=sorted(str(f.relative_to(a.root)) for f in a.root.rglob('*') if f.is_file());assert files==['CarmenTQ.exe'],files
 (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),dependencies=dependencies,owned_files_before_receipt=files,results=rows),indent=2)+'\n')
 raise SystemExit(0 if all(x['status']=='observed' for x in rows) else 1)
