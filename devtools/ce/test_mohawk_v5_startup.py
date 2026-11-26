# SPDX-License-Identifier: GPL-3.0-or-later
"""Native Mohawk v5 VM regressions using generated resources, not game scripts.

Requires an owned ThinkQuick data directory for the detector's Outline.txt.
All generated assets and profiles stay beneath a new validation directory.
"""
import argparse
import hashlib
import json
import shutil
import struct
import time
from pathlib import Path

from test_cli_client import Client


def u16(value):
    return struct.pack('>H', value)


def u32(value):
    return struct.pack('>I', value)


def archive(resources):
    count = len(resources)
    tables_end = 4 + count * 8 + count * 8
    file_table = tables_end
    data_start = 28 + file_table + 4 + count * 10
    types, tables, files, payload = b'', b'', b'', b''
    for index, (tag, ident, data) in enumerate(resources):
        table = 4 + count * 8 + index * 8
        types += tag.encode('ascii') + u16(table) + u16(table + 6)
        tables += u16(1) + u16(ident) + u16(index + 1) + u16(0)
        size = len(data)
        files += u32(data_start + len(payload)) + u16(size & 65535) + bytes([size >> 16, 0]) + u16(0)
        payload += data
    body = u16(tables_end) + u16(count) + types + tables + u32(count) + files + payload
    return b'MHWK' + u32(28 + len(body)) + b'RSRC' + u16(0x100) + u16(0) + u32(28 + len(body)) + u32(28) + u16(file_table) + u16(4 + count * 10) + body


class Code:
    def __init__(self):
        self.code = bytearray(4)
        self.strings = bytearray()
        self.names = {}

    def string(self, name, reference=False):
        if name not in self.names:
            self.names[name] = len(self.strings)
            self.strings += b'\0\0' + name.encode('ascii') + b'\0'
        return bytes([1 if reference else 6]) + u16(self.names[name])

    def expr(self, data):
        offset = len(self.code)
        self.code += data + b'\x08'
        return offset

    def resource(self):
        return u32(8 + len(self.code) + len(self.strings)) + u32(len(self.code)) + self.code + self.strings


def integer(value):
    return b'\x05\x03' + struct.pack('>i', value)


def subentry(opcode, extra=b''):
    body = u16(opcode) + u16(65535) + u16(0) + extra
    return u16(len(body)) + body


def field(kind, data):
    return u16(kind) + u16(len(data)) + data


def event(kind, entries, name=None):
    data = u16(kind) + u16(65534) + u16(len(entries)) + b''.join(entries)
    if name is not None:
        data += b'\0' + name.encode('ascii') + b'\0'
    return field(0x66, data)


