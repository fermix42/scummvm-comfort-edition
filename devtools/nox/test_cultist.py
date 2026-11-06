#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Retained real Temple traversal -> original Cultist opening/battle/final blow."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ['exe','game','predecessor','artifacts']:parser.add_argument('--'+name,type=Path,required=True)
 args=parser.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);prior_root=args.predecessor.resolve(strict=True)
 prior=json.loads((prior_root/'verification.json').read_text());assert prior['status']=='VERIFIED'and prior['phase']==36
 assert all(sha(prior_root/name)==digest for name,digest in prior['artifacts'].items())
 root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True);saves=root/'saves';shutil.copytree(prior_root/'saves',saves);ce=Path(__file__).resolve().parents[2]
 sources={str(path.relative_to(ce)).replace('\\','/'):sha(path)for path in(ce/'engines/nox').iterdir()if path.is_file()};sources['devtools/nox/test_cultist.py']=sha(Path(__file__));originals={str(path):sha(path)for path in game.glob('*.HDV')};assert originals
 setup='Retain actual original Temple traversal to168,157, all three actual Jewels/Key/Ring and real Ready staff. Actual Up triggers High Cultist opening; actual Return/default original response starts encounter. Observe full original source Cult Guardian168/471HP, Vhaak Cultist169/267HP and High Cultist170/471HP records. At first original player command BEFORE tested A/Return, CONTROLLED source-valid other dead records/cache/occupancy and living FIRST ACTUAL HIGH CULTIST sprite170HP1 (slot8 in observed10-enemy encounter), not the first Cult Guardian. Not earned damage/kills/full-health encounter victory. Actual ranged attack/High Cultist death/victory/loot must award Journal156/043840; no reward/quest outcome initialized. Exact legal result cold and actual opening/battle replay.'
 identity=dict(argv=sys.argv,executable_sha256=sha(exe),source_sha256=sources,predecessor=str(prior_root),predecessor_report_sha256=sha(prior_root/'verification.json'),mode=prior['mode'],originals=originals,initializations=setup)
 (root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n');invocations=[];startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 for stage in ['warm','cold','replay']:
  if stage=='replay':
   # The cold-result check has finished in its own process. Restore only the
   # immutable original approach save generations before launching a fresh
   # actual gameplay replay; never alter a running operation or its result.
   approach=list((prior_root/'saves').glob('nox-ce-*-slot001.*'));assert len(approach)==4
   for path in approach:shutil.copy2(path,saves/path.name)
   (root/'replay-input-save-identity.json').write_text(json.dumps({path.name:sha(path)for path in approach},indent=2)+'\n')
  config=root/(stage+'.ini');ini=configparser.ConfigParser(interpolation=None);ini['scummvm']={'updates_check':'0','gui_return_to_launcher':'false'}
  ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_cultist_validation':'true','nox_cultist_replay':str(stage=='replay').lower(),'nox_cultist_check_only':str(stage=='cold').lower()}
  with config.open('w')as output:ini.write(output)
  output=saves/('nox-cultist-cold.tsv'if stage=='cold'else'nox-cultist-replay.tsv'if stage=='replay'else'nox-cultist.tsv');output.unlink(missing_ok=True)
  argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}','nox-gameplay-test'];result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
  (root/(stage+'-stderr.txt')).write_bytes(result.stderr);invocations.append(dict(stage=stage,argv=argv,exit=result.returncode));(root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n');assert originals=={str(path):sha(path)for path in game.glob('*.HDV')};assert result.returncode==0 and output.exists()and output.read_text().startswith('PASS\t'),str(root)
 assert sha(saves/'nox-cultist-world.bin')==sha(saves/'nox-cultist-world-replay.bin'),'Original Cultist result differs across cold replay'
 report=dict(identity,status='VERIFIED',invocations=invocations,artifacts={str(path.relative_to(root)):sha(path)for path in saves.iterdir()if path.is_file()},actual_cultist_final_blow_to_journal='VERIFIED',controlled_checkpoint='VERIFIED',linked_gameplay_progression='VERIFIED',earned_continuous_progression='NOT STARTED',full_new_game_to_ending='NOT STARTED',full_health_encounter_victory='NOT STARTED')
 (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('VERIFIED:',root)
if __name__=='__main__':main()
