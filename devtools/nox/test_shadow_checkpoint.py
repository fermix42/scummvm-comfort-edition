#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual sector entry/internal original checkpoint; explicitly NOT a Shadow fight."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ['exe','game','predecessor','artifacts','fixture']:parser.add_argument('--'+name,type=Path,required=True)
 args=parser.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);prior_root=args.predecessor.resolve(strict=True);prior=json.loads((prior_root/'verification.json').read_text());assert prior['status']=='VERIFIED'and prior['case']['id']=='styx-after-actual-journal-translation'
 assert all(sha(prior_root/name)==digest for name,digest in prior['artifacts'].items())
 fixture=json.loads(args.fixture.read_text());assert fixture['status']=='VERIFIED'and fixture['mode']==prior['mode']and fixture['predecessor_report_sha256']==sha(prior_root/'verification.json')
 save_path=prior_root/'saves'/Path(fixture['predecessor_save']).name;assert sha(save_path)==fixture['predecessor_save_sha256']and [cell['current_private_tile']for cell in fixture['cells']]==[0,77]
 root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True);saves=root/'saves';shutil.copytree(prior_root/'saves',saves);ce=Path(__file__).resolve().parents[2]
 sources={str(path.relative_to(ce)).replace('\\','/'):sha(path)for path in(ce/'engines/nox').iterdir()if path.is_file()};sources['devtools/nox/test_shadow_checkpoint.py']=sha(Path(__file__));originals={str(path):sha(path)for path in game.glob('*.HDV')};assert originals
 setup='Retain actual High Cultist death/journal/automatic Archus translation/Styx guidance43B06 and all3 Jewels/Key/Ring. CONTROLLED source-authored57->58->54->surface0 route and source-decoded OUTSIDE tile77 at152,176. Hash-bound source manifest independently decodes actual retained private sectors and verifies outside77/inside0 against owned original source cells; compiled fixture verifies those exact retained compressed-sector hashes and current original foot ranges. Valid game-written sectors need not equal pristine compressed bytes. SourceFA1D..FA44 identifies043B01 + original private-save helper8E0D/FE95, NOT a spawn/win condition. Actual Up enters sectorA9, observes one original internal private checkpoint caller and source043B01, then legal CE full-runtime result/cold. No story flag, NPC, battle, reward or ending outcome initialized. Actual Shadow encounter remains NOT STARTED.'
 identity=dict(argv=sys.argv,executable_sha256=sha(exe),source_sha256=sources,predecessor=str(prior_root),predecessor_report_sha256=sha(prior_root/'verification.json'),mode=prior['mode'],originals=originals,fixture=fixture,fixture_sha256=sha(args.fixture),initializations=setup);(root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n');invocations=[];startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 for stage in ['warm','cold']:
  config=root/(stage+'.ini');ini=configparser.ConfigParser(interpolation=None);ini.optionxform=str;ini['scummvm']={'updates_check':'0','gui_return_to_launcher':'false'}
  ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_shadow_checkpoint_validation':'true','nox_shadow_checkpoint_check_only':str(stage=='cold').lower()}
  ini['nox-gameplay-test']['nox_shadow_source_A9']=fixture['hashes']['a9'];ini['nox-gameplay-test']['nox_shadow_source_B9']=fixture['hashes']['b9']
  with config.open('w')as output:ini.write(output)
  output=saves/('nox-shadow-checkpoint-cold.tsv'if stage=='cold'else'nox-shadow-checkpoint.tsv');output.unlink(missing_ok=True)
  argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}','nox-gameplay-test'];result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
  (root/(stage+'-stderr.txt')).write_bytes(result.stderr);invocations.append(dict(stage=stage,argv=argv,exit=result.returncode));(root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n');assert originals=={str(path):sha(path)for path in game.glob('*.HDV')};assert result.returncode==0 and output.exists()and output.read_text().startswith('PASS\t'),str(root)
 report=dict(identity,status='VERIFIED',scope='Actual original automatic private-checkpoint event ONLY; not Shadow encounter/refusal/victory',invocations=invocations,artifacts={str(path.relative_to(root)):sha(path)for path in saves.iterdir()if path.is_file()},original_automatic_private_checkpoint='VERIFIED',first_Shadow_encounter='NOT STARTED',controlled_checkpoint='VERIFIED',linked_gameplay_progression='VERIFIED',earned_continuous_progression='NOT STARTED',full_new_game_to_ending='NOT STARTED')
 (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('VERIFIED original private-checkpoint event ONLY:',root)
if __name__=='__main__':main()
