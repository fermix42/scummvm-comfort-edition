# SPDX-License-Identifier: GPL-3.0-or-later
"""Extended packed geometry and operator original/CE observations."""
import argparse,hashlib,json,struct
from pathlib import Path
import probe_mohawk_v5_vm as vm
from probe_mohawk_v5_original import run_case
from test_mohawk_v5_startup import Code,integer

def plans():
 rows={};eq=b'\x6c\x16';op=lambda n:bytes([0x6e,n]);cmd=lambda n,argc:bytes([0x4d,n,argc])
 def add(name,build,expected=1):
  c=Code();c.code=bytearray(8);expr=build(c);offset=c.expr(expr)
  rows[name]=dict(code=c,expression=expr,expected=expected,offset=offset,offsets=[])
 for label,text,command,answer in [('pair-x','12,-7',17,12),('pair-y','12,-7',18,-7),('spaces',' 12 , -7 ',17,12),('trailing','12,-7 junk',17,0),('missing-coordinate','12,',17,0),('malformed','bad',17,0),('short-overflow','32768,2',17,-32768)]:
  add('point-string-'+label,lambda c,t=text,k=command,v=answer:c.string(t)+cmd(k,1)+integer(v)+eq)
 for label,build,answer in [('numeric-repeat',lambda c:integer(7),7),('constructor-x',lambda c:integer(12)+integer(-7)+cmd(9,2),12)]:
  add('point-'+label,lambda c,f=build,v=answer:f(c)+cmd(17,1)+integer(v)+eq)
 def point(x,y):return integer(x)+integer(y)+cmd(9,2)
 for label,text1,text2,command,answer in [('normalized-x','9,8','2,1',17,2),('normalized-y','9,8','2,1',18,1),('trailing-zero','9,8 junk','2,1',17,0),('malformed-zero','bad','2,1',18,0)]:
  add('rect-two-string-points-'+label,lambda c,t=text1,u=text2,k=command,v=answer:c.string(t)+c.string(u)+cmd(8,2)+cmd(k,1)+integer(v)+eq)
 add('rect-two-points-normalized',lambda c:point(9,8)+point(2,1)+cmd(8,2)+cmd(17,1)+integer(2)+eq)
 add('rect-four-numbers-preserved',lambda c:integer(9)+integer(8)+integer(2)+integer(1)+cmd(8,4)+cmd(17,1)+integer(9)+eq)
 add('rect-false-control',lambda c:point(9,8)+point(2,1)+cmd(8,2)+cmd(17,1)+integer(99)+eq,0)
 add('rect-point-coordinate-copy',lambda c:integer(1)+integer(2)+integer(3)+integer(4)+cmd(8,4)+cmd(18,1)+integer(2)+eq)
 for command,label in [(13,'top'),(14,'left'),(15,'bottom'),(16,'right')]:
  add('error-command-'+label,lambda c,k=command:c.string('9,8,2,1')+cmd(k,1))
 for label,a,b,operator,answer in [('idiv-positive',7,2,0x32,3),('idiv-negative',-7,2,0x32,-3),('remainder-positive',7,2,0x34,1),('remainder-negative',-7,2,0x34,-1),('subtract',7,12,0x11,-5),('integer-boundary-add',2147483647,1,0x14,2147483648)]:
  add('operator-'+label,lambda c,x=a,y=b,k=operator,v=answer:integer(x)+integer(y)+op(k)+integer(v if v<2147483648 else -2147483648)+eq,0 if answer==2147483648 else 1)
 for label,expr in [('fractional-idiv',integer(7)+b'\x05\x04'+struct.pack('>I',5000)+op(0x32)),('zero-remainder',integer(7)+integer(0)+op(0x34)),('signed-overflow',integer(-2147483648)+integer(-1)+op(0x32))]:
  add('processor-'+label,lambda c,e=expr:e)
 return rows

def probes():return [(name,p['expression'],p['expected']) for name,p in plans().items()]

def transport(exe,case,*args,**kwargs):
 if case['name'].startswith('processor-'):case['observe_native_words']=True
 return run_case(exe,case,*args,**kwargs)

class BoundedClient(vm.Client):
 def command(self,cmd,*,check=True,timeout=8,**fields):
  return super().command(cmd,check=check,timeout=min(timeout,8),**fields)


if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['original','ce']);p.add_argument('--original',type=Path);p.add_argument('--ce',type=Path);p.add_argument('--extras',type=Path);p.add_argument('--game-data',type=Path);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
 vm.probes=probes;vm.symbol_plans=plans;vm.run_case=transport;vm.Client=BoundedClient
 (vm.original if a.mode=='original' else vm.ce)(a)
 target=a.root/'receipt.json';j=json.loads(target.read_text());j['value_harness_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
 for row in j['results']:
  name=row['name'];checks={}
  if name.startswith('processor-'):
   row['observation_kind']='error-completion'
   if a.mode=='original':
    code='0xc0000095' if name.endswith('overflow') else '0xc0000094';address='0x404354' if name.endswith('remainder') else '0x404316'
    checks=dict(native_fault=row.get('status')=='exception' and row.get('exception')==code and row.get('address')==address,
                event_eip=row.get('fault_context',{}).get('registers',[None]*8)[7]==int(address,16))
    if all(checks.values()):row.update(status='observed',actual=dict(completion='native-processor-fault',exception=code,address=address))
   else:
    operator='34' if name.endswith('remainder') else '32';checks['unsupported_operation']=row.get('status')=='engine_error' and ('Unsupported V5 binary operation '+operator) in '\n'.join(row.get('log_tail',[]))
    if all(checks.values()):row.update(status='observed',actual=dict(completion='engine-abort',diagnostic='Unsupported V5 binary operation '+operator))
  elif name.startswith('error-command-'):
   row['observation_kind']='error-completion'
   if a.mode=='original':
    checks['typed_unsupported_error']=row.get('status')=='observed' and row.get('vm_errors')==[2]
    row['actual']=dict(completion='returned',vm_errors=row.get('vm_errors'),value=row.get('returned_numeric'))
   else:
    checks['returned_without_vm_error']=row.get('status')=='observed'
    row['actual']=dict(completion='returned',vm_errors=[],value=row.get('actual'))
  else:
   checks=dict(observed=row.get('status')=='observed',no_vm_error=not row.get('vm_errors'))
  row['contract_checks']=checks
  if row.get('status')=='observed' and not all(checks.values()):row['status']='unqualified'
 target.write_text(json.dumps(j,indent=2)+'\n')
 raise SystemExit(0 if all(all(r['contract_checks'].values()) for r in j['results']) else 1)
