# SPDX-License-Identifier: GPL-3.0-or-later
"""Callback and linked code-owner research fixtures."""
from test_mohawk_v5_startup import Code, integer, event, subentry, field, u16, u32
from mohawk_v5_cross_call_probes import target_call


def plans():
    out={}
    for name in ['owner-callback','owner-callback-false-control','owner-linked','owner-linked-false-control','owner-linked-remove','owner-linked-order-only','owner-linked-remove-order-only','owner-item-id']:
        c=Code();c.code=bytearray(8);r=lambda n:c.string(n,True);eq=b'\x6c\x16';add=b'\x6e\x14';assign=b'\x6b\x22';both=b'\x6d\x2a'
        context=lambda me,caller:b'\x04'+integer(0)+add+integer(me)+eq+b'\x66'+integer(0)+add+integer(caller)+eq+both
        setup=[r('back')+integer(0)+assign,r('result')+integer(0)+assign]
        back=r('back')+context(901,902)+assign
        body=r('result')+context(902,901)+assign
        fields=event(0x1c,[subentry(0x1d,u32(c.expr(back)))],'back')+event(0x17,[target_call('callee',902)])
        other=event(0x1c,[target_call('back\0',901),subentry(0x1d,u32(c.expr(body)))],'callee')
        expr=r('back')+integer(0 if name.endswith('false-control') else 1)+eq+r('result')+integer(1)+eq+both
        if name.startswith('owner-linked'):
            setup=[r('result')+integer(0)+assign,r('who')+integer(0)+assign]
            own=r('result')+integer(1)+assign
            donor=r('result')+r('result')+integer(10)+b'\x6e\x0e'+integer(2)+add+assign
            who=r('who')+b'\x04'+integer(0)+add+assign
            other=event(0x1c,[subentry(0x1d,u32(c.expr(donor))),subentry(0x1d,u32(c.expr(who)))],'linked')
            link=c.expr(integer(902)+b'\x4d\x22\x01')
            call=subentry(0x11c,u32(0)+b'linked\0\0')
            entries=[subentry(0x1d,u32(link)),call]
            if name.startswith('owner-linked-remove'):
                remove=c.expr(integer(902)+integer(0)+b'\x4d\x22\x02')
                entries+=[subentry(0x1d,u32(remove)),call]
            fields=event(0x1c,[subentry(0x1d,u32(c.expr(own)))],'linked')+event(0x17,entries)
            expr=r('result')+integer(1 if name.startswith('owner-linked-remove') else 13 if name.endswith('false-control') else 12)+eq+r('who')+integer(901)+eq+both
            if name.endswith('order-only'):expr=r('result')+integer(1 if name.startswith('owner-linked-remove') else 12)+eq
        if name=='owner-item-id':
            body=r('who')+b'\x04'+integer(0)+add+assign
            fields=event(0x17,[subentry(0x1d,u32(c.expr(body)))]);other=b'';expr=r('who')+integer(901)+eq
        offsets=[c.expr(x) for x in setup];offset=c.expr(expr)
        out[name]=dict(code=c,expression=expr,expected=0 if name.endswith('false-control') else 1,preludes=setup,offsets=offsets,offset=offset,item_fields=fields,second_item_fields=other,trace_dispatch=True,expected_dispatch=[(901,901),(902,902)] if name.startswith('owner-callback') else [(901,901)] if name=='owner-item-id' else [(901,901),(901,901),(901,902),(901,902)]+([(901,901),(901,901)] if name.startswith('owner-linked-remove') else []))
    return out
