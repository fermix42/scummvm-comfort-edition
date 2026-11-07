#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Observe a bounded regression command and its children without desktop input."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
from test_portable_storage import DirectoryWatch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    root = args.artifacts.resolve()
    root.mkdir(parents=True, exist_ok=False)
    profile = root/'controlled profile'
    for name in ('AppData/Roaming', 'AppData/Local', 'Documents', 'Pictures', 'Temp'):
        (profile/name).mkdir(parents=True, exist_ok=True)
    trace = root/'native-writes.tsv'
    env = dict(os.environ, APPDATA=str(profile/'AppData/Roaming'),
               LOCALAPPDATA=str(profile/'AppData/Local'), USERPROFILE=str(profile),
               HOME=str(profile), TEMP=str(profile/'Temp'), TMP=str(profile/'Temp'),
               SCUMMVM_CE_WRITE_TRACE=str(trace), PYTHONDONTWRITEBYTECODE='1')
    watchers = [DirectoryWatch(profile), DirectoryWatch(Path(os.environ['USERPROFILE']))]
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    try:
        result = subprocess.run(command, env=env, capture_output=True, timeout=480)
        (root/'stdout.txt').write_bytes(result.stdout)
        (root/'stderr.txt').write_bytes(result.stderr)
    finally:
        for watcher in watchers:
            watcher.close()
        observations = [{'root': str(w.path), 'errors': w.errors, 'events': w.events} for w in watchers]
        (root/'directory-observations.json').write_text(json.dumps(observations, indent=2))
    assert result.returncode == 0, (result.returncode, root/'stdout.txt', root/'stderr.txt')
    assert not any(w.errors for w in watchers), 'Directory observation lost events'
    unexpected = []
    system_files = []
    for path in profile.rglob('*'):
        if not path.is_file():
            continue
        name = str(path.relative_to(profile)).replace('\\', '/').lower()
        if (name.startswith(('appdata/local/nvidia/', 'appdata/locallow/nvidia/')) or
                re.fullmatch(r'appdata/local/[0-9a-f]{16}\.nvph', name)):
            system_files.append(str(path.relative_to(profile)))
        else:
            unexpected.append(str(path))
    assert not unexpected, unexpected
    suspicious = [e for e in watchers[1].events if 'scummvm' in e['path'].lower()]
    assert not suspicious, suspicious
    writes = [l.split('\t', 1)[1] for l in trace.read_text(encoding='utf-8').splitlines() if l.startswith('WRITE\t')]
    assert writes and all(Path(p).is_relative_to(root.parent) for p in writes), writes
    receipt = {'status': 'PASS', 'command': command, 'exit': result.returncode,
               'successful_native_stream_write_records': len(writes),
               'all_traced_writes_under': str(root.parent),
               'controlled_profile_application_files': unexpected,
               'controlled_profile_system_gpu_files': system_files,
               'real_profile_scummvm_notifications': suspicious,
               'controlled_profile_notifications': len(watchers[0].events),
               'real_profile_notifications': len(watchers[1].events),
               'observer_errors': [],
               'limitation': 'Directory notifications are not process-attributed kernel I/O tracing; raw notifications are retained. GPU-driver files are reported separately.'}
    (root/'verification.json').write_text(json.dumps(receipt, indent=2))
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
