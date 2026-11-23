#!/usr/bin/env python3
"""Measure process-to-ready startup using only isolated native CLI roots."""
import argparse,json,time,shutil
from pathlib import Path
from test_snatcher_integration import Client,sha
p=argparse.ArgumentParser()
for key in ('exe','game','resources','output','checkpoint'):p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--repeat',type=int,default=3)
p.add_argument('--expected-rgb',type=Path)
a=p.parse_args()
for key in ('exe','game','resources','output','checkpoint'):setattr(a,key,getattr(a,key).resolve())
a.output.mkdir();rows=[]
for mode in ('normal','save'):
 for run in range(a.repeat):
  began=time.perf_counter();c=Client(a.exe,a.output/(mode+str(run)),a.game,'gens')
  bootstrap=time.perf_counter()-began
  try:
   ini=c.root/'profile.ini';text=ini.read_text().replace('autosave_period=1','autosave_period=0').replace('snatcher_decensor=false','snatcher_decensor=true').replace('[scummvm]','[scummvm]\nextrapath='+str(a.resources))
   if mode=='save':
    (c.root/'saves').mkdir(exist_ok=True);shutil.copy2(a.checkpoint,c.root/'saves/cli-game.065');text=text.replace('[cli-game]','[cli-game]\nsave_slot=65')
   ini.write_text(text)
   launch=time.perf_counter();c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game))
   c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
   elapsed=time.perf_counter()-launch;state=c.command('inspect')['adapter']
   assert not any(state.get(k) for k in ('blocker','runtime_error','render_error','audio_error'))
   row=dict(mode=mode,run=run,bootstrap_seconds=bootstrap,engine_to_ready_seconds=elapsed,process_to_ready_seconds=bootstrap+elapsed,stages_ms=state.get('startup_stages_ms'),state=state)
   if mode=='save' and a.expected_rgb:
    saved=c.command('save',slot=66,description='Cold startup boundary',capture_rgb_slot=85)
    assert (c.root/'saves'/saved['boundary_rgb']).read_bytes()==a.expected_rgb.read_bytes()
    row['cold_rgb_equal']=True
   frame=time.perf_counter();c.command('advance',frames=1);row['first_frame_seconds']=time.perf_counter()-frame
   row['process_to_first_frame_seconds']=bootstrap+elapsed+row['first_frame_seconds']
   rows.append(row);print(json.dumps({k:v for k,v in row.items() if k!='state'}),flush=True)
  finally:c.close()
(a.output/'receipt.json').write_text(json.dumps(dict(status='PASS',exe_sha256=sha(a.exe),source_sha256=sha(a.checkpoint),cases=rows),indent=2))
