#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Normal-input Cat visibility restoration and F12 diagnostic regression."""
import argparse
import hashlib
import json
from pathlib import Path
import time

from test_cli_client import Client

parser = argparse.ArgumentParser(description=__doc__)
for name in ('exe', 'assets', 'extra', 'root'):
    parser.add_argument('--' + name, type=Path, required=True)
parser.add_argument('--offscreen', action='store_true')
parser.add_argument('--idle-seconds', type=int, default=0)
args = parser.parse_args()
root = args.root.resolve()
c = Client(args.exe.resolve(), root, args.extra.resolve(), offscreen=args.offscreen)
result = {'exe_sha256': hashlib.sha256(args.exe.read_bytes()).hexdigest(), 'checks': []}


def state():
    return c.command('inspect')['adapter']


def wait(predicate, timeout=150):
    deadline = time.monotonic() + timeout
    while True:
        current = state()
        if predicate(current):
            return current
        if time.monotonic() >= deadline:
            raise AssertionError(('timeout', current['mode'], current['page'], current['phase']))
        time.sleep(.1)


def item(current, name):
    return next(i for i in current['items'] if i['name'] == name)


def key(code, phase, mapped=False):
    c.command('input', kind='key', key=code, phase=phase, map_physical=mapped)
    time.sleep(.08)


def press(code):
    key(code, 'down')
    key(code, 'up')


def click(x, y):
    c.command('input', kind='mouse_move', x=x, y=y)
    for phase in ('down', 'up'):
        c.command('input', kind='mouse_button', button=1, x=x, y=y, phase=phase)
        time.sleep(.08)


def captures():
    return sorted((root / 'logs/diagnostics').glob('*.json'))


def capture(expected_count, mapped=False):
    previous = set(captures())
    key(293, 'down', mapped)
    deadline = time.monotonic() + 30
    while len(captures()) != expected_count:
        state()
        assert time.monotonic() < deadline, 'F12 did not create a report'
        time.sleep(.1)
    state()  # Wait for the engine-thread capture to finish and close its files.
    created = set(captures()) - previous
    assert len(created) == 1
    report_path = created.pop()
    report = json.loads(report_path.read_text())
    assert report['format'] == 'ce-diagnostic/1'
    assert report['screenshot_saved']
    screenshot = report_path.parent / report['screenshot']
    assert screenshot.read_bytes().startswith(b'\x89PNG\r\n\x1a\n')
    assert report['executable_md5'] == hashlib.md5(args.exe.read_bytes()).hexdigest()
    assert report['engine_state']['mode'] == 6 and report['engine_state']['page'] == 5
    assert report['engine_state']['page_file'].upper().endswith('CH05.MHK')
    assert report['engine_state']['visibility_history']
    assert len(report['engine_state']['visibility_history']) <= 64
    history = report['engine_state']['render_history']
    assert 0 < len(history) <= 64
    assert all('drawable_animations' in frame and 'serial' in frame and 'millis' in frame for frame in history)
    assert any(i.get('animation', {}).get('nodes') for i in report['engine_state']['items'])
    assert report['engine_state']['draw_order_back_to_front']
    assert any(i['key'] == 293 for i in report['recent_input'])
    assert len(report['recent_input']) <= 64
    for secret in ('ce_link_token', 'password', 'access_token'):
        assert secret not in report.get('settings', {})
    return report_path


