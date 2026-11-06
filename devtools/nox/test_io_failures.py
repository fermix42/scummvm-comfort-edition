#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real Windows sharing-denial errors against isolated copies of actual saves.

No mocked streams or network: deny read, overwrite, commit-sidecar creation,
sidecar deletion, or registry writing; then cold-load the prior complete worlds.
"""
import argparse
import configparser
import ctypes
from ctypes import wintypes
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time
from enhancement_oracle import read_save_enhancements


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('exe','baseline','game','artifacts'):ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--scenario',choices=('read','world-write','commit-write','delete','registry-write','queue-write','complete-registry','archive-registry','complete-queue','archive-queue','metadata-save','metadata-load','assistance-registry','status'))
    args=ap.parse_args();assert os.name=='nt'
    exe=args.exe.resolve(strict=True);baseline=args.baseline.resolve(strict=True);game=args.game.resolve(strict=True)
    fixture=json.loads((baseline/'verification.json').read_text());assert fixture['status']=='VERIFIED'
    original={str(p):sha(p) for p in game.glob('*.HDV')};assert original==fixture['original_sha256']
    files=sorted((baseline/'saves').glob('nox-ce-*'));worlds={}
    for p in files:
        if p.suffix in ('.g0','.g1'):
            state=read_save_enhancements(p);sequence=struct.unpack_from('<I',p.read_bytes(),8)[0]
            if state['description'] not in worlds or sequence>worlds[state['description']][0]:worlds[state['description']]=(sequence,p,state)
    a=worlds['Observed dialogue fixture'];b=worlds['Fresh session B']
    assert a[2]['session']!=b[2]['session'] and a[2]['characters'] and a[2]['cells'],'Nonempty actual observed journal/map fixture required'
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S');root.mkdir(parents=True,exist_ok=False)
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE];kernel.CreateFileW.restype=wintypes.HANDLE
    kernel.CloseHandle.argtypes=[wintypes.HANDLE];kernel.CloseHandle.restype=wintypes.BOOL
    invalid=ctypes.c_void_p(-1).value
    env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    results=[]
    for scenario,name in enumerate(('read','world-write','commit-write','delete','registry-write','queue-write','complete-registry','archive-registry','complete-queue','archive-queue','metadata-save','metadata-load','assistance-registry'),1):
        if args.scenario=="status" and scenario<7:continue
        if args.scenario and args.scenario!="status" and name!=args.scenario:continue
        work=root/name;saves=work/'saves';saves.mkdir(parents=True)
        for p in files:shutil.copy2(p,saves/p.name)
        for name_to_copy in ('ce-playthroughs.json','ce-achievements-queue.json'):
            if (baseline/'saves'/name_to_copy).exists():shutil.copy2(baseline/'saves'/name_to_copy,saves/name_to_copy)
        ini=configparser.ConfigParser(interpolation=None);ini.read(baseline/'dialogue.ini',encoding='utf-8')
        assert ini.has_section('scummvm') and not ini['scummvm'].get('ce_link_token')
        ini['scummvm']['ce_achievements_last_sync_status']='I/O validation baseline: no queue failure reported' 
        domain=ini['nox-gameplay-test'];domain['nox_io_validation']=str(scenario);domain['nox_io_cold']='false';domain['nox_io_a_file']=a[1].name;domain['nox_io_b_file']=b[1].name
        config=work/'io.ini'
        with config.open('w',encoding='utf-8') as out:ini.write(out)
        hashes={p.name:sha(p) for p in files};b_prefix=b[1].name[:-3];protected={k:v for k,v in hashes.items() if k.startswith(b_prefix)}
        commands=[];failure=None;handle=None;child=None
        def command(phase):return [str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={work/(phase+"-engine.log")}', 'nox-gameplay-test']
        try:
            argv=command('warm');child=subprocess.Popen(argv,env=env,startupinfo=startup,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            marker=saves/'nox-io-ready.tsv';deadline=time.monotonic()+15
            def marker_text(path):
                try:
                    text=path.read_text()
                    return text if text.endswith('\n') else ''
                except OSError:return ''
            while not marker_text(marker) and child.poll() is None and time.monotonic()<deadline:time.sleep(.01)
            assert marker_text(marker) and child.poll() is None,'Real lock coordination marker missing'
            target=marker_text(marker).strip();path=(saves/target).resolve();assert path.parent==saves.resolve() and path.name==target
            assert target.startswith('nox-ce-') or scenario>=5 and target in ('ce-playthroughs.json','ce-achievements-queue.json')
            if scenario==3:assert not path.exists(),'Commit-sidecar denial was not after old metadata removal'
            share=0 if scenario==1 else 3 if scenario==4 else 1
            handle=kernel.CreateFileW(str(path),0x80000000,share,None,4 if scenario==3 else 3,0x80,None)
            assert handle not in (None,invalid),'Windows lock acquisition failed: '+str(ctypes.get_last_error())
            (saves/'nox-io-locked.tsv').write_text('LOCKED\n')
            result_path=saves/'nox-io-result.tsv';deadline=time.monotonic()+15
            while not marker_text(result_path) and child.poll() is None and time.monotonic()<deadline:time.sleep(.01)
            assert marker_text(result_path) and child.poll() is None,'I/O operation did not reach coordinated release point'
            kernel.CloseHandle(handle);handle=None
            (saves/'nox-io-unlocked.tsv').write_text('UNLOCKED\n')
            stdout,stderr=child.communicate(timeout=20)
            (work/'warm-stdout.txt').write_bytes(stdout);(work/'warm-stderr.txt').write_bytes(stderr)
            commands.append({'argv':argv,'exit':child.returncode,'locked_file':target,'share_mode':share,'real_windows_lock':True,'released_before_exit':True})
            assert child.returncode==0 and (saves/'nox-io-release.tsv').read_text().startswith('PASS\t'),'Warm process/release acknowledgement failed'
            fields=(saves/'nox-io-result.tsv').read_text().strip().split('\t');actual=dict(x.split('=',1) for x in fields[1:])
            assert fields[0]=='RESULT' and all(actual[k]=='1' for k in ('rejected','live_unchanged','history_unchanged','prior_complete')),'Actual failure/rollback gate: '+json.dumps(actual)
            assert all((saves/k).exists() and sha(saves/k)==v for k,v in protected.items()),'Unrelated B generation/sidecar changed'
            if scenario in (1,4) or scenario>=5:assert {p.name:sha(p) for p in saves.glob('nox-ce-*')}==hashes,'Failed operation did not restore all original world/sidecar files'
            assert sha(saves/a[1].name)==hashes[a[1].name] and sha(saves/(a[1].name+'.ce-achievements.json'))==hashes[a[1].name+'.ce-achievements.json'],'Prior complete generation/commit changed'
            ini.read(config,encoding='utf-8');ini['nox-gameplay-test']['nox_io_cold']='true'
            with config.open('w',encoding='utf-8') as out:ini.write(out)
            argv=command('cold');cold=subprocess.run(argv,env=env,startupinfo=startup,capture_output=True,timeout=25)
            (work/'cold-stdout.txt').write_bytes(cold.stdout);(work/'cold-stderr.txt').write_bytes(cold.stderr)
            commands.append({'argv':argv,'exit':cold.returncode})
            assert cold.returncode==0 and (saves/'nox-io-cold.tsv').read_text().startswith('PASS\t'),'Prior complete world cold recovery failed'
            assert all((saves/k).exists() and sha(saves/k)==v for k,v in protected.items()),'Cold recovery changed unrelated B files'
        except (AssertionError,FileNotFoundError,subprocess.TimeoutExpired) as error:failure=str(error)
        finally:
            if child and child.poll() is None:child.kill();child.communicate(timeout=10)
            if handle not in (None,invalid):kernel.CloseHandle(handle)
            (work/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        results.append({'case':name,'status':'IN PROGRESS' if failure else 'VERIFIED','failure':failure,'actual_files':{p.name:sha(p) for p in saves.glob('nox-ce-*')},'modified_fixture_files':[name for name in sorted(set(hashes)|{p.name for p in saves.glob('nox-ce-*')}) if not (saves/name).exists() or sha(saves/name)!=hashes.get(name)],'commands':commands})
    assert all(sha(Path(p))==v for p,v in original.items())
    report={'status':'IN PROGRESS' if any(r['failure'] for r in results) else 'VERIFIED','scope':'Actual Windows sharing-denial read/world-write/commit-write/delete/registry-write; full live state and prior cold world recovery, unrelated session isolation','executable_sha256':sha(exe),'fixture_executable_sha256':fixture['executable_sha256'],'fixture':str(baseline),'enhancement_provenance':'actual observed dialogue in explicitly assisted authored-exit adjacency lifecycle fixture; no earned campaign claim','save_mode':a[2]['saved_mode'],'cases':results,'original_sha256':original,'artifacts':str(root)}
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
    if report['status']!='VERIFIED':raise SystemExit(1)


if __name__=='__main__':main()
