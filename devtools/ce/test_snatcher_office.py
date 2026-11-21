#!/usr/bin/env python3
"""Bounded ordinary-input first chief-office entry from a reception checkpoint."""
import argparse, json, shutil, threading
from pathlib import Path
from test_snatcher_integration import Client, sha

p=argparse.ArgumentParser()
for name in ('exe','game','sidecar','checkpoint','output'):
    p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--decensor',action='store_true')
p.add_argument('--extended',action='store_true')
p.add_argument('--office-start',action='store_true')
a=p.parse_args();a.output.mkdir()
c=Client(a.exe,a.output/'profile',a.game,'gens')
c.watch.cancel();c.watch=threading.Timer(600 if a.extended else 300,c.p.kill);c.watch.start()
states=[]
def state():
    s=c.command('inspect')['adapter']
    assert not any(s.get(k) for k in ('blocker','runtime_error','render_error','audio_error')),s
    return s
def tap(code):
    for phase in ('down','up'):
        c.command('input',kind='key',key=code,phase=phase,map_physical=True)
        c.command('advance',frames=2)
def ready():
    for _ in range(120):
        s=state()
        if s['command_ready'] and s['mouse_ready'] and s['choice_references']:return s
        pending=s.get('pending_native_request',{})
        if pending.get('kind')==7 and pending.get('first')==33:tap(32)
        elif s['dialogue_waiting']:tap(13)
        c.command('advance',frames=60)
    raise AssertionError('Ordinary dialogue did not finish')
def pick(ref,via=None):
    for _ in range(5):
        s=ready();refs=s['choice_references']
        if ref in refs:break
        if via in refs:pick(via);continue
        if s['selected_path']:tap(8);c.command('advance',frames=40);continue
        raise AssertionError((ref,refs))
    assert ref in refs,(ref,refs)
    for _ in range(16):
        if s['command_reference']==ref:break
        tap(274);c.command('advance',frames=16);s=ready()
    assert s['command_reference']==ref,s
    tap(13);c.command('advance',frames=40)
    s=ready();states.append(dict(reference=ref,state=s))
    (a.output/'states.json').write_text(json.dumps(states,indent=2))
    return s
try:
    profile=c.root/'profile.ini'
    profile.write_text(profile.read_text().replace('snatcher_decensor=false','snatcher_decensor='+str(a.decensor).lower()).replace('[scummvm]','[scummvm]\nextrapath='+str(a.sidecar.parent)))
    (c.root/'saves').mkdir(exist_ok=True)
    shutil.copy2(a.checkpoint,c.root/'saves/cli-game.005')
    c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game))
    c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
    c.command('load',slot=5)
    c.command('input',kind='logical',action='trace')
    if a.office_start:
        office=ready()
    else:
        pick(410);pick(725)
        pick(149,290);pick(149,290);pick(194,290)
        hall=pick(410)
        c.command('save',slot=19,description='Before first chief office entry')
        office=pick(322)
    assert (office['scene_resource'],office['scene_index'])==(10,1),office
    assert office['scene_layout']['scroll_y1']==-88*65536,office['scene_layout']
    assert office['scene_layout']['y1']==88,'Office background retained the pre-scroll origin'
    c.command('save',slot=20,description='First chief office entry')
    c.command('screenshot')
    (a.output/'office.json').write_text(json.dumps(office,indent=2))
    if a.extended:
        pick(149,290);pick(149,290)
        for topic in (87,110,130,269):pick(topic,290)
        pick(639);pick(715);pick(547)
        c.command('wait',field='adapter.pending_graphics_tasks',equals=0,timeout_ms=10000)
        c.command('screenshot')
        c.command('save',slot=21,description='Genuine inventory item shown')
        pick(433);revisited=pick(322)
        assert (revisited['scene_resource'],revisited['scene_index'])==(10,0),revisited
        assert revisited['scene_layout']['y1']==88
        c.command('screenshot')
        pick(433);computer=pick(346)
        assert (computer['scene_resource'],computer['scene_index'])==(12,4),computer
        c.command('screenshot')
        c.command('save',slot=22,description='Computer room via ordinary input')
finally:c.close()
if a.extended:
    c=Client(a.exe,a.output/'profile',a.game,'gens',reuse=True)
    c.label='computer-cold'
    try:
        c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game))
        c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
        c.command('load',slot=22)
        cold=state()
        for field in ('resource','node','packed_digest','scene_resource','scene_index','choice_references','selected_path'):
            assert cold[field]==computer[field],(field,cold[field],computer[field])
        assert cold['scene_layout']==computer['scene_layout']
        c.command('advance',frames=30);state();c.command('screenshot')
        c.command('save',slot=23,description='Cold continued computer room')
    finally:c.close()
(a.output/'receipt.json').write_text(json.dumps(dict(status='PASS',executable_sha256=sha(a.exe),checkpoint_sha256=sha(a.checkpoint),decensor=a.decensor,extended=a.extended,office_start=a.office_start,cold_continue=a.extended,ordinary_input=True,real_devices=False),indent=2))
