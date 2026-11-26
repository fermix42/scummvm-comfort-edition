# SPDX-License-Identifier: GPL-3.0-or-later
"""Generated named-script fixtures and an explicit in-memory BCOD fetch bridge."""
import struct
from test_mohawk_v5_startup import Code, integer, field, event, subentry, u16, u32


def plans():
    out={}
    for name in ['named-supplied','named-default','named-default-side-effect','local-shadowing','nested-named','named-false-control','local-shadowing-false']:
        c=Code();c.code=bytearray(8);r=lambda n:c.string(n,True);assign=b'\x6b\x22';eq=b'\x6c\x16'
        # A default expression always executes, including when an actual is supplied.
        default=integer(7)
        setup=[r('result')+integer(0)+assign]
        if name=='named-default-side-effect':
            setup.append(r('effects')+integer(0)+assign)
            default=r('effects')+r('effects')+integer(1)+b'\x6e\x14'+assign
        declaration=c.expr(b'\x62'+r('argument')+default+assign+b'\x07\x76\x07')
        entries=[] if name.startswith('local-shadowing') else [subentry(0x1d,u32(declaration))]
        if name.startswith('local-shadowing'):
            setup.append(r('temporary')+integer(99)+assign)
            local=c.expr(b'\x61'+r('temporary')+b'\x07\x08')
            entries.append(subentry(0x1d,u32(local)))
            entries.append(subentry(0x1d,u32(c.expr(r('temporary')+integer(5)+assign))))
            body=c.expr(r('result')+r('temporary')+assign)
        else:body=c.expr(r('result')+r('argument')+assign)
        entries.append(subentry(0x1d,u32(body)))
        fields=event(0x1c,entries,'callee')
        arg=c.expr(b'\x63'+integer(42)+b'\x07\x76\x07') if name!='named-default' and not name.startswith('local-shadowing') else 0
        call=subentry(0x11c,u32(arg)+b'callee\0')
        if name=='nested-named':
            fields+=event(0x1c,[call],'outer')
            call=subentry(0x11c,u32(0)+b'outer\0')
        fields+=event(0x17,[call])
        if name.startswith('local-shadowing'):expr=r('temporary')+integer(100 if name=='local-shadowing-false' else 99)+eq+r('result')+integer(5)+eq+b'\x6d\x2a'
        elif name=='named-default-side-effect':expr=r('result')+integer(42)+eq+r('effects')+integer(1)+eq+b'\x6d\x2a'
        else:expr=r('result')+integer(7 if name=='named-default' else 43 if name=='named-false-control' else 42)+eq
        offsets=[c.expr(x) for x in setup];offset=c.expr(expr)
        out[name]=dict(code=c,expression=expr,expected=0 if name.endswith('false') or name=='named-false-control' else 1,preludes=setup,offsets=offsets,offset=offset,item_fields=fields)
    from mohawk_v5_lifecycle_probes import plans as lifecycle_plans
    out.update(lifecycle_plans())
    return out


def stub(remote,write,output,case,plan):
    from probe_mohawk_v5_original import u32 as le
    vm=remote+0x1300;symbols=remote+0x1800;page=remote+0x2000;owner=remote+0x2200;item=remote+0x2400;data=remote+0x3000;fields=remote+0x4000;header=remote+0x4800;bridge=remote+0x5000
    payload=bytearray(plan['code'].resource());struct.pack_into('>I',payload,8,len(plan['code'].code));write(data,payload)
    write(fields,plan['item_fields']);write(header,struct.pack('>4h4H',0,0,1,1,3,0,901,0)+b'Probe\0')
    write(page+0x1c,le(vm));write(0x49a678,le(page))
    # Fetch-only bridge: same receiver, same generated BCOD, same offset; the
    # original VM, item dispatch, parsers, argument/default/frame code still run.
    bridge_code=b'\xff\x05'+le(output+28)+b'\xff\x74\x24\x04'+b'\x68'+le(data)+b'\x51\xb9'+le(vm)+b'\xb8'+le(0x40375e)+b'\xff\xd0\xc2\x04\x00'
    write(bridge,bridge_code);write(0x41d38d,b'\xe9'+le(bridge-0x41d38d-5))
    if case['name'].startswith('reentrant-'):
        # Count the original already-active branch, then execute displaced bytes.
        hook=remote+0x5100
        write(hook,b'\xff\x05'+le(output+32)+b'\x8b\x8d\x5c\xff\xff\xff'+b'\xe9'+le(0x4037c0-(hook+17)))
        write(0x4037ba,b'\xe9'+le(hook-0x4037ba-5)+b'\x90')
    s=bytearray(b'\xdb\xe3')
    def call(fn,receiver=None,args=(),cdecl=False):
        nonlocal s
        for a in reversed(args):s+=b'\x68'+le(a)
        if receiver is not None:s+=b'\xb9'+le(receiver)
        s+=b'\xb8'+le(fn)+b'\xff\xd0'
        if cdecl and args:s+=b'\x83\xc4'+bytes([len(args)*4])
    call(0x47cd44,args=[1],cdecl=True);s+=b'\xa3'+le(output+8)
    call(0x47a9ec);s+=b'\xa3'+le(output+20)
    call(0x477810);s+=b'\xdb\xe3'
    call(0x402e3f);call(0x401ed0,0x49a540)
    call(0x41482c,symbols,[1024]);call(0x414c47,symbols);s+=b'\xa3'+le(output+16)
    call(0x402eef,vm,[symbols]);s+=b'\xa3'+le(output+12)
    call(0x4185d0,item,[page,owner,header])
    for off in plan['offsets']:call(0x40375e,vm,[0,data,off])
    cursor=0
    while cursor<len(plan['item_fields']):
        call(0x419460,item,[1,0,fields+cursor])
        cursor+=4+struct.unpack_from('>H',plan['item_fields'],cursor+2)[0]
    call(0x41e14e,item,[0x17,0])
    call(0x40375e,vm,[0,data,plan['offset']]);s+=b'\x89\xc1';call(0x40fe5d);s+=b'\xdd\x1d'+le(output)
    call(0x414d09,symbols)
    call(0x405840,0x49a508);s+=b'\xa3'+le(output+24)
    return s
