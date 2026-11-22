#!/usr/bin/env python3
"""Recover the owner credits snapshot and verify full-frame palette continuation."""
import argparse,json,shutil,threading
from pathlib import Path
from test_snatcher_integration import Client,sha

import struct,zlib
p=argparse.ArgumentParser()
for k in ('exe','baseline-exe','game','checkpoint','report','resources','output'):p.add_argument('--'+k,type=Path,required=True)
a=p.parse_args();a.output.mkdir();original=json.loads(a.report.read_text())['engine'];expected=sha(a.checkpoint)
def png(path,data):
 def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v))
 w,h=256,224;rows=b''.join(b'\0'+data[y*w*3:(y+1)*w*3] for y in range(h));path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
def client(reuse=False):
 c=Client(a.exe,a.output/'profile',a.game,'gens',reuse=reuse);c.watch.cancel();c.watch=threading.Timer(300,c.p.kill);c.watch.start();return c
fields=('packed_digest','game_ticks','credits_scene','credits_index','credits_phase','credits_completion_serial','cd_consumed_frames','cd_track','scene_resource','scene_index')
baseline=Client(a.baseline_exe,a.output/'baseline-profile',a.game,'gens')
try:
 cfg=baseline.root/'profile.ini';cfg.write_text(cfg.read_text().replace('autosave_period=1','autosave_period=0').replace('snatcher_decensor=false','snatcher_decensor=true').replace('[scummvm]','[scummvm]\nextrapath='+str(a.resources)))
 saves=baseline.root/'saves';saves.mkdir(exist_ok=True);shutil.copy2(a.checkpoint,saves/'cli-game.065')
 baseline.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));baseline.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
 old=baseline.command('load',slot=65)['checkpoint']
finally:baseline.close()
c=client()
try:
 cfg=c.root/'profile.ini';cfg.write_text(cfg.read_text().replace('autosave_period=1','autosave_period=0').replace('snatcher_decensor=false','snatcher_decensor=true').replace('[scummvm]','[scummvm]\nextrapath='+str(a.resources)))
 saves=c.root/'saves';saves.mkdir(exist_ok=True);shutil.copy2(a.checkpoint,saves/'cli-game.065')
 c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
 loaded=c.command('load',slot=65,capture_rgb_slot=70);state=loaded['checkpoint']
 for k in fields:
  if k=='cd_consumed_frames':assert abs(state[k]-old[k])<=1,(k,state[k],old[k])
  else:assert state[k]==old[k],(k,state[k],old[k])
  if k!='cd_consumed_frames':assert state[k]==original[k],(k,state[k],original[k])
 assert state['palette_publication_mode']==0 and any(t['category']=='credits_palette_recovered' for t in state['trace'])
 data=(saves/loaded['boundary_rgb']).read_bytes();assert data==a.report.parent.joinpath('layers/layer-6.rgb').read_bytes(),'Engine recovery differs from isolated scene recovery'
 bad=a.report.parent.joinpath('layers/layer-0.rgb').read_bytes();assert data[:136*256*3]==bad[:136*256*3],'Recovery altered upper frame'
 changed=sum(data[i:i+3]!=bad[i:i+3] for i in range(0,len(data),3));assert changed>1000
 png(a.output/'recovered.png',data);(a.output/'recovered-state.json').write_text(json.dumps(state,indent=2))
 saved=c.command('save',slot=70,description='Credits full-frame palette regression',capture_rgb_slot=72);savedState=saved['checkpoint'];savedRgb=(saves/saved['boundary_rgb']).read_bytes()
finally:c.close()
c=client(reuse=True)
try:
 c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
 loaded=c.command('load',slot=70,capture_rgb_slot=71);cold=loaded['checkpoint']
 for k in fields:
  if k!='cd_consumed_frames':assert cold[k]==savedState[k],(k,cold[k],savedState[k])
 # Save responses observe the live mixer after serialization. Record its CD
 # reporting delta separately; story/timeline and RGB boundaries must be exact.
 cdDelta=cold['cd_consumed_frames']-savedState['cd_consumed_frames']
 assert (saves/loaded['boundary_rgb']).read_bytes()==savedRgb
 assert cold['palette_publication_mode']==0 and not any(t['category']=='credits_palette_recovered' for t in cold['trace'])
 states=[]
 for i in range(40):
  c.command('advance',frames=30);now=c.command('inspect')['adapter'];states.append(now)
  assert now['palette_publication_mode']==0 and not any(now.get(k) for k in ('blocker','runtime_error','render_error','audio_error'))
  if now['credits_scene']>cold['credits_scene']:break
 assert now['credits_scene']>cold['credits_scene'],'Credits did not reach next authored scene'
 (a.output/'continued-states.json').write_text(json.dumps(states,indent=2))
 assert sha(a.checkpoint)==expected and sha(saves/'cli-game.065')==expected
 result=dict(status='PASS',exe_sha256=sha(a.exe),baseline_exe_sha256=sha(a.baseline_exe),recovery_preserved_progress=True,cold_rgb_equal=True,recovery_idempotent=True,continued_to_next_scene=True,source_snapshot_unchanged=True,changed_pixels=changed,cold_cd_frame_delta=cdDelta)
 (a.output/'receipt.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True)
finally:c.close()