def fixture(label):
    c = Code()
    ref = lambda name: c.string(name, True)
    prop = b'\x04' + c.string('property') + b'\x20'
    assign = b'\x6b\x22'
    equal = b'\x6c\x16'
    init = b''
    extra_items = b''
    expected_error = None
    if label == 'arithmetic':
        init = field(0x194, u32(c.expr(ref('answer') + integer(6) + integer(7) + b'\x6e\x0e' + assign)))
        test = ref('answer') + integer(42) + equal
    elif label == 'real-arithmetic':
        real = lambda value: b'\x05\x04' + struct.pack('>d', value)
        test = real(1.5) + real(.5) + b'\x6e\x14' + integer(2) + equal
    elif label == 'literal-string':
        text = c.string('literal')
        test = b'\x05\x02' + text[1:] + text + equal
    elif label == 'constant':
        test = b'\x31' + u16(42) + integer(42) + equal
    elif label == 'property':
        init = field(0x194, u32(c.expr(prop + integer(42) + assign)))
        test = prop + integer(42) + equal
    elif label == 'list-index':
        indexed = ref('array') + integer(2) + b'\x1b'
        init = field(0x194, u32(c.expr(indexed + integer(42) + assign)))
        test = indexed + integer(42) + equal
    elif label == 'list-literal':
        init = field(0x194, u32(c.expr(ref('array') + b'\x1a' + integer(10) + b'\x07' + integer(20) + b'\x07\x1b' + assign)))
        test = ref('array') + integer(2) + b'\x1b' + integer(20) + equal
    elif label == 'nested-index':
        indexed = ref('array') + integer(1) + b'\x1b'
        init = field(0x194, u32(c.expr(indexed + integer(42) + assign)))
        init += field(0x194, u32(c.expr(ref('outer') + b'\x1a' + indexed + b'\x07' + integer(0) + b'\x07\x1b' + assign)))
        test = ref('outer') + integer(1) + b'\x1b' + integer(42) + equal
    elif label == 'logical':
        test = integer(0) + integer(1) + b'\x6d\x37' + integer(1) + equal
    elif label == 'post-decrement':
        init = field(0x194, u32(c.expr(ref('counter') + integer(43) + assign)))
        init += field(0x194, u32(c.expr(ref('counter') + b'\x6f\x12')))
        test = ref('counter') + integer(42) + equal
    elif label == 'script-link':
        declaration = c.expr(b'\x62' + ref('argument') + b'\x07\x76\x07')
        body = c.expr(prop + ref('argument') + integer(1) + b'\x6e\x14' + assign)
        donor = event(0x1c, [subentry(0x1d, u32(declaration)), subentry(0x1d, u32(body))], 'linked')
        extra_items = struct.pack('>4h4H', 0, 0, 1, 1, 3, 0, 902, len(donor)) + b'Donor\0' + donor
        init = field(0x194, u32(c.expr(c.string('Donor') + b'\x4d\x22\x01')))
        argument = c.expr(integer(41))
        init += event(0x17, [subentry(0x11c, u32(argument) + b'linked\0')])
        test = prop + integer(42) + equal
    elif label in ['named-arguments', 'code-target', 'padded-named-event', 'return-expression', 'return-void']:
        declaration = c.expr(b'\x62' + ref('argument') + b'\x07\x76\x07')
        locals_ = c.expr(b'\x61' + ref('temporary') + b'\x07')
        body = c.expr(ref('answer') + ref('argument') + integer(1) + b'\x6e\x14' + assign)
        other = c.expr(ref('answer') + integer(999) + assign)
        entries = [subentry(0x1d, u32(x)) for x in [declaration, locals_, body]]
        if label == 'padded-named-event':
            entries.append(subentry(0x11c, u32(0) + b'stub\0\0'))
            init += event(0x1c, [subentry(0x1d, u32(c.expr(ref('called') + integer(1) + assign)))], 'stub')
        if label in ['return-expression', 'return-void']:
            entries.append(subentry(0xfffc, u32(c.expr(integer(42))) if label == 'return-expression' else b''))
            entries.append(subentry(0x1d, u32(other)))
        init += event(0x1c, entries, 'first')
        init += event(0x1c, [subentry(0x1d, u32(other))], 'second')
        argument = c.expr(integer(41))
        if label == 'code-target':
            target = c.expr(b'\x04')
            body = u16(0x11c) + u16(65535) + u16(65534) + u16(1) + u16(target) + u32(argument) + b'first\0'
            init += event(0x17, [u16(len(body)) + body])
        else:
            init += event(0x17, [subentry(0x11c, u32(argument) + b'first\0')])
        test = ref('answer') + integer(42) + equal
        if label == 'padded-named-event':
            test += ref('called') + integer(1) + equal + b'\x6d\x2a'
    elif label == 'invalid-token':
        init = field(0x194, u32(c.expr(b'\xff')))
        expected_error = 'Unsupported V5 bytecode token ff'
        test = integer(1)
    elif label == 'assertion-fails':
        expected_error = 'Only the immediate SetPageFade(0) transition is supported'
        test = integer(0)
    else:
        raise ValueError(label)
    condition = c.expr(test)
    fail = c.expr(integer(1) + b'\x4d\x5e\x01')  # Nonzero fade must remain an explicit error.
    # True skips the failure; false takes it. The second branch tests null code.
    init += event(0x17, [subentry(0xfffb, u32(condition) + u16(2)), subentry(0xfffb, u32(0) + u16(3)), subentry(0x1d, u32(fail))])
    item = struct.pack('>4h4H', 0, 0, 1, 1, 3, 0, 901, len(init)) + b'Probe\0' + init + extra_items
    data = archive([('BCOD', 900, c.resource()), ('BITL', 900, item), ('VRSN', 900, u16(5)), ('tPAL', 900, u16(0) + u16(256) + bytes(1024))])
    return data, expected_error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--extras', type=Path, required=True)
    parser.add_argument('--game-data', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    args = parser.parse_args()
    # The native CLI compares the profile's asset identity to the launch path.
    # Normalize before writing either so relative invocations are reproducible.
    args.exe = args.exe.resolve()
    args.extras = args.extras.resolve()
    args.game_data = args.game_data.resolve()
    args.root = args.root.resolve()
    args.root.mkdir(parents=True, exist_ok=False)
    rows = []
    for label in ['arithmetic', 'real-arithmetic', 'literal-string', 'constant', 'property', 'list-index', 'list-literal', 'nested-index', 'logical', 'post-decrement', 'named-arguments', 'code-target', 'padded-named-event', 'return-expression', 'return-void', 'script-link', 'invalid-token', 'assertion-fails']:
        assets = args.root / label / 'assets'
        assets.mkdir(parents=True)
        shutil.copyfile(args.game_data / 'Outline.txt', assets / 'Outline.txt')
        page = assets / 'ACME Support/Cache/StartUp.MHK'
        page.parent.mkdir(parents=True)
        data, expected_error = fixture(label)
        page.write_bytes(data)
        c = Client(args.exe, args.root / label / 'native', args.extras)
        row = {'case': label}
        try:
            (c.root / 'profile.ini').write_text('[scummvm]\nenable_unsupported_game_warning=false\nconsole=false\nconfirm_exit=false\nautosave_period=0\nce_link_token=\n\n[cli-game]\nengineid=mohawk\ngameid=carmentq\nplatform=windows\npath=' + str(assets) + '\ncheck_gamedata=false\n')
            c.command('launch', engine='mohawk', game='carmentq', platform='windows', path=str(assets), timeout=20)
            deadline = time.monotonic() + 1.5
            while c.process.poll() is None and time.monotonic() < deadline:
                time.sleep(.05)
            alive = c.process.poll() is None
            if alive:
                row['inspect'] = c.command('inspect')
                c.close()
            else:
                c.errors.close()
            log = (args.root / label / 'native/engine.log').read_text(errors='replace')
            row['passed'] = alive if expected_error is None else not alive and expected_error in log
            row['log_tail'] = log.splitlines()[-5:]
        except Exception as exc:
            row.update(passed=False, exception=str(exc))
            c.abort()
        rows.append(row)
        print(label, 'PASS' if row['passed'] else 'FAIL', flush=True)
    report = {'exe_sha256': hashlib.sha256(args.exe.read_bytes()).hexdigest(), 'cases': rows}
    (args.root / 'results.json').write_text(json.dumps(report, indent=2))
    raise SystemExit(0 if all(row['passed'] for row in rows) else 1)


if __name__ == '__main__':
    main()
