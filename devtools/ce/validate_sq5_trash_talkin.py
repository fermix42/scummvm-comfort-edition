#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Audit local audio and smoke-test real CE detection and SCI startup.

Uses SDL dummy video/audio, isolated configuration/saves and no account state.
This cannot establish audible quality, visible lip sync or gameplay correctness.
"""
import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import wave


def audio_audit(voices):
    formats, durations = collections.Counter(), {}
    for path in voices.glob('*.wav'):
        with wave.open(str(path), 'rb') as audio:
            assert audio.getcomptype() == 'NONE' and audio.getsampwidth() == 2, path
            assert audio.getnchannels() in (1, 2) and audio.getnframes() > 0, path
            size = audio.getnframes() * audio.getsampwidth() * audio.getnchannels()
            assert len(audio.readframes(audio.getnframes())) == size, path
            durations[path.stem] = audio.getnframes() * 1000 / audio.getframerate()
            formats[str((audio.getnchannels(), audio.getsampwidth(), audio.getframerate()))] += 1
    assert len(durations) == 2851
    timing = []
    for path in voices.glob('*.tim'):
        data = path.read_bytes()
        assert data[:8] == b'SQ5WORD1', path
        _, duration, count = struct.unpack_from('<III', data, 8)
        assert len(data) == 52 + 8 * count and count > 0, path
        assert data[20:52].decode() == hashlib.md5(path.with_suffix('.wav').read_bytes()).hexdigest(), path
        assert abs(duration - durations[path.stem]) <= 2, path
        words = [struct.unpack_from('<II', data, 52 + i * 8) for i in range(count)]
        assert all(end > 0 and ms <= duration + 250 for end, ms in words), path
        assert all(b[0] > a[0] and b[1] >= a[1] for a, b in zip(words, words[1:])), path
        timing.append({'id': path.stem, 'words': count, 'duration_ms': duration})
    assert len(timing) == 6
    return {'wav_files': len(durations), 'formats': dict(formats), 'timing_files': timing}


def runtime(exe, game, out, name, enhanced, seconds, fixture):
    cfg = out / (name + '.ini')
    saves = out / (name + '-saves')
    saves.mkdir(exist_ok=True)
    capture = out / (name + '-speech.tsv')
    # Discard only this harness's old trace files, so a prior successful
    # initialization cannot mask a failure in the current run.
    for path in (capture, capture.with_name(capture.name + '.tmp')):
        path.unlink(missing_ok=True)
    cfg.write_text('[scummvm]\nfullscreen=false\ngfx_mode=surfacesdl\n'
                   'aspect_ratio=false\nmusic_driver=null\ntts_enabled=false\n'
                   'speech_mute=false\nsubtitles=true\njoystick_num=-1\n'
                   f'savepath={saves}\nsq5_capture_file={capture}\nextrapath={fixture}\n')
    detection = subprocess.run([str(exe), '--config=' + str(cfg), '--detect', '--path=' + str(game)],
                               capture_output=True, text=True, timeout=30)
    assert detection.returncode == 0 and 'sci:sq5' in detection.stdout, detection.stdout + detection.stderr
    assert ("Trash Talkin' Edition v0.2.12" in detection.stdout) == enhanced, detection.stdout
    (out / (name + '-detection.log')).write_text(detection.stdout + detection.stderr)
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
    logfile = out / (name + '-startup.log')
    console = out / (name + '-console.log')
    with console.open('w') as stream:
        process = subprocess.Popen([str(exe), '--config=' + str(cfg), '--logfile=' + str(logfile),
                                    '--debugflags=ResMan', '--debuglevel=1',
                                    '--auto-detect', '--path=' + str(game)],
                                   env=env, stdout=stream, stderr=subprocess.STDOUT)
        try:
            process.wait(timeout=seconds)
            raise AssertionError(f'{name} exited before the smoke deadline: {console.read_text()}')
        except subprocess.TimeoutExpired:
            # Terminate only the exact isolated process created above.
            process.terminate()
            process.wait(timeout=10)
    text = console.read_text()
    assert 'Running Space Quest V' in text, text
    assert not any(term in text for term in ('ERROR:', 'Assertion failed', 'program counter gone astray')), text
    assert 'Patching 65000.wav - OK' in text, 'Genuine numeric WAV patches must remain supported'
    tuple_patches = re.findall(r'Patching (\d+-[^\s]*\.wav) - OK', text)
    assert not tuple_patches, f'Tuple recordings replaced native SCI audio: {tuple_patches[:5]}'
    # DumpFile uses an atomic .tmp file until the engine closes normally.
    # This bounded smoke deliberately terminates the process; inspect that
    # temporary capture too. Its creation proves the edition SciTTS ran.
    captures = [path for path in (capture, capture.with_name(capture.name + '.tmp')) if path.exists()]
    assert bool(captures) == enhanced, (name, captures)
    plays = [line for path in captures for line in path.read_text().splitlines() if line.startswith('#play')]
    return {'detection': 'passed', 'startup_seconds': seconds, 'engine_remained_running': True,
            'edition_voice_initialization': bool(captures), 'speech_play_events': plays,
            'numeric_wave_patch': 'passed', 'tuple_wave_patch_collisions': len(tuple_patches),
            'termination': 'bounded smoke process terminated by harness'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--original', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--seconds', type=int, default=20)
    args = parser.parse_args()
    assert 1 <= args.seconds <= 60
    out = args.evidence.resolve()
    out.mkdir(parents=True, exist_ok=True)
    result = {'audio': audio_audit(args.game.resolve() / 'voices')}
    fixture = out / 'wave-patch-fixture'
    fixture.mkdir(exist_ok=True)
    shutil.copy2(args.game.resolve() / 'voices/104-2-0-0-1.wav', fixture / '65000.wav')
    for name, game, enhanced in [('enhanced', args.game, True), ('original', args.original, False)]:
        result[name] = runtime(args.exe.resolve(), game.resolve(), out, name, enhanced, args.seconds, fixture)
    (out / 'validation.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
