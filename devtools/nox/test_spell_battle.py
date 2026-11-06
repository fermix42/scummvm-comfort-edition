#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual Nox Vortex inventory readiness from an archived real binding result."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['exe','game','predecessor','artifacts']:parser.add_argument('--'+name,required=True,type=Path)
    parser.add_argument("--guardian",type=int,choices=[1,2,3],default=0)
    parser.add_argument("--guardian-predecessor",type=int,choices=[1,2,3],default=0)
    parser.add_argument("--guardian-missing",action="store_true")
    parser.add_argument("--melee-ending",action="store_true")
    parser.add_argument("--retained-staff",action="store_true")
    parser.add_argument("--full-boss",action="store_true")
    parser.add_argument("--small-encounter",action="store_true")
    parser.add_argument("--transport-predecessor",action="store_true")
    args=parser.parse_args();assert not(args.melee_ending and (args.small_encounter or args.full_boss));assert not(args.full_boss and args.small_encounter);exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);prior_root=args.predecessor.resolve(strict=True)
    prior=json.loads((prior_root/'verification.json').read_text());assert prior['status']=='VERIFIED'
    assert all(sha(prior_root/n)==h for n,h in prior['artifacts'].items()),'Binding archive drift'
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
    saves=root/'saves';shutil.copytree(prior_root/'saves',saves)
    ce=Path(__file__).resolve().parents[2]
    sources={str(p.relative_to(ce)).replace('\\','/'):sha(p) for p in (ce/'engines/nox').iterdir() if p.is_file()}
    sources['devtools/nox/test_spell_battle.py']=sha(Path(__file__))
    originals={str(p):sha(p)for p in game.glob('*.HDV')};assert originals
    setup='Retain actual earned binding/readiness and controlled reachable level10 mage; earlier seven pages remain controlled. '
    if args.small_encounter:setup+='Controlled original shipwreck map7/type8 floor25,35 via loader; real A/Right against original living Rat, real tactical alignment movement and Cast. No enemy stats modified. Not earned travel.'
    else:setup+='Controlled map8/type0 position121,131 before real Down onto authored encounter exit121,132. Original tactical starting position retained; '+('original full3210HP boss retained.'if args.full_boss else'living boss HP3210 ->1 and adjacent empty-floor party position4,8 ->5,8 before Cast; canonical occupancy grids retained, all defenses retained. Not earned damage/movement.')
    if args.melee_ending:setup='Retain actual earned binding/readiness and Max transport; earlier seven pages and reachable level10 mage remain controlled. Controlled map8 approach121,131 before real Down onto authored final encounter. Controlled owned UNREADIED Drake Staff71 plus source-supported spending of54 accumulated Range lessons (score10 to64, remaining54 to0, practice unchanged) at WORLD input BEFORE actual Ready UI, final approach and battle admission; original battle layout retained. Controlled living bossHP1 before attack, original defenses retained. Actual A/Return accepts the selected ranged weapon target; no Vortex cast, no earned damage or movement claim.'
    if args.guardian:setup='Retain actual bound/readied Vortex and reachable controlled level10 mage. Retain actual artifact acquisition or actual absence. Controlled authored depths route and south floor adjacency only before actual A/Up. Original full guardian HP/stats/defenses/positions retained; actual C/1/direction directly from original tactical layout; ZERO guardian alignment moves. No damage, reward or artifact outcome initialized.'
    if args.retained_staff:
        assert args.melee_ending and args.transport_predecessor
        setup='Retain actual later Styx guidance, original British Ankh/Max portal/real plane entry and all actual Jewels/Key/Ring/journal/translation. Retain actual Drake Staff71 owned equipped owner4, Range64/remaining0/derivedrange15; read-only source/record/character checks, NO new equipment/training initialization or repeated Ready. CONTROLLED final approach121,131 before real Down, and original living boss3210HP->1 BEFORE actual A/Return; not earned damage. Original defenses and original layout retained. Actual death/victory/ending presentation/acknowledgement/cold checks remain required. Earlier explicit rollback and setups remain inherited, not continuous new-game progression.'
    if args.guardian_predecessor:setup+=' Retain actual previous guardian result and all inventory/flags. CONTROLLED recovery of original reachable mage HP to existing maximum125 and MP to INT110 BEFORE next battle; not earned healing. No quest/reward/enemy/world edits during recovery.'
    identity={'guardian_predecessor':args.guardian_predecessor,'guardian':args.guardian,'guardian_missing':args.guardian_missing,'argv':sys.argv,'executable_sha256':sha(exe),'source_sha256':sources,'predecessor':str(prior_root),'predecessor_report_sha256':sha(prior_root/'verification.json'),'mode':prior['mode'],'originals':originals,'melee_ending':args.melee_ending,'transport_predecessor':args.transport_predecessor,'small_encounter':args.small_encounter,'full_health_boss':args.full_boss,'initializations':setup}
    if prior.get('retained_all_eight_actual_pages'):
        identity['retained_all_eight_actual_pages']=True;identity['initializations']=identity['initializations'].replace('earlier seven pages remain controlled.','all eight pages were acquired through actual Search/Axea, followed by actual research/binding. Routes/tools/pre-Search RNG and reachable caster stats remain controlled.')
    (root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n');invocations=[]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    for stage in (['warm','approach-cold','negative-cold']if args.guardian_missing else['warm','cold','approach-cold','ack-cold']if args.melee_ending else['warm','cold','approach-cold']):
        cold=stage not in ['warm','negative-cold'];approach=stage=='approach-cold';ini=configparser.ConfigParser(interpolation=None)
        ini['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
        ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_guardian_replay':str(stage=='negative-cold').lower(),'nox_guardian_validation':str(args.guardian),'nox_guardian_missing':str(args.guardian_missing).lower(),'nox_ending_ack_cold':str(stage=='ack-cold').lower(),'nox_ending_melee_validation':str(args.melee_ending).lower(),'nox_spell_ready_validation':'true','nox_spell_cast_validation':'true','nox_spell_battle_validation':'true','nox_spell_transport_predecessor':str(args.transport_predecessor).lower(),'nox_spell_small_encounter':str(args.small_encounter).lower(),'nox_spell_full_health_boss':str(args.full_boss).lower(),'nox_spell_approach_check_only':str(approach).lower(),'nox_spell_ready_check_only':str(cold).lower()}
        config=root/(stage+'.ini')
        ini['nox-gameplay-test']['nox_ending_retained_staff']=str(args.retained_staff).lower()
        ini['nox-gameplay-test']['nox_guardian_predecessor']=str(args.guardian_predecessor)
        with config.open('w',encoding='utf-8')as out:ini.write(out)
        output=saves/('nox-ending-ack-cold.tsv'if stage=='ack-cold'else'nox-spell-approach-cold.tsv'if approach else'nox-spell-cold.tsv'if cold else'nox-spell-ready.tsv');output.unlink(missing_ok=True)
        argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}','nox-gameplay-test']
        result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
        (root/(stage+'-stderr.txt')).write_bytes(result.stderr);invocations.append({'stage':stage,'argv':argv,'exit':result.returncode});(root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n')
        assert originals=={str(p):sha(p)for p in game.glob('*.HDV')},'Original image changed'
        assert result.returncode==0 and output.exists()and output.read_text().startswith('PASS\t'),f'Spell {stage} failed: {root}'
        if not cold:assert (saves/('nox-vortex-battle-return.png'if args.small_encounter or args.guardian else'nox-vortex-ending-presentation.png')).is_file(),'Required actual milestone absent'
    if args.guardian_missing:
        assert sha(saves/'nox-guardian-refusal.bin')==sha(saves/'nox-guardian-refusal-replay.bin'),'Actual refusal cold replay differs'
    if args.melee_ending:
        assert len({sha(saves/n)for n in ['nox-vortex-ending-presentation.png','nox-ending-cold.png','nox-ending-ack.png','nox-ending-ack-cold.png']})==1,'Ending raster changed across cold/acknowledgement'
    report=dict(identity,refusal_cold_replay='VERIFIED'if args.guardian_missing else'NOT STARTED',post_defeat_game_save='NOT STARTED',post_ending_behavior='VERIFIED'if args.melee_ending else'NOT STARTED',status='VERIFIED',invocations=invocations,artifacts={str(p.relative_to(root)):sha(p)for p in saves.iterdir()if p.is_file()},controlled_checkpoint=True,linked_gameplay_progression=True,earned_continuous=False,full_new_game_to_ending=False,casting='NOT STARTED'if args.melee_ending else'VERIFIED',battle_effect='VERIFIED',weapon_final_blow='VERIFIED'if args.melee_ending else'NOT STARTED',guardian_effect='VERIFIED'if args.guardian else'NOT STARTED',guardian_reward='VERIFIED'if args.guardian and not args.guardian_missing else'NOT STARTED',ending_presentation='NOT STARTED'if args.small_encounter or args.guardian else'VERIFIED')
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('VERIFIED:',root,flush=True)
if __name__=='__main__':main()
