#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Kill an owned CE process during a real save overwrite, then verify rollback."""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--game',type=Path,required=True)
    parser.add_argument('--artifacts',type=Path,required=True)
    parser.add_argument('--enhancement-fixture',action='store_true',help='Use explicitly controlled nonempty journal/discovery owners for rollback')
    parser.add_argument('--classic',action='store_true',help='Use Classic-mode session saves')
    args=parser.parse_args()
    baseline_command=[sys.executable,str(Path(__file__).with_name('test_gameplay.py')),
        '--exe',str(args.exe),'--game',str(args.game),'--artifacts',str(args.artifacts),'--native-video']
    if args.enhancement_fixture:baseline_command.append('--enhancement-fixture')
    if args.classic:baseline_command.append('--classic')
    baseline=subprocess.run(baseline_command,
        capture_output=True,text=True,timeout=180)
    assert baseline.returncode==0, baseline.stderr
    evidence=json.loads(baseline.stdout)
    if args.enhancement_fixture:assert evidence['controlled_enhancement_generations']==4
    root=Path(evidence['artifacts']);saves=root/'saves';config=root/'gameplay.ini'
    old_hashes={p.name:sha(p) for p in saves.glob('nox-ce-*.g?')}
    frame_a=(saves/'nox-engine-cold-a.png').read_bytes()
    command=[f'--logfile={root / "interrupted-engine.log"}' if item.startswith('--logfile=') else item for item in evidence['cold_argv']]
    ini=configparser.ConfigParser(interpolation=None);ini.read(config,encoding='utf-8')
    ini['nox-gameplay-test']['nox_failure_validation']='1'
    with config.open('w',encoding='utf-8') as out:ini.write(out)
    env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
    startup=None
    if os.name=='nt':
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    marker=saves/'nox-partial-write-ready.tsv'
    process=subprocess.Popen(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,startupinfo=startup)
    try:
        deadline=time.monotonic()+30
        while not marker.exists() and process.poll() is None and time.monotonic()<deadline:
            time.sleep(0.05)
        assert marker.exists() and process.poll() is None, 'Partial-write marker missing or child exited early'
        fields=marker.read_text().strip().split('\t')
        ready=dict(item.split('=',1) for item in fields[1:])
        assert fields[0]=='READY' and (ready['x'],ready['y'])==('24','38')
        protected=saves/ready['protected'];partial=saves/ready['partial']
        assert protected.parent.resolve()==saves.resolve() and partial.parent.resolve()==saves.resolve()
        assert protected.name in old_hashes and sha(protected)==old_hashes[protected.name]
        assert partial.exists() and 32<partial.stat().st_size<protected.stat().st_size, 'Not a partial actual snapshot'
        process.kill()
        stdout,stderr=process.communicate(timeout=10)
        (root/'interrupted-stdout.txt').write_bytes(stdout);(root/'interrupted-stderr.txt').write_bytes(stderr)
        assert process.returncode!=0 and sha(protected)==old_hashes[protected.name]
        assert not (saves/(partial.name+'.ce-achievements.json')).exists(), 'Partial generation was committed'
        interrupted={'argv':command,'exit':process.returncode,'protected':protected.name,
                     'protected_sha256':sha(protected),'partial':partial.name,'partial_bytes':partial.stat().st_size,
                     'new_x':24,'recovered_expected_x':25}
        (root/'interrupted-invocation.json').write_text(json.dumps(interrupted,indent=2)+'\n')
    finally:
        if process.poll() is None:
            process.kill();process.communicate(timeout=10)
    ini['nox-gameplay-test']['nox_failure_validation']='2'
    with config.open('w',encoding='utf-8') as out:ini.write(out)
    recovery_command=[f'--logfile={root / "recovery-engine.log"}' if item.startswith('--logfile=') else item for item in command]
    recovered=subprocess.run(recovery_command,env=env,capture_output=True,timeout=60,startupinfo=startup)
    (root/'recovery-stdout.txt').write_bytes(recovered.stdout);(root/'recovery-stderr.txt').write_bytes(recovered.stderr)
    assert recovered.returncode==0 and (saves/'nox-failure.tsv').is_file(), 'Recovery/rollback report missing'
    report=(saves/'nox-failure.tsv').read_text().strip()
    assert report.startswith('PASS\t') and (saves/'nox-engine-cold-a.png').read_bytes()==frame_a
    assert sha(protected)==old_hashes[protected.name]
    assert all(sha(Path(p))==value for p,value in evidence['original_sha256'].items()), 'Original HDV changed'
    result={'status':'VERIFIED','scope':'actual killed-process gameplay overwrite recovery and corrupt-load live-state rollback',
            'enhancement_provenance':'controlled-owner-fixture' if args.enhancement_fixture else 'none',
            'executable_sha256':evidence['executable_sha256'],'interrupted':interrupted,'recovery_argv':recovery_command,'recovery_exit':recovered.returncode,
            'recovery':report,'original_sha256':evidence['original_sha256'],'artifacts':str(root)}
    (root/'failure-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
