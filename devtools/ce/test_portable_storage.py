#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real Windows portable-folder verification. Only disposable artifact copies are written.

Normal launches use no --config/--savepath/--test-cli. Nox's existing opt-in
gameplay fixture drives normal Engine/event/save code, then exits automatically.
Native CLI tests retain their original, separately enforced owned test roots.
"""
import argparse
import configparser
import csv
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
from storage_observation import system_resource_kind, classify_notifications, native_io_path


def digest(p):
    return hashlib.sha256(native_io_path(p).read_bytes()).hexdigest()


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


def copy_installation(source, destination):
    source, destination = str(Path(source).resolve()), str(Path(destination).resolve())
    if os.name == 'nt':
        prefix = chr(92) * 2 + '?' + chr(92)
        source, destination = prefix + source, prefix + destination
    return shutil.copytree(source, destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--extra', required=True, type=Path)
    parser.add_argument('--nox', required=True, type=Path)
    parser.add_argument('--artifacts', required=True, type=Path)
    parser.add_argument('--installation', type=Path, help='Copy the complete prepared portable package, including resources/docs/licenses')
    parser.add_argument('--relocate-to', type=Path, help='Explicit fresh destination for an additional cross-drive relocation run')
    args = parser.parse_args()
    root = args.artifacts.resolve()
    root.mkdir(parents=True, exist_ok=False)
    exe, extra, nox = args.exe.resolve(), args.extra.resolve(), args.nox.resolve()
    a, b, cwd = root / 'A portable 日本語', root / 'B relocated Ω', root / 'unrelated cwd'
    if args.relocate_to:
        b = args.relocate_to.resolve()
        assert not b.exists() and b != root and not root.is_relative_to(b)
        b.parent.mkdir(parents=True, exist_ok=True)
    cwd.mkdir()
    if args.installation:
        installation = args.installation.resolve(strict=True)
        assert (installation/exe.name).read_bytes() == exe.read_bytes()
        copy_installation(installation, a)
    else:
        a.mkdir()
    for p in ([] if args.installation else exe.parent.iterdir()):
        if p.suffix.lower() in ('.exe', '.dll') and p.name != 'scummvm-tests.exe':
            shutil.copy2(p, a / p.name)
    # Engine data remains inside the installation; game assets stay externally referenced.
    if not args.installation:
        (a / 'data').mkdir()
    for p in ([] if args.installation else extra.iterdir()):
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


    def check(name, condition):
        assert condition, name
        checks.append(name)

    def run(folder, argv, label, expected=0):
        command = [str(folder/exe.name), *argv]
        local_trace = folder/'ce-write-trace.tsv'
        trace_start = len(local_trace.read_text(encoding='utf-8').splitlines()) if local_trace.exists() else 0
        result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, timeout=100,
                                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        (root/(label+'-stdout.txt')).write_bytes(result.stdout)
        (root/(label+'-stderr.txt')).write_bytes(result.stderr)
        phase_trace = local_trace.read_text(encoding='utf-8').splitlines()[trace_start:] if local_trace.exists() else []
        with trace.open('a', encoding='utf-8') as collected:
            collected.write('\n'.join(phase_trace)+'\n')
        phase_writes = [line.split('\t', 1)[1] for line in phase_trace if line.startswith('WRITE\t')]
        commands.append({'argv': command, 'cwd': str(cwd), 'exit': result.returncode,
                         'successful_native_stream_writes': phase_writes})
        check(label+' all writes stay beneath this executable',
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
        sidecars_before = {p.name: json.loads(native_io_path(p).read_text()) for p in saves.glob('*.ce-achievements.json')}
        check('achievement sidecars generated by real saves', bool(sidecars_before))

        check('normal portable log exists', (a/'logs/scummvm.log').is_file())
        check('temporary directory portable', (a/'temp').is_dir())
        ini = load_ini(a/'scummvm.ini')
        ini[target]['nox_snapshot_validation'] = 'true'
        ini[target]['nox_metadata_validation'] = 'true'
        ini[target]['nox_host_validation'] = 'true'
        save_ini(a/'scummvm.ini', ini)
        before = {str(p.relative_to(a)): digest(p) for p in saves.iterdir() if native_io_path(p).is_file()}
        copy_installation(a, b)
        # Remove A from its original address, preserving it in the disposable artifacts.
        parked = root/'A parked after relocation'
        assert a.resolve().is_relative_to(root) and parked.resolve().is_relative_to(root)
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
            native_io_path(b/'saves'/name).is_file() and all(json.loads(native_io_path(b/'saves'/name).read_text()).get(k) == value.get(k)
                for k in ('client_playthrough_id', 'popup_seen', 'progress')) for name, value in sidecars_before.items()))

        check('B new log location', (b/'logs/scummvm.log').is_file())
        check('unrelated working directory untouched', not list(cwd.iterdir()))
        check('no CE-owned controlled profile files', not [p for p in profile.rglob('*') if native_io_path(p).is_file() and not system_resource_kind(p.relative_to(profile))])
        check('configuration has no A storage dependency', str(a) not in (b/'scummvm.ini').read_text())
        check('A saves preserved', all(digest(parked/name) == sha for name, sha in before.items()))
        # Explicit paths: independent copy, separate save owner and normal gameplay.
        custom = b/'explicit paths'; custom.mkdir()
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
        # Strict architecture: external CLI writes are errors, never a mode.
        outside = root/'external storage'; outside.mkdir()
        sentinel = outside/'official.ini'
        sentinel.write_text('[scummvm]\nkeep=official\n', encoding='utf-8')
        original = digest(sentinel)
        for option, destination in (
                ('--config', sentinel), ('-c', sentinel),
                ('--savepath', outside/'saves'), ('--logfile', outside/'log.txt'),
                ('--screenshotpath', outside/'screenshots'),
                ('--iconspath', outside/'icons'), ('--dlcspath', outside/'dlcs'),
                ('--savepath', b/'../escaped saves'),
                ('--savepath', root/'B relocated Ω sibling/saves')):
            result = subprocess.run([str(b/exe.name), option+'='+str(destination), '--version'],
                                    cwd=cwd, env=env, capture_output=True, timeout=20)
            check('external CLI destination rejected '+option+' '+str(destination),
                  result.returncode != 0 and b'No Windows profile fallback' in result.stderr)
        check('external official config unchanged', digest(sentinel) == original)
        check('no external managed files', list(outside.iterdir()) == [sentinel])
        # Legacy INI paths normalize without reading or changing external data.
        legacy = load_ini(b/'scummvm.ini')
        for key in ('savepath', 'screenshotpath', 'logfile', 'iconspath', 'dlcspath', 'cachepath', 'temppath'):
            legacy['scummvm'][key] = str(outside/('legacy-'+key))
        legacy[target]['savepath'] = str(outside/'legacy-game-saves')
        legacy['scummvm']['extrapath'] = str(extra)
        save_ini(b/'scummvm.ini', legacy)
        run(b, ['--add', '--path='+str(nox)], 'normalize-legacy-config')
        normalized = load_ini(b/'scummvm.ini')
        for key in ('savepath', 'screenshotpath', 'logfile', 'iconspath', 'dlcspath', 'cachepath', 'temppath'):
            check('normalized legacy '+key, not Path(normalized['scummvm'][key]).is_absolute()
                  and '..' not in Path(normalized['scummvm'][key]).parts)
        check('normalized per-game savepath', normalized[target]['savepath'] == 'saves')
        # An absolute selection of the installation root must serialize as '.'
        # too, rather than retaining the old installation address after a move.
        normalized['scummvm']['cachepath'] = str(b)
        save_ini(b/'scummvm.ini', normalized)
        run(b, ['--add', '--path='+str(nox)], 'normalize-installation-root')
        check('absolute executable-root storage serializes relatively', load_ini(b/'scummvm.ini')['scummvm']['cachepath'] == '.')
        check('external engine assets preserved', normalized['scummvm']['extrapath'] == str(extra))
        check('legacy external data never migrated or modified', list(outside.iterdir()) == [sentinel] and digest(sentinel) == original)
        # Separate installations and an executable-only upgrade retain save bytes.
        prior = {p.name: digest(p) for p in (b/'saves').iterdir() if p.is_file()}
        independent = root/'independent installation'; independent.mkdir()
        for p in b.iterdir():
            if p.suffix.lower() in ('.exe', '.dll'): shutil.copy2(p, independent/p.name)
        run(independent, ['--add', '--path='+str(nox)], 'independent-installation')
        check('separate installation config independent', 'ce_playthrough_tracking' not in (independent/'scummvm.ini').read_text())
        check('independent installation did not touch original saves', all(digest(b/'saves'/name) == sha for name, sha in prior.items()))
        shutil.copy2(exe, b/exe.name)
        run(b, ['--version'], 'upgrade-preservation')
        check('upgrade preserves config and saves', (b/'scummvm.ini').is_file() and all(digest(b/'saves'/name) == sha for name, sha in prior.items()))
        # A junction beneath the executable must not redirect managed output.
        link = b/'escape-junction'
        junction = subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(outside)], capture_output=True)
        check('test junction created', junction.returncode == 0)
        result = subprocess.run([str(b/exe.name), '--logfile='+str(link/'escaped.log'), '--version'],
                                cwd=cwd, env=env, capture_output=True, timeout=20)
        check('junction storage escape rejected', result.returncode != 0 and not (outside/'escaped.log').exists())
        link.rmdir()  # Remove only the junction entry, preserving its target.
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
        readonly = root/'read-only installation'; readonly.mkdir()
        for p in b.iterdir():
            if p.suffix.lower() in ('.exe', '.dll'): shutil.copy2(p, readonly/p.name)
        identity = subprocess.check_output(['whoami', '/user', '/fo', 'csv', '/nh'], text=True)
        sid = next(csv.reader(identity.splitlines()))[1]
        # Deny write bits only. Generic W also denies SYNCHRONIZE, which can
        # prevent CreateProcess from opening an otherwise executable image.
        deny = subprocess.run(['icacls', str(readonly), '/deny', '*'+sid+':(OI)(CI)(WD,AD,WEA,WA)'], capture_output=True)
        check('read-only test ACL installed', deny.returncode == 0)
        try:
            failure = subprocess.run([str(readonly/exe.name), '--version'], cwd=cwd, env=env,
                                     capture_output=True, timeout=20)
            check('read-only installation errors without profile fallback', failure.returncode != 0 and b'No Windows profile fallback' in failure.stderr)
        finally:
            restored = subprocess.run(['icacls', str(readonly), '/remove:d', '*'+sid, '/T'], capture_output=True)
            check('disposable test ACL restored', restored.returncode == 0)
    finally:
        for w in watchers: w.close()
        observations = [{'root': str(w.path), 'events': w.events, 'errors': w.errors} for w in watchers]
        (root/'directory-observations.json').write_text(json.dumps(observations, indent=2))
        (root/'commands.json').write_text(json.dumps(commands, indent=2))
    check('native directory observation had no overflows', not any(w.errors for w in watchers))
    system_events, unexpected = classify_notifications(watchers[0].events, profile)
    driver_events = [e for e in system_events if e['resource_kind'] in ('nvidia_driver_cache', 'windows_direct3d_shader_cache')]
    check('no application-owned controlled profile notifications', not unexpected)
    suspicious = [e for w in watchers[1:] for e in w.events if 'scummvm' in e['path'].lower()]
    check('no ScummVM notifications under real Windows profile', not suspicious)
    writes = [line.split('\t', 1)[1] for line in trace.read_text(encoding='utf-8').splitlines() if line.startswith('WRITE\t')]
    check('native stream writes recorded', bool(writes))
    check('all traced writes under disposable installation/test locations',
          all((Path(p).is_relative_to(root) or Path(p).is_relative_to(b)) and not Path(p).is_relative_to(profile) for p in writes))
    evidence = {'status': 'PASS', 'checks': checks, 'check_count': len(checks),
                'executable_sha256': digest(exe), 'normal_launches_use_test_cli': False,
                'complete_installation_copied': str(args.installation.resolve()) if args.installation else None,
                'relocated_executable_root': str(b),
                'across_drives': a.drive.lower() != b.drive.lower(),
                'observation': 'ReadDirectoryChangesW real/controlled profiles plus native stream trace',
                'observed_system_gpu_driver_events': driver_events,
                'observed_system_resource_events': system_events,
                'real_profile_event_count': sum(len(w.events) for w in watchers[1:]),
                'limitations': ['Notification observer is directory-based, not process-attributed kernel I/O tracing.',
                                'Gameplay here is Nox; MI4 is exercised separately through the owned native CLI roots.',
                                'Does not prove every optional third-party engine or system component never writes elsewhere.']}
    (root/'verification.json').write_text(json.dumps(evidence, indent=2))
    print(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    main()
