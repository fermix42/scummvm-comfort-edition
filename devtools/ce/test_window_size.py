#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Windows launcher sizing regression using normal visible SDL windows.

Run only against an idle portable installation. Backs up/restores its INI and
log; no games are launched. Measures client rectangles without injecting input.
"""
import argparse
import ctypes
from ctypes import wintypes as wt
import hashlib
import json
from pathlib import Path
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot(root):
    return {str(p.relative_to(root)): digest(p)
            for p in (root / 'saves').rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--mode', choices=('opengl', 'surfacesdl'))
    args = parser.parse_args()
    exe = args.exe.resolve()
    root = exe.parent
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    user32 = ctypes.WinDLL('user32', use_last_error=True)
    user32.SetProcessDPIAware()
    callback_type = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    user32.EnumWindows.argtypes = [callback_type, wt.LPARAM]
    user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
    user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
    user32.IsWindowVisible.argtypes = [wt.HWND]

    def windows(pid):
        found = []

        @callback_type
        def visit(hwnd, unused):
            owner = wt.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
            if owner.value == pid and user32.IsWindowVisible(hwnd):
                rect = wt.RECT()
                if user32.GetClientRect(hwnd, ctypes.byref(rect)) and rect.right > 100 and rect.bottom > 100:
                    found.append([rect.right, rect.bottom])
            return True

        user32.EnumWindows(visit, 0)
        return found

    backups = {}
    for relative in ('scummvm.ini', 'logs/scummvm.log'):
        path = root / relative
        backups[relative] = path.read_bytes() if path.exists() else None
        if backups[relative] is not None:
            (evidence / ('original-' + path.name)).write_bytes(backups[relative])
    saves_before = snapshot(root)
    results = []
    try:
        for mode, stretches in [('opengl', ['center', 'pixel-perfect', 'even-pixels', 'fit', 'stretch', 'fit_force_aspect']),
                                ('surfacesdl', ['center', 'pixel-perfect', 'fit', 'stretch', 'fit_force_aspect'])]:
            if args.mode and args.mode != mode:
                continue
            cases = [(0, 'pixel-perfect', None)]
            cases += [(0, stretch, size) for stretch in stretches
                      for size in [(1000, 700), (1200, 800)]]
            cases += [(0, 'pixel-perfect', (500, 350)), (3, 'pixel-perfect', (1000, 700)),
                      (3, 'pixel-perfect', (1200, 800))]
            for layout, stretch, size in cases:
                label = f'{mode}-{stretch}-layout{layout}-' + ('saved' if size is None else f'{size[0]}x{size[1]}')
                (root / 'scummvm.ini').write_text(
                    '[scummvm]\nlast_window_width=800\nlast_window_height=600\n'
                    f'window_layout={layout}\nwindow_layout_borderless=true\nwindow_layout_display=0\n'
                    'window_maximized=false\nfullscreen=false\nconfirm_exit=false\n'
                    'opengl_discrete_window_resolutions=false\nupdates_check=0\ntts_enabled=false\n',
                    encoding='utf-8')
                command = [str(exe), '--no-fullscreen', f'--gfx-mode={mode}', f'--stretch-mode={stretch}']
                if size:
                    command.append(f'--window-size={size[0]},{size[1]}')
                process = None
                with (evidence / (label + '.stdout')).open('wb') as out, (evidence / (label + '.stderr')).open('wb') as err:
                    try:
                        process = subprocess.Popen(command, cwd=root, stdout=out, stderr=err)
                        deadline = time.monotonic() + 15
                        while time.monotonic() < deadline and process.poll() is None:
                            if windows(process.pid):
                                break
                            time.sleep(.2)
                        time.sleep(1.5)
                        first = windows(process.pid)
                        time.sleep(.5)
                        second = windows(process.pid)
                        expected = list(size) if size else ([800, 600] if mode == 'opengl' else [640, 480])
                        passed = (first == second and process.poll() is None and
                                  (second == [expected] if layout == 0 else len(second) == 1))
                        row = dict(mode=mode, stretch=stretch, layout=layout, requested=size,
                                   expected=expected if layout == 0 else 'CE layout',
                                   client_sizes=second, stable=first == second, passed=passed)
                        results.append(row)
                        print(json.dumps(row), flush=True)
                    finally:
                        # Launcher-only process management; no injected input/window messages.
                        if process and process.poll() is None:
                            process.terminate()
                            process.wait(timeout=10)
                    log = root / 'logs/scummvm.log'
                    if log.exists():
                        (evidence / (label + '.log')).write_bytes(log.read_bytes())
                (evidence / 'results.json').write_text(json.dumps(results, indent=2))
        # The same explicit CE layout must win over both different CLI requests.
        layout_sizes = [r['client_sizes'] for r in results if r['layout'] == 3]
        assert layout_sizes and all(s == layout_sizes[0] for s in layout_sizes)
    finally:
        for relative, contents in backups.items():
            path = root / relative
            if contents is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(contents)
        restored = all((root / p).read_bytes() == data if data is not None else not (root / p).exists()
                       for p, data in backups.items())
        saves_unchanged = snapshot(root) == saves_before
        receipt = dict(exe=str(exe), exe_sha256=digest(exe), runs=len(results),
                       passed=sum(r['passed'] for r in results), settings_and_log_restored=restored,
                       saves_unchanged=saves_unchanged)
        (evidence / 'receipt.json').write_text(json.dumps(receipt, indent=2))
        print(json.dumps(receipt), flush=True)
        assert restored and saves_unchanged, 'Restoration/preservation check failed'
    assert all(r['passed'] for r in results), 'Window-size regression failed; see results.json'


if __name__ == '__main__':
    main()
