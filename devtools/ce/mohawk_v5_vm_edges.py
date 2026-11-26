# SPDX-License-Identifier: GPL-3.0-or-later
"""Original/CE list, unary, key and masked nonfinite comparison probes."""
import argparse,hashlib,json,math,struct
from pathlib import Path
import probe_mohawk_v5_vm as vm
from probe_mohawk_v5_original import run_case
from mohawk_v5_value_probes import BoundedClient
from test_mohawk_v5_startup import Code,integer
BASE_STUB=vm.stub


def plans():
 rows={};eq=b'\x6c\x16';assign=b'\x6b\x22';cmd=lambda n,k:bytes([0x4d,n,k])
 def add(name,build,pre=None,expected=1):
  c=Code();c.code=bytearray(8);preludes=pre(c) if pre else [];offsets=[c.expr(e) for e in preludes];expr=build(c);offset=c.expr(expr)
  rows[name]=dict(code=c,expression=expr,expected=expected,offset=offset,offsets=offsets,preludes=preludes)
 ref=lambda c:c.string('array',True);idx=lambda c,n:ref(c)+integer(n)+b'\x1b'
 add('list-autocreate',lambda c:idx(c,2)+integer(42)+eq,lambda c:[idx(c,2)+integer(42)+assign])
 add('list-replaces-scalar',lambda c:idx(c,2)+integer(42)+eq,lambda c:[ref(c)+integer(7)+assign,idx(c,2)+integer(42)+assign])
 add('list-short-index-wrap',lambda c:idx(c,1)+integer(42)+eq,lambda c:[idx(c,65537)+integer(42)+assign])
 add('list-literal',lambda c:idx(c,2)+integer(20)+eq,lambda c:[ref(c)+b'\x1a'+integer(10)+b'\x07'+integer(20)+b'\x07\x1b'+assign])
 for label,answer in [('copy',42),('alias-control',99)]:
  add('list-nested-'+label,lambda c,n=answer:c.string('outer',True)+integer(1)+b'\x1b'+integer(n)+eq,lambda c:[idx(c,1)+integer(42)+assign,c.string('outer',True)+b'\x1a'+idx(c,1)+b'\x07'+integer(0)+b'\x07\x1b'+assign,idx(c,1)+integer(99)+assign],0 if answer==99 else 1)
 for operator,label,expected in [(0x15,'post-increment',41),(0x69,'pre-increment',42),(0x12,'post-decrement',41),(0x68,'pre-decrement',40)]:
  add('unary-'+label,lambda c,k=operator,n=expected:c.string('counter',True)+bytes([0x6f,k])+integer(n)+eq,lambda c:[c.string('counter',True)+integer(41)+assign])
 add('unary-increment-mutates',lambda c:c.string('counter',True)+integer(42)+eq,lambda c:[c.string('counter',True)+integer(41)+assign,c.string('counter',True)+b'\x6f\x15'])
 add('error-edge-unary-reference',lambda c:integer(41)+b'\x6f\x15')
 add('error-edge-nonlist',lambda c:integer(7)+integer(1)+b'\x1b')
 for n,label in [(0,'zero'),(-1,'negative')]:add('edge-index-'+label,lambda c,k=n:idx(c,k)+integer(0)+eq)
 key=lambda c,text:c.string(text)+cmd(0x52,1)
 add('key-character-number',lambda c:key(c,'a')+integer(0)+b'\x6e\x14'+integer(97)+eq)
 add('key-virtual-number',lambda c:integer(65)+cmd(0x52,1)+integer(0)+b'\x6e\x14'+integer(65)+eq)
 add('key-equal',lambda c:key(c,'a')+key(c,'a')+eq)
 add('key-false-control',lambda c:key(c,'a')+key(c,'b')+eq,expected=0)
 add('key-modified-character',lambda c:c.string('a')+integer(2)+cmd(0x52,2)+integer(0)+b'\x6e\x14'+integer(97)+eq)
 def nonfinite(c):
  inf=lambda:c.string('1e309')+integer(0)+b'\x6e\x14'
  return inf()+inf()+b'\x6e\x11'
 for operator,label in [(0x16,'equal'),(0x25,'not-equal'),(0x1d,'less'),(0x23,'less-equal'),(0x1e,'greater'),(0x24,'greater-equal')]:add('masked-nan-'+label,lambda c,k=operator:nonfinite(c)+integer(0)+bytes([0x6c,k]),expected={0x16:1,0x25:0,0x1d:0,0x23:0,0x1e:1,0x24:1}[operator])
 add('error-edge-nan-divisor',lambda c:integer(7)+nonfinite(c)+b'\x6e\x21')
 numeric=lambda c,t:c.string(t)+integer(0)+b'\x6e\x14'
 fraction=lambda c:[c.string('counter',True)+c.string('1.5')+assign]
 add('unary-fractional-prefix',lambda c:c.string('counter',True)+b'\x6f\x69'+numeric(c,'2.5')+eq,fraction)
 add('unary-fractional-post-mutation',lambda c:c.string('counter',True)+numeric(c,'2.5')+eq,lambda c:fraction(c)+[c.string('counter',True)+b'\x6f\x15'])
 add('compound-fractional-add',lambda c:c.string('counter',True)+c.string('0.5')+b'\x6b\x13'+integer(2)+eq,fraction)
 add('compound-fractional-multiply',lambda c:c.string('counter',True)+integer(2)+b'\x6b\x18'+integer(3)+eq,fraction)
 add('error-edge-compound-zero',lambda c:c.string('counter',True)+integer(0)+b'\x6b\x19',fraction)
 add('error-edge-zero-index-write',lambda c:idx(c,0)+integer(42)+assign)
 return rows


