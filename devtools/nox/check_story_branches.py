#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private v119 parity check. Usage: script CHECKOUT EXE GAME ARTIFACTS.

Requires the retained private fixtures described in README-parity.md.
"""
from pathlib import Path
import sys
ce,exe,game,root=map(lambda arg:Path(arg).resolve(),sys.argv[1:])
sys.path[:0]=[str(ce/'devtools/nox'),str(ce/'devtools/ce')]
from test_encounter_restart import Client,inspect,launch,press,save_state,select_save
from test_exploration_map import shot
fixtures=game/'conversion/ce-native-integration527'
data=(game/'Nox Archaist Extracted/NOXARCHAIST/NA/GAME').read_bytes()
root.mkdir()
cases=[
 ('portal-refusal',273,54,36,11,0,None),
 ('portal',273,57,38,44,0,None),
 ('barrier',274,73,12,25,data[0x155],None),
 ('protected',274,73,12,26,0,None),
 ('plate-legal',274,64,11,12,0,(31,1)),
]
for name,key,map_id,x,y,damage,flag in cases:
 work=root/name
 source=fixtures/f'I3-setup-{name}'/'saves/nox-native-controlled-world-before-input.bin'
 c=launch(exe,work,game,exe.parent,source,False,0)
 try:
  before=inspect(c)['adapter']
  press(c,key)
  for _ in range(15):
   after=inspect(c)['adapter']
   if after['can_save']:break
   press(c,13,13)
  else:raise AssertionError(name)
  assert (after['map'],after['x'],after['y'])==(map_id,x,y),(name,after)
  assert after['party'][0]['hp']==before['party'][0]['hp']-damage,(name,damage)
  if flag:assert after['quest_fields'][flag[0]]==flag[1],name
  shot(c,work)
  saved=save_state(c,work,101)
 finally:c.close()
 cold=Client(exe,work,exe.parent)
 try:
  cold.launch(game,'apple2',game='nox',engine='nox')
  cold.command('wait',field='adapter.stage',equals='title',timeout_ms=10000)
  cold.command('load',chooser_slot=select_save(cold,101)['chooser_slot'])
  restored=save_state(cold,work,102)
  for section in ('CORE','RAND','INPT','TURN','BTTL'):
   assert restored[section]==saved[section],(name,section)
 finally:cold.close()
 print('PASS',name,'ordinary movement and cold continuation')

# Load the earned Ruin 30 approach unchanged, and pair it with a declared
# missing-flag checkpoint at the same authored gate.
from test_party_invulnerability import encode_checkpoint, sections
earned=game/'conversion/ce-native-acceptance528/earned-modern/saves'
raw=next(earned.glob('*slot591.g0')).read_bytes()
at=raw.index(b'NPLS',32)
size=int.from_bytes(raw[at+6:at+10],'little')
packet=raw[at+10:at+10+size]
for have_key in (False,True):
 parts=sections(packet)
 core=bytearray(parts['CORE'])
 at=core.index(b'NWLD')+11+12
 assert core[at]&8
 if not have_key:
  core[at]&=~8
  parts['CORE']=core
 source=root/f'dark-key-{have_key}.bin'
 source.write_bytes(packet if have_key else encode_checkpoint(packet,parts))
 work=root/f'dark-gate-{have_key}'
 c=launch(exe,work,game,exe.parent,source,False,0)
 try:
  a=inspect(c)['adapter']
  assert (a['map'],a['x'],a['y'])==(30,33,8)
  press(c,111,79)
  press(c,275)
  for _ in range(6 if have_key else 1):press(c,275)
  a=inspect(c)['adapter']
  expected=(97,150,167) if have_key else (30,33,8)
  assert (a['map'],a['x'],a['y'])==expected,a
  shot(c,work)
 finally:c.close()
 print('PASS Dark Key gate',have_key,expected)
