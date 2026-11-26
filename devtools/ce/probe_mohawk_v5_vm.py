# SPDX-License-Identifier: GPL-3.0-or-later
"""Initialized evaluator and CE assertion probes. No engine modifications."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import time
from probe_mohawk_v5_original import run_case, u32
from inspect_mohawk_v5_reference import inspect
from test_mohawk_v5_startup import Code, archive, field, event, subentry, integer, u16
from test_cli_client import Client


def probes():
    real=lambda n:b'\x05\x04'+struct.pack('>I',round(n*10000))
    equal=b'\x6c\x16'
    return [
        ('integer-add',integer(6)+integer(7)+b'\x6e\x14'+integer(13)+equal,1),
        ('integer-multiply',integer(6)+integer(7)+b'\x6e\x0e'+integer(42)+equal,1),
        ('negative-control',integer(6)+integer(7)+b'\x6e\x14'+integer(99)+equal,0),
        ('fixed-point-add',real(1.5)+real(.5)+b'\x6e\x14'+integer(2)+equal,1),
        ('fractional-or',real(.5)+integer(0)+b'\x6d\x37'+integer(1)+equal,1),
        ('fractional-not',real(.5)+b'\x6d\x35'+integer(0)+equal,1),
    ] + [(name,p['expression'],p['expected']) for name,p in symbol_plans().items()]


def symbol_plans():
    plans = {}
    for name in ['string-equal','string-false','string-concat','numeric-string','numeric-string-space','numeric-string-exponent','numeric-string-invalid','reference-write','copy-value','compound-reference','call-stringLen','call-abs','call-min','error-divide-zero','error-non-reference','error-call-arity']:
        c=Code();c.code=bytearray(8);r=lambda n:c.string(n,True);t=lambda n:c.string(n)
        assign=b'\x6b\x22';eq=b'\x6c\x16';pre=[];expected=1
        if name=='string-equal':expr=t('alpha')+t('alpha')+eq
        elif name=='string-false':expr=t('alpha')+t('beta')+eq;expected=0
        elif name=='string-concat':expr=t('ab')+t('cd')+b'\x6e\x0b'+t('abcd')+eq
        elif name.startswith('numeric-string'):
            text={'numeric-string':'02','numeric-string-space':'  02  ','numeric-string-exponent':'2e0','numeric-string-invalid':'2x'}[name]
            expr=t(text)+integer(2)+eq;expected=0 if name=='numeric-string-invalid' else 1
        elif name=='reference-write':pre=[r('counter')+integer(41)+assign];expr=r('counter')+integer(1)+b'\x6e\x14'+integer(42)+eq
        elif name=='copy-value':pre=[r('a')+integer(1)+assign,r('b')+r('a')+assign,r('a')+integer(2)+assign];expr=r('b')+integer(1)+eq
        elif name=='compound-reference':pre=[r('counter')+integer(41)+assign,r('counter')+integer(1)+b'\x6b\x13'];expr=r('counter')+integer(42)+eq
        elif name=='call-stringLen':expr=t('abc')+b'\x4d\x03\x01'+integer(3)+eq
        elif name=='call-abs':expr=integer(-7)+b'\x4d\x07\x01'+integer(7)+eq
        elif name=='call-min':expr=integer(5)+integer(3)+b'\x4d\x06\x02'+integer(3)+eq
        elif name=='error-divide-zero':expr=integer(1)+integer(0)+b'\x6e\x21';expected=0
        elif name=='error-non-reference':expr=integer(1)+integer(2)+assign;expected=0
        else:expr=b'\x4d\x03\x00';expected=0
        offsets=[c.expr(x) for x in pre];offset=c.expr(expr)
        plans[name]=dict(expression=expr,expected=expected,preludes=pre,offsets=offsets,offset=offset,code=c)
    from mohawk_v5_call_probes import plans as call_plans
    plans.update(call_plans())
    return plans


def resource(expression):
    c=Code();c.code=bytearray(8);offset=c.expr(expression)
    return c.resource(),offset


def stub(remote, write, output, case):
    vm=remote+0x1300;data=remote+0x1500
    plan=symbol_plans().get(case['name'])
    if plan and 'item_fields' in plan:
        from mohawk_v5_call_probes import stub as call_stub
        return call_stub(remote,write,output,case,plan)
    if plan:
        payload=bytearray(plan['code'].resource());offset=plan['offset']
        struct.pack_into('>I',payload,8,len(plan['code'].code))
    else:payload,offset=resource(bytes.fromhex(case['expression_hex']))
    write(data,payload)
    s=bytearray(b'\xdb\xe3')
    def call(address,receiver=None,args=(),cdecl=False):
        nonlocal s
        for a in reversed(args):s+=b'\x68'+u32(a)
        if receiver is not None:s+=b'\xb9'+u32(receiver)
        s+=b'\xb8'+u32(address)+b'\xff\xd0'
        if cdecl and args:s+=b'\x83\xc4'+bytes([len(args)*4])
    call(0x47cd44,args=[1],cdecl=True)  # Original CRT heap initialization.
    s+=b'\xa3'+u32(output+8)
    call(0x47a9ec if plan else 0x47b977)  # CRT TLS/thread state includes static locks.
    if plan:
        s+=b'\xa3'+u32(output+20)
        call(0x477810);s+=b'\xdb\xe3'  # Register original CRT floating parsers; controlled x87 state.
    call(0x401ed0,0x49a540)  # Result value constructor.
    symbols=remote+0x1800
    if plan:
        call(0x41482c,symbols,[1024]);call(0x414c47,symbols)
        s+=b'\xa3'+u32(output+16)
    call(0x402eef,vm,[symbols if plan else 0])  # Original evaluator constructor: creates value stack.
    s+=b'\xa3'+u32(output+12)
    # The third word is string-table offset at code[0:4], not an extra header.
    # Original and CE both address code from wrapper +8.
    if plan:
        for initial in plan['offsets']:call(0x40375e,vm,[0,data,initial])
    call(0x40375e,vm,[0,data,offset])
    s+=b'\x89\xc1';call(0x40fe5d)
    s+=b'\xdd\x1d'+u32(output)
    if plan:
        call(0x414d09,symbols)
    return s


def program_hash(name,expression):
    plan=symbol_plans().get(name)
    payload=plan['code'].resource() if plan else resource(expression)[0]
    return hashlib.sha256(payload+(plan.get('item_fields',b'')+(b'\0second-item-902\0'+plan['second_item_fields'] if plan.get('second_item_fields') else b'') if plan else b'')).hexdigest()


def original(args):
    identity=inspect(args.original.resolve());root=args.root.resolve();root.mkdir(parents=True,exist_ok=False)
    exe=root/'CarmenTQ.exe';shutil.copyfile(args.original,exe);rows=[]
    for name,expr,expected in probes():
        if getattr(args,'symbols_only',False) and name not in symbol_plans():continue
        if getattr(args,'cross_calls_only',False) and name not in __import__('mohawk_v5_cross_call_probes').plans():continue
        if getattr(args,'lifecycle_only',False) and name not in __import__('mohawk_v5_lifecycle_probes').plans():continue
        if getattr(args,'calls_only',False) and not name.startswith('numeric-string') and 'item_fields' not in symbol_plans().get(name,{}):continue
        case=dict(name=name,expression_hex=expr.hex(),output='double',expected=float(expected),verify_initialized_vm=True,after_loader=True,observe_vm_error=name in symbol_plans(),verify_symbols=name in symbol_plans(),verify_dispatch_context=name.startswith('expression-return'),verify_second_item=bool(symbol_plans().get(name,{}).get('second_item_fields')),verify_reentry=name.startswith('reentrant-'),verify_script_fixture=bool(symbol_plans().get(name,{}).get('item_fields')),allocation_size=0x10000 if symbol_plans().get(name,{}).get('item_fields') else 0x2000)
        try:result=run_case(exe,case,5,stub_factory=stub)
        except (RuntimeError,OSError) as error:result=dict(status='harness_error',error=str(error))
        if not name.startswith(('error-','recovery-')) and result.get('vm_errors'):
            result['status']='unqualified'
        if name.startswith('error-') and result.get('status')=='observed' and result.get('vm_errors'):
            result['returned_numeric']=result['actual'];result['actual']={'completion':'returned'}
            result['observation_kind']='error-completion'
            result.pop('matches_static_contract',None);result.pop('expected',None)
        if name.startswith('recovery-'):
            result.pop('expected',None);result.pop('matches_static_contract',None)
            result['observation_kind']='error-completion'
            result['returned_numeric']=result.get('actual')
            result['actual']={'completion':'returned','recovery_predicate':result.get('actual')}
            if result.get('vm_errors') != symbol_plans()[name]['expected_vm_errors']:result['status']='unqualified'
        rows.append(dict(name=name,expression_sha256=hashlib.sha256(expr).hexdigest(),program_sha256=program_hash(name,expr),**result))
        print(name,result,flush=True)
    receipt=dict(kind='initialized original evaluator assertions; manual CRT heap/locks plus evaluator constructor; application startup skipped',original_sha256=identity['exe_sha256'],harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),transport_sha256=hashlib.sha256(Path(__file__).with_name('probe_mohawk_v5_original.py').read_bytes()).hexdigest(),results=rows)
    (root/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return 0 if all(row['status']=='observed' for row in rows) else 1


def ce(args):
    root=args.root.resolve();root.mkdir(parents=True,exist_ok=False);rows=[]
    for name,expr,expected in probes():
        if getattr(args,'symbols_only',False) and name not in symbol_plans():continue
        if getattr(args,'cross_calls_only',False) and name not in __import__('mohawk_v5_cross_call_probes').plans():continue
        if getattr(args,'lifecycle_only',False) and name not in __import__('mohawk_v5_lifecycle_probes').plans():continue
        if getattr(args,'calls_only',False) and not name.startswith('numeric-string') and 'item_fields' not in symbol_plans().get(name,{}):continue
        assets=root/name/'assets';assets.mkdir(parents=True);shutil.copyfile(args.game_data/'Outline.txt',assets/'Outline.txt')
        plan=symbol_plans().get(name)
        if plan:c=plan['code'];condition=plan['offset']
        else:c=Code();c.code=bytearray(8);condition=c.expr(expr)
        fail=c.expr(integer(1)+b'\x4d\x5e\x01')
        fields=(plan.get('item_fields',b'') if plan else b'')+(b''.join(field(0x194,struct.pack('>I',off)) for off in plan['offsets']) if plan else b'')+event(0x17,[subentry(0xfffb,struct.pack('>I',condition)+u16(2)),subentry(0xfffb,struct.pack('>I',0)+u16(3)),subentry(0x1d,struct.pack('>I',fail))])
        item=struct.pack('>4h4H',0,0,1,1,3,0,901,len(fields))+b'Probe\0'+fields
        if plan and plan.get('second_item_fields'):
            other=plan['second_item_fields'];item=struct.pack('>4h4H',0,0,1,1,3,0,902,len(other))+b'Target\0'+other+item
        data=archive([('BCOD',900,c.resource()),('BITL',900,item),('VRSN',900,u16(5)),('tPAL',900,u16(0)+u16(256)+bytes(1024))])
        page=assets/'ACME Support/Cache/StartUp.MHK';page.parent.mkdir(parents=True);page.write_bytes(data)
        client=Client(args.ce.resolve(),root/name/'native',args.extras.resolve());row=dict(name=name,expression_sha256=hashlib.sha256(expr).hexdigest(),program_sha256=program_hash(name,expr))
        try:
            (client.root/'profile.ini').write_text('[scummvm]\nenable_unsupported_game_warning=false\nconsole=false\nconfirm_exit=false\nautosave_period=0\nce_link_token=\n\n[cli-game]\nengineid=mohawk\ngameid=carmentq\nplatform=windows\npath='+str(assets)+'\ncheck_gamedata=false\n')
            client.command('launch',engine='mohawk',game='carmentq',platform='windows',path=str(assets),timeout=20)
            deadline=time.monotonic()+1.5
            while client.process.poll() is None and time.monotonic()<deadline:time.sleep(.05)
            if client.process.poll() is None:
                inspection=client.command('inspect');adapter=inspection.get('adapter',{})
                ready=adapter.get('phase',0)>=2 and any(i.get('id')==901 and i.get('loaded') for i in adapter.get('items',[]))
                row.update(status='observed' if ready else 'unqualified',inspection=inspection)
                if ready:row['actual']=1.0
                client.close()
            else:
                client.errors.close();log=(client.root/'engine.log').read_text(errors='replace')
                if 'Only the immediate SetPageFade(0) transition is supported' in log:row.update(status='observed',actual=0.0)
                else:
                    row.update(status='engine_error',log_tail=log.splitlines()[-8:])
                    diagnostic={'error-divide-zero':'V5 division by zero!', 'error-non-reference':'V5 assignment requires a reference!', 'error-call-arity':'incorrect number of parameters (0) to stringLen!'}
                    if name in diagnostic and diagnostic[name] in log:
                        row.update(status='observed',actual={'completion':'engine-abort'},observation_kind='error-completion')
        except Exception as e:row.update(status='transport_error',error=str(e));client.abort()
        if name.startswith('recovery-'):
            row['observation_kind']='error-completion'
            if row.get('status')=='engine_error' and {'recovery-divide-zero':'V5 division by zero','recovery-non-reference':'V5 assignment requires a reference','recovery-call-arity':'incorrect number of parameters (0) to stringLen'}[name] in '\n'.join(row.get('log_tail',[])):row.update(status='observed',actual={'completion':'engine-abort'})
        rows.append(row);print(name,row.get('status'),row.get('actual'),flush=True)
    receipt=dict(kind='CE generated assertion observations; success/failure sentinel, not raw VM-value reads',exe_sha256=hashlib.sha256(args.ce.read_bytes()).hexdigest(),harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),results=rows)
    (root/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return 0 if all(row['status']=='observed' for row in rows) else 1


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['original','ce']);p.add_argument('--root',type=Path,required=True);p.add_argument('--original',type=Path);p.add_argument('--ce',type=Path);p.add_argument('--extras',type=Path);p.add_argument('--game-data',type=Path);p.add_argument('--symbols-only',action='store_true');p.add_argument('--calls-only',action='store_true');p.add_argument('--lifecycle-only',action='store_true');p.add_argument('--cross-calls-only',action='store_true');a=p.parse_args()
    raise SystemExit((original if a.mode=='original' else ce)(a))
