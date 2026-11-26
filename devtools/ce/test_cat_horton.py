#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise Horton's real shuffle, bidirectional mapping, swaps and completion."""
import argparse
import hashlib
import json
from pathlib import Path
import time
from test_cli_client import Client

p = argparse.ArgumentParser(description=__doc__)
for name in ('exe', 'assets', 'extra', 'root'):
    p.add_argument('--'+name, type=Path, required=True)
p.add_argument('--offscreen', action='store_true')
p.add_argument('--rounds', type=int, default=3)
p.add_argument('--expect-crash', action='store_true')
a = p.parse_args()
c = Client(a.exe.resolve(), a.root.resolve(), a.extra.resolve(), offscreen=a.offscreen)
result = dict(exe_sha256=hashlib.sha256(a.exe.read_bytes()).hexdigest(), rounds=[])

def state():
    s = c.command('inspect', timeout=15)['adapter']
    (a.root/'latest-state.json').write_text(json.dumps(s, indent=2))
    return s

def wait(predicate, timeout=120):
    end = time.monotonic()+timeout
    while time.monotonic() < end:
        s = state()
        if predicate(s):
            return s
        time.sleep(.05)
    raise AssertionError(('timeout', s['mode'], s['page'], s['phase']))

def item(s, name):
    return next(i for i in s['items'] if i['name'] == name)

def mouse(kind, x, y, **fields):
    c.command('input', kind=kind, x=x, y=y, **fields)
    time.sleep(.08)

def click(x, y):
    mouse('mouse_move', x, y)
    for phase in ('down', 'up'):
        mouse('mouse_button', x, y, button=1, phase=phase)

def key(k):
    for phase in ('down', 'up'):
        c.command('input', kind='key', key=k, phase=phase)
        time.sleep(.08)

def mapping(s):
    v = s['variables']
    pieces = {f'P{n}' for n in range(1, 7)}
    slots = {f'QD{n}' for n in range(1, 7)}
    assert {v[k] for k in slots} == pieces, v
    assert {v[k] for k in pieces} == slots, v
    assert all(v[v[k]] == k for k in pieces | slots), v
    return {k: v[k] for k in pieces | slots}

def drag(s, piece, slot):
    l, t, r, b = item(s, piece)['rect']
    x, y = (l+r)//2, (t+b)//2
    l, t, r, b = item(s, slot)['rect']
    dx, dy = (l+r)//2, (t+b)//2
    mouse('mouse_move', x, y)
    mouse('mouse_button', x, y, button=1, phase='down')
    assert item(state(), piece)['dragging'], ('piece did not enter drag', piece)
    for n in range(1, 7):
        mouse('mouse_move', x+(dx-x)*n//6, y+(dy-y)*n//6)
    mouse('mouse_button', dx, dy, button=1, phase='up')
    return wait(lambda s: not item(s, piece)['dragging'])

try:
    c.launch(a.assets.resolve(), 'windows', 'catinthehat', 'mohawk')
    time.sleep(2)
    key(27)
    wait(lambda s: s['mode']==2 and s['page']==1 and s['phase']==3 and s['cursor_visible'])
    click(106, 430)
    wait(lambda s: s['mode']==2 and s['page']==3 and s['phase']==3)
    for _ in range(3):
        click(557, 324)
        time.sleep(2)
    click(297, 324)
    s = wait(lambda s: s['mode']==6)
    for _ in range(20):
        if s['page']==7:
            break
        assert s['page'] < 7, s['page']
        key(275)
        time.sleep(.5)
        s = state()
    assert s['page']==7, s['page']
    s = wait(lambda s: s['phase']==3 and s['cursor_visible'])
    c.command('screenshot')
    for round_no in range(a.rounds):
        click(191, 182)
        if a.expect_crash:
            time.sleep(3)
            state()
            raise AssertionError('expected the old dynamic assignment error')
        s = wait(lambda s: s['variables'].get('str1')=='' and not item(s, 'JumbleAnim')['playing'])
        before = mapping(s)
        assert s['variables']['str1']=='', s['variables']
        assert any(before[f'QD{n}'] != f'P{n}' for n in range(1, 7)), before
        c.command('screenshot')
        swaps = []
        for n in range(1, 7):
            piece, slot = f'P{n}', f'QD{n}'
            if mapping(s)[slot] == piece:
                continue
            old = mapping(s)
            displaced, origin = old[slot], old[piece]
            s = drag(s, piece, slot)
            now = mapping(s)
            assert now[piece]==slot and now[slot]==piece, (old, now)
            assert now[displaced]==origin and now[origin]==displaced, (old, now)
            swaps.append(dict(piece=piece, destination=slot, displaced=displaced, mapping=now))
        assert s['variables']['matches']==6, s['variables']
        wait(lambda s: item(s, 'HortonAnim')['playing'])
        c.command('screenshot')
        s = wait(lambda s: not item(s, 'HortonAnim')['playing'] and item(s, 'Horton')['visible'])
        assert all(mapping(s)[f'QD{n}']==f'P{n}' for n in range(1, 7))
        result['rounds'].append(dict(shuffle=before, swaps=swaps, completed=True))
        print('completed round', round_no+1, flush=True)
    key(293)
    state()
    captures = list((a.root/'logs/diagnostics').glob('*.json'))
    assert len(captures)==1
    report = json.loads(captures[0].read_text())
    assert report['engine_state']['variables']['matches']==6
    assert report['screenshot_saved']
    result.update(status='passed', diagnostic=str(captures[0]))
except Exception as e:
    result.update(status='failed', error=repr(e))
    if a.expect_crash:
        text = (a.root.parent/(a.root.name+'-stderr.log')).read_text(errors='replace')
        if 'missing EOS (got 22)' in text:
            result.update(status='passed', expected_error='missing EOS (got 22)')
finally:
    try:
        c.close()
    except Exception:
        c.abort()
    (a.root/'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)
raise SystemExit(0 if result['status']=='passed' else 1)
