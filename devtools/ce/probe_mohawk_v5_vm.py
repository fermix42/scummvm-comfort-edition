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
    ]


def resource(expression):
    c=Code();c.code=bytearray(8);offset=c.expr(expression)
    return c.resource(),offset


def stub(remote, write, output, case):
    vm=remote+0x1300;data=remote+0x1500
    payload,offset=resource(bytes.fromhex(case['expression_hex']));write(data,payload)
    s=bytearray(b'\xdb\xe3')
    def call(address,receiver=None,args=(),cdecl=False):
        nonlocal s
        for a in reversed(args):s+=b'\x68'+u32(a)
        if receiver is not None:s+=b'\xb9'+u32(receiver)
        s+=b'\xb8'+u32(address)+b'\xff\xd0'
        if cdecl and args:s+=b'\x83\xc4'+bytes([len(args)*4])
    call(0x47cd44,args=[1],cdecl=True)  # Original CRT heap initialization.
    s+=b'\xa3'+u32(output+8)
    call(0x47b977)  # Original static critical-section initialization.
    call(0x401ed0,0x49a540)  # Result value constructor.
    call(0x402eef,vm,[0])  # Original evaluator constructor: creates value stack.
    s+=b'\xa3'+u32(output+12)
    # Original reader consumes a third header word; numeric probe offsets are
    # relative to its +12 code base, versus CE's +8 base. Same expression bytes.
    call(0x40375e,vm,[0,data,offset-4])
    s+=b'\x89\xc1';call(0x40fe5d)
    s+=b'\xdd\x1d'+u32(output)
    return s


def original(args):
    identity=inspect(args.original.resolve());root=args.root.resolve();root.mkdir(parents=True,exist_ok=False)
    exe=root/'CarmenTQ.exe';shutil.copyfile(args.original,exe);rows=[]
    for name,expr,expected in probes():
        case=dict(name=name,expression_hex=expr.hex(),output='double',expected=float(expected),verify_initialized_vm=True)
        result=run_case(exe,case,5,stub_factory=stub);rows.append(dict(name=name,expression_sha256=hashlib.sha256(expr).hexdigest(),**result))
        print(name,result,flush=True)
    receipt=dict(kind='initialized original evaluator assertions; manual CRT heap/locks plus evaluator constructor; application startup skipped',original_sha256=identity['exe_sha256'],harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),transport_sha256=hashlib.sha256(Path(__file__).with_name('probe_mohawk_v5_original.py').read_bytes()).hexdigest(),results=rows)
    (root/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return 0 if all(row['status']=='observed' for row in rows) else 1


def ce(args):
    root=args.root.resolve();root.mkdir(parents=True,exist_ok=False);rows=[]
    for name,expr,expected in probes():
        assets=root/name/'assets';assets.mkdir(parents=True);shutil.copyfile(args.game_data/'Outline.txt',assets/'Outline.txt')
        c=Code();c.code=bytearray(8);condition=c.expr(expr)
        fail=c.expr(integer(1)+b'\x4d\x5e\x01')
        fields=event(0x17,[subentry(0xfffb,struct.pack('>I',condition)+u16(2)),subentry(0xfffb,struct.pack('>I',0)+u16(3)),subentry(0x1d,struct.pack('>I',fail))])
        item=struct.pack('>4h4H',0,0,1,1,3,0,901,len(fields))+b'Probe\0'+fields
        data=archive([('BCOD',900,c.resource()),('BITL',900,item),('VRSN',900,u16(5)),('tPAL',900,u16(0)+u16(256)+bytes(1024))])
        page=assets/'ACME Support/Cache/StartUp.MHK';page.parent.mkdir(parents=True);page.write_bytes(data)
        client=Client(args.ce.resolve(),root/name/'native',args.extras.resolve());row=dict(name=name,expression_sha256=hashlib.sha256(expr).hexdigest())
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
                else:row.update(status='engine_error',log_tail=log.splitlines()[-8:])
        except Exception as e:row.update(status='transport_error',error=str(e));client.abort()
        rows.append(row);print(name,row.get('status'),row.get('actual'),flush=True)
    receipt=dict(kind='CE generated assertion observations; success/failure sentinel, not raw VM-value reads',exe_sha256=hashlib.sha256(args.ce.read_bytes()).hexdigest(),harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),results=rows)
    (root/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return 0 if all(row['status']=='observed' for row in rows) else 1


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['original','ce']);p.add_argument('--root',type=Path,required=True);p.add_argument('--original',type=Path);p.add_argument('--ce',type=Path);p.add_argument('--extras',type=Path);p.add_argument('--game-data',type=Path);a=p.parse_args()
    raise SystemExit((original if a.mode=='original' else ce)(a))
