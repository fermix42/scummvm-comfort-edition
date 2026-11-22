#!/usr/bin/env python3
"""Submit KONAMI through ordinary JORDAN mouse input; verify live and persistent art."""
import argparse,configparser,json,shutil,struct,threading
from pathlib import Path
from test_snatcher_integration import Client,sha
p=argparse.ArgumentParser()
for k in ('exe','game','checkpoint','resources','output'):p.add_argument('--'+k,type=Path,required=True)
a=p.parse_args();a.output.mkdir();expected=sha(a.checkpoint)

def client(name,reuse=False):
 c=Client(a.exe,a.output/name,a.game,'gens',reuse=reuse);c.watch.cancel();c.watch=threading.Timer(300,c.p.kill);c.watch.start();return c

def inspect(c):
 s=c.command('inspect')['adapter'];assert not any(s.get(k) for k in ('blocker','runtime_error','render_error','audio_error'));return s

def ready(c):
 for _ in range(180):
  s=inspect(c)
  if s['entry_active'] and s['mouse_ready']:return s
  c.command('advance',frames=2)
 raise AssertionError('JORDAN entry not ready')

def click(c,x,y):
 for phase in ('down','up'):
  c.command('input',kind='mouse_button',button=1,phase=phase,x=x,y=y);c.command('advance',frames=2)

def submit(c):
 s=ready(c);instance=s['test_instance_serial']
 for char in 'KONAMI':
  ready(c);n=ord(char)-65;click(c,27+16*(n%13),163 if n<13 else 179)
 ready(c);entry=c.command('input',kind='logical',action='entry_transition_inspect');assert bytes.fromhex(entry['buffer_hex']).rstrip(b'\0')==b'KONAMI'
 click(c,200,196)
 for _ in range(250):
  s=inspect(c)
  if not s['entry_active'] and s['entry_session']==0 and s['command_ready']:break
  if s['dialogue_waiting']:
   for phase in ('down','up'):c.command('input',kind='key',key=13,phase=phase);c.command('advance',frames=1)
  c.command('advance',frames=4)
 else:raise AssertionError('KONAMI response did not settle')
 assert s['test_instance_serial']==instance
 return s

def inventory(c):return {x['name']:x['md5'] for x in c.command('input',kind='logical',action='decensor_resource_inventory')['files']}
def stamp(path):
 b=path.read_bytes();at=80
 for _ in range(struct.unpack_from('>H',b,12)[0]):
  id,ver,flags,res,n=struct.unpack_from('>HHHHI',b,at);at+=12
  if id==2:
   v=b[at:at+n];assert v[:4]==b'SNNR' and v[72]==39;return struct.unpack_from('>Q',v,73+25*8)[0]
  at+=n
 raise AssertionError('Narrative stamp missing')
results={}
for enabled in (False,True):
 name='on' if enabled else 'off';c=client(name)
 try:
  cfg=c.root/'profile.ini';cfg.write_text(cfg.read_text().replace('autosave_period=1','autosave_period=0').replace('snatcher_decensor=false','snatcher_decensor='+str(enabled).lower()).replace('[scummvm]','[scummvm]\nextrapath='+str(a.resources)))
  saves=c.root/'saves';saves.mkdir(exist_ok=True);shutil.copy2(a.checkpoint,saves/'cli-game.065')
  c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000);c.command('load',slot=65)
  before=ready(c);assert before['decensor_enabled']==enabled
  c.command('save',slot=66,description='Before KONAMI');oldStamp=stamp(saves/'cli-game.066');oldFiles=inventory(c)
  state=submit(c);newFiles=inventory(c);assert state['decensor_enabled'] and state['decensor_applications']==5
  changes=sorted(k for k in oldFiles if oldFiles[k]!=newFiles[k]);assert changes==([] if enabled else ['DATA_F0.BIN','DATA_G0.BIN','DATA_H09.BIN','DATA_P0.BIN','SP25.BIN']),changes
  c.command('save',slot=67,description='After live KONAMI');newStamp=stamp(saves/'cli-game.067');assert (oldStamp==newStamp)==enabled
  cfgState=configparser.ConfigParser();cfgState.read(cfg);assert cfgState.getboolean('cli-game','snatcher_decensor')
  results[name]=dict(status='PASS',live_enabled=True,applications=state['decensor_applications'],changed_resources=changes,scenario_stamp_changed=oldStamp!=newStamp,same_engine_instance=True,persisted=True)
  (a.output/(name+'-state.json')).write_text(json.dumps(state,indent=2));assert sha(saves/'cli-game.065')==expected
 finally:c.close()
c=client('off',True)
try:
 c.command('launch',engine='snatcher',game='snatcher',platform='segacd',path=str(a.game));c.command('wait',field='adapter.ready',equals=True,timeout_ms=30000)
 assert inspect(c)['decensor_enabled'];c.command('load',slot=67);assert inspect(c)['decensor_enabled']
finally:c.close()
assert sha(a.checkpoint)==expected
receipt=dict(status='PASS',exe_sha256=sha(a.exe),runs=results,cold_option_and_save=True,source_snapshot_unchanged=True)
(a.output/'receipt.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt,indent=2),flush=True)
