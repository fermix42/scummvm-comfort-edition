# SPDX-License-Identifier: GPL-3.0-or-later
"""Original disk-backed MHK cache through original synchronous I/O fallback; paired CE predicates."""
import argparse,hashlib,json,shutil,struct,time
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from test_mohawk_v5_startup import Code,integer,archive,event,subentry,u16,u32 as be32
from test_cli_client import Client


NAMES=['disk-true','disk-false','disk-release-reload','disk-close-reopen','disk-missing-id','disk-bad-magic']

def fixture(name):
    c=Code();c.code=bytearray(8)
    expression=integer(6)+integer(7)+b'\x6e\x14'+integer(99 if name=='disk-false' else 13)+b'\x6c\x16'
    offset=c.expr(expression)
    fail=c.expr(integer(1)+b'\x4d\x5e\x01');struct.pack_into('>I',c.code,0,len(c.code))
    fields=event(0x17,[subentry(0xfffb,be32(offset)+u16(2)),subentry(0xfffb,be32(0)+u16(3)),subentry(0x1d,be32(fail))])
    item=struct.pack('>4h4H',0,0,1,1,3,0,901,len(fields))+b'Probe\0'+fields
    data=archive([('BCOD',900,c.resource()),('BITL',900,item),('VRSN',900,u16(5)),('tPAL',900,u16(0)+u16(256)+bytes(1024))])
    if name=='disk-bad-magic':data=b'X'+data[1:]
    return dict(archive=data,bcod=c.resource(),offset=offset,expression_sha256=hashlib.sha256(expression).hexdigest(),program_sha256=hashlib.sha256(data).hexdigest())


def stub(remote,write,output,case):
    path=remote+0x2000;text=remote+0x2200;slots=output+128;vm=remote+0x1300
    # Scheduling only: original 45641c catches 0x12c and executes original
    # file operation 456411 synchronously. No file/cache result is substituted.
    write(0x48366f,b'\xb8'+u32(0x12c)+b'\xc2\x0c\x00')
    write(text,case['archive'].encode('ascii')+b'\0');s=bytearray();stops=[]
    def call(fn,receiver=None,args=(),cdecl=False):
        nonlocal s
        for a in reversed(args):s+=(b'\xff\x35'+u32(a[1])) if isinstance(a,tuple) else b'\x68'+u32(a)
        if receiver is not None:s+=b'\xb9'+u32(receiver)
        s+=b'\xb8'+u32(fn)+b'\xff\xd0'
        if cdecl and args:s+=b'\x83\xc4'+bytes([4*len(args)])
    def save(n):
        nonlocal s
        s+=b'\xa3'+u32(slots+4*n)+b'\xc7\x05'+u32(slots+124)+u32(n)
    def zero_stop():
        nonlocal s
        s+=b'\x85\xc0\x0f\x84';stops.append(len(s));s+=bytes(4)
    def mask():
        nonlocal s
        s+=b'\x0f\xb7\xc0'
    def blob(index):
        nonlocal s
        s+=b'\x89\xc6\xbf'+u32(output+0x6000+index*256)+b'\xb9'+u32(case['observe_native_blob_size'])+b'\xf3\xa4'
    mem=lambda n:('mem',slots+4*n)
    call(0x47cd44,args=[1],cdecl=True);call(0x47a9ec);call(0x477810);s+=b'\xdb\xe3'
    call(0x44b859,remote+0x2500);s+=b'\xa3'+u32(0x49af84)+b'\xa3'+u32(0x49af80)
    s+=b'\x66\xc7\x05'+u32(0x49af78)+b'\x01\x00'+b'\x66\xc7\x05'+u32(0x49af7a)+b'\x01\x00'
    call(0x456fe5,args=[0,0]);mask();save(0)
    call(0x45424a,args=[0]);mask();save(1)
    # Original cache root constructors, without unrelated configuration reads.
    call(0x456b06,args=[0]);mask();save(15);s+=b'\x66\xa3'+u32(0x49b176)
    call(0x483c5d,args=[mem(15),1]);call(0x483dda);s+=b'\x66\xa3'+u32(0x49b174)
    call(0x4531df,path,[text]);save(2)
    call(0x45a9bc,args=[path,1]);save(3)
    s+=b'\x0f\xb7\x15'+u32(0x49b168)+b'\x89\x15'+u32(slots+4*13);zero_stop()
    call(0x458edc,args=[0x42434f44,901 if case['name']=='disk-missing-id' else 900,mem(3)]);save(4)
    s+=b'\x0f\xb7\x15'+u32(0x49b168)+b'\x89\x15'+u32(slots+4*14);zero_stop()
    call(0x45a126,args=[mem(4),1]);mask();save(5);zero_stop()
    call(0x45695a,args=[mem(5)]);save(6);zero_stop();blob(0)
    if case['name'] in ['disk-true','disk-false']:
        call(0x401ed0,0x49a540);call(0x402eef,vm,[0]);call(0x40375e,vm,[0,mem(6),case['offset']]);s+=b'\x89\xc1';call(0x40ffa9);save(7)
    call(0x4568ec,args=[mem(5)]);mask();save(8)
    call(0x45a2c3,args=[mem(4),1]);mask();save(9)
    call(0x483e1d,args=[mem(5)]);mask();save(10)
    if case['name']=='disk-close-reopen':
        call(0x45a3c8,args=[mem(3),0,0]);mask();save(16);call(0x483e1d,args=[mem(3)]);mask();save(17)
        call(0x45a9bc,args=[path,1]);save(18);zero_stop();call(0x458edc,args=[0x42434f44,900,mem(18)]);save(19);zero_stop()
    else:
        s+=b'\xa1'+u32(slots+12);save(18);s+=b'\xa1'+u32(slots+16);save(19)
    if case['name'] in ['disk-close-reopen','disk-release-reload']:
        call(0x45a126,args=[mem(19),1]);mask();save(20);zero_stop();call(0x45695a,args=[mem(20)]);save(21);zero_stop();blob(1)
        call(0x4568ec,args=[mem(20)]);call(0x45a2c3,args=[mem(19),1]);mask();save(23)
    call(0x45a3c8,args=[mem(18),0,0]);mask();save(24)
    call(0x483e1d,args=[mem(18)]);mask();save(25)
    for pos in stops:struct.pack_into('<i',s,pos,len(s)-(pos+4))
    s+=b'\xa1'+u32(slots+28)+b'\xa3'+u32(output)
    return s


