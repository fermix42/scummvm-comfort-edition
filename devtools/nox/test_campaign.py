#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Controlled campaign checkpoint through real CE operations; no earned-run claim."""
import argparse,configparser,datetime,hashlib,json,os,subprocess
from pathlib import Path

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--game',type=Path,required=True)
    parser.add_argument('--artifacts',type=Path,required=True)
    parser.add_argument('--classic',action='store_true')
    parser.add_argument('--linked',action='store_true',help='Retain acquired log, use ordinary return route and verify real NPC recognition')
    parser.add_argument('--page',type=int,choices=range(1,9),help='Source-guarded parchment Search1..7;8 chains all seven actual acquisitions')
    parser.add_argument('--missing-page-tools',action='store_true')
    parser.add_argument('--page-seed',type=int,default=0)
    args=parser.parse_args()
    assert not(args.page and args.linked)
    assert 0<=args.page_seed<=0x7fffffff and (not args.missing_page_tools or args.page==8)
    exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True)
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    saves=root/'saves';saves.mkdir(parents=True,exist_ok=False)
    ce=Path(__file__).resolve().parents[2];sources={str(p.relative_to(ce)).replace('\\','/'):sha(p)for p in(ce/'engines/nox').iterdir()if p.is_file()};sources['devtools/nox/test_campaign.py']=sha(Path(__file__))
    identity={'executable_sha256':sha(exe),'source_sha256':sources,'page':args.page,'linked':args.linked,'classic':args.classic,'missing_page_tools':args.missing_page_tools,'page_seed':args.page_seed}
    (root/'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    originals={str(p):sha(p) for p in game.glob('*.HDV')};assert originals
    ini=configparser.ConfigParser(interpolation=None)
    ini['scummvm']={'gui_return_to_launcher':'false','updates_check':'0'}
    ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_gameplay_validation':'true','nox_campaign_validation':'true','nox_classic_saves':str(args.classic).lower()}
    ini['nox-gameplay-test']['nox_campaign_linked']=str(args.linked).lower()
    ini['nox-gameplay-test']['nox_campaign_page']=str(args.page or 0)
    ini['nox-gameplay-test']['nox_pages_missing_tools']=str(args.missing_page_tools).lower();ini['nox-gameplay-test']['nox_pages_seed']=str(args.page_seed)
    config=root/'campaign.ini';invocations=[]
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    for stage in ('warm','cold'):
        ini['nox-gameplay-test']['nox_campaign_reload']=str(stage=='cold').lower()
        with config.open('w',encoding='utf-8') as out:ini.write(out)
        argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root/(stage+"-engine.log")}', 'nox-gameplay-test']
        result=subprocess.run(argv,env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy'),startupinfo=startup,capture_output=True,timeout=90)
        (root/(stage+'-stderr.txt')).write_bytes(result.stderr)
        invocations.append({'argv':argv,'exit':result.returncode})
        (root/'invocations.json').write_text(json.dumps(invocations,indent=2)+'\n')
        assert originals=={str(p):sha(p) for p in game.glob('*.HDV')},'Original image changed'
        report=saves/('nox-campaign-cold.tsv' if stage=='cold' else 'nox-campaign.tsv')
        assert result.returncode==0 and report.exists() and report.read_text().startswith('PASS\t'),f'{stage} failed: {root}'
    outputs={str(p.relative_to(root)):sha(p) for p in saves.iterdir() if p.is_file()}
    report={'status':'VERIFIED','scope':'controlled Captain log acquisition, repeat Search and exact cold restoration only','coverage_category':'controlled checkpoint verified','earned_continuous':False,'full_new_game_to_ending':False,'linked_npc_reaction':'NOT STARTED','executable_sha256':sha(exe),'originals':originals,'invocations':invocations,'artifacts':outputs,'setup':'position/camera/redraw before actual Open/Search; no injected item/quest outcome','mode':'Classic' if args.classic else 'Modern'}
    report['source_sha256']=sources
    if args.linked:
        assert 'linked_recognition=1' in (saves/'nox-campaign.tsv').read_text()
        assert 'linked_recognition=1' in (saves/'nox-campaign-cold.tsv').read_text()
        report.update(scope='controlled acquisition followed by linked ordinary-input NPC recognition, repeat refusal and exact cold restoration',linked_npc_reaction='VERIFIED',coverage_category='linked gameplay progression verified from controlled checkpoint')
    if args.page:
        mask=(0 if args.missing_page_tools else 253)if args.page==8 else [4,8,16,32,64,128,1][args.page-1]
        assert f'page_mask={mask}\t' in (saves/'nox-campaign.tsv').read_text()
        report.update(scope='controlled parchment acquisition, repeat Search and exact cold restoration only',page_mask=mask,setup='three authored-exit loader setups then source-guarded adjacency; all before tested Search; no page/item/quest outcomes injected')
        if args.page==8:report.update(scope='Seven linked actual parchment Search/repeats/exact cold restoration; no page/count/flag initialization',linked_seven_pages='VERIFIED',setup='CONTROLLED authored ship->surface->54->78 then78->54->58->57 routes and source-object adjacency before each Search only. All seven real inventory97 awards/matching43Ebits retained; starting50HP unchanged. Not earned unrelated travel/locks or full continuous campaign.')
        if args.page==8:report.update(page_seed=args.page_seed,setup=report['setup']+' CONTROLLED initial lockpicks421='+str(0 if args.missing_page_tools else 25)+'; valid original PRNG6307..A='+str(args.page_seed)+' before each first Search only. Original pick/trap rolls, consumption, HP and all page outcomes execute unchanged. All source floors checked against actual blocked terrain ranges; no swimming/vehicle flag initialization.')
        if args.missing_page_tools:report.update(scope='Actual no-lockpick original chest Search refusal/no page award and exact cold',linked_seven_pages='NOT STARTED',no_lockpick_refusal='VERIFIED')
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(root);print(report['scope'])
if __name__=='__main__':main()
