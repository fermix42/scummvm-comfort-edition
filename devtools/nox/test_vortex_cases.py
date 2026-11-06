#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Refusal and alternate binding paths from immutable earned Vortex checkpoints."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',required=True,type=Path)
    parser.add_argument('--game',required=True,type=Path)
    parser.add_argument('--predecessor',required=True,type=Path)
    parser.add_argument('--artifacts',required=True,type=Path)
    parser.add_argument('--case',type=int,choices=(1,2,3),action='append')
    args=parser.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);predecessor=args.predecessor.resolve(strict=True)
    prior=json.loads((predecessor/'phase-1-complete.json').read_text())
    assert prior['status']=='VERIFIED'
    assert all(sha(predecessor/name)==expected for name,expected in prior['artifacts'].items()),'Predecessor archive drift'
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
    ce=Path(__file__).resolve().parents[2]
    sources={name:sha(ce/name) for name in ['engines/nox/vortex_validation.h','engines/nox/campaign_validation.h','engines/nox/runtime.h','engines/nox/dialogue_validation.h','engines/nox/nox.cpp','engines/nox/nox.h','devtools/nox/test_vortex_cases.py']}
    originals={str(p):sha(p) for p in game.glob('*.HDV')};assert originals
    identity={'argv':sys.argv,'executable_sha256':sha(exe),'source_sha256':sources,'predecessor':str(predecessor),'predecessor_report_sha256':sha(predecessor/'phase-1-complete.json'),'mode':prior['mode'],'originals':originals,'initializations':'Only authored map exit loaders and source-supported adjacency/redraw. Branch from explicitly controlled seven-page prerequisite or actual Axea eight-page state. Starting seven pages are not earned. No outcome injection.'}
    (root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    for case in args.case or (1,2,3):
        case_root=root/f'case-{case}';case_root.mkdir();saves=case_root/'saves';shutil.copytree(predecessor/'phase-1-saves',saves)
        invocations=[]
        for cold in (False,True):
            stage='cold' if cold else 'warm';ini=configparser.ConfigParser(interpolation=None)
            ini['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
            ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_campaign_reload':'true','nox_classic_saves':str(prior['mode']=='Classic').lower(),'nox_vortex_phase':'2','nox_vortex_scenario':str(case),'nox_vortex_check_only':str(cold).lower()}
            config=case_root/(stage+'.ini')
            with config.open('w',encoding='utf-8') as out:ini.write(out)
            output=saves/(f'nox-vortex-scenario-cold-{case}.tsv' if cold else f'nox-vortex-scenario-{case}.tsv')
            assert not output.exists(),'Copied unexpected scenario result'
            argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={case_root/(stage+"-engine.log")}', 'nox-gameplay-test']
            result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
            (case_root/(stage+'-stderr.txt')).write_bytes(result.stderr);invocations.append({'stage':stage,'argv':argv,'exit':result.returncode})
            (case_root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n')
            assert originals=={str(p):sha(p) for p in game.glob('*.HDV')},'Original image changed'
            assert result.returncode==0 and output.exists() and output.read_text().startswith('PASS\t'),f'Case {case} {stage} failed: {root}'
        report=dict(identity,status='VERIFIED',scenario=case,invocations=invocations,artifacts={str(p.relative_to(case_root)):sha(p) for p in saves.iterdir() if p.is_file()},controlled_checkpoint=True,linked_gameplay_progression=True,earned_continuous=False,full_new_game_to_ending=False)
        (case_root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(f'VERIFIED case {case}: {case_root}',flush=True)
    print(root,flush=True)
if __name__=='__main__':main()
