#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded JSON pipe client for the native CE interface; never sends desktop input."""
import hashlib
import json
import pathlib
import queue
import subprocess
import threading
import time
import os


def desktop_observation(pid):
    """Read-only ownership checks; does not activate windows or send OS input."""
    if os.name != 'nt':
        return None
    import ctypes
    from ctypes import wintypes
    user = ctypes.WinDLL('user32', use_last_error=True)
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.GetForegroundWindow.restype = wintypes.HWND
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    owned, visible = [], []

    @callback_type
    def visit(window, _):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == pid:
            owned.append(int(window))
            if user.IsWindowVisible(window):
                visible.append(int(window))
        return True

    user.EnumWindows(visit, 0)
    point = wintypes.POINT()
    user.GetCursorPos(ctypes.byref(point))
    return {'owned_windows': len(owned), 'visible_owned_windows': visible,
            'foreground': int(user.GetForegroundWindow() or 0), 'cursor': [point.x, point.y]}


class Client:
    def __init__(self, exe, root, extra, offscreen=False, transcript=None):
        self.root = pathlib.Path(root).resolve()
        self.root.mkdir(parents=True, exist_ok=True)
        self.events = queue.Queue()
        self.serial = 0
        self.records = []
        self.transcript = pathlib.Path(transcript) if transcript else self.root.parent / (self.root.name + '.jsonl')
        self.errors = (self.root.parent / (self.root.name + '-stderr.log')).open('w', encoding='utf-8')
        args = [str(exe), '--test-cli', f'--test-root={self.root}', f'--extrapath={extra}']
        if offscreen:
            args.append('--test-render=offscreen')
        self.process = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=self.errors, text=True, encoding='utf-8',
                                        creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        self.record({'process': self.process.pid, 'args': args,
                     'exe_sha256': hashlib.sha256(pathlib.Path(exe).read_bytes()).hexdigest()})

        def read():
            try:
                for line in self.process.stdout:
                    self.events.put(json.loads(line))
            except Exception as exc:
                self.events.put(exc)
            finally:
                self.events.put(EOFError('native CLI pipe closed'))

        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        try:
            ready = self.receive(15)
            if ready.get('event') != 'ready':
                raise RuntimeError(ready)
        except BaseException:
            self.abort()
            raise

    def record(self, record):
        self.records.append(record)
        with self.transcript.open('a', encoding='utf-8') as f:
            f.write(json.dumps({'time': time.monotonic(), **record}) + '\n')

    def receive(self, timeout=130):
        try:
            result = self.events.get(timeout=timeout)
        except queue.Empty:
            raise TimeoutError(f'native CLI did not respond in {timeout}s') from None
        if isinstance(result, Exception):
            raise result
        self.record({'response': result})
        desktop = desktop_observation(self.process.pid)
        if desktop is not None:
            self.record({'desktop_observation': desktop})
            if desktop['visible_owned_windows']:
                raise AssertionError(f'test process created visible windows: {desktop}')
        if result.get('error') == 'engine_fatal':
            raise RuntimeError(result.get('message', 'engine fatal error'))
        return result

    def command(self, cmd, *, check=True, timeout=130, **fields):
        self.serial += 1
        request = {'id': self.serial, 'cmd': cmd, **fields}
        self.record({'request': request})
        self.process.stdin.write(json.dumps(request) + '\n')
        self.process.stdin.flush()
        result = self.receive(timeout)
        if result.get('id') != self.serial:
            raise AssertionError(f'response ID mismatch: {result}')
        if check and not result.get('ok'):
            raise AssertionError(result)
        return result

    def launch(self, path, platform, game='monkey4', engine='grim'):
        return self.command('launch', path=str(path), platform=platform, game=game, engine=engine)

    def press(self, action, frames=3):
        self.command('input', kind='logical', action=action, phase='down')
        self.command('advance', frames=frames)
        self.command('input', kind='logical', action=action, phase='up')
        return self.command('advance', frames=2)

    def type_text(self, value):
        for ch in value:
            self.command('input', kind='key', key=ord(ch.lower()), ascii=ord(ch), phase='down')
            self.command('advance', frames=1)
            self.command('input', kind='key', key=ord(ch.lower()), ascii=ord(ch), phase='up')
        return self.command('inspect')

    def axis(self, axis, position, frames=3):
        self.command('input', kind='axis', axis=axis, position=position)
        self.command('advance', frames=frames)
        self.command('input', kind='axis', axis=axis, position=0)
        return self.command('advance', frames=2)

    def close(self, expected=0):
        try:
            if self.process.poll() is None:
                self.command('quit', timeout=10)
                self.process.stdin.close()
            code = self.process.wait(timeout=15)
            self.record({'exit_code': code})
            if code != expected:
                raise AssertionError(f'exit {code}, expected {expected}')
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait(timeout=10)
            self.errors.close()

    def abort(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)
        self.errors.close()
