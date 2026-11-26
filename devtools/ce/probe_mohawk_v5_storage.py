# SPDX-License-Identifier: GPL-3.0-or-later
"""Owned original datafile/profile write and cold-restart probes."""
import argparse,hashlib,json,shutil
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from mohawk_v5_native_io import Native,boot
from probe_mohawk_v5_components import COMPONENTS

def stub(remote,write,output,case):
 n=Native(remote,write,output);boot(n);vm=remote+0x4000;symbols=remote+0x4800
 n.call(0x41482c,symbols,[1024]);n.call(0x414c47,symbols);n.call(0x401ed0,0x49a540);n.call(0x402eef,vm,[symbols]);n.s+=b'\xc7\x05'+u32(0x49a6a8)+u32(vm)
 obj=remote+0x2800;result=remote+0x3100;lst=remote+0x3200
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
 if case.get('capture_list'):
  n.s+=b'\xa1'+u32(result+8);n.save(26);n.call(0x406130,n.mem(26));n.s+=b'\x0f\xb7\xc0';n.save(27)
  for i in range(1,case.get('list_elements',2)+1):
   n.call(0x416a83,n.mem(26),[i]);n.s+=b'\x89\xc1';n.call(0x40ffa9);n.save(27+i)
 if case.get('destroy'):n.call(0x446491,obj)
 if case.get('clear_profile_cache'):n.call(0x458570)
 n.s+=b'\x0f\xb7\x05'+u32(obj+0x14);n.save(16)
 n.s+=b'\x0f\xb7\x05'+u32(0x49b098);n.save(17)
 n.s+=b'\x0f\xb7\x05'+u32(0x49b158);n.save(18)
 n.call(0x40ffa9,result);n.s+=b'\xa3'+u32(output)
 n.s+=b'\x0f\xb6\x05'+u32(result);n.save(20)
 if case.get('text'):n.s+=b'\xa1'+u32(result+8);n.save(22)
 n.s+=b'\xbe'+u32(result)+b'\xbf'+u32(output+0x6000)+b'\xb9'+u32(16)+b'\xf3\xa4'
 return n.s

def qualify(row,case,file,root):
 row.update(case);raw=file.read_bytes() if file.exists() else None;row['file_hex']=raw.hex() if raw is not None else None
 row['fixture_sha256']=hashlib.sha256(json.dumps(case,sort_keys=True).encode()).hexdigest()
 row['template_sha256']=hashlib.sha256(json.dumps(case,sort_keys=True).replace(str(root).replace('\\','\\\\'),'<owned-root>').encode()).hexdigest()
 checks={};w=row.get('native_words',[]);name=case['name']
 if row['status']=='observed':
  expected_file=b'[Probe]\r\nanswer=42;\r\n'
  if name.startswith('buffered-close') or name in ['buffered-destructor','buffered-cache-cleanup','history-unsaved','history-unsaved-cold']:expected_file=b''
  elif name=='read-only-existing-write':expected_file=b'[Probe]\r\nanswer=99;\r\n'
  elif name=='open-missing-parent':expected_file=None
  elif name in ['history-save','history-cold']:expected_file=b'[Level1]\r\nDeckA=[7,9]\r\n'
  elif name.startswith('string-value'):expected_file=b'[Probe]\r\ntext="a""b";\r\n'
  expected_value=42 if name in ['write-through','write-through-cold','buffered-close','buffered-flush','buffered-flush-cold','buffered-destructor'] else 0
  profile_error=0x296c if name in ['buffered-close-cold','history-unsaved-cold'] else 0
  checks=dict(memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,constructor_vtable=w[19]==COMPONENTS[case['family']][2],no_vm_errors=not row.get('vm_errors'),
              exact_file=raw==expected_file,value=row['actual']==expected_value,filesystem_status=w[17]==(0x2847 if name=='open-missing-parent' else 0),profile_status=w[18]==profile_error)
  if case['family']=='datafile':checks['open_flag']=w[16]==(0 if name in ['read-only-existing-write','open-missing-parent','buffered-destructor','buffered-cache-cleanup'] else 1)
  if case.get('capture_list'):
   expected=[] if name=='history-unsaved-cold' else ([9,7] if name=='history-unsaved' else [7,9])
   checks.update(list_type=w[20]==13,list_count=w[27]==len(expected),list_elements=w[28:28+len(expected)]==expected)
  if case.get('text'):checks.update(string_type=w[20]==7,string_value=row.get('native_text')==['a"b',None])
  if not all(checks.values()):row['status']='unqualified'
 row['contract_checks']=checks
 return row


