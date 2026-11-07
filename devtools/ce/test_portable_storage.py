#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real Windows portable-folder verification. Only disposable artifact copies are written.

Normal launches use no --config/--savepath/--test-cli. Nox's existing opt-in
gameplay fixture drives normal Engine/event/save code, then exits automatically.
Native CLI tests retain their original, separately enforced owned test roots.
"""
import argparse
import configparser
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import threading


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


class DirectoryWatch:
    """Native recursive notifications, including writes not made through ScummVM streams."""
    def __init__(self, path):
        self.path, self.events, self.errors = Path(path), [], []
        self.api = ctypes.WinDLL('kernel32', use_last_error=True)
        self.api.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                       wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        self.api.CreateFileW.restype = wintypes.HANDLE
        self.api.ReadDirectoryChangesW.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                                                 wintypes.BOOL, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
                                                 wintypes.LPVOID, wintypes.LPVOID]
        self.api.CancelIoEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID]
        self.api.CloseHandle.argtypes = [wintypes.HANDLE]
        self.api.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.api.OpenThread.restype = wintypes.HANDLE
        self.api.CancelSynchronousIo.argtypes = [wintypes.HANDLE]
        self.handle = self.api.CreateFileW(str(self.path), 1, 7, None, 3, 0x02000000, None)
        assert self.handle != wintypes.HANDLE(-1).value, (path, ctypes.get_last_error())
        self.stopping = False
        self.ready = threading.Event()
        self.thread = threading.Thread(target=self.read, daemon=True)
        self.thread.start()
        self.ready.wait(5)

    def read(self):
        buf = ctypes.create_string_buffer(65536)
        while not self.stopping:
            count = wintypes.DWORD()
            self.ready.set()
            result = self.api.ReadDirectoryChangesW(self.handle, buf, len(buf), True, 0x13,
                                                   ctypes.byref(count), None, None)
            if not result:
                if not self.stopping:
                    self.errors.append(ctypes.get_last_error())
                break
            if not count.value:
                self.errors.append('notification buffer overflow')
                continue
            raw, offset = buf.raw[:count.value], 0
            while True:
                next_offset, action, length = struct.unpack_from('<III', raw, offset)
                name = raw[offset + 12:offset + 12 + length].decode('utf-16-le')
                self.events.append({'action': action, 'path': str(self.path / name)})
                if not next_offset:
                    break
                offset += next_offset

    def close(self):
        self.stopping = True
        self.api.CancelIoEx(self.handle, None)
        thread_handle = self.api.OpenThread(1, False, self.thread.native_id)
        if thread_handle:
            self.api.CancelSynchronousIo(thread_handle)
            self.api.CloseHandle(thread_handle)
        self.thread.join(5)
        self.api.CloseHandle(self.handle)
        assert not self.thread.is_alive(), 'Directory observer did not stop'


def load_ini(p):
    ini = configparser.ConfigParser(interpolation=None, strict=False)
    ini.read(p, encoding='utf-8')
    return ini


def save_ini(p, ini):
    with p.open('w', encoding='utf-8') as f:
        ini.write(f)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--extra', required=True, type=Path)
    parser.add_argument('--nox', required=True, type=Path)
    parser.add_argument('--artifacts', required=True, type=Path)
    args = parser.parse_args()
    root = args.artifacts.resolve()
    root.mkdir(parents=True, exist_ok=False)
    exe, extra, nox = args.exe.resolve(), args.extra.resolve(), args.nox.resolve()
    a, b, cwd = root / 'A portable 日本語', root / 'B relocated Ω', root / 'unrelated cwd'
    a.mkdir(); cwd.mkdir()
    for p in exe.parent.iterdir():
        if p.suffix.lower() in ('.exe', '.dll') and p.name != 'scummvm-tests.exe':
            shutil.copy2(p, a / p.name)
    # Engine data remains inside the installation; game assets stay externally referenced.
    (a / 'data').mkdir()
    for p in extra.iterdir():
        if p.is_file() and p.suffix.lower() in ('.dat', '.m4b', '.zip'):
            shutil.copy2(p, a / 'data' / p.name)
    profile = root / 'controlled profile'
    profile.mkdir()
    for name in ('AppData/Roaming', 'AppData/Local', 'Documents', 'Pictures', 'Temp'):
        (profile / name).mkdir(parents=True, exist_ok=True)
    trace = root / 'native-writes.tsv'
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
               APPDATA=str(profile/'AppData/Roaming'), LOCALAPPDATA=str(profile/'AppData/Local'),
               USERPROFILE=str(profile), HOME=str(profile), TEMP=str(profile/'Temp'), TMP=str(profile/'Temp'),
               SCUMMVM_CE_WRITE_TRACE=str(trace))
    commands, checks = [], []

    def driver_file(path):
        relative = str(Path(path).relative_to(profile)).replace('\\', '/').lower()
        return (relative in ('appdata/local/nvidia', 'appdata/locallow/nvidia') or
                relative.startswith(('appdata/local/nvidia/', 'appdata/locallow/nvidia/')) or
                bool(re.fullmatch(r'appdata/local/[0-9a-f]{16}\.nvph', relative)))


    def check(name, condition):
        assert condition, name
        checks.append(name)

    def run(folder, argv, label, expected=0):
        command = [str(folder/exe.name), *argv]
        trace_start = len(trace.read_text(encoding='utf-8').splitlines()) if trace.exists() else 0
        result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, timeout=100,
                                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        (root/(label+'-stdout.txt')).write_bytes(result.stdout)
        (root/(label+'-stderr.txt')).write_bytes(result.stderr)
        phase_trace = trace.read_text(encoding='utf-8').splitlines()[trace_start:] if trace.exists() else []
        phase_writes = [line.split('\t', 1)[1] for line in phase_trace if line.startswith('WRITE\t')]
        commands.append({'argv': command, 'cwd': str(cwd), 'exit': result.returncode,
                         'successful_native_stream_writes': phase_writes})
        if not any(arg.startswith(('--config=', '--savepath=', '--logfile=')) for arg in argv):
            check(label+' default writes stay beneath this executable',
                  all(Path(path).is_relative_to(folder) for path in phase_writes))
        check(label+' exit', result.returncode == expected)
        return result

    watchers = [DirectoryWatch(profile)]
    real_profile = Path(os.environ['USERPROFILE'])
    if real_profile.exists():
        watchers.append(DirectoryWatch(real_profile))
    try:
        # Two normal processes race on lazy startup-directory creation.
        children = [subprocess.Popen([str(a/exe.name), '--version'], cwd=cwd, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0)) for _ in range(2)]
        for child in children:
            child.communicate(timeout=30)
            check('concurrent normal portable startup '+str(child.pid), child.returncode == 0)
        run(a, ['--add', '--path='+str(nox)], 'first-add')
        ini = load_ini(a/'scummvm.ini')
        targets = [s for s in ini.sections() if ini[s].get('engineid') == 'nox']
        check('normal first launch created adjacent config and game target', bool(targets))
        target = targets[0]
        check('external game-data path preserved', Path(ini[target]['path']) == nox)
        check('no serialized absolute default savepath', not ini['scummvm'].get('savepath'))
        ini[target]['nox_gameplay_validation'] = 'true'
        ini[target]['ce_playthrough_tracking'] = 'true'
        ini[target]['check_gamedata'] = 'false'
        ini['scummvm']['confirm_exit'] = 'false'
        ini['scummvm']['updates_check'] = '0'
        save_ini(a/'scummvm.ini', ini)
        ordinary = ['--gfx-mode=surfacesdl', '--music-driver=null', '--no-console', target]
        run(a, ordinary, 'normal-gameplay-A')
        saves = a/'saves'
        check('Nox real gameplay report', (saves/'nox-gameplay.tsv').read_text().startswith('PASS\t'))
        check('Nox engine save report', (saves/'nox-engine-saves.tsv').read_text().startswith('PASS\t'))
        check('Nox paired generations exist', len(list(saves.glob('nox-ce-*.g[01]'))) >= 4)
        check('CE registry exists', (saves/'ce-playthroughs.json').is_file())
        check('CE queue exists', (saves/'ce-achievements-queue.json').is_file())
        registry_before = {row['client_playthrough_id'] for row in json.loads((saves/'ce-playthroughs.json').read_text())['playthroughs']}
        sidecars_before = {p.name: json.loads(p.read_text()) for p in saves.glob('*.ce-achievements.json')}
        check('achievement sidecars generated by real saves', bool(sidecars_before))

        check('normal portable log exists', (a/'logs/scummvm.log').is_file())
        check('temporary directory portable', (a/'temp').is_dir())
        ini = load_ini(a/'scummvm.ini')
        ini[target]['nox_snapshot_validation'] = 'true'
        ini[target]['nox_metadata_validation'] = 'true'
        ini[target]['nox_host_validation'] = 'true'
        save_ini(a/'scummvm.ini', ini)
        before = {str(p.relative_to(a)): digest(p) for p in saves.iterdir() if p.is_file()}
        shutil.copytree(a, b)
        # Remove A from its original address, preserving it in the disposable artifacts.
        parked = root/'A parked after relocation'
        a.rename(parked)
        run(b, ordinary, 'normal-cold-B')
        check('old installation address absent', not a.exists())
        check('Nox relocated cold restore', 'cold_load=1' in (b/'saves/nox-cold.tsv').read_text())
        check('Nox relocated engine sessions', (b/'saves/nox-engine-cold.tsv').read_text().startswith('PASS\t'))
        check('Nox restored frame identical', (b/'saves/nox-cold-restored.png').read_bytes() == (b/'saves/nox-gameplay.png').read_bytes())
        check('B still owns CE metadata', (b/'saves/ce-playthroughs.json').is_file())
        registry_after = {row['client_playthrough_id'] for row in json.loads((b/'saves/ce-playthroughs.json').read_text())['playthroughs']}
        check('same Play Session identities after relocation', registry_before <= registry_after)
        check('relocated achievement sidecar owners and progress', all(
            (b/'saves'/name).is_file() and all(json.loads((b/'saves'/name).read_text()).get(k) == value.get(k)
                for k in ('client_playthrough_id', 'popup_seen', 'progress')) for name, value in sidecars_before.items()))

        check('B new log location', (b/'logs/scummvm.log').is_file())
        check('unrelated working directory untouched', not list(cwd.iterdir()))
        check('no CE-owned controlled profile files', not [p for p in profile.rglob('*') if p.is_file() and not driver_file(p)])
        check('configuration has no A storage dependency', str(a) not in (b/'scummvm.ini').read_text())
        check('A saves preserved', all(digest(parked/name) == sha for name, sha in before.items()))
        # Explicit paths: independent copy, separate save owner and normal gameplay.
        custom = root/'explicit paths'; custom.mkdir()
        custom_saves = custom/'chosen saves'; custom_saves.mkdir()
        custom_ini = custom/'chosen.ini'
        selected = load_ini(b/'scummvm.ini')
        selected[target].pop('nox_snapshot_validation', None)
        selected[target].pop('nox_host_validation', None)
        selected[target].pop('nox_metadata_validation', None)
        selected[target]['savepath'] = str(custom_saves)
        selected['scummvm']['savepath'] = str(custom/'unused global saves')
        selected['scummvm']['temppath'] = str(custom/'chosen temp')
        save_ini(custom_ini, selected)
        run(b, ['--config='+str(custom_ini), '--logfile='+str(custom/'chosen.log'), *ordinary], 'explicit-paths')
        check('game-specific override respected', (custom_saves/'nox-engine-saves.tsv').is_file())
        check('explicit log respected', (custom/'chosen.log').is_file())
        check('explicit temppath respected', (custom/'chosen temp').is_dir())
        check('global savepath not mistaken for active per-game override', not (custom/'unused global saves').exists())
        cli_saves = custom/'CLI chosen saves'; cli_saves.mkdir()
        run(b, ['--config='+str(custom_ini), '--savepath='+str(cli_saves),
                '--logfile='+str(custom/'CLI chosen.log'), *ordinary], 'CLI-savepath')
        check('CLI savepath overrides per-game and global paths', (cli_saves/'nox-engine-saves.tsv').is_file())
        relative = load_ini(custom_ini)
        relative[target]['savepath'] = 'relative save choice'
        save_ini(custom_ini, relative)
        run(b, ['--config='+str(custom_ini), '--logfile='+str(custom/'relative.log'), *ordinary], 'relative-savepath')
        check('relative configured savepath is executable-relative', (b/'relative save choice/nox-engine-saves.tsv').is_file())
        relative_cli_saves = custom/'relative CLI saves'; relative_cli_saves.mkdir()
        run(b, ['--config='+os.path.relpath(custom_ini, cwd),
                '--savepath='+os.path.relpath(relative_cli_saves, cwd),
                '--logfile='+str(custom/'relative-CLI.log'), *ordinary], 'relative-CLI-paths')
        check('explicit relative CLI config and savepath preserve caller semantics', (relative_cli_saves/'nox-engine-saves.tsv').is_file())
        # Default path creation must fail explicitly when a component is a file.
        blocked = root/'blocked'; blocked.mkdir()
        for p in b.iterdir():
            if p.suffix.lower() in ('.exe', '.dll'): shutil.copy2(p, blocked/p.name)
        (blocked/'temp').write_text('preserve this file')
        failure = subprocess.run([str(blocked/exe.name), '--version'], cwd=cwd, env=env,
                                 capture_output=True, timeout=15, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        (root/'blocked-stderr.txt').write_bytes(failure.stderr)
        check('invalid portable directory rejected', failure.returncode != 0)
        check('actionable portable storage error', b'No Windows profile fallback' in failure.stderr and b'D:\\Games\\ScummVM CE' in failure.stderr)
        check('blocked component preserved', (blocked/'temp').read_text() == 'preserve this file')
    finally:
        for w in watchers: w.close()
        observations = [{'root': str(w.path), 'events': w.events, 'errors': w.errors} for w in watchers]
        (root/'directory-observations.json').write_text(json.dumps(observations, indent=2))
        (root/'commands.json').write_text(json.dumps(commands, indent=2))
    check('native directory observation had no overflows', not any(w.errors for w in watchers))
    driver_events = [e for e in watchers[0].events if driver_file(e['path'])]
    driver_parents = {str(Path(e['path']).parent).lower() for e in driver_events}
    driver_parents |= {str(profile/'AppData').lower(), str(profile/'AppData/Local').lower(),
                       str(profile/'AppData/locallow').lower(), str(profile/'AppData/locallow/NVIDIA').lower(),
                       str(profile/'AppData/Local/NVIDIA').lower()}
    unexpected = [e for e in watchers[0].events if not driver_file(e['path']) and
                  not (e['action'] in (1, 3) and e['path'].lower() in driver_parents)]
    check('no application-owned controlled profile notifications', not unexpected)
    suspicious = [e for w in watchers[1:] for e in w.events if 'scummvm' in e['path'].lower()]
    check('no ScummVM notifications under real Windows profile', not suspicious)
    writes = [line.split('\t', 1)[1] for line in trace.read_text(encoding='utf-8').splitlines() if line.startswith('WRITE\t')]
    check('native stream writes recorded', bool(writes))
    check('all traced writes under disposable installation/test locations',
          all(Path(p).is_relative_to(root) for p in writes))
    evidence = {'status': 'PASS', 'checks': checks, 'check_count': len(checks),
                'executable_sha256': digest(exe), 'normal_launches_use_test_cli': False,
                'observation': 'ReadDirectoryChangesW real/controlled profiles plus native stream trace',
                'observed_system_gpu_driver_events': driver_events,
                'real_profile_event_count': sum(len(w.events) for w in watchers[1:]),
                'limitations': ['Notification observer is directory-based, not process-attributed kernel I/O tracing.',
                                'Gameplay here is Nox; MI4 is exercised separately through the owned native CLI roots.',
                                'Does not prove every optional third-party engine or system component never writes elsewhere.']}
    (root/'verification.json').write_text(json.dumps(evidence, indent=2))
    print(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    main()
