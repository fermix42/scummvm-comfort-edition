#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual retained pass walking/first Shadow attack, exact cold gameplay replay."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ['exe','game','predecessor','artifacts']:parser.add_argument('--'+name,type=Path,required=True)
 parser.add_argument('--final-plane',action='store_true');parser.add_argument('--attack-seed',type=int,default=0);parser.add_argument('--pass-turn',action='store_true');parser.add_argument('--walk',default='U'*7+'LL');args=parser.parse_args();assert 0<=args.attack_seed<=0x7fffffff and 0<len(args.walk)<=64 and all(key in'UDLR.'for key in args.walk)
 exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);prior_root=args.predecessor.resolve(strict=True);prior=json.loads((prior_root/'verification.json').read_text());assert prior['status']=='VERIFIED'and (args.final_plane or prior['original_automatic_private_checkpoint']=='VERIFIED')
 assert all(sha(prior_root/name)==digest for name,digest in prior['artifacts'].items())
 root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True);saves=root/'saves';shutil.copytree(prior_root/'saves',saves);ce=Path(__file__).resolve().parents[2]
 sources={str(path.relative_to(ce)).replace('\\','/'):sha(path)for path in(ce/'engines/nox').iterdir()if path.is_file()};sources['devtools/nox/test_shadow_confrontation.py']=sha(Path(__file__));originals={str(path):sha(path)for path in game.glob('*.HDV')};assert originals
 setup='Retain ACTUAL sector entry/internal private checkpoint43B07 from verified source-supported outside setup, all actual Jewels/Key/Ring/journal/translation/Styx and real staff Ready. NO new position, party, NPC, enemy HP, damage or quest initialization. Actual bounded input path starts152,175. Source F53E..F584 identifies exact world0 position150,168 plus43B01 as first Shadow trigger; default U*7+LL follows actual retained source0/77/189/185 terrain. Original admission must match all32 owned COMBAT.S361C bytes, full3210HP/sprite133 and actual enemy-record highbit80; guardian-only scratch60D4 is not this predicate. Real ranged A/Return must enter original physical attack and preserve full enemyHP; stop at actual defeat or stable next player command, raw diagnostic only. No normal combat save/restore, victory, ending, or earned continuous campaign claim.'
 if args.pass_turn:setup=setup.replace('Real ranged A/Return must enter original physical attack and preserve full enemyHP; stop at actual defeat or stable next player command','Deliberately pass one real player turn; require original enemy Vortex12, MP215 to128, Mage125 to0 and original stone-dead GameOver input; preserve full enemyHP')
 else:setup=setup.replace('NO new position, party, NPC, enemy HP, damage or quest initialization.','No new position, party, NPC, enemy HP, damage or quest initialization. CONTROLLED original four-byte RNG state6307..630A before A only; source1089..11BA increments/mixes any integer state including zero. Actual source hit roll and physical damage must run, never injected. Earlier6093=1 was an attack result/miss, NOT proof of terrain obstruction; previous placement experiments were unnecessary, failed and excluded. Original battle position retained.')+' Controlled pre-A seed='+str(args.attack_seed)+'.'
 if args.final_plane:
  assert args.pass_turn and args.walk=='D'
  setup='Retain actual Ankh/Max portal, jewels, Ring, staff Ready and plane8. CONTROLLED source-valid final approach121,131 before actual Down only. Original full3210HP Shadow admission, real bounded passes until original GameOver; no enemy HP, quest, damage or ending initialization. Cold process restores exact legal pre-Down save and repeats actual battle, not a combat save. No win or continuous progression claim.'
 identity=dict(argv=sys.argv,executable_sha256=sha(exe),source_sha256=sources,predecessor=str(prior_root),predecessor_report_sha256=sha(prior_root/'verification.json'),mode=prior['mode'],originals=originals,walk=args.walk,pass_turn=args.pass_turn,initializations=setup);(root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n');invocations=[];startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 for stage in ['warm','cold-replay']:
  config=root/(stage+'.ini');ini=configparser.ConfigParser(interpolation=None);ini['scummvm']={'updates_check':'0','gui_return_to_launcher':'false'}
  ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_shadow_confrontation_validation':'true','nox_shadow_confrontation_replay':str(stage!='warm').lower(),'nox_shadow_walk':args.walk}
  ini['nox-gameplay-test']['nox_shadow_final_plane']=str(args.final_plane).lower()
  ini['nox-gameplay-test']['nox_shadow_confrontation_pass']=str(args.pass_turn).lower()
  ini['nox-gameplay-test']['nox_shadow_attack_seed']=str(args.attack_seed)
  with config.open('w')as output:ini.write(output)
  output=saves/('nox-first-shadow-replay.tsv'if stage!='warm'else'nox-first-shadow.tsv');output.unlink(missing_ok=True)
  argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}','nox-gameplay-test'];result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
  (root/(stage+'-stderr.txt')).write_bytes(result.stderr);invocations.append(dict(stage=stage,argv=argv,exit=result.returncode));(root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n');assert originals=={str(path):sha(path)for path in game.glob('*.HDV')};assert result.returncode==0 and output.exists()and output.read_text().startswith('PASS\t'),str(root)
 assert sha(saves/'nox-first-shadow-refusal.bin')==sha(saves/'nox-first-shadow-refusal-replay.bin'),'Actual first Shadow result differs across cold replay'
 report=dict(identity,status='VERIFIED',scope='Actual full-health mortal-plane Shadow physical attack refusal/raw cold gameplay replay; not victory/ending/full campaign',invocations=invocations,artifacts={str(path.relative_to(root)):sha(path)for path in saves.iterdir()if path.is_file()},actual_first_Shadow_attack_refusal='VERIFIED',controlled_checkpoint='VERIFIED',linked_gameplay_progression='VERIFIED',earned_continuous_progression='NOT STARTED',full_new_game_to_ending='NOT STARTED',normal_combat_save_supported=False)
 if args.pass_turn:
  report.pop('actual_first_Shadow_attack_refusal');report.update(scope='Actual full-health mortal-plane Shadow casts Vortex after deliberate pass; original GameOver presentation and exact cold replay, not campaign ending',actual_enemy_Vortex_defeat_presentation='VERIFIED')
 if args.final_plane:report.update(scope='Actual full-health final-plane Shadow defeat/GameOver and exact cold gameplay replay; no ending',actual_enemy_Vortex_defeat_presentation='NOT STARTED',actual_final_plane_defeat_presentation='VERIFIED')
 (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('VERIFIED actual first Shadow refusal:',root)
if __name__=='__main__':main()
