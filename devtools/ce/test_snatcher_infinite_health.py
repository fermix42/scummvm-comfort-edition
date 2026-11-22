#!/usr/bin/env python3
"""Exercise ordinary hospital attacks through CE master/individual/challenge gates."""
import argparse,json,shutil,threading
from pathlib import Path
from test_snatcher_integration import Client,sha
p=argparse.ArgumentParser()
for k in ('exe','game','checkpoint','resources','output'):p.add_argument('--'+k,type=Path,required=True)
a=p.parse_args();a.output.mkdir();original=sha(a.checkpoint)
results={}
cases=(('default',False,False,False),('selected_only',True,False,False),('master_only',False,True,False),('enabled',True,True,False),('challenge',True,True,True))
for name,selected,master,challenge in cases:
 enabled=selected and master and not challenge
 c=Client(a.exe,a.output/name,a.game,'gens');c.watch.cancel();c.watch=threading.Timer(300,c.p.kill);c.watch.start()
 try:
  cfg=c.root/'profile.ini';cfg.write_text(cfg.read_text().replace('autosave_period=1','autosave_period=0').replace('snatcher_decensor=false','snatcher_decensor=true').replace('[scummvm]','[scummvm]\nce_achievements_challenge_mode='+str(challenge).lower()+'\nextrapath='+str(a.resources)).replace('[cli-game]','[cli-game]\nce_cheats_enabled='+str(master).lower()+'\nsnatcher_infinite_health='+str(selected).lower()))
  saves=c.root/'saves';saves.mkdir(exist_ok=True);shutil.copy2(a.checkpoint,saves/'cli-game.065')
  c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
  initial=c.command('load',slot=65)['checkpoint'];assert initial['shooting_infinite_health']==enabled
  policy=c.command('inspect');assert policy['challenge']==challenge and policy['cheats']==(master and not challenge)
  assert ('cheats.enabled' in policy['ce']['assistance'])==(master and not challenge)
  states=[]
  for frame in range(0,901,30):
   now=c.command('inspect')['adapter'];states.append(now)
   assert not any(now.get(k) for k in ('blocker','runtime_error','render_error','audio_error'))
   if not enabled and now['shooting_damage']>initial['shooting_damage']:break
   if enabled:assert now['shooting_damage']==initial['shooting_damage'] and now['shooting_active']
   c.command('advance',frames=30)
  if not enabled:assert now['shooting_damage']>initial['shooting_damage'],'Baseline did not take enemy damage'
  else:
   c.command('save',slot=70,description='Infinite health option regression')
   restored=c.command('load',slot=70)['checkpoint'];assert restored['shooting_infinite_health'] and restored['shooting_damage']==initial['shooting_damage']
  results[name]=dict(selected=selected,master=master,challenge=challenge,effective=enabled,frames=frame,damage_before=initial['shooting_damage'],damage_after=now['shooting_damage'])
  print(name,results[name],flush=True)
  (a.output/(name+'-states.json')).write_text(json.dumps(states,indent=2))
 finally:c.close()
assert sha(a.checkpoint)==original
receipt=dict(status='PASS',exe_sha256=sha(a.exe),normal_damage=True,cheat_prevents_damage=True,save_load_retains_option=True,master_gate_verified=True,challenge_gate_verified=True,shared_assistance_flags_verified=True,source_snapshot_unchanged=True,runs=results)
(a.output/'receipt.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt,indent=2),flush=True)