try:
    c.launch(args.assets.resolve(), 'windows', 'catinthehat', 'mohawk')
    time.sleep(2)
    press(27)
    wait(lambda s: s['mode'] == 2 and s['page'] == 1 and s['phase'] == 3 and s['cursor_visible'])
    click(106, 430)
    wait(lambda s: s['mode'] == 2 and s['page'] == 3 and s['phase'] == 3)
    for _ in range(3):
        click(557, 324)
        time.sleep(2)
    click(297, 324)
    current = wait(lambda s: s['mode'] == 6)
    while current['page'] < 5:
        previous = (current['page'], current['subpage'])
        press(275)
        current = wait(lambda s: s['mode'] == 6 and (s['page'], s['subpage']) != previous)
        time.sleep(.5)
    wait(lambda s: s['page'] == 5 and s['phase'] == 3 and s['cursor_visible'])
    if args.idle_seconds:
        # No clicks after entering the scene: exercise the automatic idle path.
        names = ('SallyR', 'TommyR')
        starts = dict.fromkeys(names, 0)
        previous = dict.fromkeys(names, False)
        overlap_since = dict.fromkeys(names)
        maximum_overlap = dict.fromkeys(names, 0.0)
        checked_frames = set()
        deadline = time.monotonic() + args.idle_seconds
        while time.monotonic() < deadline:
            current = state()
            now = time.monotonic()
            for frame in current['render_history']:
                if frame['mode'] != 6 or frame['page'] != 5 or frame['phase'] != 3:
                    continue
                sprites = set(frame['drawable_animations'])
                if 1020 in sprites:  # The opening scene still supplies both poses.
                    continue
                assert sprites & {1200, 1230}, ('Sally absent at draw boundary', frame)
                assert sprites & {1220, 1210}, ('Tommy absent at draw boundary', frame)
                checked_frames.add(frame['serial'])
            for name, reaction_name in (('SallyR', 'SallyReact'), ('TommyR', 'TommyReact')):
                idle = item(current, name)
                reaction = item(current, reaction_name)
                starts[name] += int(idle['playing'] and not previous[name])
                previous[name] = idle['playing']
                if idle['visible'] and reaction['visible']:
                    if overlap_since[name] is None:
                        overlap_since[name] = now
                    duration = now - overlap_since[name]
                    maximum_overlap[name] = max(maximum_overlap[name], duration)
                    assert duration < 1, ('persistent doubled pose', name, idle, reaction)
                else:
                    overlap_since[name] = None
            time.sleep(.02)
        assert all(n >= 2 for n in starts.values()), starts
        for name in ('SallyR', 'TommyR', 'SallyReact', 'TommyReact'):
            assert item(current, name)['uninitialized_draws'] == 0, item(current, name)
        result['idle_starts'] = starts
        result['idle_render_frames_checked'] = len(checked_frames)
        assert len(checked_frames) >= 100, len(checked_frames)
        result['maximum_pose_overlap_seconds'] = maximum_overlap
        result['checks'].append('automatic idle sequence: both children repeat, no persistent doubled poses')
        result['checks'].append('no rendered idle/reaction frame before first animation frame initialization')
        result['checks'].append('both children have drawable sprite cells at every observed idle render boundary')
    wait(lambda s: not any(item(s, name)['playing'] for name in
                          ('SallyR', 'TommyR', 'SallyReact', 'TommyReact')))
    click(505, 220)
    wait(lambda s: item(s, 'Tommy')['playing'])
    current = wait(lambda s: not item(s, 'Tommy')['playing'] and not item(s, 'Tommy')['visible'])
    reaction = item(current, 'SallyReact')
    assert not reaction['visible'] and reaction['visibility_balance'] < 0, reaction
    result['checks'].append('Tommy interaction preserves Sally reaction hidden balance')
    c.command('screenshot')
    first = capture(1)
    key(293, 'down')  # A held key must not create another report.
    state()
    assert len(captures()) == 1
    key(293, 'up')
    second = capture(2, mapped=True)
    key(293, 'up', mapped=True)
    assert first != second
    result['checks'].append('F12 writes PNG and rich JSON, suppresses held repeats, supports physical mapping')
    result['reports'] = [str(first), str(second)]
    assert state()['page'] == 5
    result['status'] = 'passed'
except Exception as error:
    result.update(status='failed', error=repr(error))
finally:
    try:
        c.close()
    except Exception:
        c.abort()
    (root / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)
raise SystemExit(0 if result['status'] == 'passed' else 1)
