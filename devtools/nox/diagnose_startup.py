#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded serial SDL startup probes; every outcome is retained, never acceptance."""
import argparse
import configparser
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=1, choices=range(1, 11))
    args = parser.parse_args()
    exe, fixture, artifacts = args.exe.resolve(), args.fixture.resolve(), args.artifacts.resolve()
    assert not artifacts.is_relative_to(Path(__file__).resolve().parents[2]), 'Keep private fixtures outside CE Git'
    root = artifacts / datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    root.mkdir(parents=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    reports = []
    for index in range(args.samples):
        case = root / str(index)
        case.mkdir()
        shutil.copytree(fixture / 'saves', case / 'saves')
        ini = configparser.ConfigParser(interpolation=None)
        ini.read(fixture / 'gameplay.ini', encoding='utf-8')
        assert not ini['scummvm'].get('ce_link_token'), 'Offline fixtures only'
        with (case / 'probe.ini').open('w', encoding='utf-8') as stream:
            ini.write(stream)
        log = case / 'engine.log'
        command = [str(exe), '--gfx-mode=surfacesdl', f'--config={case / "probe.ini"}',
                   f'--savepath={case / "saves"}', f'--logfile={log}', 'nox-gameplay-test']
        child = subprocess.Popen(command, env=dict(os.environ, SDL_VIDEODRIVER='windows', SDL_AUDIODRIVER='dummy'),
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, startupinfo=startup)
        end, fatal = time.monotonic() + 30, ''
        while child.poll() is None and time.monotonic() < end:
            if log.exists():
                fatal = next((line for line in log.read_text(errors='replace').splitlines()
                              if 'SDL_BlitSurface failed' in line), '')
                if fatal:
                    break
            time.sleep(.05)
        stopped = child.poll() is None
        if stopped:
            child.kill()  # Only this probe's owned child, after fatal error or timeout.
        out, err = child.communicate(timeout=10)
        (case / 'stdout.txt').write_bytes(out)
        (case / 'stderr.txt').write_bytes(err)
        report = {'scope': 'Startup diagnostic only; not gameplay acceptance', 'fatal': fatal,
                  'exit': child.returncode, 'stopped_owned_child': stopped, 'argv': command,
                  'executable_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(), 'artifacts': str(case)}
        (case / 'diagnostic.json').write_text(json.dumps(report, indent=2) + '\n')
        reports.append(report)
        print(json.dumps({'sample': index, 'fatal': fatal, 'exit': child.returncode, 'artifacts': str(case)}), flush=True)
    (root / 'diagnostic.json').write_text(json.dumps(reports, indent=2) + '\n')


if __name__ == '__main__':
    main()