if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
 a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
 for name,mode in [('write-through',None),('buffered-close',13),('buffered-flush',13)]:
  file=a.root/(name+'.dat');steps=[(1,[str(file)])]
  if mode:steps.append((mode,[]))
  steps.extend([(3,['Probe']),(7,['answer',42])])
  if name=='buffered-flush':steps.append((12,[]))
  steps.extend([(2,[]),(1,[str(file)]),(5,['Probe']),(15,['answer'])])
  case=dict(name=name,family='datafile',steps=steps)
  config=dict(case,output='int32',expected=0,after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_native_text=case.get('text',False),observe_native_blob_size=16,native_blob_offset=0x6000,observe_vm_error=True)
  row=run_case(exe,config,10,stub_factory=stub);row.pop('expected',None);row.pop('matches_static_contract',None);qualify(row,case,file,a.root);rows.append(row)
  print(name,row['status'],row.get('actual'),row.get('native_words',[])[:12],row['file_hex'],flush=True)
  cold=dict(name=name+'-cold',family='datafile',steps=[(1,[str(file)]),(5,['Probe']),(15,['answer'])])
  row=run_case(exe,dict(cold,output='int32',expected=0,after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_vm_error=True),10,stub_factory=stub)
  row.pop('expected',None);row.pop('matches_static_contract',None);qualify(row,cold,file,a.root);rows.append(row)
  print(cold['name'],row['status'],row.get('actual'),row.get('vm_errors'),row['file_hex'],flush=True)
 extras=[
  dict(name='read-only-existing-write',family='datafile',steps=[(18,[str(a.root/'write-through.dat')]),(5,['Probe']),(7,['answer',99]),(2,[])],file='write-through.dat'),
  dict(name='open-missing-parent',family='datafile',steps=[(1,[str(a.root/'missing-parent/Probe.dat')])],file='missing-parent/Probe.dat'),
  dict(name='history-save',family='aaUserHistory',steps=[(1,[str(a.root)+'\\']),(2,['ProbeUser']),(4,[1]),(6,['DeckA']),(11,[7]),(11,[9]),(11,[7]),(10,[]),(8,[])],file='ProbeUser.dat',capture_list=True),
  dict(name='history-cold',family='aaUserHistory',steps=[(1,[str(a.root)+'\\']),(2,['ProbeUser']),(4,[1]),(6,['DeckA']),(9,[]),(8,[])],file='ProbeUser.dat',capture_list=True),
  dict(name='buffered-destructor',family='datafile',steps=[(1,[str(a.root/'destructor.dat')]),(13,[]),(3,['Probe']),(7,['answer',42])],file='destructor.dat',destroy=True),
  dict(name='buffered-cache-cleanup',family='datafile',steps=[(1,[str(a.root/'cleanup.dat')]),(13,[]),(3,['Probe']),(7,['answer',42]),(2,[])],file='cleanup.dat',clear_profile_cache=True),
  dict(name='history-unsaved',family='aaUserHistory',steps=[(1,[str(a.root)+'\\']),(2,['Unsaved']),(4,[1]),(6,['DeckA']),(11,[7]),(11,[9]),(8,[])],file='Unsaved.dat',capture_list=True),
  dict(name='history-unsaved-cold',family='aaUserHistory',steps=[(1,[str(a.root)+'\\']),(2,['Unsaved']),(4,[1]),(6,['DeckA']),(9,[]),(8,[])],file='Unsaved.dat',capture_list=True,list_elements=0),
  dict(name='string-value',family='datafile',steps=[(1,[str(a.root/'string.dat')]),(3,['Probe']),(7,['text','a"b']),(15,['text'])],file='string.dat',text=True),
  dict(name='string-value-cold',family='datafile',steps=[(1,[str(a.root/'string.dat')]),(5,['Probe']),(15,['text'])],file='string.dat',text=True)]
 for case in extras:
  row=run_case(exe,dict(case,output='int32',expected=0,after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_native_text=case.get('text',False),observe_native_blob_size=16,native_blob_offset=0x6000,observe_vm_error=True),10,stub_factory=stub)
  row.pop('expected',None);row.pop('matches_static_contract',None);file=a.root/case['file'];qualify(row,case,file,a.root);rows.append(row)
  print(case['name'],row['status'],row.get('actual'),row.get('native_words',[])[26:30],row['file_hex'],flush=True)
 dependencies={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ['probe_mohawk_v5_storage.py','probe_mohawk_v5_components.py','probe_mohawk_v5_original.py','probe_mohawk_v5_native_resources.py','mohawk_v5_native_io.py']}
 files={str(f.relative_to(a.root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in a.root.rglob('*') if f.is_file()}
 assert set(files)=={'CarmenTQ.exe','write-through.dat','buffered-close.dat','buffered-flush.dat','ProbeUser.dat','destructor.dat','cleanup.dat','Unsaved.dat','string.dat'},files
 (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),dependencies=dependencies,owned_files_before_receipt=files,results=rows),indent=2)+'\n')
 raise SystemExit(0 if all(r['status']=='observed' and r['contract_checks'] and all(r['contract_checks'].values()) for r in rows) else 1)
