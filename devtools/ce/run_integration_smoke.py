#!/usr/bin/env python3
"""Read-only CLI smoke checks with isolated config/save paths and recorded output."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--artifacts', type=Path, required=True)
args = parser.parse_args()
exe = args.exe.resolve(strict=True)
root = args.artifacts.resolve()
root.mkdir(parents=True, exist_ok=False)
(root / 'saves').mkdir()
(root / 'isolated.ini').write_text('[scummvm]\nupdates_check=0\n', encoding='utf-8')
startup = None
if os.name == 'nt':
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
outputs = {}
commands = []
for name, option in [('version', '--version'), ('engines', '--list-engines'),
                     ('games', '--list-all-games')]:
    command = [str(exe), f'--config={root / "isolated.ini"}',
               f'--savepath={root / "saves"}', option]
    result = subprocess.run(command, capture_output=True, timeout=60, startupinfo=startup)
    (root / f'{name}.stdout.txt').write_bytes(result.stdout)
    (root / f'{name}.stderr.txt').write_bytes(result.stderr)
    commands.append({'argv': command, 'exit': result.returncode})
    assert result.returncode == 0, (name, result.returncode, result.stderr)
    outputs[name] = result.stdout.decode('utf-8', errors='replace')
assert '2026.4.0' in outputs['version'], outputs['version']
expected = ['nox', 'grim', 'cine', 'sci', 'macventure', 'phoenixvr', 'hpl1',
            'dm', 'avalanche', 'chamber', 'fool', 'eem', 'harvester', 'wintermute']
ids = {line.split()[0] for line in outputs['engines'].splitlines() if line.split()}
assert set(expected) <= ids, sorted(set(expected) - ids)
report = {'status': 'PASS', 'checks': 3, 'executable_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
          'engine_ids_verified': expected, 'commands': commands,
          'scope': 'CLI version, compiled engine registration and detection catalog; no gameplay claim'}
(root / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
