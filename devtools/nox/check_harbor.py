#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private v119 parity check. Usage: script CHECKOUT EXE GAME ARTIFACTS.

Requires the retained private fixtures described in README-parity.md.
"""
from pathlib import Path
import sys
ce,exe,game,root=map(lambda arg:Path(arg).resolve(),sys.argv[1:])
sys.path[:0]=[str(ce/'devtools/nox'), str(ce/'devtools/ce')]
from test_encounter_restart import inspect,launch,press
from test_exploration_map import shot
from test_party_invulnerability import core_parts
c=game/'conversion/ce-m4-vendor347-world'
root.mkdir()
def fixture(folder,name='nox-native-exploration.bin'):
 return c/folder/'saves'/name
def opened(folder):
 return launch(exe,root/folder,game,exe.parent,fixture(folder),False,0)
vendor='boat-menu-cold'
client=opened(vendor)
try:
 before=inspect(client)["adapter"]["gold"]
 shot(client,root/vendor)
 press(client,49,49)
 shot(client,root/vendor)
 press(client,121,89)
 shot(client,root/vendor)
 assert inspect(client)['adapter']['gold']==before-55
 print('PASS harbor vendor menu, Skiff purchase and original 55-gold debit')
finally:client.close()
for name,x,y,foot,key in [
 ('boat1-real-B-owned-authored-boat',14,17,False,(98,66)),
 ('boat1-real-sail-owned-purchase',14,16,False,(273,0)),
 ('boat1-real-X-after-purchased-sailing',14,16,True,(120,88)),
]:
 client=opened(name)
 try:
  press(client,*key)
  a=inspect(client)['adapter']
  assert (a['x'],a['y'],a['on_foot'])==(x,y,foot),a
  print('PASS',name,x,y,foot)
 finally:client.close()
