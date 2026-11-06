#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual Nox Vortex inventory readiness from an archived real binding result."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['exe','game','predecessor','artifacts']:parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);prior_root=args.predecessor.resolve(strict=True)
    prior=json.loads((prior_root/'verification.json').read_text());assert prior['status']=='VERIFIED'
    assert all(sha(prior_root/n)==h for n,h in prior['artifacts'].items()),'Binding archive drift'
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
    saves=root/'saves';shutil.copytree(prior_root/'saves',saves)
    ce=Path(__file__).resolve().parents[2]
    sources={n:sha(ce/n)for n in ['engines/nox/spell_validation.h','engines/nox/campaign_validation.h','engines/nox/runtime.h','engines/nox/runtime.cpp','engines/nox/runtime_observers.cpp','engines/nox/nox.cpp','engines/nox/nox.h','devtools/nox/test_spell_cast.py']}
    originals={str(p):sha(p)for p in game.glob('*.HDV')};assert originals
    identity={'argv':sys.argv,'executable_sha256':sha(exe),'source_sha256':sources,'predecessor':str(prior_root),'predecessor_report_sha256':sha(prior_root/'verification.json'),'mode':prior['mode'],'originals':originals,'initializations':'Retain actual spell-ready checkpoint including controlled mage. No new initialization. Real C/1/Return and original battle-only refusal.'}
    if prior.get('retained_all_eight_actual_pages'):identity['retained_all_eight_actual_pages']=True
    (root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n');invocations=[]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    for cold in [False,True]:
        stage='cold'if cold else'warm';ini=configparser.ConfigParser(interpolation=None)
        ini['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
        ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_spell_ready_validation':'true','nox_spell_cast_validation':'true','nox_spell_ready_check_only':str(cold).lower()}
        config=root/(stage+'.ini')
        with config.open('w',encoding='utf-8')as out:ini.write(out)
        output=saves/('nox-spell-cold.tsv'if cold else'nox-spell-ready.tsv');output.unlink(missing_ok=True)
        argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}','nox-gameplay-test']
        result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
        (root/(stage+'-stderr.txt')).write_bytes(result.stderr);invocations.append({'stage':stage,'argv':argv,'exit':result.returncode});(root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n')
        assert originals=={str(p):sha(p)for p in game.glob('*.HDV')},'Original image changed'
        assert result.returncode==0 and output.exists()and output.read_text().startswith('PASS\t'),f'Spell {stage} failed: {root}'
    report=dict(identity,status='VERIFIED',invocations=invocations,artifacts={str(p.relative_to(root)):sha(p)for p in saves.iterdir()if p.is_file()},controlled_checkpoint=True,linked_gameplay_progression=True,earned_continuous=False,full_new_game_to_ending=False,world_refusal='VERIFIED',casting='IN PROGRESS',battle_effect='NOT STARTED')
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('VERIFIED:',root,flush=True)
if __name__=='__main__':main()
