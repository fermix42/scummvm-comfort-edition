# SPDX-License-Identifier: GPL-3.0-or-later
"""Selected original filesystem/cache initialization; serialized original fallback."""
from probe_mohawk_v5_original import u32
from probe_mohawk_v5_native_resources import Native


def boot(n):
    n.write(0x48366f,b'\xb8'+u32(0x12c)+b'\xc2\x0c\x00')
    n.call(0x47cd44,args=[1],cdecl=True);n.call(0x47a9ec);n.call(0x477810);n.s+=b'\xdb\xe3'
    n.call(0x44b859,n.remote+0x2500);n.s+=b'\xa3'+u32(0x49af84)+b'\xa3'+u32(0x49af80)
    n.write(0x49af78,b'\x01\x00');n.write(0x49af7a,b'\x01\x00')
    n.call(0x456fe5,args=[0,0]);n.s+=b'\x0f\xb7\xc0';n.save(0)
    n.call(0x45424a,args=[0]);n.s+=b'\x0f\xb7\xc0';n.save(1)
    n.call(0x456b06,args=[0]);n.s+=b'\x0f\xb7\xc0';n.save(15);n.s+=b'\x66\xa3'+u32(0x49b176)
    n.call(0x483c5d,args=[n.mem(15),1]);n.call(0x483dda);n.s+=b'\x66\xa3'+u32(0x49b174)


def open_archive(n,path):
    n.write(n.remote+0x2200,path.encode('ascii')+b'\0');n.call(0x4531df,n.remote+0x2000,[n.remote+0x2200])
    n.call(0x45a9bc,args=[n.remote+0x2000,1]);n.save(3)
    n.call(0x458edc,args=[0x42434f44,900,n.mem(3)]);n.save(4)
