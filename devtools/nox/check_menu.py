#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private v119 parity check. Usage: script CHECKOUT EXE GAME ARTIFACTS.

Requires the retained private fixtures described in README-parity.md.
"""
from pathlib import Path
import configparser,struct,sys,zlib
ce,exe,game,root=map(lambda arg:Path(arg).resolve(),sys.argv[1:])
sys.path[:0]=[str(ce/'devtools/nox'),str(ce/'devtools/ce')]
from test_cli_client import Client
from test_encounter_restart import press
from test_exploration_map import shot
conversion=game/'conversion'
def pixels(path, rect=(350, 304, 532, 352)):
    b = path.read_bytes()
    at = 8
    compressed = b''
    while at < len(b):
        n = int.from_bytes(b[at:at + 4], 'big')
        tag = b[at + 4:at + 8]
        v = b[at + 8:at + 8 + n]
        at += 12 + n
        if tag == b'IHDR':
            w, h, depth, color, *_ = struct.unpack('>IIBBBBB', v)
            assert depth == 8 and color in (2, 6)
            channels = 3 if color == 2 else 4
        if tag == b'IDAT':
            compressed += v
    data = zlib.decompress(compressed)
    stride = w * channels
    prev = bytearray(stride)
    rows = []
    at = 0
    for y in range(h):
        f = data[at]
        at += 1
        row = bytearray(data[at:at + stride])
        at += stride
        for x in range(stride):
            a = row[x - channels] if x >= channels else 0
            u = prev[x]
            ul = prev[x - channels] if x >= channels else 0
            if f == 1:
                row[x] = row[x] + a & 255
            elif f == 2:
                row[x] = row[x] + u & 255
            elif f == 3:
                row[x] = row[x] + (a + u) // 2 & 255
            elif f == 4:
                p = a + u - ul
                pa, pb, pc = (abs(p - a), abs(p - u), abs(p - ul))
                row[x] = row[x] + (a if pa <= pb and pa <= pc else u if pb <= pc else ul) & 255
            else:
                assert f == 0
        rows.append(row)
        prev = row
    assert (w, h) == (560, 384), (w, h)
    left, top, right, bottom = rect
    return b''.join((bytes(row[left * channels:right * channels]) for row in rows[top:bottom]))
root.mkdir()
work=root/'client'
c=Client(exe,work,exe.parent)
cfg=configparser.ConfigParser()
cfg['scummvm']={'disable_saved_screenshot_osd':'true','ce_achievements_popups_enabled':'false','autosave_period':'0','enable_unsupported_game_warning':'false'}
cfg['cli-game']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'check_gamedata':'false'}
with (work/'profile.ini').open('w') as stream:cfg.write(stream)
try:
 c.launch(game,'apple2',game='nox',engine='nox')
 c.command('wait',field='adapter.music_ready',equals=True,timeout_ms=10000)
 press(c,32,32)
 r=(0,0,560,384)
 for row in range(7):
  if row:press(c,274)
  capture=shot(c,work).replace(root/f'menu-{row}.png')
  actual=pixels(capture,r)
  refs=[conversion/'startup1148'/folder/f'original-menu-{row}.png' for folder in ('','phase1','phase2','phase3')]
  nearest=min(sum(a!=b for a,b in zip(actual,pixels(ref,r))) for ref in refs)
  assert nearest==0,(row,nearest)
 press(c,274)
 actual=pixels(shot(c,work),r)
 refs=[conversion/'startup1148'/folder/'original-menu-0.png' for folder in ('','phase1','phase2','phase3')]
 assert any(actual==pixels(ref,r) for ref in refs)
 print('PASS all seven complete menu frames and wrapping against original instruction-rendered references')
finally:c.close()
