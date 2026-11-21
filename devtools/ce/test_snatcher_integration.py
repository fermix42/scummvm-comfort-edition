#!/usr/bin/env python3
"""Bounded combined-build checks. Original media stays read-only; SDL dummy only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import threading

def sha(path):
    return hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()

class Client:
    def __init__(self, exe, root, game, core, reuse=False):
        root.mkdir(exist_ok=reuse)
        self.root, self.records = root, []
        self.label = root.name + ('-cold' if reuse else '')
        self.stderr = (root.parent / (root.name + '-stderr.log')).open('w')
        self.p = subprocess.Popen([str(exe), '--test-cli', '--test-root=' + str(root)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr,
            text=True, encoding='utf-8', creationflags=subprocess.CREATE_NO_WINDOW,
            env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'))
        self.watch = threading.Timer(110, self.p.kill)
        self.watch.start()
        self.q = queue.Queue()
        def read():
            for line in self.p.stdout:
                self.q.put(line)
            self.q.put(None)
        threading.Thread(target=read, daemon=True).start()
        assert self.receive()['event'] == 'ready'
        if not reuse:
            (root / 'profile.ini').write_text('[scummvm]\nconfirm_exit=false\nconsole=false\n'
            'enable_unsupported_game_warning=false\nautosave_period=1\n\n[cli-game]\n'
            'engineid=snatcher\ngameid=snatcher\nplatform=segacd\ncheck_gamedata=false\n'
            f'path={game}\nsnatcher_fm_core={core}\nsnatcher_decensor=false\n', encoding='utf-8')
    def receive(self):
        line = self.q.get(timeout=95)
        assert line is not None, self.p.poll()
        result = json.loads(line)
        self.records.append({'response': result})
        assert result.get('ok'), result
        return result
    def command(self, cmd, **fields):
        request = dict(cmd=cmd, **fields)
        self.records.append({'request': request})
        self.p.stdin.write(json.dumps(request) + '\n')
        self.p.stdin.flush()
        return self.receive()
    def close(self):
        try:
            if self.p.poll() is None:
                self.command('quit')
                self.p.wait(timeout=15)
        finally:
            if self.p.poll() is None:
                self.p.kill()
                self.p.wait(timeout=10)
            self.watch.cancel()
            self.stderr.close()
            (self.root.parent / (self.label + '-transcript.json')).write_text(json.dumps(self.records, indent=2))
        assert self.p.returncode == 0, self.p.returncode

def run(exe, output, game, core, checkpoint):
    root = output / core
    c = Client(exe, root, game, core)
    try:
        if checkpoint:
            (root / 'saves').mkdir(exist_ok=True)
            shutil.copyfile(checkpoint, root / 'saves' / 'cli-game.001')
        c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(game))
        c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
        state = c.command('inspect')
        assert state['video_driver'] == 'dummy' and state['adapter']['fm_core'] == core
        assert not state['adapter']['decensor_enabled'] and state['adapter']['mouse_enabled']
        def probe(action, **fields):
            return c.command('input', kind='logical', action=action, **fields)
        probe('autosave_policy_probe')
        assert not (root / 'saves' / 'cli-game.000').exists()
        assert not (root / 'saves' / 'cli-game.099').exists()
        c.command('save', slot=0, description='Manual zero must survive autosave')
        zero = root / 'saves' / 'cli-game.000'
        before = sha(zero)
        probe('autosave_policy_probe')
        assert sha(zero) == before
        c.command('save', slot=99, description='Manual maximum')
        last = sha(root / 'saves' / 'cli-game.099')
        probe('autosave_policy_probe')
        assert sha(zero) == before and sha(root / 'saves' / 'cli-game.099') == last
        c.command('load', slot=0)
        probe('save_anywhere_probe', slot=80)
        probe('save_owner_contracts')
        probe('resource_transport_probe')
        probe('fm_backend_probe')
        capacity = probe('save_capacity')
        assert capacity['default_unknown_zero'] and capacity['error_unchanged']
        c.command('advance', frames=4)
        storage = c.command('storage')
        for phase in ('lost', 'gained'):
            c.command('input', kind='focus', phase=phase)
            c.command('advance', frames=1)
            if phase == 'lost':
                assert not c.command('inspect')['adapter']['mouse_ready']
        c.command('input', kind='key', key=112, ascii=112, phase='down', map_physical=True)
        c.command('advance', frames=1)
        c.command('input', kind='key', key=112, ascii=112, phase='up', map_physical=True)
        assert c.command('inspect')['adapter']['input_paused']
        c.command('input', kind='key', key=112, ascii=112, phase='down', map_physical=True)
        c.command('advance', frames=1)
        c.command('input', kind='key', key=112, ascii=112, phase='up', map_physical=True)
        assert not c.command('inspect')['adapter']['input_paused']
        c.command('input', kind='mouse_move', x=120, y=120)
        c.command('input', kind='mouse_move', x=120, y=120, outside_active_area=True, map_physical=True)
        c.command('input', kind='mouse_wheel', x=120, y=120, direction=1)
        c.command('advance', frames=1)
        if checkpoint:
            c.command('load', slot=1)
            probe('save_anywhere_probe', slot=80)
            probe('autosave_policy_probe')
        assert sha(zero) == before
        listing = c.command('saves')
        assert {0, 99}.issubset({s['chooser_slot'] for s in listing['saves']})
        final = c.command('inspect')['adapter']
        assert not any(final.get(k) for k in ('blocker', 'runtime_error', 'audio_error', 'render_error'))
    finally:
        c.close()
    # Cold restore through the normal startup slot setting.
    text = (root / 'profile.ini').read_text()
    (root / 'profile.ini').write_text(text.replace('[cli-game]', '[cli-game]\nsave_slot=0'))
    c = Client(exe, root, game, core, reuse=True)
    try:
        c.command('launch', engine='snatcher', game='snatcher', platform='segacd', path=str(game))
        c.command('wait', field='adapter.ready', equals=True, timeout_ms=30000)
        assert c.command('inspect')['adapter']['fm_core'] == core
        c.command('load', slot=0)
        assert sha(zero) == before
        c.command('input', kind='logical', action='autosave_policy_probe')
        assert sha(zero) == before
    finally:
        c.close()
    return dict(core=core, game=str(game), manual_zero_sha256=before, storage=storage,
                exit_code=c.p.returncode, cold_restore=True,
                diagnostic_checkpoint_sha256=sha(checkpoint) if checkpoint else None)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--exe', type=Path, required=True)
    p.add_argument('--game', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--checkpoint', type=Path)
    a = p.parse_args()
    a.output.mkdir()
    records = [run(a.exe, a.output, a.game, core, a.checkpoint) for core in ('gens', 'nuked')]
    (a.output / 'receipt.json').write_text(json.dumps(dict(status='PASS', exe_sha256=sha(a.exe), records=records), indent=2))

if __name__ == '__main__':
    main()
