#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ordinary local CE session archive/delete/keep-save workflows through real GUI.

Uses previously verified normal gameplay saves; does not invent world state or
contact the website. Every case uses its own copy, then a fresh-process reload.
"""
import argparse
import configparser
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from enhancement_oracle import read_save_enhancements


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe',type=Path,required=True);ap.add_argument('--game',type=Path,required=True)
    ap.add_argument('--baseline',type=Path,required=True);ap.add_argument('--artifacts',type=Path,required=True)
    ap.add_argument('--scenario',choices=('archive','delete-keep','delete-saves','empty-session'),help='Run one bounded workflow instead of all four')
    args=ap.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);baseline=args.baseline.resolve(strict=True)
    original={str(p):sha(p) for p in game.glob('*.HDV')};assert original
    fixture=json.loads((baseline/'verification.json').read_text());assert fixture['status']=='VERIFIED'
    assert fixture['original_sha256']==original
    data_files=sorted((baseline/'saves').glob('nox-ce-*'));assert len(data_files)==8
    worlds={}
    for path in data_files:
        if path.suffix in ('.g0','.g1'):
            state=read_save_enhancements(path);worlds.setdefault(state['description'],state)
    a,b=worlds['Session A slot1'],worlds['Session B slot1'];assert a['session']!=b['session'] and a['saved_mode']==b['saved_mode']
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    cases=[]
    for scenario,name in ((1,'archive'),(2,'delete-keep'),(3,'delete-saves'),(4,'empty-session')):
        if args.scenario and name!=args.scenario:continue
        work=root/name;saves=work/'saves';saves.mkdir(parents=True)
        for source in data_files: shutil.copy2(source,saves/source.name)
        shutil.copy2(baseline/'saves/ce-playthroughs.json',saves/'ce-playthroughs.json')
        baseline_hashes={p.name:sha(p) for p in data_files}
        a_files=set()
        for path in data_files:
            if path.suffix in ('.g0','.g1') and read_save_enhancements(path)['session']==a['session']:
                a_files.add(path.name);a_files.add(path.name+'.ce-achievements.json')
        ini=configparser.ConfigParser(interpolation=None)
        ini['scummvm']={'gui_return_to_launcher':'false','confirm_exit':'false','updates_check':'0','gui_saveload_chooser':'list'}
        ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),
            'nox_management_validation':str(scenario),'nox_management_original_a':a['session'],
            'nox_classic_saves':'true' if a['saved_mode']=='Classic' else 'false'}
        config=work/'management.ini'
        with config.open('w',encoding='utf-8') as out:ini.write(out)
        commands=[]
        for cold in (False,True):
            phase='cold' if cold else 'warm';ini['nox-gameplay-test']['nox_management_cold']='true' if cold else 'false'
            # Read back the real persisted CE configuration before setting one
            # diagnostic field; do not erase install/session state on restart.
            if cold:
                ini.read(config,encoding='utf-8');ini['nox-gameplay-test']['nox_management_cold']='true'
            with config.open('w',encoding='utf-8') as out:ini.write(out)
            argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={work / (phase+"-engine.log")}', 'nox-gameplay-test']
            result=subprocess.run(argv,env=env,startupinfo=startup,capture_output=True,timeout=60)
            (work/(phase+'-stdout.txt')).write_bytes(result.stdout);(work/(phase+'-stderr.txt')).write_bytes(result.stderr)
            commands.append({'argv':argv,'exit':result.returncode})
            (work/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
            assert result.returncode==0,f'Management process failed: {work}'
            report=saves/('nox-management-cold.tsv' if cold else 'nox-management.tsv')
            assert report.is_file() and report.read_text().startswith('PASS\t'),f'Workflow acceptance missing: {work}'
            actual={p.name:sha(p) for p in saves.glob('nox-ce-*')}
            protected=set(baseline_hashes)-(a_files if scenario==3 else set())
            assert all(actual.get(key)==baseline_hashes[key] for key in protected),'Kept save or other session changed'
            if scenario==3:assert a_files.isdisjoint(actual),'Deleted generation/sidecar revived'
            registry=json.loads((saves/'ce-playthroughs.json').read_text())['playthroughs'];by_id={r['client_playthrough_id']:r for r in registry}
            assert b['session'] in by_id and not by_id[b['session']]['casual'] and by_id[b['session']]['assistance_flags']==[]
            assert len(registry)==(1 if scenario==3 else 2)
            if scenario in (2,3):assert a['session'] not in by_id
            queue_path=saves/'ce-achievements-queue.json'
            if queue_path.exists():
                queued=json.loads(queue_path.read_text())
                if scenario in (2,3):
                    deleted=[e for e in queued if e['client_playthrough_id']==a['session']]
                    assert deleted and all(e['state']=='permanent_failure' for e in deleted),'Deleted offline owner remains pending upload'
                if scenario==4:
                    deleted=[e for e in queued if e['client_playthrough_id'] not in by_id]
                    assert deleted and any(e['request_type']=='playthrough_start' for e in deleted) and all(e['state']=='permanent_failure' for e in deleted),'Deleted empty session remains pending upload'

            if scenario==1:assert by_id[a['session']]['status']=='abandoned' and not by_id[a['session']]['archived']
            if scenario==2:
                recovered=[r for r in registry if r['client_playthrough_id']!=b['session']];assert len(recovered)==1
                assert recovered[0]['casual'] and 'unknown_save_history' in recovered[0]['assistance_flags']
                assert len(actual)==12 and len(actual.keys()-baseline_hashes.keys())==4
                recovered_worlds=[read_save_enhancements(p) for p in saves.glob('nox-ce-*.g[01]') if p.name not in baseline_hashes]
                assert len(recovered_worlds)==2 and all(w['session']==recovered[0]['client_playthrough_id'] and w['characters']==a['characters'] and w['cells']==a['cells'] and w['saved_mode']==a['saved_mode'] for w in recovered_worlds)
            assert all(sha(Path(path))==digest for path,digest in original.items()),'Original HDV modified'
        captures=list(saves.glob('nox-management-*.png'));assert captures
        cases.append({'scenario':name,'warm':(saves/'nox-management.tsv').read_text().strip(),'cold':(saves/'nox-management-cold.tsv').read_text().strip(),'captures':len(captures),'commands':commands})
    evidence={'status':'VERIFIED','scope':('Selected '+args.scenario+' workflow with actual chooser mouse/confirmation and cold restart; no other scenario claim') if args.scenario else 'Actual local chooser mouse/confirmation archive/show/unarchive and delete with/without saves, empty session, exact-world recovery in a new Casual owner and cold restart. No website or complete corrupt-history/failure-matrix claim.','executable_sha256':sha(exe),'fixture_executable_sha256':fixture['executable_sha256'],'fixture':str(baseline),'saved_mode':a['saved_mode'],'original_sha256':original,'cases':cases,'artifacts':str(root)}
    (root/'verification.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(evidence,indent=2))


if __name__=='__main__':main()
