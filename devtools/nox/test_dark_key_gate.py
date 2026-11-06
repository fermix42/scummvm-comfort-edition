#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Retained actual Dark Key -> original door passage/underworld exit, with refusal."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ['exe','game','predecessor','artifacts']:parser.add_argument('--'+name,type=Path,required=True)
 parser.add_argument('--missing-key',action='store_true');args=parser.parse_args()
 exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);prior_root=args.predecessor.resolve(strict=True);prior=json.loads((prior_root/'verification.json').read_text());assert prior['status']=='VERIFIED'
 assert prior.get('phase')==33 if args.missing_key else prior.get('battle_final_blow_to_key')=='VERIFIED'
 assert all(sha(prior_root/name)==digest for name,digest in prior['artifacts'].items())
 root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True);saves=root/'saves';shutil.copytree(prior_root/'saves',saves);ce=Path(__file__).resolve().parents[2]
 sources={str(path.relative_to(ce)).replace('\\','/'):sha(path)for path in(ce/'engines/nox').iterdir()if path.is_file()};sources['devtools/nox/test_dark_key_gate.py']=sha(Path(__file__));originals={str(path):sha(path)for path in game.glob('*.HDV')};assert originals
 setup='Retain actual earned Dark Key154/040C08 from original victory/loot, or actual earlier no-key state. Controlled authored return route and source-valid WEST outside floor33,8 only. Original RUIN30 closed gate34,8 and UM97 reciprocal entrance7519 validated. Real O/Right then six real Right steps through gate/corridor/stairs arrive UM97 at150,167; missing key real O/Right/Right refuses and stays33,8. No key, door or transition outcome initialized after Open. Prior controlled battle damage/dead minions and controlled equipment remain explicit inherited provenance; not earned continuous travel.'
 identity=dict(argv=sys.argv,executable_sha256=sha(exe),source_sha256=sources,predecessor=str(prior_root),predecessor_report_sha256=sha(prior_root/'verification.json'),mode=prior['mode'],originals=originals,missing_key=args.missing_key,initializations=setup)
 (root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n');invocations=[];startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 for stage in ['warm','cold','replay']:
  config=root/(stage+'.ini');ini=configparser.ConfigParser(interpolation=None);ini['scummvm']={'updates_check':'0','gui_return_to_launcher':'false'}
  ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_dark_key_validation':'true','nox_dark_key_gate':'true','nox_dark_key_missing_key':str(args.missing_key).lower(),'nox_dark_key_gate_replay':str(stage=='replay').lower(),'nox_dark_key_gate_check_only':str(stage=='cold').lower()}
  with config.open('w')as output:ini.write(output)
  output=saves/('nox-dark-key-gate-cold.tsv'if stage=='cold'else'nox-dark-key-gate-replay.tsv'if stage=='replay'else'nox-dark-key-gate.tsv');output.unlink(missing_ok=True)
  argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}','nox-gameplay-test'];result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
  (root/(stage+'-stderr.txt')).write_bytes(result.stderr);invocations.append(dict(stage=stage,argv=argv,exit=result.returncode));(root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n');assert originals=={str(path):sha(path)for path in game.glob('*.HDV')};assert result.returncode==0 and output.exists()and output.read_text().startswith('PASS\t'),str(root)
 assert sha(saves/'nox-dark-key-gate-world.bin')==sha(saves/'nox-dark-key-gate-world-replay.bin'),'Original gate result differs across cold replay'
 report=dict(identity,status='VERIFIED',invocations=invocations,artifacts={str(path.relative_to(root)):sha(path)for path in saves.iterdir()if path.is_file()},actual_physical_gate='VERIFIED',controlled_checkpoint='VERIFIED',linked_gameplay_progression='VERIFIED',earned_continuous_progression='NOT STARTED',full_new_game_to_ending='NOT STARTED')
 (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('VERIFIED:',root)
if __name__=='__main__':main()
