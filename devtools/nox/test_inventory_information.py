#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise repeated weapon information through the native inventory menu."""
import argparse
from pathlib import Path
import struct
import sys

from test_encounter_restart import launch, inspect, press, save_state
from test_exploration_map import shot
from test_party_invulnerability import core_parts, digest, encode_checkpoint, sections


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'game', 'extra', 'save', 'artifacts'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--plus-one', action='store_true', help='Use the v119 +1 Broadsword in the disposable fixture')
    args = parser.parse_args()
    root = args.artifacts.resolve()
    root.mkdir()
    source_digest = digest(args.save)
    raw = args.save.read_bytes()
    native = raw.index(b'NPLS', 32)
    size = struct.unpack_from('<I', raw, native + 6)[0]
    checkpoint = raw[native + 10:native + 10 + size]
    _, _, inventory, _ = core_parts(checkpoint)
    weapons = [i for i in range(0, len(inventory), 6)
               if inventory[i] != 254 and inventory[i] & 15 == 0]
    item_id = 17 if args.plus_one else 4
    if args.plus_one and all(inventory[i + 1] != item_id for i in weapons):
        # Add one unreadied copy to this disposable checkpoint.
        slot = next(i for i in range(0, len(inventory), 6) if inventory[i] == 254)
        parts = sections(checkpoint)
        core = bytearray(parts['CORE'])
        offset = core.index(inventory)
        core[offset + slot:offset + slot + 6] = bytes((0, item_id, 1, 0, 0, 1))
        parts['CORE'] = core
        checkpoint = encode_checkpoint(checkpoint, parts)
        inventory = core_parts(checkpoint)[2]
        weapons = sorted(weapons + [slot])
    selected = [inventory[i + 1] for i in weapons].index(item_id)
    fixture = root / 'broadsword-world.bin'
    fixture.write_bytes(checkpoint)
    work = root / 'client'
    client = launch(args.exe.resolve(), work, args.game.resolve(), args.extra.resolve(), fixture, False, 0)
    try:
        before = save_state(client, work, 101)
        press(client, 9, 9)
        press(client, 9, 9)
        press(client, 50, 50)
        for _ in range(selected):
            press(client, 274)
        for attempt in range(3):
            state = inspect(client)['adapter']
            assert (state['inventory_tab'], state['inventory_selected'], state['inventory_phase']) == (1, selected, 1)
            press(client, 105, 105, map_physical=True)
            assert inspect(client)['adapter']['inventory_phase'] == 4
            if attempt == 0:
                print(f'Item information screenshot: {shot(client, work)}')
            press(client, 27, 27)
        press(client, 27, 27)
        after = save_state(client, work, 102)
        assert before['CORE'] == after['CORE'] and before['RAND'] == after['RAND']
        assert digest(args.save) == source_digest
        print(f'PASS weapon {item_id} information opened and closed three times with inventory and gameplay state unchanged')
    finally:
        if sys.exc_info()[0] is None:
            client.close()
        else:
            client.abort()


if __name__ == '__main__':
    main()
