# SPDX-License-Identifier: GPL-3.0-or-later
"""Two-item context and expression-call result fixtures; no engine changes."""
from test_mohawk_v5_startup import Code, integer, event, subentry, u16, u32


def target_call(name, target):
    body=u16(0x11c)+u16(65535)+u16(1)+u16(0)+u16(target)+u32(0)+name.encode()+b'\0'
    return u16(len(body))+body


def plans():
    out={}
    names=['distinct-sender-self','distinct-sender-false-control','distinct-sender-restored','expression-return','expression-return-false-control','expression-return-preserves-left','expression-return-copy','expression-return-void','expression-return-nested','expression-return-siblings','expression-return-distinct']
    for name in names:
        c=Code();c.code=bytearray(8);r=lambda n:c.string(n,True);t=lambda n:c.string(n)
        eq=b'\x6c\x16';assign=b'\x6b\x22';add=b'\x6e\x14';both=b'\x6d\x2a'
        setup=[r('result')+integer(0)+assign];other=b''
        # The original reader consumes argument/target delimiters and one
        # trailing token before returning to the surrounding expression.
        invoke=lambda n:t(n)+b'\x77\x10\x10\x10\x07'
        if name.startswith('distinct-'):
            body=r('result')+b'\x04'+integer(0)+add+integer(902)+eq+b'\x66'+integer(0)+add+integer(901)+eq+both+assign
            other=event(0x1c,[subentry(0x1d,u32(c.expr(body)))],'callee')
            fields=event(0x17,[target_call('callee',902)])
            expr=b'\x66'+integer(0)+eq if name=='distinct-sender-restored' else r('result')+integer(0 if 'false-control' in name else 1)+eq
        else:
            entries=[subentry(0xfffc,u32(c.expr(integer(42))))]
            if name=='expression-return-void':entries=[subentry(0x1d,u32(c.expr(integer(17)))),subentry(0xfffc)]
            fields=event(0x1c,entries,'callee')
            value=invoke('callee')
            if name=='expression-return-nested':
                fields+=event(0x1c,[subentry(0xfffc,u32(c.expr(integer(1)+value+add)))],'outer')
                value=invoke('outer')
            if name=='expression-return-distinct':
                other=fields;fields=b'';value=t('callee')+b'\x77\x10\x10'+integer(902)+b'\x07\x10\x07'
            if name=='expression-return-copy':
                body=r('first')+value+assign
                next_body=r('second')+invoke('different')+assign
                fields+=event(0x1c,[subentry(0xfffc,u32(c.expr(integer(99))))],'different')
                expr=r('first')+integer(42)+eq+r('second')+integer(99)+eq+both
            else:
                body=r('result')+(integer(3) if name=='expression-return-preserves-left' else b'')+value+(add if name=='expression-return-preserves-left' else b'')+assign
                expected=17 if name=='expression-return-void' else 43 if name in ['expression-return-nested','expression-return-false-control'] else 45 if name=='expression-return-preserves-left' else 42
                expr=r('result')+integer(expected)+eq
            if name=='expression-return-siblings':
                fields+=event(0x1c,[subentry(0xfffc,u32(c.expr(integer(99))))],'different')
                body=r('result')+value+invoke('different')+add+assign;expr=r('result')+integer(141)+eq
            entries=[subentry(0x1d,u32(c.expr(body)))]
            if name=='expression-return-copy':entries.append(subentry(0x1d,u32(c.expr(next_body))))
            fields+=event(0x17,entries)
        offsets=[c.expr(x) for x in setup];offset=c.expr(expr)
        out[name]=dict(code=c,expression=expr,expected=0 if 'false-control' in name else 1,preludes=setup,offsets=offsets,offset=offset,item_fields=fields,second_item_fields=other)
    return out
