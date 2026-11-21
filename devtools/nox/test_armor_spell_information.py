#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Capture v119 armor and spell information and exercise the normal spell selector."""
import argparse
from pathlib import Path

from test_encounter_restart import inspect, launch, press, save_state
from test_exploration_map import shot
from test_party_invulnerability import core_parts, encode_checkpoint, sections


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'game', 'extra', 'artifacts'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root = args.artifacts.resolve()
    root.mkdir()
    work = root / 'client'
    source = args.game / 'conversion/ce-m4-inventory353-world/missile-book-real-exit-world/saves/nox-native-exploration-final.bin'
    checkpoint = source.read_bytes()
    _, _, inventory, _ = core_parts(checkpoint)
    # Give the disposable party one unreadied Mage Necklace +2 to cover INT and nonzero resistance.
    slot = next(i for i in range(0, len(inventory), 6) if inventory[i] == 254)
    parts = sections(checkpoint)
    core = bytearray(parts['CORE'])
    offset = core.index(inventory)
    core[offset + slot:offset + slot + 6] = bytes((1, 18, 1, 0, 0, 7))
    parts['CORE'] = core
    fixture = root / 'fixture.bin'
    fixture.write_bytes(encode_checkpoint(checkpoint, parts))
    armor_before_necklace = sum(inventory[i] != 254 and inventory[i] & 15 == 1 for i in range(0, slot, 6))
    client = launch(args.exe.resolve(), work, args.game.resolve(), args.extra.resolve(), fixture, False, 0)

    def capture(label):
        shot(client, work).replace(root / (label + '.png'))

    def info(label):
        press(client, 105, 105, map_physical=True)
        assert inspect(client)['adapter']['inventory_phase'] == 4
        capture(label)
        press(client, 27, 27)

    try:
        before = save_state(client, work, 101)
        press(client, 9, 9)
        press(client, 9, 9)
        press(client, 51, 51)
        info('cloth-helm')
        for _ in range(armor_before_necklace):
            press(client, 274)
        info('mage-necklace')
        press(client, 53, 53)
        capture('owned-spellbooks')
        info('lesser-heal')
        press(client, 274)
        info('nox-vortex')
        press(client, 274)
        info('magic-missile')
        press(client, 13, 13)
        assert inspect(client)['adapter']['inventory_phase'] == 2
        capture('assign-magic-missile')
        press(client, 27, 27)
        assert inspect(client)['adapter']['inventory_phase'] == 1
        press(client, 27, 27)
        cancelled = save_state(client, work, 102)
        assert before['CORE'] == cancelled['CORE'] and before['RAND'] == cancelled['RAND']
        press(client, 9, 9)
        press(client, 9, 9)
        press(client, 53, 53)
        press(client, 274)
        press(client, 274)
        press(client, 13, 13)
        press(client, 57, 57)
        assert inspect(client)['adapter']['inventory_message'] == 'Item readied'
        press(client, 112, 112)
        capture('readied-spells')
        press(client, 27, 27)
        after = save_state(client, work, 103)
        old_book = before['CORE'].index(b'NBSK')
        new_book = after['CORE'].index(b'NBSK')
        expected = bytearray(before['CORE'][old_book:old_book + 26])
        expected[15] = 0
        expected[25] = 8
        assert after['CORE'][new_book:new_book + 26] == expected
        print('PASS armor and spell screenshots, clean cancellation, Magic Missile on slot 9')
    finally:
        client.close()

    low_core = bytearray(parts['CORE'])
    party_offset = low_core.index(b'NPAR\x01\x00') + 6
    for member in range(6):
        low_core[party_offset + member * 128 + 18] = 7
    parts['CORE'] = low_core
    low_fixture = root / 'low-intelligence.bin'
    low_fixture.write_bytes(encode_checkpoint(checkpoint, parts))
    low = launch(args.exe.resolve(), root / 'low-intelligence', args.game.resolve(), args.extra.resolve(), low_fixture, False, 0)
    try:
        for key, ascii_value in [(9, 9), (9, 9), (53, 53), (274, 0), (274, 0), (13, 13)]:
            press(low, key, ascii_value)
        state = inspect(low)['adapter']
        assert state['inventory_phase'] == 1
        assert state['inventory_message'] == 'Intelligence too low to wield'
        print('PASS insufficient INT is refused before opening the ready-slot prompt')
    finally:
        low.close()


if __name__ == '__main__':
    main()
