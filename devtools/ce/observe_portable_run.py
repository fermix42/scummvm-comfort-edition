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
from storage_observation import system_resource_kind, classify_notifications, native_io_path


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
    # Limit collection to this command's declared disposable artifacts. Other
    # concurrent suites and historical receipts are not fresh evidence here.
    trace_root = Path(command[command.index('--artifacts')+1]).resolve() if '--artifacts' in command else root
    assert trace_root.is_relative_to(root.parent), 'Observed test artifacts must be siblings of the observer'
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
    # CE never writes the collector path. Instrumentation is installation-local.
    with trace.open('w', encoding='utf-8') as collected:
        for local in trace_root.rglob('ce-write-trace.tsv'):
            # Retain every destination, including external ones: filtering by
            # destination here would conceal the very regressions we detect.
            collected.write(local.read_text(encoding='utf-8'))
    assert not any(w.errors for w in watchers), 'Directory observation lost events'
    unexpected = []
    system_files = []
    for path in profile.rglob('*'):
        if not native_io_path(path).is_file():
            continue
        name = str(path.relative_to(profile)).replace('\\', '/').lower()
        if system_resource_kind(name):
            system_files.append(str(path.relative_to(profile)))
        else:
            unexpected.append(str(path))
    assert not unexpected, unexpected
    system_events, unknown_events = classify_notifications(watchers[0].events, profile)
    assert not unknown_events, unknown_events
    suspicious = [e for e in watchers[1].events if 'scummvm' in e['path'].lower()]
    assert not suspicious, suspicious
    writes = [l.split('\t', 1)[1] for l in trace.read_text(encoding='utf-8').splitlines() if l.startswith('WRITE\t')]
    assert writes and all(Path(p).is_relative_to(root.parent) and not Path(p).is_relative_to(profile) for p in writes), writes
    receipt = {'status': 'PASS', 'command': command, 'exit': result.returncode,
               'successful_native_stream_write_records': len(writes),
               'trace_collection_root': str(trace_root),
               'trace_records_may_include_copied_installation_history': True,
               'all_traced_writes_under': str(root.parent),
               'controlled_profile_application_files': unexpected,
               'controlled_profile_system_resource_files': system_files,
               'observed_system_resource_events': system_events,
               'real_profile_scummvm_notifications': suspicious,
               'controlled_profile_notifications': len(watchers[0].events),
               'real_profile_notifications': len(watchers[1].events),
               'observer_errors': [],
               'limitation': 'Directory notifications are not process-attributed kernel I/O tracing; raw notifications are retained. GPU-driver files are reported separately.'}
    (root/'verification.json').write_text(json.dumps(receipt, indent=2))
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
