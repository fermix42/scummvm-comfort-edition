#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cold-process M2 owner profiles; explicitly not earned campaign stages."""
import argparse, configparser, datetime, hashlib, json, os, shutil, struct, subprocess
from pathlib import Path
from enhancement_oracle import read_save_enhancements

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for key in ('exe','game','baseline','artifacts'):ap.add_argument('--'+key,type=Path,required=True)
 args=ap.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);base=args.baseline.resolve(strict=True)
 fixture=json.loads((base/'verification.json').read_text());assert fixture['status']=='VERIFIED'
 original={str(p):sha(p) for p in game.glob('*.HDV')};assert original==fixture['original_sha256']
 files=sorted((base/'saves').glob('nox-ce-*'));worlds={}
 for p in files:
  if p.suffix in ('.g0','.g1'):
   state=read_save_enhancements(p);sequence=struct.unpack_from('<I',p.read_bytes(),8)[0]
   if state['description'] not in worlds or sequence>worlds[state['description']][0]:worlds[state['description']]=(sequence,p,state)
 early=worlds['Before dialogue'];middle=worlds['Observed dialogue fixture'];other=worlds['Fresh session B']
 assert early[2]['session']==middle[2]['session']!=other[2]['session']
 root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
 env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy');startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 cases=[]
 for profile,label in enumerate(('early-empty','middle-observed','late-controlled-rich'),1):
  work=root/label;saves=work/'saves';saves.mkdir(parents=True)
  for p in files:shutil.copy2(p,saves/p.name)
  for name in ('ce-playthroughs.json','ce-achievements-queue.json'):shutil.copy2(base/'saves'/name,saves/name)
  protected={p.name:sha(p) for p in files if p.name.startswith(other[1].name[:-3])}
  ini=configparser.ConfigParser(interpolation=None);ini.read(base/'dialogue.ini',encoding='utf-8');assert not ini['scummvm'].get('ce_link_token')
  domain=ini['nox-gameplay-test'];domain['nox_state_profile']=str(profile);domain['nox_profile_source']=(early if profile==1 else middle)[1].name
  config=work/'profile.ini';commands=[]
  for phase in ('warm','cold'):
   domain['nox_profile_cold']='true' if phase=='cold' else 'false'
   with config.open('w',encoding='utf-8') as out:ini.write(out)
   argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={work/(phase+'-engine.log')}', 'nox-gameplay-test']
   completed=subprocess.run(argv,env=env,startupinfo=startup,capture_output=True,timeout=30)
   (work/(phase+'-stdout.txt')).write_bytes(completed.stdout);(work/(phase+'-stderr.txt')).write_bytes(completed.stderr)
   commands.append({'argv':argv,'exit':completed.returncode});report=saves/('nox-profile-'+phase+'.tsv')
   assert completed.returncode==0 and report.read_text().startswith('PASS\t')
   assert all((saves/name).exists() and sha(saves/name)==value for name,value in protected.items()),'Other session changed'
  candidates=[]
  for p in saves.glob('*.g?'):
   state=read_save_enhancements(p)
   if state['description']==f'Owner profile {profile}':candidates.append((struct.unpack_from('<I',p.read_bytes(),8)[0],p,state))
  assert len(candidates)==2;_,path,state=max(candidates)
  assert state['session']==early[2]['session'] and state['saved_mode']==early[2]['saved_mode']
  replies=sum(len(c['replies']) for c in state['characters']);revealed=sum(c['revealed'] for c in state['cells']);notes=sum(bool(c['annotation']) for c in state['cells'])
  assert replies==(0 if profile==1 else 9)
  if profile==3:assert revealed>=256 and notes>=16
  records=json.loads((saves/'ce-playthroughs.json').read_text())['playthroughs'];assert len(records)==2
  by_id={r['client_playthrough_id']:r for r in records};assert not by_id[other[2]['session']]['casual'] and not by_id[other[2]['session']]['assistance_flags']
  if profile==3:assert 'nox_controlled_owner_profile' in by_id[state['session']]['assistance_flags']
  queue=json.loads((saves/'ce-achievements-queue.json').read_text());assert all(e['attempt_count']==0 and e['request_type']!='achievement' for e in queue)
  cases.append({'profile':label,'status':'VERIFIED','replies':replies,'cells':len(state['cells']),'revealed':revealed,'annotations':notes,'runtime_sha256':sha(saves/'nox-profile-runtime.bin'),'warm':(saves/'nox-profile-warm.tsv').read_text().strip(),'cold':(saves/'nox-profile-cold.tsv').read_text().strip(),'commands':commands})
 assert all(sha(Path(p))==value for p,value in original.items())
 report={'status':'VERIFIED','scope':'M2 representative owner-content stages and cold exact runtime restoration; NOT early/middle/late earned campaign coverage','executable_sha256':sha(exe),'fixture_executable_sha256':fixture['executable_sha256'],'fixture':str(base),'saved_mode':early[2]['saved_mode'],'provenance':'All profiles use an explicitly assisted authored-exit adjacency world. Middle has actual observed replies; late adds controlled reveal/annotation data without changing game progression.','cases':cases,'original_sha256':original,'artifacts':str(root)}
 (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