def probes():return [(n,p['expression'],p['expected']) for n,p in plans().items()]

def stub(remote,write,output,case):
 write(0x49a678,struct.pack('<I',remote+0x1b00))  # Empty original page lookup shell; missing item ID must resolve to null, not fault.
 s=bytearray(BASE_STUB(remote,write,output,case));s+=b'\xd9\x3d'+struct.pack('<I',output+64)+b'\xdd\x3d'+struct.pack('<I',output+66)
 return s


def transport(exe,case,*args,**kwargs):
 case.update(observe_native_words=True,observe_native_blob_size=4,native_blob_offset=64);kwargs['stub_factory']=stub
 return run_case(exe,case,*args,**kwargs)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['original','ce']);p.add_argument('--original',type=Path);p.add_argument('--ce',type=Path);p.add_argument('--extras',type=Path);p.add_argument('--game-data',type=Path);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 vm.probes=probes;vm.symbol_plans=plans;vm.run_case=transport;vm.Client=BoundedClient
 (vm.original if a.mode=='original' else vm.ce)(a)
 target=a.root/'receipt.json';j=json.loads(target.read_text());j['edge_harness_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
 for row in j['results']:
  name=row['name'];checks={}
  if name.startswith('error-edge-'):
   row['observation_kind']='error-completion'
   if a.mode=='original':
    error={'error-edge-unary-reference':14,'error-edge-nonlist':15,'error-edge-nan-divisor':4,'error-edge-compound-zero':4,'error-edge-zero-index-write':14}[name]
    checks['typed_error']=row.get('status')=='observed' and row.get('vm_errors')==[error]
    row['actual']=dict(completion='returned',vm_errors=row.get('vm_errors'))
   elif row.get('status')=='observed':
    checks['returned_without_vm_error']=not row.get('vm_errors');row['actual']=dict(completion='returned',vm_errors=[])
   else:
    diag={'error-edge-unary-reference':'Unsupported V5 unary reference operation 15','error-edge-compound-zero':'Unsupported V5 assignment 19','error-edge-zero-index-write':'Invalid V5 list index'}.get(name,'')
    checks['exact_abort']=bool(diag) and row.get('status')=='engine_error' and diag in '\n'.join(row.get('log_tail',[]))
    if checks['exact_abort']:row.update(status='observed',actual=dict(completion='engine-abort',diagnostic=diag))
  else:checks.update(observed=row.get('status')=='observed',no_vm_error=not row.get('vm_errors'))
  if a.mode=='original' and row.get('status')=='observed':
   control,status=struct.unpack('<HH',bytes.fromhex(row['native_blobs'][0]));row['x87']=dict(control=control,status=status);checks['controlled_fp_mask']=control==0x37f
   if name.startswith('masked-nan-') or name=='error-edge-nan-divisor':checks['masked_invalid_recorded']=bool(status&1)
  row['contract_checks']=checks
  if row.get('status')=='observed' and not all(checks.values()):row['status']='unqualified'
 j['dependencies_lf_sha256']={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes().replace(b'\r\n',b'\n')).hexdigest() for name in ['mohawk_v5_vm_edges.py','mohawk_v5_value_probes.py','probe_mohawk_v5_vm.py','probe_mohawk_v5_original.py','inspect_mohawk_v5_reference.py','test_mohawk_v5_startup.py','test_cli_client.py']}
 target.write_text(json.dumps(j,indent=2)+'\n');raise SystemExit(0 if all(r['status']=='observed' for r in j['results']) else 1)
