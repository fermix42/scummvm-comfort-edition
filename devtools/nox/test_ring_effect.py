#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual original death-magic encounter using earned Ring or real refusal state."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['exe','game','predecessor','artifacts']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--ready-defense',action='store_true');p.add_argument('--defense-fixture',type=Path);p.add_argument('--missing',action='store_true');a=p.parse_args()
 exe=a.exe.resolve(strict=True);game=a.game.resolve(strict=True);prior_root=a.predecessor.resolve(strict=True);prior=json.loads((prior_root/'verification.json').read_text())
 assert prior['status']=='VERIFIED'and prior['phase']==((33 if a.missing else 32)if a.ready_defense else(29 if a.missing else 28))
 assert all(sha(prior_root/n)==h for n,h in prior['artifacts'].items())
 root=a.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True);saves=root/'saves';fixture=None
 if a.defense_fixture:
  fixture_root=a.defense_fixture.resolve(strict=True);fixture=json.loads((fixture_root/'controlled-fixture.json').read_text());assert fixture['kind']=='controlled-defensive-equipment'and fixture['missing']==a.missing and fixture['mode']==prior['mode']and fixture['predecessor_report_sha256']==sha(prior_root/'verification.json');assert all(sha(fixture_root/n)==h for n,h in fixture['artifacts'].items());shutil.copytree(fixture_root/'saves',saves)
 else:shutil.copytree(prior_root/'saves',saves)
 ce=Path(__file__).resolve().parents[2];sources={str(f.relative_to(ce)).replace('\\','/'):sha(f)for f in(ce/'engines/nox').iterdir()if f.is_file()};sources['devtools/nox/test_ring_effect.py']=sha(Path(__file__))
 originals={str(f):sha(f)for f in game.glob('*.HDV')};assert originals
 identity=dict(argv=sys.argv,executable_sha256=sha(exe),source_sha256=sources,predecessor=str(prior_root),predecessor_report_sha256=sha(prior_root/'verification.json'),mode=prior['mode'],originals=originals,missing=a.missing,initializations='Retain actual Ring password/pickup or actual wrong-word/no-Ring state and all three actual Jewels. CONTROLLED original reachable mage recovery HP125/MP110 before next operation, authored route to original Lord Drathe encounter and source-valid south floor15,21 only. Original full enemy/party tactical layout retained. Actual A/Up admission, Tactical and real Space passes allow original enemy to cast. No enemy damage, spell selection, Ring or outcome initialization. Scope is surviving an individual completed original death spell; the full eight-enemy victory and Dark Key award remain separate. Later defeat is retained in full, with per-cast HP observations proving first-spell survival; no intermediate effect is skipped. Raw battle diagnostic is not a player save.')
 if fixture:identity['controlled_defense_fixture']=dict(path=str(fixture_root),sha256=sha(fixture_root/'controlled-fixture.json'),initialization=fixture['initialization']);identity['initializations']+=' Reuse explicitly CONTROLLED actual-readied armor118 checkpoint before battle, exact cold and source validity required; original broader failed test remains excluded.'
 if a.ready_defense:identity['initializations']+=' Controlled recovery/owned armor initialization occurred in predecessor32/33; actual Ready/cold verified there. This battle stage initializes no equipment or health and validates the inherited defensive profile.'
 (root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n');calls=[]
 startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 for replay in [False,True]:
  stage='cold-replay'if replay else'warm';ini=configparser.ConfigParser(interpolation=None);ini['scummvm']={'updates_check':'0','gui_return_to_launcher':'false'}
  ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_vortex_phase':str(31 if a.missing else 30),'nox_ring_replay':str(replay).lower()}
  ini['nox-gameplay-test']['nox_ring_defense_fixture']=str(bool(fixture)).lower()
  ini['nox-gameplay-test']['nox_ring_ready_defense']=str(a.ready_defense).lower()
  config=root/(stage+'.ini')
  with config.open('w')as f:ini.write(f)
  output=saves/('nox-ring-effect-replay.tsv'if replay else'nox-ring-effect.tsv');output.unlink(missing_ok=True)
  argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}','nox-gameplay-test']
  result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120);(root/(stage+'-stderr.txt')).write_bytes(result.stderr);calls.append(dict(stage=stage,argv=argv,exit=result.returncode));(root/'invocations.json').write_text(json.dumps(calls,indent=2)+'\n')
  assert originals=={str(f):sha(f)for f in game.glob('*.HDV')}
  assert result.returncode==0 and output.exists()and output.read_text().startswith('PASS\t'),str(root)
 assert sha(saves/'nox-ring-effect.bin')==sha(saves/'nox-ring-effect-replay.bin'),'Full resulting original runtime differs across cold replay'
 r=dict(identity,status='VERIFIED',invocations=calls,artifacts={str(f.relative_to(root)):sha(f)for f in saves.iterdir()if f.is_file()},controlled_checkpoint='VERIFIED',linked_gameplay_progression='VERIFIED',earned_continuous_progression='NOT STARTED',full_new_game_to_ending='NOT STARTED',result_persistence='Exact legal pre-encounter restore plus byte-identical full original battle runtime cold replay; not combat saving')
 (root/'verification.json').write_text(json.dumps(r,indent=2)+'\n');print('VERIFIED:',root)
if __name__=='__main__':main()
