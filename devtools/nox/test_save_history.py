#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Five bounded local history cases required by M2.4c, using real gameplay saves."""
import argparse
import configparser
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
from enhancement_oracle import read_save_enhancements


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('exe','game','baseline','artifacts'): ap.add_argument('--'+name,type=Path,required=True)
    args=ap.parse_args();exe=args.exe.resolve(strict=True);game=args.game.resolve(strict=True);baseline=args.baseline.resolve(strict=True)
    fixture=json.loads((baseline/'verification.json').read_text());assert fixture['status']=='VERIFIED'
    assert fixture['executable_sha256']==sha(exe),'Generate a current genuine gameplay baseline first'
    original={str(p):sha(p) for p in game.glob('*.HDV')};assert original==fixture['original_sha256']
    files=sorted((baseline/'saves').glob('nox-ce-*'));assert len(files)==8
    worlds={}
    for path in files:
        if path.suffix in ('.g0','.g1'):
            state=read_save_enhancements(path)
            worlds.setdefault(state['description'],[]).append((path,state))
    a=max(worlds['Session A slot1'],key=lambda item:struct.unpack_from('<I',item[0].read_bytes(),8)[0])
    b=max(worlds['Session B slot1'],key=lambda item:struct.unpack_from('<I',item[0].read_bytes(),8)[0])
    for path in files:
        if path.suffix=='.json':
            sidecar=json.loads(path.read_text());assert sidecar['save_hash']==sha(Path(str(path).removesuffix('.ce-achievements.json')))
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    cases=[]
    for scenario,name in enumerate(('absent','corrupt','mismatched','stale-generation','legacy-unbound'),1):
        work=root/name;saves=work/'saves';saves.mkdir(parents=True)
        for source in files:shutil.copy2(source,saves/source.name)
        shutil.copy2(baseline/'saves/ce-playthroughs.json',saves/'ce-playthroughs.json')
        ini=configparser.ConfigParser(interpolation=None)
        ini['scummvm']={'gui_return_to_launcher':'false','confirm_exit':'false','updates_check':'0'}
        ini['nox-gameplay-test']={'engineid':'nox','gameid':'nox','platform':'apple2','path':str(game),'nox_history_validation':str(scenario),'nox_history_a_file':a[0].name,'nox_history_b_file':b[0].name,'nox_history_a_owner':a[1]['session']}
        config=work/'history.ini';commands=[]
        protected_b={p.name:sha(p) for p in files if any(p.name.startswith(w[0].name[:-3]) for w in worlds['Session B slot1'])}
        after_warm=None
        for cold in (False,True):
            if cold:ini.read(config,encoding='utf-8')
            phase='cold' if cold else 'warm';ini['nox-gameplay-test']['nox_history_cold']='true' if cold else 'false'
            with config.open('w',encoding='utf-8') as out:ini.write(out)
            argv=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={work/(phase+"-engine.log")}', 'nox-gameplay-test']
            result=subprocess.run(argv,env=env,startupinfo=startup,capture_output=True,timeout=60)
            (work/(phase+'-stdout.txt')).write_bytes(result.stdout);(work/(phase+'-stderr.txt')).write_bytes(result.stderr)
            commands.append({'argv':argv,'exit':result.returncode});(work/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
            report=saves/('nox-history-cold.tsv' if cold else 'nox-history.tsv')
            assert result.returncode==0 and report.is_file() and report.read_text().startswith('PASS\t'),str(work)
            actual={p.name:sha(p) for p in saves.glob('nox-ce-*')}
            assert all(actual.get(k)==v for k,v in protected_b.items()),'Other session files changed'
            if cold:assert actual==after_warm,'Cold load rewrote save/history'
            else:after_warm=actual
            records=json.loads((saves/'ce-playthroughs.json').read_text())['playthroughs'];by_id={r['client_playthrough_id']:r for r in records}
            assert len(records)==2 and not by_id[b[1]['session']]['casual'] and by_id[b[1]['session']]['assistance_flags']==[]
            if scenario>3:assert by_id[a[1]['session']]['casual'] and 'unknown_save_history' in by_id[a[1]['session']]['assistance_flags']
            else:assert not by_id[a[1]['session']]['casual']
        cases.append({'case':name,'warm':(saves/'nox-history.tsv').read_text().strip(),'cold':(saves/'nox-history-cold.tsv').read_text().strip(),'commands':commands})
    assert all(sha(Path(p))==v for p,v in original.items())
    report={'status':'VERIFIED','scope':'Five bounded M2.4c local missing/invalid history cases; content hash is not authentication','executable_sha256':sha(exe),'fixture':str(baseline),'saved_mode':a[1]['saved_mode'],'original_sha256':original,'cases':cases,'artifacts':str(root)}
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
