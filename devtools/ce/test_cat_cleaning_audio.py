#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cleaning-scene-only background continuity, pickup audio and page cleanup."""
import argparse,hashlib,json,time
from pathlib import Path
from test_cli_client import Client
p=argparse.ArgumentParser(description=__doc__)
for name in ('exe','assets','extra','root'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--offscreen',action='store_true')
a=p.parse_args();c=Client(a.exe.resolve(),a.root.resolve(),a.extra.resolve(),offscreen=a.offscreen)
result=dict(exe_sha256=hashlib.sha256(a.exe.read_bytes()).hexdigest(),interactions=[],samples=[])
last=None
def state():
    s=c.command('inspect',timeout=15)['adapter']
    (a.root/'latest-state.json').write_text(json.dumps(s,indent=2));return s
def item(s,name):return next(i for i in s['items'] if i['name']==name)
def monitor():
    global last
    s=state();amb=item(s,'AmbSND')
    assert amb['playing'] and amb['sound']==1073 and amb['sound_starts']==1,amb
    samples=amb['sound_samples']
    assert last is None or samples>=last,('background restarted',last,samples)
    last=samples;result['samples'].append(samples);return s
def wait(test,timeout=120,watch=False):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        s=monitor() if watch else state()
        if test(s):return s
        time.sleep(.04)
    raise AssertionError(('timeout',s['mode'],s['page'],s['phase']))
def key(k):
    for phase in ('down','up'):
        c.command('input',kind='key',key=k,phase=phase);time.sleep(.08)
def click(x,y):
    c.command('input',kind='mouse_move',x=x,y=y)
    for phase in ('down','up'):
        c.command('input',kind='mouse_button',button=1,x=x,y=y,phase=phase);time.sleep(.08)
try:
    c.launch(a.assets.resolve(),'windows','catinthehat','mohawk');time.sleep(2);key(27)
    wait(lambda s:s['mode']==2 and s['page']==1 and s['phase']==3 and s['cursor_visible'])
    click(106,430);wait(lambda s:s['mode']==2 and s['page']==3 and s['phase']==3)
    for _ in range(3):click(557,324);time.sleep(2)
    click(297,324);s=wait(lambda s:s['mode']==6)
    for _ in range(40):
        if s['page']==12:break
        assert s['page']<12,s['page']
        key(275);time.sleep(.4);s=state()
    assert s['page']==12
    wait(lambda s:s['phase']==3 and s['cursor_visible'])
    monitor();c.command('screenshot')
    start=time.monotonic()
    for name,x,y in [('BookA',161,440),('Chair',400,425),('Phone',607,304)]:
        before=last;click(x,y)
        wait(lambda s:item(s,name+'Grab')['playing'],watch=True)
        clips={};end=time.monotonic()+30
        while time.monotonic()<end:
            s=monitor();grab=item(s,name+'Grab')
            if grab['sound']!=65535:
                samples=grab['sound_samples'];clip=clips.setdefault(str(grab['sound']),[samples,samples])
                clip[0]=min(clip[0],samples);clip[1]=max(clip[1],samples)
            if not grab['playing'] and s['cursor_visible']:break
            time.sleep(.04)
        else:raise AssertionError(('pickup did not finish',name))
        assert clips and all(v[1]>v[0] for v in clips.values()),(name,clips)
        assert not item(s,name)['visible'],('object not cleaned',name)
        assert last>before
        result['interactions'].append(dict(object=name,clips=clips,background_before=before,background_after=last))
        print('cleaned',name,'background samples',last,flush=True)
    while time.monotonic()-start<35:monitor();time.sleep(.08)
    assert last-result['samples'][0]>2*300866,('did not span two source loops',last)
    key(293);monitor()
    report_path=next((a.root/'logs/diagnostics').glob('*.json'))
    report=json.loads(report_path.read_text());amb=item(report['engine_state'],'AmbSND')
    assert amb['sound_starts']==1 and amb['sound']==1073 and report['screenshot_saved']
    result['diagnostic']=str(report_path)
    key(275);s=wait(lambda s:s['page']!=12)
    assert not any(i['name']=='AmbSND' for i in s['items'])
    result.update(status='passed',checks=['one continuous ambient start across multiple loop lengths and three pickups',
        'foreground pickup clips advance samples; objects are cleaned','page exit removes ambient owner and uses normal page audio cleanup'])
except Exception as e:result.update(status='failed',error=repr(e));print(repr(e),flush=True)
finally:
    try:c.close()
    except Exception:c.abort()
    (a.root/'result.json').write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k!='samples'}),flush=True)
raise SystemExit(0 if result['status']=='passed' else 1)
