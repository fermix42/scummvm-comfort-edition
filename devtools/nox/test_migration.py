#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cold-load actual M1-v1 saves through the current CE executable, without rewriting originals."""
import argparse
import configparser
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--game',type=Path,required=True)
    parser.add_argument('--m1-evidence',type=Path,required=True)
    parser.add_argument('--artifacts',type=Path,required=True)
    args=parser.parse_args()
    original=args.m1_evidence.resolve(strict=True);exe=args.exe.resolve(strict=True)
    game=args.game.resolve(strict=True);repo=Path(__file__).resolve().parents[2]
    root=args.artifacts.resolve()/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    assert not root.is_relative_to(repo),'Private gameplay fixtures must remain outside CE Git'
    root.mkdir(parents=True,exist_ok=False);saves=root/'saves';saves.mkdir()
    expected=json.loads((original/'verification.json').read_text())
    assert expected['status']=='VERIFIED'
    originals={str(p):sha(p) for p in game.glob('*.HDV')}
    sources={p.name:sha(p) for p in (original/'saves').iterdir() if p.is_file()}
    copied=[]
    for p in (original/'saves').iterdir():
        if p.name=='ce-playthroughs.json' or p.name.startswith('nox-ce-') or p.name.startswith('nox-gameplay-snapshot-fixture.g'):
            shutil.copy2(p,saves/p.name);copied.append(p.name)
            if p.suffix in ('.g0','.g1'):
                assert p.read_bytes()[:4]==b'NXSV' and struct.unpack_from('<I',p.read_bytes(),4)[0]==1,'Not a genuine M1-v1 generation'
    config=root/'migration.ini'
    config.write_text('[scummvm]\ngui_return_to_launcher=false\nconfirm_exit=false\nupdates_check=0\nce_link_token=\n\n[nox-gameplay-test]\nengineid=nox\ngameid=nox\nplatform=apple2\npath='+str(game)+'\nnox_snapshot_validation=true\nnox_classic_saves=true\n',encoding='utf-8')
    command=[str(exe),'--gfx-mode=surfacesdl',f'--config={config}',f'--savepath={saves}',f'--logfile={root / "engine.log"}','nox-gameplay-test']
    startup=None
    if os.name=='nt':
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    env=dict(os.environ,SDL_VIDEODRIVER='windows',SDL_AUDIODRIVER='dummy')
    run=subprocess.run(command,env=env,capture_output=True,timeout=60,startupinfo=startup)
    (root/'stdout.txt').write_bytes(run.stdout);(root/'stderr.txt').write_bytes(run.stderr)
    assert run.returncode==0 and (saves/'nox-engine-cold.tsv').read_text().startswith('PASS\t'),'M1 Engine migration failed'
    assert 'saved_mode_restored=1' in (saves/'nox-engine-cold.tsv').read_text(),'M1 Modern policy did not override new-session Classic default'
    for new,old in [('nox-cold-restored.png','nox-gameplay.png'),('nox-engine-cold-a.png','nox-engine-cold-a.png'),('nox-engine-cold-b.png','nox-engine-cold-b.png')]:
        assert (saves/new).read_bytes()==(original/'saves'/old).read_bytes(),'M1 restored frame differs'
    old_records=json.loads((original/'saves/ce-playthroughs.json').read_text())['playthroughs']
    records=json.loads((saves/'ce-playthroughs.json').read_text())['playthroughs']
    assert {r['client_playthrough_id'] for r in records}=={r['client_playthrough_id'] for r in old_records},'M1 session identity changed'
    assert all(r['casual'] and 'unknown_save_history' in r['assistance_flags'] for r in records),'Unbound M1 history was trusted'
    assert all(sha(saves/name)==sources[name] for name in copied if name.startswith('nox-ce-')),'Legacy world/sidecar rewritten merely by loading'
    assert all(sha(original/'saves'/name)==digest for name,digest in sources.items()),'M1 evidence modified'
    assert all(sha(Path(p))==digest for p,digest in originals.items()),'Original HDV modified'
    report={'status':'VERIFIED','scope':'actual M1 envelope-v1 cold runtime/Engine loads, two session identities and continuation; source evidence unchanged',
            'executable_sha256':sha(exe),'m1_executable_sha256':expected['executable_sha256'],'m1_evidence':str(original),'copied_fixtures':copied,'legacy_history':'same-session-permanent-Casual-unknown; original copied snapshots/sidecars unchanged',
            'argv':command,'exit':run.returncode,'engine_report':(saves/'nox-engine-cold.tsv').read_text().strip(),'original_sha256':originals,'artifacts':str(root)}
    (root/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
