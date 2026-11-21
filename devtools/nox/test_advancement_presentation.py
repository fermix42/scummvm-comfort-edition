#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check v119 prayer presentation, rejection, character ownership and cold input.

Uses retained private checkpoints in isolated portable validation directories.
The two-member case is explicitly controlled before the UI transaction.
"""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

from test_party_invulnerability import encode_checkpoint, sections
from test_resources import digest, run_validation_process


def advancement(packet):
    battle = sections(packet)['BTTL']
    at = battle.index(b'NADV\x01\x00')
    return battle[at:at + 144]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'game', 'artifacts'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    exe, game, root = (getattr(args, n).resolve() for n in ('exe', 'game', 'artifacts'))
    assert root.is_relative_to(exe.parent)
    root.mkdir()
    source = game / 'conversion/ce-m4-advancement248-workflow'
    original = {str(p): digest(p) for p in game.glob('*.HDV')}
    report = dict(executable_sha256=digest(exe), sources={}, runs={})
    fonts = [(game / f'Nox Archaist Extracted/NOXARCHAIST/NA/FONT{n}').read_bytes() for n in (1, 2)]

    def retained(name):
        path = source / name / 'saves/nox-native-exploration-final.bin'
        report['sources'][str(path)] = digest(path)
        return path.read_bytes()

    def run(label, checkpoint, text='', keys=None, **options):
        folder = root / label
        saves = folder / 'saves'
        saves.mkdir(parents=True)
        keyfile = folder / 'keys.bin'
        events = [(ord(c.lower()), ord(c.upper())) for c in text] if keys is None else keys
        keyfile.write_bytes(b''.join(struct.pack('<HH', *key) for key in events))
        (saves / 'nox-native-exploration.bin').write_bytes(checkpoint)
        cfg = configparser.ConfigParser(interpolation=None)
        cfg['scummvm'] = dict(confirm_exit='false', updates_check='0')
        cfg['nox-test'] = dict(engineid='nox', gameid='nox', platform='apple2', path=str(game),
                               check_gamedata='false', ce_playthrough_tracking='false',
                               nox_native_startup_keys=str(keyfile), nox_native_startup_resume='true',
                               nox_native_startup_capture='true')
        cfg['nox-test'].update(options)
        ini = folder / 'scummvm.ini'
        with ini.open('w') as stream:
            cfg.write(stream)
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        argv = [str(exe), '--config=' + str(ini), '--savepath=' + str(saves),
                '--logfile=' + str(folder / 'engine.log'), '--no-console', 'nox-test']
        result = run_validation_process(argv, folder, dict(os.environ, SDL_VIDEODRIVER='windows', SDL_AUDIODRIVER='dummy'), startup, timeout=60)
        (folder / 'stderr.txt').write_bytes(result.stderr)
        packet = (saves / 'nox-native-exploration-final.bin').read_bytes()
        report['runs'][label] = dict(exit=result.returncode, checkpoint_sha256=hashlib.sha256(packet).hexdigest(), keys=events)
        (root / 'verification.json').write_text(json.dumps(report, indent=2))
        return packet

    def glyph(frame, x, row):
        return bytes(frame[(y & 7) * 1024 + ((y >> 3) & 7) * 128 + (y >> 6) * 40 + x] for y in range(row * 8, row * 8 + 8))

    def matches(packet, value, x, y, inverse=0):
        frame = sections(packet)['BTTL'][-8192:]
        for font in fonts:
            if all(glyph(frame, x + i, y) == bytes(b ^ inverse for b in font[(ord(c) - 32) * 8:(ord(c) - 31) * 8]) for i, c in enumerate(value)):
                return
        raise AssertionError(f'Original glyphs missing at {x},{y}: {value!r} inverse={inverse}')

    # Begin from an actual retained won battle: reward/advancement remain normal.
    start = run('victory-introduction', retained('actual-fight'), keys=[(27, 27)])
    s = advancement(start)
    assert s[7:13] == bytes((1, 0, 0, 12, 0, 0)), s[:16]
    record = sections(start)['BTTL'][8:136]
    name = bytes(c & 127 for c in record[75:90]).split(b'\0')[0].decode()
    matches(start, name, 1, 1)
    matches(start, name, 25, 1, 127)
    matches(start, 'How long dost ', 25, 14)
    matches(start, 'Mins Left: 12', 25, 20)
    matches(start, 'STR-', 1, 13, 127)
    oracle = game / 'conversion/combat-contract-research/advancement-settlement-reference/input.bin'
    run('original-record-oracle', start, nox_native_battle_advancement_oracle=str(oracle))
    oracle_report = (root / 'original-record-oracle/saves/nox-native-battle-advancement-oracle.tsv').read_text()
    assert 'cases=135' in oracle_report
    report['original_record_cases'] = 135

    empty = run('empty-return', start, '\r')
    assert advancement(empty) == s, 'Empty Return must not allocate zero'
    refused = run('over-budget', start, '99\r')
    assert advancement(refused) == s, 'Original numeric service restarts after refusal'
    partial = run('partial-number', refused, '1')
    deleted = run('delete-number', partial, keys=[(127, 127)])
    assert advancement(deleted) == s
    backed = run('backspace-number', partial, keys=[(8, 8)])
    assert advancement(backed) == s
    warm = run('all-strength', refused, '12\r')
    cold = run('cold-number', partial, '2\r')
    assert advancement(warm) == advancement(cold)
    matches(warm, 'Prayer Minutes', 25, 14)
    matches(warm, 'STR-12', 25, 15)
    matches(warm, 'Accept? (Y/N)', 25, 19)
    matches(warm, name, 1, 1)
    rejected = run('negative-confirmation', warm, 'n')
    assert advancement(rejected) == s
    dexterity = run('dexterity-prompt', rejected, '4\r')
    matches(dexterity, 'How long dost ', 25, 14)
    matches(dexterity, 'thou plead to ', 25, 15)
    matches(dexterity, 'DEX-', 1, 14, 127)
    intelligence = run('intelligence-prompt', dexterity, '3\r')
    matches(intelligence, 'thou meditate ', 25, 15)
    matches(intelligence, 'INT-', 1, 15, 127)
    retry = run('repeated-proposal', intelligence, '5\r')
    assert advancement(retry)[7] == 2
    matches(retry, 'STR-4', 25, 15)
    matches(retry, 'DEX-3', 25, 16)
    matches(retry, 'INT-5', 25, 17)
    matches(retry, 'MP -', 1, 11, 127)
    accepted = run('accept-mixed', retry, 'y')
    after = sections(accepted)['BTTL'][8:136]
    assert after[1] == record[1] + 1
    assert list(after[16:19]) == [record[16] + 4, record[17] + 3, record[18] + 5]
    assert struct.unpack_from('<H', after, 72)[0] > struct.unpack_from('<H', record, 72)[0]
    summary = accepted
    if sections(summary)['BTTL'][8 + 2560 + 19] == 1:
        summary = run('leave-loot', summary, 'l')
    returned = run('return-world', summary, '\r')
    world = sections(returned)
    assert len(world['BTTL']) == 8 and not world['BTTL'][6] & 1
    party = world['CORE'].index(b'NPAR\x01\x00') + 6
    assert world['CORE'][party:party + 128] == after
    restored = run('cold-world', returned)
    assert sections(restored)['CORE'] == world['CORE']

    insufficient = run('too-few-minutes', start, '0\r0\r0\r')
    assert advancement(insufficient)[7] == 3
    matches(insufficient, 'Mortal! You ', 25, 14)
    again = run('retry-insufficient', insufficient, ' ')
    assert advancement(again) == s

    # Two named characters with different builds, controlled BEFORE tested UI.
    pieces = sections(start)
    core, battle = bytearray(pieces['CORE']), bytearray(pieces['BTTL'])
    party = core.index(b'NPAR\x01\x00') + 6
    second = bytearray(battle[8:136])
    second[16:19] = bytes((20, 21, 22))
    second[4] = 22
    second[75:90] = b'Bridgit\0' + bytes(7)
    core[party + 128:party + 256] = second
    battle[136:264] = second
    battle[8 + 2560 + 4] = 2
    pieces.update(CORE=core, BTTL=battle)
    two = encode_checkpoint(start, pieces)
    (root / 'controlled-two-members.bin').write_bytes(two)
    proposal = run('two-first-strength', two, '12\r')
    next_member = run('two-next-character', proposal, 'y')
    assert advancement(next_member)[8] == 1
    matches(next_member, 'Bridgit', 1, 1)
    matches(next_member, 'Bridgit', 25, 3, 127)
    proposal2 = run('two-second-intelligence', next_member, '0\r0\r12\r')
    matches(proposal2, 'Bridgit', 1, 1)
    final = run('two-confirm-second', proposal2, 'y')
    records = sections(final)['BTTL'][8:776]
    assert records[16] == record[16] + 12 and records[18] == record[18]
    assert records[128 + 16] == 20 and records[128 + 18] == 34
    assert {str(p): digest(p) for p in game.glob('*.HDV')} == original
    report.update(status='PASS', scope='Original v119 prayer text/layout, numeric refusal, N/retry, Y growth, distinct sequential character builds, cold numeric continuation; controlled two-member UI fixture')
    (root / 'verification.json').write_text(json.dumps(report, indent=2))
    print('PASS original advancement presentation, character ownership, allocation and cold input')


if __name__ == '__main__':
    main()
