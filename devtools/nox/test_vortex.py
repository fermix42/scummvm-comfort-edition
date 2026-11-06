#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Controlled seven-page prerequisite -> real Axea -> Valess -> Nystul, with labeled setups."""
import argparse,configparser,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',required=True,type=Path)
    parser.add_argument('--game',required=True,type=Path)
    parser.add_argument('--artifacts',required=True,type=Path)
    parser.add_argument('--classic',action='store_true')
    parser.add_argument('--through',type=int,choices=(1,2,3),default=3)
    parser.add_argument('--resume',type=Path,help='Owned existing test root; continue from its completed phase')
    parser.add_argument('--earned-pages',type=Path,help='Verified seven linked real Search result; preserve all earned page flags/counts')
    args=parser.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True)
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    if args.resume:
        predecessor=args.resume.resolve(strict=True)
        assert (predecessor/'phase-1-complete.json').exists(),'Resume requires a verified first phase'
        for audit in sorted(predecessor.glob('phase-*-complete.json')):
            evidence=json.loads(audit.read_text())
            assert evidence['mode']==('Classic' if args.classic else 'Modern'),'Resume save mode mismatch'
            assert all(sha(predecessor/name)==expected for name,expected in evidence['artifacts'].items()),'Predecessor archive drift'
        shutil.copytree(predecessor,root)
    saves=root/'saves'
    if args.earned_pages:
        assert not args.resume
        predecessor=args.earned_pages.resolve(strict=True);prior=json.loads((predecessor/'verification.json').read_text());assert prior['status']=='VERIFIED' and prior['linked_seven_pages']=='VERIFIED' and prior['mode']==('Classic'if args.classic else'Modern')
        assert all(sha(predecessor/n)==h for n,h in prior['artifacts'].items());shutil.copytree(predecessor/'saves',saves)
    elif not args.resume:saves.mkdir(parents=True,exist_ok=False)
    else:
        last=max(p for p in (1,2,3) if (root/f'phase-{p}-complete.json').exists())
        # A failed later attempt may have written its slot before its cold check.
        # Restore the actual last verified predecessor into this new owned copy.
        for artifact in (root/f'phase-{last}-saves').iterdir():
            if artifact.is_file():shutil.copyfile(artifact,saves/artifact.name)
    ce=Path(__file__).resolve().parents[2];sources={str(p.relative_to(ce)).replace('\\','/'):sha(p)for p in(ce/'engines/nox').iterdir()if p.is_file()};sources['devtools/nox/test_vortex.py']=sha(Path(__file__))
    identity={'argv':sys.argv,'executable_sha256':sha(exe),'source_sha256':sources,'classic':args.classic,'controlled_initialization':'Original party retained. BCD noon plus source-checked inventory97 quantity7/quest43E=FD initialized once before Axea. Starting seven pages are CONTROLLED, not earned. All eighth-page/research/binding outcomes are real and retained; map/adjacency setups explicitly labelled.'}
    if args.earned_pages:identity.update(predecessor=str(predecessor),predecessor_report_sha256=sha(predecessor/'verification.json'),controlled_initialization='Retain ALL SEVEN actual Search page97 awards/43EFD/qty7 from verified linked collection; no page/count/flag initialization. Original starting party retained. CONTROLLED BCDnoon/authored castle57->58->54->surface->Axea35 route and NPC adjacency before real Axea VORTEX/repeat, then retained actual eight pages -> real Valess CREATE/repeat -> real Nystul BIND/repeat. No award/research/binding outcome writes. Controlled travel is not earned continuous campaign.')
    (root/('resume-identity.json' if args.resume else 'input-identity.json')).write_text(json.dumps(identity,indent=2)+'\n')
    originals={str(p):sha(p) for p in game.glob('*.HDV')};assert originals
    ini=configparser.ConfigParser(interpolation=None)
    ini['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
    ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_classic_saves':str(args.classic).lower()}
    ini['nox-gameplay-test']['nox_vortex_earned_pages']=str(bool(args.earned_pages)).lower()
    config=root/'vortex.ini';invocations=[]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    for phase in range(1,args.through+1):
        if args.resume and (root/f'phase-{phase}-complete.json').exists():continue
        for stage in ['warm','cold']:
            domain=ini['nox-gameplay-test'];domain['nox_vortex_phase']=str(phase);domain['nox_vortex_check_only']=str(stage=='cold').lower();domain['nox_campaign_reload']=str(stage=='cold' or phase>1 or bool(args.earned_pages)).lower()
            with config.open('w',encoding='utf-8') as out:ini.write(out)
            prefix=f'phase-{phase}-{stage}';argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(prefix+"-engine.log")}', 'nox-gameplay-test']
            shutil.copyfile(config,root/(prefix+'.ini'))
            output=saves/(f'nox-vortex-phase-{phase}.tsv' if stage=='warm' else f'nox-vortex-cold-{phase}.tsv')
            output.unlink(missing_ok=True) # Only an owned, newly copied/generated diagnostic report.
            result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=120)
            (root/(prefix+'-stderr.txt')).write_bytes(result.stderr);invocations.append({'phase':phase,'stage':stage,'argv':argv,'exit':result.returncode})
            (root/'invocations-latest.json').write_text(json.dumps(invocations,indent=2)+'\n')
            assert originals=={str(p):sha(p) for p in game.glob('*.HDV')},'Original image changed'
            assert result.returncode==0 and output.exists() and output.read_text().startswith('PASS\t'),f'{prefix} failed: {root}'
        archive=root/f'phase-{phase}-saves';shutil.copytree(saves,archive)
        report={'status':'VERIFIED','phase':phase,'mode':'Classic' if args.classic else 'Modern','executable_sha256':sha(exe),'source_sha256':sources,'originals':originals,'invocations':[i for i in invocations if i['phase']==phase],'artifacts':{str(p.relative_to(root)):sha(p) for p in archive.iterdir() if p.is_file()},'coverage_category':'linked gameplay progression from controlled map/adjacency setups','earned_continuous':False,'full_new_game_to_ending':False}
        (root/f'phase-{phase}-complete.json').write_text(json.dumps(report,indent=2)+'\n');print(f'VERIFIED phase {phase}: {root}',flush=True)
    if args.earned_pages and args.through==3:
        final=dict(identity,status='VERIFIED',mode='Classic'if args.classic else'Modern',invocations=invocations,artifacts={str(p.relative_to(root)):sha(p)for p in saves.iterdir()if p.is_file()},controlled_checkpoint='VERIFIED',linked_gameplay_progression='VERIFIED',all_eight_actual_pages_research_binding='VERIFIED',earned_continuous_progression='NOT STARTED',full_new_game_to_ending='NOT STARTED')
        (root/'verification.json').write_text(json.dumps(final,indent=2)+'\n')
    print(root,flush=True)
if __name__=='__main__':main()
