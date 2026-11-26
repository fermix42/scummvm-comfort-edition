# SPDX-License-Identifier: GPL-3.0-or-later
"""Generated return/recovery/active-evaluator/context research fixtures."""
from test_mohawk_v5_startup import Code, integer, event, subentry, u32


def plans():
    out={}
    names=['return-expression','return-void','return-false-control','recovery-divide-zero','recovery-non-reference','recovery-call-arity','reentrant-value','reentrant-side-effect','reentrant-false-control','sender-self','sender-self-false-control','sender-restored']
    for name in names:
        c=Code();c.code=bytearray(8);r=lambda n:c.string(n,True)
        assign=b'\x6b\x22';eq=b'\x6c\x16';add=b'\x6e\x14'
        setup=[r('result')+integer(0)+assign];entries=[];fields=b'';errors=[]
        if name.startswith('return-'):
            entries=[subentry(0x1d,u32(c.expr(r('result')+integer(41)+assign))),subentry(0xfffc,u32(c.expr(r('result')+integer(42)+assign)) if name!='return-void' else b''),subentry(0x1d,u32(c.expr(r('result')+integer(999)+assign)))]
            fields=event(0x1c,entries,'callee')+event(0x17,[subentry(0x11c,u32(0)+b'callee\0')])
            expr=r('result')+integer(41 if name=='return-void' else 43 if name=='return-false-control' else 42)+eq
        elif name.startswith('recovery-'):
            bad,code={'recovery-divide-zero':(integer(1)+integer(0)+b'\x6e\x21',4),'recovery-non-reference':(integer(1)+integer(2)+assign,14),'recovery-call-arity':(b'\x4d\x03\x00',16)}[name]
            errors=[code]
            # Both evaluations execute on the same original VM and script frame.
            fields=event(0x17,[subentry(0x1d,u32(c.expr(bad))),subentry(0x1d,u32(c.expr(r('result')+integer(42)+assign)))])
            expr=r('result')+integer(42)+eq
        elif name.startswith('reentrant-'):
            inner=c.expr(r('result')+integer(7)+assign)
            middle=c.expr(integer(3)+integer(inner)+b'\x74'+add)
            body=integer(2)+integer(middle)+b'\x74'+add
            if name=='reentrant-side-effect':expr=body+integer(12)+eq+r('result')+integer(7)+eq+b'\x6d\x2a'
            else:expr=body+integer(13 if name=='reentrant-false-control' else 12)+eq
            fields=event(0x17,[subentry(0x1d,u32(c.expr(r('predicate')+expr+assign)))])
            expr=r('predicate')
        else:
            body=r('result')+b'\x04\x66'+eq+assign
            fields=event(0x1c,[subentry(0x1d,u32(c.expr(body)))],'callee')+event(0x17,[subentry(0x11c,u32(0)+b'callee\0')])
            expr=b'\x66'+integer(0)+eq if name=='sender-restored' else r('result')+integer(0 if name=='sender-self-false-control' else 1)+eq
        offsets=[c.expr(x) for x in setup];offset=c.expr(expr)
        out[name]=dict(code=c,expression=expr,expected=0 if 'false-control' in name else 1,preludes=setup,offsets=offsets,offset=offset,item_fields=fields,expected_vm_errors=errors)
    return out
