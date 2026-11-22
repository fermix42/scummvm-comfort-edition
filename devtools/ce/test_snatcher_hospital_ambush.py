#!/usr/bin/env python3
"""Recover the owner hospital combat snapshot and exercise ordinary mouse fire."""
import argparse,json,shutil,threading
from pathlib import Path
from test_snatcher_integration import Client,sha

import struct,zlib
p=argparse.ArgumentParser()
for k in ('exe','game','checkpoint','report','resources','output'):p.add_argument('--'+k,type=Path,required=True)
a=p.parse_args();a.output.mkdir();original=json.loads(a.report.read_text())['engine'];expected=sha(a.checkpoint)
def png(path,data):
 def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v))
 w,h=256,224;rows=b''.join(b'\0'+data[y*w*3:(y+1)*w*3] for y in range(h));path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
def client(reuse=False):
 c=Client(a.exe,a.output/'profile',a.game,'gens',reuse=reuse);c.watch.cancel();c.watch=threading.Timer(300,c.p.kill);c.watch.start();return c
fields=('packed_digest','shooting_damage','shooting_shots','shooting_clear_hits','shooting_set_hits','shooting_spawned','shooting_phase','shooting_mode')
c=client()
try:
 cfg=c.root/'profile.ini';cfg.write_text(cfg.read_text().replace('autosave_period=1','autosave_period=0').replace('snatcher_decensor=false','snatcher_decensor=true').replace('[scummvm]','[scummvm]\nextrapath='+str(a.resources)))
 saves=c.root/'saves';saves.mkdir(exist_ok=True);shutil.copy2(a.checkpoint,saves/'cli-game.065')
 c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
 loaded=c.command('load',slot=65,capture_rgb_slot=70);state=loaded['checkpoint']
 for k in fields:assert state[k]==original[k],(k,state[k],original[k])
 assert state['shooting_targets']==original['shooting_targets'],'Recovery changed target/animation state'
 assert any(t['category']=='hospital_map_recovered' for t in state['trace']),'Old erroneous map was not recognized'
 data=(saves/loaded['boundary_rgb']).read_bytes();png(a.output/'recovered.png',data);(a.output/'recovered-state.json').write_text(json.dumps(state,indent=2))
 c.command('save',slot=70,description='Hospital map recovery cold regression')
finally:c.close()
c=client(reuse=True)
try:
 c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
 loaded=c.command('load',slot=70,capture_rgb_slot=71);cold=loaded['checkpoint']
 for k in fields:assert cold[k]==state[k],(k,cold[k],state[k])
 assert (saves/loaded['boundary_rgb']).read_bytes()==data,'Corrected map did not survive cold save/load'
 assert not any(t['category']=='hospital_map_recovered' for t in cold['trace']),'Correct save was rewritten again'
 # The retained source profile has its one authored target in cell1. Aim and
 # fire through ordinary events; no target, RNG, state or story injection.
 c.command('input',kind='mouse_move',x=128,y=24);c.command('advance',frames=1)
 for phase in ('down','up'):
  c.command('input',kind='mouse_button',button=1,phase=phase,x=128,y=24);c.command('advance',frames=1)
 states=[]
 for _ in range(120):
  now=c.command('inspect')['adapter'];states.append(now)
  assert not any(now.get(k) for k in ('blocker','runtime_error','render_error','audio_error')),{k:now.get(k) for k in ('blocker','runtime_error','render_error','audio_error')}
  if now['shooting_clear_hits']>cold['shooting_clear_hits'] or now['shooting_set_hits']>cold['shooting_set_hits']:break
  c.command('advance',frames=1)
 else:raise AssertionError('Ordinary aimed shot did not hit the active target')
 (a.output/'shot-states.json').write_text(json.dumps(states,indent=2));assert now['shooting_shots']==cold['shooting_shots']+1
 assert sha(a.checkpoint)==expected and sha(saves/'cli-game.065')==expected
 result=dict(status='PASS',exe_sha256=sha(a.exe),recovery_preserved_progress=True,cold_map_and_progress_equal=True,recovery_idempotent=True,ordinary_mouse_shot=True,hit_registered=True,damage_before=cold['shooting_damage'],damage_after=now['shooting_damage'],shots=now['shooting_shots'],clear_hits=now['shooting_clear_hits'],set_hits=now['shooting_set_hits'],source_snapshot_unchanged=True)
 (a.output/'receipt.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True)
finally:c.close()
