#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused Cat clock input, both winding directions, all hours and audio."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import time
from test_cli_client import Client

p=argparse.ArgumentParser(description=__doc__)
for name in ('exe','assets','extra','root'):
    p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--offscreen',action='store_true')
p.add_argument('--controls-only',action='store_true',help='One hour boundary, then focused release/re-entry checks')
a=p.parse_args()
c=Client(a.exe.resolve(),a.root.resolve(),a.extra.resolve(),offscreen=a.offscreen)
result=dict(exe_sha256=hashlib.sha256(a.exe.read_bytes()).hexdigest(),hours=[],checks=[],controls_only=a.controls_only)
sounds={1:{1013},2:{1014,1015,1016},3:{1017,1018,1019},4:{1020},
        5:{1021,1022,1023},6:{1024},7:{1025},8:{1026,1027,1028},
        9:{1029},10:{1030},11:{1031},12:{1032,1033,1034}}
animated={2,3,5,8,12}

def state():
    s=c.command('inspect',timeout=15)['adapter']
    (a.root/'latest-state.json').write_text(json.dumps(s,indent=2))
    return s

def wait(predicate,timeout=120):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        s=state()
        if predicate(s):return s
        time.sleep(.03)
    raise AssertionError(('timeout',s['mode'],s['page'],s['phase'],s['variables']))

def item(s,name):
    return next(i for i in s['items'] if i['name']==name)

def mouse(kind,x,y,**fields):
    c.command('input',kind=kind,x=x,y=y,**fields)
    time.sleep(.06)

def click(x,y):
    mouse('mouse_move',x,y)
    for phase in ('down','up'):mouse('mouse_button',x,y,button=1,phase=phase)

def key(k):
    for phase in ('down','up'):
        c.command('input',kind='key',key=k,phase=phase)
        time.sleep(.08)

def point(n):
    return round(209+20*math.sin(n*math.pi/6)),round(241-34*math.cos(n*math.pi/6))

def move(n):
    mouse('mouse_move',*point(n))
    s=state()
    assert int(s['variables']['ZoneNum'])==n,('wrong dial zone',n,s['rollover'],s['variables'])
    assert item(s,f'min{n}')['visible'],('minute hand did not move',n,s['variables'])
    expected_hour_hand=f"hr{s['variables']['HrNum']}"+('h' if 6<=n<12 else '')
    assert s['variables']['CurHr']==expected_hour_hand,s['variables']
    assert item(s,expected_hour_hand)['visible'],('hour hand did not move',s['variables'])
    return s

def grab(n):
    # Exactly one entry move: a second move must not be needed to position
    # the synthetic hand pointer before the first click.
    mouse('mouse_move',250,300)
    mouse('mouse_move',*point(n))
    s=state();hand=item(s,'Hand')
    x,y=point(n);l,t,r,b=hand['rect']
    assert hand['visible'] and l<=x<r and t<=y<b,('stale hover pointer',hand)
    mouse('mouse_button',x,y,button=1,phase='down')
    s=state()
    assert s['focus']==1910 and s['variables']['HandDown']==1,('clock not grabbed',s['focus'],s['variables'])
    return s

def finish_hour(s,expected,direction):
    name=f'Hour{expected}'
    assert s['variables']['HrNum']==expected and item(s,name)['playing'],('wrong hour',expected,s['variables'])
    clips={};drawn=False;maximum_position=0
    started_ms=max((f['millis'] for f in s['render_history']),default=0)
    hold_to_end = expected in (4,6)
    if not hold_to_end:mouse('mouse_button',*point(12),button=1,phase='up')
    end=time.monotonic()+45
    while time.monotonic()<end:
        s=state();hour=item(s,name)
        maximum_position=max(maximum_position,hour['position'])
        sound=hour['sound']
        if sound!=65535:
            samples=hour['sound_samples']
            clip=clips.setdefault(sound,dict(minimum=samples,maximum=samples))
            clip['minimum']=min(clip['minimum'],samples)
            clip['maximum']=max(clip['maximum'],samples)
        drawn |= any(f['millis']>=started_ms and hour['id'] in f['drawable_animations'] for f in s['render_history'])
        if not hour['playing'] and s['variables']['HourChime']==0:break
        time.sleep(.03)
    else:raise AssertionError(('hour did not finish',name))
    assert set(clips)==sounds[expected],(name,clips,sounds[expected])
    assert all(v['maximum']>v['minimum'] for v in clips.values()),(name,clips)
    assert hour['uninitialized_draws']==0,(name,hour)
    if expected in animated:
        assert maximum_position>0 and drawn,('hour graphics never drawn',name)
    if hold_to_end:
        # Exercise both a stationary release after the sequence and a move
        # while still holding, before release. Neither may leave HandDown set.
        x,y=point(12)
        if expected == 4:
            x+=1
            mouse('mouse_move',x,y)
        mouse('mouse_button',x,y,button=1,phase='up')
        s=state()
    assert s['focus']==0 and s['variables']['HandDown']==0
    assert item(s,'Clock')['enabled'] and item(s,'Clock')['visible']
    result['hours'].append(dict(hour=expected,direction=direction,clips=clips,
                                maximum_position=maximum_position,graphics_drawn=drawn,
                                held_through_completion=hold_to_end))
    print('completed',direction,name,flush=True)
    return s

