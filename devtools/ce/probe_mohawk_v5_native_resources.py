# SPDX-License-Identifier: GPL-3.0-or-later
"""Real original resident-cache, deleting-destructor and cross-resource code probes.
Resident map metadata is seeded; disk archive opening and page loading are not qualified.
"""
import argparse,hashlib,json,shutil,struct
from pathlib import Path
from probe_mohawk_v5_original import run_case,u32
from test_mohawk_v5_startup import Code,integer,field,event,subentry,u32 as be32,u16


class Native:
    def __init__(self,remote,write,output):self.s=bytearray();self.remote=remote;self.write=write;self.output=output;self.slots=output+128
    def mem(self,n):return ('mem',self.slots+4*n)
    def call(self,fn,receiver=None,args=(),cdecl=False):
        for a in reversed(args):self.s+=(b'\xff\x35'+u32(a[1])) if isinstance(a,tuple) else b'\x68'+u32(a)
        if receiver is not None:self.s+=(b'\x8b\x0d'+u32(receiver[1])) if isinstance(receiver,tuple) else b'\xb9'+u32(receiver)
        self.s+=b'\xb8'+u32(fn)+b'\xff\xd0'
        if cdecl and args:self.s+=b'\x83\xc4'+bytes([4*len(args)])
    def save(self,n):self.s+=b'\xa3'+u32(self.slots+4*n)
    def load(self,n):self.s+=b'\xa1'+u32(self.slots+4*n)
    def alloc(self,payload,source,h,p):
        self.write(source,payload);self.call(0x456b06,args=[len(payload)]);self.s+=b'\x0f\xb7\xc0';self.save(h)
        self.call(0x45695a,args=[self.mem(h)]);self.save(p)
        self.s+=b'\x89\xc7\xbe'+u32(source)+b'\xb9'+u32(len(payload))+b'\xf3\xa4'
        self.call(0x4568ec,args=[self.mem(h)])
    def store_field(self,ptr_slot,offset,value_slot,word=False):
        self.load(value_slot);self.s+=b'\x8b\x15'+u32(self.slots+4*ptr_slot)+(b'\x66' if word else b'')+b'\x89\x82'+u32(offset)
    def cache(self,index,payload):
        base=1 if index==0 else 8;source=self.remote+0x8000+index*0x1000
        m=bytearray(0x80);struct.pack_into('<I',m,0,0x50414d52);struct.pack_into('<I',m,0x10,self.remote+0x5000)
        self.alloc(m,source,base,base+1)
        self.alloc(bytes([1,0,5,0]),source+0x100,base+2,base+3)
        self.alloc(payload,source+0x200,base+4,base+5)
        self.store_field(base+1,0x1a,base+2,True);self.store_field(base+1,0x50,base+4,True)
        self.load(base);self.s+=b'\x0d'+u32(0x10000);self.save(base+6)


def code(value,expected=42):
    c=Code();c.code=bytearray(8);r=lambda n:c.string(n,True)
    entry=c.expr(r('result')+integer(value)+b'\x6b\x22')
    setup=c.expr(r('result')+integer(0)+b'\x6b\x22')
    final=c.expr(r('result')+integer(expected)+b'\x6c\x16')
    payload=bytearray(c.resource());struct.pack_into('>I',payload,8,len(c.code))
    return bytes(payload),entry,setup,final


