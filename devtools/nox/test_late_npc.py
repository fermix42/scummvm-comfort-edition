#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run bounded actual NPC dialogue cases with immutable local source/evidence manifests."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ['exe','game','predecessor','artifacts','case']:parser.add_argument('--'+name,type=Path,required=True)
 parser.add_argument('--controlled-setup',type=Path)
 args=parser.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);prior_root=args.predecessor.resolve(strict=True);case_path=args.case.resolve(strict=True);case=json.loads(case_path.read_text())
 assert case['source_anchors']and case['initializations']and case['id'];prior=json.loads((prior_root/'verification.json').read_text());assert prior['status']=='VERIFIED'
 assert all(sha(prior_root/name)==digest for name,digest in prior['artifacts'].items())
 starting_saves=prior_root/'saves';controlled_setup=None
 if args.controlled_setup:
  setup_path=args.controlled_setup.resolve(strict=True);setup=json.loads(setup_path.read_text())
  assert setup['kind']=='CONTROLLED_SETUP' and setup['status']=='IN PROGRESS'
  assert Path(setup['parent_report']).resolve()==(prior_root/'verification.json').resolve() and setup['parent_report_sha256']==sha(prior_root/'verification.json')
  assert setup['mode']==prior['mode'] and setup['initializations']
  assert all(sha(setup_path.parent/name)==digest for name,digest in setup['artifacts'].items())
  starting_saves=setup_path.parent/'saves';controlled_setup=dict(path=str(setup_path),sha256=sha(setup_path),initializations=setup['initializations'],scope=setup['scope'])
 root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True);saves=root/'saves';shutil.copytree(starting_saves,saves);ce=Path(__file__).resolve().parents[2]
 sources={str(path.relative_to(ce)).replace('\\','/'):sha(path)for path in(ce/'engines/nox').iterdir()if path.is_file()};sources['devtools/nox/test_late_npc.py']=sha(Path(__file__));originals={str(path):sha(path)for path in game.glob('*.HDV')};assert originals
 identity=dict(argv=sys.argv,executable_sha256=sha(exe),source_sha256=sources,predecessor=str(prior_root),predecessor_report_sha256=sha(prior_root/'verification.json'),controlled_setup=controlled_setup,mode=prior['mode'],originals=originals,case=case,case_sha256=sha(case_path),initializations=case['initializations']);(root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n');invocations=[];startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 for stage in ['warm','cold']:
  values={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_vortex_phase':'38','nox_vortex_check_only':str(stage=='cold').lower(),'nox_late_predecessor':case['predecessor_label'],'nox_late_noon':str(case['noon']).lower(),'nox_late_repeat':str(case['repeat']).lower(),'nox_late_map':str(case['map']),'nox_late_actor':str(case['actor']),'nox_late_reply_resource':case['reply']['resource'],'nox_late_reply_phrase':case['reply']['contains'],'nox_late_route_count':str(len(case['routes'])),'nox_late_topic_count':str(len(case['topics']))}
  for i,(map_id,map_type)in enumerate(case['routes']):values.update({f'nox_late_route_{i}_map':str(map_id),f'nox_late_route_{i}_type':str(map_type)})
  for i,topic in enumerate(case['topics']):values[f'nox_late_topic_{i}']=topic
  if 'search'in case:
   values['nox_late_search']='true'
   for key,value in case['search'].items():values['nox_search_'+key]=str(value).lower()if isinstance(value,bool)else str(value)
  if 'gate'in case:
   values['nox_late_gate']='true'
   for key,value in case['gate'].items():values['nox_gate_'+key]=str(value).lower()if isinstance(value,bool)else str(value)
  if case.get('opening'):values['nox_late_opening']='true'
  if case.get('strict_words'):values['nox_late_strict_words']='true'
  if case.get('prebinding'):values['nox_late_prebinding']='true'
  if case.get('current_npc'):values['nox_late_current_npc']='true'
  if case.get('bump_mouth'):values['nox_late_bump_mouth']='true'
  if case.get('full_guardian'):values['nox_late_full_guardian']='true'
  if case.get('recover'):values['nox_late_recover']='true'
  if case.get('guard_encounter'):values['nox_late_guard_encounter']='true'
  if case.get('cultist_encounter'):values['nox_late_cultist_encounter']='true'
  if case.get('staged_cultists'):values['nox_late_staged_cultists']='true'
  if 'actor_direction'in case:values['nox_late_actor_direction']=str(case['actor_direction'])
  if 'clock'in case:values['nox_late_clock_hour']=str(case['clock'][0]);values['nox_late_clock_minute']=str(case['clock'][1])
  if 'battle'in case:values['nox_late_battle']='true'
  if 'world'in case:
   values['nox_late_world']='true';world=case['world'];commands=world.get('commands',[[key]for key in world.get('keys',[])]);values['nox_world_move_count']=str(len(commands));values['nox_world_absent_count']=str(len(world.get('absent',[])))
   for i,command in enumerate(commands):
    values[f'nox_world_command_{i}_size']=str(len(command))
    for j,key in enumerate(command):values[f'nox_world_command_{i}_key_{j}']=str(key)
   for i,actor in enumerate(world.get('absent',[])):values[f'nox_world_absent_{i}']=str(actor)
   for key,value in world.items():
    if key not in ['keys','commands','absent']:values['nox_world_'+key]=str(value)
  for label in ['before','after']:
   expected=case[label];prefix='nox_late_'+label;values[prefix+'_field_count']=str(len(expected['flags']));values[prefix+'_item_count']=str(len(expected['items']))
   for i,(field,mask,value)in enumerate(expected['flags']):values.update({f'{prefix}_field_{i}_address':str(field),f'{prefix}_field_{i}_mask':str(mask),f'{prefix}_field_{i}_value':str(value)})
   for i,(item,quantity)in enumerate(expected['items']):values.update({f'{prefix}_item_{i}_id':str(item),f'{prefix}_item_{i}_quantity':str(quantity)})
  config=root/(stage+'.ini');ini=configparser.ConfigParser(interpolation=None);ini['scummvm']={'updates_check':'0','gui_return_to_launcher':'false','joystick_num':'-1'};ini['nox-gameplay-test']=values
  with config.open('w')as output:ini.write(output)
  output=saves/('nox-vortex-cold-38.tsv'if stage=='cold'else'nox-vortex-phase-38.tsv');output.unlink(missing_ok=True)
  argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}','nox-gameplay-test'];result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
  (root/(stage+'-stderr.txt')).write_bytes(result.stderr);invocations.append(dict(stage=stage,argv=argv,exit=result.returncode));(root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n');assert originals=={str(path):sha(path)for path in game.glob('*.HDV')};assert result.returncode==0 and output.exists()and output.read_text().startswith('PASS\t'),str(root)
 report=dict(identity,status='VERIFIED',invocations=invocations,artifacts={str(path.relative_to(root)):sha(path)for path in saves.iterdir()if path.is_file()},actual_NPC_case='VERIFIED',controlled_checkpoint='VERIFIED',linked_gameplay_progression='VERIFIED',earned_continuous_progression='NOT STARTED',full_new_game_to_ending='NOT STARTED')
 if 'search'in case:report.pop('actual_NPC_case');report['actual_Search_case']='VERIFIED'
 if 'gate'in case:report.pop('actual_NPC_case');report['actual_key_gate_case']='VERIFIED'
 if 'world'in case:report.pop('actual_NPC_case');report['actual_world_operation']='VERIFIED'
 if 'battle'in case:report['actual_quest_battle']='VERIFIED'
 (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('VERIFIED:',root)
if __name__=='__main__':main()
