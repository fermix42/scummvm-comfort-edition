# SPDX-License-Identifier: GPL-3.0-or-later
"""Original allocator-driven cache purge/retain/lock policy without exhausting host RAM."""
import argparse,hashlib,json,shutil
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from probe_mohawk_v5_disk_cache import fixture
from mohawk_v5_native_io import Native,boot,open_archive


def stub(remote,write,output,case):
    n=Native(remote,write,output);m=n.mem;boot(n);open_archive(n,case['archive'])
    n.call(0x483dc8,args=[0x45bba6])  # Register the actual original cache purge callback.
    n.call(0x45a126,args=[m(4),0 if case['name']=='evict-retain-zero' else 1]);n.s+=b'\x0f\xb7\xc0';n.save(5)
    n.call(0x45695a,args=[m(5)]);n.save(6)
    n.call(0x483c5d,args=[m(5),0 if case['name']=='evict-nondiscardable' else 1])
    if case['name']!='evict-locked':n.call(0x4568ec,args=[m(5)])
    n.call(0x457403,args=[4096,0]);n.save(7)
    n.call(0x483cbc,args=[m(5)]);n.save(8)
    if case['name']=='evict-locked':
        n.call(0x4568ec,args=[m(5)]);n.call(0x457403,args=[4096,0]);n.save(9);n.call(0x483cbc,args=[m(5)]);n.save(10)
    n.call(0x45a126,args=[m(4),1 if case['name']=='evict-retain-zero' else 0]);n.s+=b'\x0f\xb7\xc0';n.save(11)
    n.call(0x45695a,args=[m(11)]);n.save(12)
    n.s+=b'\x89\xc6\xbf'+u32(output+0x6000)+b'\xb9'+u32(case['observe_native_blob_size'])+b'\xf3\xa4'
    n.call(0x4568ec,args=[m(11)]);n.call(0x45a2c3,args=[m(4),1]);n.s+=b'\x0f\xb7\xc0';n.save(13)
    n.call(0x45a3c8,args=[m(3),0,0]);n.s+=b'\x0f\xb7\xc0';n.save(14)
    n.load(8);n.s+=b'\xa3'+u32(output);return n.s


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
    if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized executable')
    a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[];f=fixture('disk-true')
    for name in ['evict-retained','evict-retain-zero','evict-locked','evict-nondiscardable']:
        folder=a.root/name;folder.mkdir();archive=folder/'Probe.MHK';archive.write_bytes(f['archive'])
        row=run_case(exe,dict(name=name,archive=str(archive),output='int32',expected=0,after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_native_blob_size=len(f['bcod']),native_blob_offset=0x6000,observe_vm_error=True),10,stub_factory=stub)
        row.pop('expected',None);row.pop('matches_static_contract',None);checks={};w=row.get('native_words',[]);size=len(f['bcod'])
        if row['status']=='observed':
            protected=name in ['evict-locked','evict-nondiscardable']
            checks=dict(memory_initialized=w[0]==0,filesystem_initialized=w[1]==0,opened=w[3]!=0 and w[4]!=0,loaded=w[5]!=0 and w[6]!=0,
                        first_purge=w[7]==(0 if protected else size),first_size=w[8]==(size if protected else 0),
                        reloaded=w[11]!=0 and w[12]!=0,exact_payload=bytes.fromhex(row['native_blobs'][0])==f['bcod'],
                        released_and_closed=w[13]==0 and w[14]==0,no_vm_errors=not row.get('vm_errors'),
                        archive_preserved=hashlib.sha256(archive.read_bytes()).hexdigest()==f['program_sha256'])
            if name=='evict-locked':checks.update(unlocked_purge=w[9]==size,unlocked_size=w[10]==0)
            if not all(checks.values()):row['status']='unqualified'
        row.update(name=name,expression_sha256=f['expression_sha256'],program_sha256=f['program_sha256'],contract_checks=checks)
        rows.append(row);print(name,row['status'],checks,flush=True)
    dependencies={name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ['probe_mohawk_v5_eviction.py','probe_mohawk_v5_original.py','probe_mohawk_v5_native_resources.py','mohawk_v5_native_io.py','probe_mohawk_v5_disk_cache.py']}
    (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),dependencies=dependencies,results=rows),indent=2)+'\n')
    raise SystemExit(0 if all(x['status']=='observed' for x in rows) else 1)