def original(args):
    if hashlib.sha256(args.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized original')
    args.root=args.root.resolve();args.root.mkdir(parents=True,exist_ok=False);exe=args.root/'CarmenTQ.exe';shutil.copyfile(args.original,exe);rows=[]
    for name in NAMES:
        f=fixture(name);directory=args.root/name;directory.mkdir();mhk=directory/'Probe.MHK';mhk.write_bytes(f['archive'])
        case=dict(name=name,archive=str(mhk),offset=f['offset'],output='int32',expected=0,after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_native_blob_size=len(f['bcod']),native_blob_offset=0x6000,observe_vm_error=True)
        if len(f['bcod'])>256 or len(str(mhk))>=256:raise ValueError('Fixture exceeds bounded original layout')
        row=run_case(exe,case,10,stub_factory=stub);row.pop('expected',None);row.pop('matches_static_contract',None);w=row.get('native_words',[]);checks={}
        if row['status']=='observed':
            checks=dict(memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,no_vm_errors=not row.get('vm_errors'),archive_preserved=hashlib.sha256(mhk.read_bytes()).hexdigest()==f['program_sha256'])
            if name=='disk-bad-magic':checks.update(open_rejected=w[3]==0,open_error=w[13]==0x28a4)
            elif name=='disk-missing-id':checks.update(opened=w[3]!=0,lookup_rejected=w[4]==0,native_error=w[14]==0x28a3)
            else:
                checks.update(opened=w[3]!=0 and w[4]!=0,exact_payload=bytes.fromhex(row['native_blobs'][0])==f['bcod'],released=w[9]==0 and w[10]==0,archive_closed=w[24]==0 and w[25]==0)
                if name in ['disk-close-reopen','disk-release-reload']:checks.update(second_exact_payload=bytes.fromhex(row['native_blobs'][1])==f['bcod'],second_release=w[23]==0)
                if name=='disk-close-reopen':checks.update(first_close=w[16]==0 and w[17]==0,reopened=w[18]!=0)
            if not all(checks.values()):row['status']='unqualified'
        row.update(name=name,expression_sha256=f['expression_sha256'],program_sha256=f['program_sha256'],contract_checks=checks);rows.append(row);print(name,row['status'],row.get('actual'),checks,flush=True)
    receipt=dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),transport_sha256=hashlib.sha256(Path(__file__).with_name('probe_mohawk_v5_original.py').read_bytes()).hexdigest(),results=rows)
    (args.root/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');return 0 if all(x['status']=='observed' for x in rows) else 1


def ce(args):
    args.root=args.root.resolve();args.root.mkdir(parents=True,exist_ok=False);rows=[]
    for name in ['disk-true','disk-false']:
        f=fixture(name);assets=args.root/name/'assets';assets.mkdir(parents=True);shutil.copyfile(args.game_data/'Outline.txt',assets/'Outline.txt');page=assets/'ACME Support/Cache/StartUp.MHK';page.parent.mkdir(parents=True);page.write_bytes(f['archive'])
        client=Client(args.ce.resolve(),args.root/name/'native',args.extras.resolve());row=dict(name=name,expression_sha256=f['expression_sha256'],program_sha256=f['program_sha256'])
        try:
            (client.root/'profile.ini').write_text('[scummvm]\nenable_unsupported_game_warning=false\nconsole=false\nconfirm_exit=false\nautosave_period=0\nce_link_token=\n\n[cli-game]\nengineid=mohawk\ngameid=carmentq\nplatform=windows\npath='+str(assets)+'\ncheck_gamedata=false\n')
            client.command('launch',engine='mohawk',game='carmentq',platform='windows',path=str(assets),timeout=20);deadline=time.monotonic()+1.5
            while client.process.poll() is None and time.monotonic()<deadline:time.sleep(.05)
            if client.process.poll() is None:
                ad=client.command('inspect').get('adapter',{});ready=ad.get('phase',0)>=2 and any(x.get('id')==901 and x.get('loaded') for x in ad.get('items',[]));row.update(status='observed' if ready else 'unqualified',actual=1 if ready else None);client.close()
            else:
                client.errors.close();log=(client.root/'engine.log').read_text(errors='replace');row.update(status='observed',actual=0) if 'Only the immediate SetPageFade(0) transition is supported' in log else row.update(status='engine_error',diagnostics=log.splitlines()[-5:])
        except Exception as error:row.update(status='transport_error',error=str(error));client.abort()
        rows.append(row);print(name,row,flush=True)
    (args.root/'receipt.json').write_text(json.dumps(dict(kind='CE matching generated MHK assertion observations',exe_sha256=hashlib.sha256(args.ce.read_bytes()).hexdigest(),harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),results=rows),indent=2)+'\n');return 0 if all(x['status']=='observed' for x in rows) else 1


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['original','ce']);p.add_argument('--original',type=Path);p.add_argument('--ce',type=Path);p.add_argument('--extras',type=Path);p.add_argument('--game-data',type=Path);p.add_argument('--root',type=Path,required=True);a=p.parse_args();raise SystemExit((original if a.mode=='original' else ce)(a))
