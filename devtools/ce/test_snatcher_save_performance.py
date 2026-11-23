#!/usr/bin/env python3
"""Bounded native save benchmark; private roots and original saves remain untouched."""
import argparse,json,shutil,time,threading
from pathlib import Path
from test_snatcher_integration import Client,sha
p=argparse.ArgumentParser()
for k in ('exe','game','resources','output'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--checkpoint',type=Path,action='append',required=True)
p.add_argument('--core',choices=('gens','nuked'),default='gens');p.add_argument('--verify',action='store_true');a=p.parse_args();a.exe=a.exe.resolve();a.output=a.output.resolve();a.game=a.game.resolve();a.resources=a.resources.resolve();a.output.mkdir()
results=[]
for index,source in enumerate(a.checkpoint):
 c=Client(a.exe,a.output/('case'+str(index)),a.game,a.core);c.watch.cancel();c.watch=threading.Timer(600,c.p.kill);c.watch.start()
 def timed(cmd,**kw):
  start=time.perf_counter();v=c.command(cmd,**kw);return v,time.perf_counter()-start
 try:
  cfg=c.root/'profile.ini';cfg.write_text(cfg.read_text().replace('autosave_period=1','autosave_period=0').replace('snatcher_decensor=false','snatcher_decensor=true').replace('[scummvm]','[scummvm]\nextrapath='+str(a.resources)))
  saves=c.root/'saves';saves.mkdir(exist_ok=True);shutil.copy2(source,saves/'cli-game.065');identity=sha(source)
  c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
  loaded,legacyLoad=timed('load',slot=65,capture_rgb_slot=80)
  saved,write=timed('save',slot=66,description='Save optimization regression',capture_rgb_slot=81)
  cold,read=timed('load',slot=66,capture_rgb_slot=82)
  assert (saves/saved['boundary_rgb']).read_bytes()==(saves/cold['boundary_rgb']).read_bytes()
  for k in ('packed_digest','game_ticks','scene_resource','scene_index','shooting_damage','shooting_shots','credits_index','credits_scene'):
   assert saved['checkpoint'][k]==cold['checkpoint'][k],(k,saved['checkpoint'][k],cold['checkpoint'][k])
  assert not any(cold['checkpoint'].get(k) for k in ('blocker','runtime_error','render_error','audio_error'))
  for slot in range(67,77):shutil.copy2(saves/'cli-game.066',saves/('cli-game.%03d'%slot))
  listing,listSeconds=timed('saves')
  if a.verify:
   probe=c.command('input',kind='logical',action='save_anywhere_probe',slot=79);assert probe['ok']
  # Replacing a legacy slot upgrades only the replacement and retains backup.
  c.command('save',slot=65,description='Replacement format regression')
  assert (saves/'cli-game.065').read_bytes()[:6]==b'SNSV\x00\x02'
  assert sha(saves/'cli-game.065.bak')==identity
  # A damaged published payload falls back to its intact prior generation.
  shutil.copy2(saves/'cli-game.066',saves/'cli-game.077.bak')
  damaged=bytearray((saves/'cli-game.066').read_bytes());damaged[-1]^=1
  (saves/'cli-game.077').write_bytes(damaged)
  assert sha(source)==identity
  result=dict(source=str(source),source_bytes=source.stat().st_size,new_bytes=(saves/'cli-game.066').stat().st_size,legacy_load_seconds=legacyLoad,save_seconds=write,load_seconds=read,list_12_slots_seconds=listSeconds,rgb_equal=True,logical_state_equal=True,probe=a.verify)
  results.append(result);print(json.dumps(result),flush=True)
 finally:c.close()
 # Reopen the normal engine in a new process; compare the saved boundary.
 c=Client(a.exe,c.root,a.game,a.core,reuse=True)
 try:
  c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game))
  c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
  start=time.perf_counter();cold=c.command('load',slot=66,capture_rgb_slot=83)
  result['cold_load_seconds']=time.perf_counter()-start
  assert (saves/saved['boundary_rgb']).read_bytes()==(saves/cold['boundary_rgb']).read_bytes()
  result['cold_rgb_equal']=True
  c.command('load',slot=77)
  result['corrupt_primary_backup_loaded']=True
 finally:c.close()
(a.output/'receipt.json').write_text(json.dumps(dict(status='PASS',exe_sha256=sha(a.exe),cases=results),indent=2))