try:
    c.launch(a.assets.resolve(),'windows','catinthehat','mohawk');time.sleep(2);key(27)
    wait(lambda s:s['mode']==2 and s['page']==1 and s['phase']==3 and s['cursor_visible'])
    click(106,430);wait(lambda s:s['mode']==2 and s['page']==3 and s['phase']==3)
    for _ in range(3):click(557,324);time.sleep(2)
    click(297,324);s=wait(lambda s:s['mode']==6)
    for _ in range(25):
        if s['page']==8:break
        assert s['page']<8,s['page']
        key(275);time.sleep(.5);s=state()
    assert s['page']==8
    s=wait(lambda s:s['phase']==3 and s['cursor_visible'])
    assert s['variables']['HrNum']==2
    c.command('screenshot')
    grab(6)
    for n in range(7,13):s=move(n)
    s=finish_hour(s,3,'clockwise')
    result['checks'].append('single-entry hover and immediate click grab the hand')
    expected=3
    if not a.controls_only:
        for _ in range(11):
            grab(12)
            for n in range(1,13):s=move(n)
            expected=expected%12+1
            s=finish_hour(s,expected,'clockwise')
        assert {h['hour'] for h in result['hours']}==set(range(1,13))
        result['checks'].append('all twelve hourly sequences, authored sounds, graphics and forward 12-to-1 wrap')
        result['checks'].append('hold through hourly completion, then release stationary or after moving')
        for _ in range(2):
            grab(12)
            for n in list(range(11,0,-1))+[12]:s=move(n)
            expected=(expected+10)%12+1
            s=finish_hour(s,expected,'counterclockwise')
        assert expected==12
        result['checks'].append('reverse winding and 1-to-12 wrap')
    grab(6)
    mouse('mouse_button',*point(6),button=1,phase='up')
    s=state();assert s['focus']==0 and s['variables']['HandDown']==0
    # Repeated stationary grabs must work after release, too.
    for _ in range(2):
        mouse('mouse_button',*point(6),button=1,phase='down')
        s=state();assert s['focus']==1910 and s['variables']['HandDown']==1
        mouse('mouse_button',*point(6),button=1,phase='up')
        s=state();assert s['focus']==0 and s['variables']['HandDown']==0
    grab(12)
    mouse('mouse_move',250,300)
    s=state();assert s['variables']['ClockRoll']==0 and not item(s,'HandCursor')['visible']
    # The authored clock only accepts adjacent sectors after an hour boundary.
    s=move(1)
    assert s['variables']['ClockRoll']==1 and s['variables']['HandDown']==1
    mouse('mouse_move',250,300)
    mouse('mouse_button',250,300,button=1,phase='up')
    s=state();assert s['focus']==0 and s['variables']['HandDown']==0
    assert item(s,'Clock')['enabled'] and item(s,'Clock')['visible']
    grab(6)
    mouse('mouse_button',*point(6),button=1,phase='up')
    result['checks'].append('stationary re-grab, release inside/outside, leave/re-enter while held')
    c.command('screenshot');key(293);state()
    reports=list((a.root/'logs/diagnostics').glob('*.json'));assert len(reports)==1
    report=json.loads(reports[0].read_text())
    assert report['screenshot_saved'] and 'HrNum' in report['engine_state']['variables']
    assert report['engine_state']['variables']['HandDown']==0 and report['engine_state']['focus']==0
    assert report['executable_md5']==hashlib.md5(a.exe.read_bytes()).hexdigest()
    result.update(status='passed',diagnostic=str(reports[0]))
except Exception as e:
    result.update(status='failed',error=repr(e));print(repr(e),flush=True)
finally:
    try:c.close()
    except Exception:c.abort()
    (a.root/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result),flush=True)
raise SystemExit(0 if result['status']=='passed' else 1)