def stub(remote,write,output,case):
    n=Native(remote,write,output);m=n.mem;page=remote+0x2000;owner=remote+0x2200;item=remote+0x2400;donor=remote+0x2800;owner2=remote+0x2a00;vm=remote+0x1300;symbols=remote+0x1800
    n.call(0x47cd44,args=[1],cdecl=True);n.call(0x47a9ec);n.call(0x477810);n.s+=b'\xdb\xe3'
    n.call(0x456fe5,args=[0,0]);n.s+=b'\x0f\xb7\xc0';n.save(0)
    # Original supports single-thread sync mode; no fake cache/allocator functions.
    write(0x49af5c,b'\x01\x00');n.call(0x44b37e,remote+0x5100)
    f=bytearray(0x40);struct.pack_into('<I',f,0,0x454c4946);struct.pack_into('<I',f,0x10,remote+0x5100);struct.pack_into('<H',f,0x16,0);write(remote+0x5000,f)
    if case['name'].startswith('resident-'):
        n.cache(0,b'REAL-CACHE-PAYLOAD\0')
        n.call(0x45a126,args=[m(7),1]);n.s+=b'\x0f\xb7\xc0';n.save(15)
        n.call(0x45a2c3,args=[m(7),1]);n.s+=b'\x0f\xb7\xc0';n.save(17)
        n.call(0x456868,args=[m(5)]);n.save(18)
        n.s+=b'\x8b\x00';n.save(16)
        n.call(0x45a2c3,args=[m(7),1]);n.s+=b'\x0f\xb7\xc0';n.save(19)
        n.call(0x483e1d,args=[m(5)]);n.save(20)
        n.s+=b'\xa3'+u32(output)
        return n.s
    n.call(0x402e3f);n.call(0x401ed0,0x49a540);n.call(0x41482c,symbols,[1024]);n.call(0x414c47,symbols);n.call(0x402eef,vm,[symbols])
    write(page+0x1c,u32(vm));write(0x49a678,u32(page))
    header=remote+0x4000;header2=remote+0x4100
    write(header,struct.pack('>4h4H',0,0,1,1,3,0,901,0)+b'Probe\0');write(header2,struct.pack('>4h4H',0,0,1,1,3,0,902,0)+b'Victim\0')
    if case['name'].startswith('destructor-') or case['name']=='resource-unload-real':
        if case['name']=='resource-unload-real':
            n.cache(0,b'LIVE-THROUGH-TEARDOWN\0');write(owner,u32(1));n.load(7);n.s+=b'\xa3'+u32(owner+0x14)
        n.call(0x476f70,args=[0xf0],cdecl=True);n.save(23);n.call(0x4185d0,m(23),[page,owner,header2])
        if case['name']=='destructor-incoming-link':
            n.call(0x4185d0,item,[page,owner2,header])
            link=remote+0x4200;write(link,field(0x192,u16(902)+u16(1)));n.call(0x419460,item,[1,0,link])
        n.call(0x401ed0,remote+0x6000);n.call(0x4060e0,remote+0x6000,[m(23)])
        n.call(0x401ed0,remote+0x6020);n.call(0x401f60,remote+0x6020,[remote+0x6000])
        if case['name']=='resource-unload-real':
            n.call(0x4268d3,owner);n.call(0x483e1d,args=[m(5)]);n.save(20)
            n.s+=b'\xa1'+u32(owner);n.save(28)
            n.s+=b'\xa1'+u32(owner+0x14);n.save(29)
            n.call(0x4268d3,owner)  # Cleared owner must make repeat unload a no-op.
        else:n.call(0x41f3e0,m(23),[1])  # Real deleting destructor and original free.
        n.call(0x40f869,remote+0x6000,[remote+0x6200]);n.call(0x44994e,remote+0x6200);n.save(22)
        n.call(0x40f869,remote+0x6020,[remote+0x6220]);n.call(0x44994e,remote+0x6220);n.save(24)
        if case['name']=='destructor-incoming-link':
            n.s+=b'\xa1'+u32(item+0xdc)+b'\x8b\x40\x08';n.save(26)
        n.s+=b'\xa1'+u32(owner+4)+b'\xa3'+u32(output)
        return n.s
    payload,entry,setup,final=code(99,43 if case['name']=='cross-resource-false-control' else 42);other,_,_,_=code(42)
    n.cache(0,payload);n.cache(1,other);n.load(7);n.s+=b'\xa3'+u32(owner+0x14);n.load(7 if case['name']=='cross-resource-alias-control' else 14);n.s+=b'\xa3'+u32(owner2+0x14)
    n.call(0x4185d0,item,[page,owner,header]);n.call(0x4185d0,donor,[page,owner2,header2])
    # No BCOD-fetch bridge: original 41d38d resolves the owner's real cache key.
    data=remote+0x3000;write(data,payload);n.call(0x40375e,vm,[0,data,setup])
    fields=remote+0x4300;write(fields,event(0x1c,[subentry(0x1d,be32(entry))],'linked'));n.call(0x419460,donor,[1,0,fields])
    link=remote+0x4200;write(link,field(0x192,u16(902)+u16(1)));n.call(0x419460,item,[1,0,link])
    name=remote+0x4400;write(name,b'linked\0');n.call(0x41e14e,item,[0x1c,name])
    n.call(0x40375e,vm,[0,data,final]);n.s+=b'\x89\xc1';n.call(0x40ffa9);n.s+=b'\xa3'+u32(output)
    n.s+=b'\xa1'+u32(item+8);n.save(25)
    n.s+=b'\x3d'+u32(item)+b'\x0f\x94\xc0\x0f\xb6\xc0';n.save(27)
    return n.s


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--original',type=Path,required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
    if hashlib.sha256(a.original.read_bytes()).hexdigest()!='219d81ea3d91b31da406e6891d01ba6ad12005e77b4d9f8f83930161dbfc9966':raise ValueError('Unrecognized original')
    a.root=a.root.resolve();a.root.mkdir(parents=True,exist_ok=False);exe=a.root/'CarmenTQ.exe';shutil.copyfile(a.original,exe);rows=[]
    for name in ['resident-retain-release','destructor-aliases','destructor-incoming-link','resource-unload-real','cross-resource-linked','cross-resource-false-control','cross-resource-alias-control']:
        case=dict(name=name,output='int32',expected=0,after_loader=True,allocation_size=0x10000,observe_native_words=True,observe_vm_error=True)
        result=run_case(exe,case,8,stub_factory=stub);result.pop('expected',None);result.pop('matches_static_contract',None)
        words=result.get('native_words',[]);checks={}
        if result['status']=='observed':
            checks['no_vm_errors']=not result.get('vm_errors')
            checks['memory_initialized']=words[0]==0
            if name.startswith('resident-'):
                checks.update(real_cache_handle=words[15]==words[5]!=0,retained_release=words[17]==0 and words[18]==words[6] and words[16]==int.from_bytes(b'REAL','little'),final_release=words[19]==0 and words[20]==0)
            elif name.startswith('destructor-') or name=='resource-unload-real':
                checks.update(aliases_invalidated=result.get('native_text')==['~Victim','~Victim'],resource_item_list_empty=result['actual']==0)
                if name=='resource-unload-real':checks['code_handle_freed']=words[20]==0 and words[28]==0 and words[29]==0
                if name=='destructor-incoming-link':checks['incoming_link_not_repaired']=words[26]==words[23]!=0
            else:checks.update(distinct_cached_resources=words[7]!=words[14] and words[6]!=words[13] and words[7]!=0 and words[14]!=0,value=result['actual']==(0 if name.endswith('control') else 1),receiver_restored=words[27]==1)
            if not all(checks.values()):result['status']='unqualified'
        result['contract_checks']=checks;print(name,json.dumps(result));rows.append(dict(name=name,**result))
    (a.root/'receipt.json').write_text(json.dumps(dict(kind=__doc__,fixture_sha256={str(v)+'-expected-'+str(e):hashlib.sha256(code(v,e)[0]).hexdigest() for v,e in [(99,42),(42,42),(99,43)]},original_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),transport_sha256=hashlib.sha256(Path(__file__).with_name('probe_mohawk_v5_original.py').read_bytes()).hexdigest(),results=rows),indent=2))

    raise SystemExit(0 if all(x['status']=='observed' and all(x['contract_checks'].values()) for x in rows) else 1)
