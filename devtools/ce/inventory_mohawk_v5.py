# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only ThinkQuick structural inventory; emits identities and counts, not scripts.

Byte frequencies in BCOD are deliberately not reported as opcode coverage:
operands, literals and padding must first be decoded with a verified grammar.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct


def resources(data, issues=None):
    if data[:4] != b'MHWK':
        raise ValueError('Not a Mohawk archive')
    base = struct.unpack_from('>I', data, 20)[0]
    table = base + struct.unpack_from('>H', data, 24)[0]
    files = []
    invalid_files = {}
    for i in range(struct.unpack_from('>I', data, table)[0]):
        offset, low, high, flags, _ = struct.unpack_from('>IHBBH', data, table + 4 + 10 * i)
        size = low + (high << 16) + ((flags & 7) << 24)
        if offset + size > len(data):
            error = f'File table entry {i + 1}: offset {offset}, size {size}, archive size {len(data)}'
            if issues is None:
                raise ValueError(error)
            invalid_files[i + 1] = {'offset': offset, 'claimed_size': size, 'archive_size': len(data)}
            files.append(None)
        else:
            files.append(data[offset:offset + size])
    for i in range(struct.unpack_from('>H', data, base + 2)[0]):
        tag, offset, _ = struct.unpack_from('>4sHH', data, base + 4 + 8 * i)
        start = base + offset
        for j in range(struct.unpack_from('>H', data, start)[0]):
            ident, index = struct.unpack_from('>HH', data, start + 2 + 4 * j)
            if not 1 <= index <= len(files):
                raise ValueError('Invalid resource file index')
            if files[index - 1] is None:
                issues.append({'tag': tag.decode('latin1'), 'id': ident, 'file_index': index,
                               **invalid_files[index],
                               'error': 'resource extends beyond archive; not decoded'})
                continue
            yield tag.decode('latin1'), ident, files[index - 1]


def items(data):
    position = 0
    while position < len(data):
        kind, resource, ident, size = struct.unpack_from('>4H', data, position + 8)
        start = data.index(0, position + 16) + 1
        end = start + size
        if end > len(data):
            raise ValueError('Item extends beyond BITL')
        fields = []
        while start < end:
            field, length = struct.unpack_from('>HH', data, start)
            if start + 4 + length > end:
                raise ValueError('Field extends beyond item')
            fields.append((field, data[start + 4:start + 4 + length]))
            start += 4 + length
        yield kind, resource, ident, fields
        position = end


def script_refs(entry):
    """BITL list subentry, excluding event/length word; follows checkpoint grammar."""
    opcode, _, argc = struct.unpack_from('>3H', entry)
    cursor = 6
    refs = []
    if argc in (0xfffe, 0xffff, 0x3f3f):
        count = struct.unpack_from('>H', entry, cursor)[0]
        cursor += 2
        if argc == 0xfffe:
            refs += list(struct.unpack_from('>' + 'H' * count, entry, cursor))
            cursor += 2 * count
        else:
            for _ in range(count):
                cursor = entry.index(0, cursor) + 1
            cursor += cursor % 2
    else:
        cursor += 4 * argc
    if opcode in (0x1d, 0x11c, 0xfffb, 0xfffd) or (opcode == 0xfffc and len(entry) - cursor >= 4):
        refs.append(struct.unpack_from('>I', entry, cursor)[0])
    return refs


def decode_prefixes(payload, refs):
    """Inventory only verified-size prefixes; unknown tokens terminate each root.

    Grammar is the provisional CE evaluator, not an original-interpreter spec.
    Does not evaluate values, infer reachability, or resynchronize after errors.
    """
    total, size = struct.unpack_from('>II', payload)
    if total != len(payload) or size + 8 > total:
        return {'error': 'BCOD size mismatch'}
    code = payload[8:8 + size]
    tokens, commands, operators = Counter(), Counter(), Counter()
    stops = []
    complete = 0
    for root in sorted(set(refs) - {0}):
        pos = root
        declarations = False
        try:
            while True:
                start = pos
                token = code[pos]
                pos += 1
                tokens[f'0x{token:02x}'] += 1
                if token == 8:
                    complete += 1
                    break
                if token in (0x61, 0x62):
                    declarations = True
                elif token in (1, 6, 0x31):
                    pos += 2
                elif token == 5:
                    kind = code[pos]
                    pos += 1
                    length = {2: 2, 3: 4, 4: 8}.get(kind)
                    if length is None:
                        raise ValueError(f'unknown literal kind 0x{kind:02x}')
                    pos += length
                elif token in (0x4d, 0x4e):
                    command, argc = code[pos:pos + 2]
                    commands[f'0x{token:02x}:0x{command:02x}'] += 1
                    pos += 2
                elif token in (0x6b, 0x6c, 0x6d, 0x6e, 0x6f):
                    operators[f'0x{token:02x}:0x{code[pos]:02x}'] += 1
                    pos += 1
                elif token not in (4, 7, 0x1a, 0x1b, 0x20, 0x39, 0x3a) and not (declarations and token == 0x76):
                    raise ValueError(f'unknown token 0x{token:02x}')
                if pos > len(code):
                    raise ValueError('truncated operand')
        except (ValueError, IndexError) as exc:
            stops.append({'root': root, 'offset': start, 'reason': str(exc)})
    return {'grammar': 'provisional CE checkpoint; prefix-only, not semantic validation',
            'code_bytes': size, 'roots': len(set(refs) - {0}), 'complete_roots': complete,
            'tokens': dict(sorted(tokens.items())), 'commands': dict(sorted(commands.items())),
            'operators': dict(sorted(operators.items())), 'stops': stops}


def command_table():
    source = (Path(__file__).resolve().parents[2] / 'engines/mohawk/livingbooks_code.cpp').read_text()
    result = {}
    for token, name in ((0x4d, 'general'), (0x4e, 'item')):
        body = source.split(f'CodeCommandInfo {name}CommandInfo[', 1)[1].split('};', 1)[0]
        for index, (label, handler) in enumerate(re.findall(r'\{\s*("[^"]+"|nullptr)\s*,\s*([^}]+)\}', body), 1):
            result[f'0x{token:02x}:0x{index:02x}'] = {
                'name': None if label == 'nullptr' else label.strip('"'),
                'handler_present': handler.strip() != 'nullptr',
                'explicit_stub': 'cmdUnimplemented' in handler}
    return result


def lexical_inventory(payload, reference):
    """Decode every declared code byte using original-reader operand lengths.

    The small symbolic stack only extracts syntactically direct component-call
    arguments. It never follows branches or substitutes runtime variable values.
    """
    total, size = struct.unpack_from('>II', payload)
    if total != len(payload) or size + 8 > total:
        raise ValueError('BCOD size mismatch')
    code = payload[8:8 + size]
    strings = {}
    cursor = 8 + size
    string_base = cursor
    while cursor + 2 <= len(payload):
        if payload[cursor:cursor + 2] != b'\0\0':
            break
        end = payload.index(0, cursor + 2)
        strings[cursor - string_base] = payload[cursor + 2:end].decode('latin1')
        cursor = end + 1
    tokens, commands, literals = Counter(), Counter(), Counter()
    sites, components, bindings, errors = [], [], [], []
    boundaries = set()
    stack = []
    list_starts = []
    invocations = []
    pos = 0
    unknown = {'unknown': True}

    def pop():
        return stack.pop() if stack else unknown

    while pos < size:
        start = pos
        boundaries.add(start)
        token = code[pos]
        pos += 1
        key = f'0x{token:02x}'
        recipe = reference['tokens'][key]['operand']
        tokens[key] += 1
        operand = None
        try:
            if recipe in ('word', 'command_word'):
                operand = struct.unpack_from('>H', code, pos)[0]
                pos += 2
            elif recipe == 'literal':
                kind = code[pos]
                pos += 1
                literals[str(kind)] += 1
                if kind == 2:
                    ident = struct.unpack_from('>H', code, pos)[0]
                    operand = strings.get(ident)
                    pos += 2
                elif kind == 3:
                    operand = struct.unpack_from('>i', code, pos)[0]
                    pos += 4
                elif kind == 4:
                    operand = struct.unpack_from('>I', code, pos)[0] / reference['real_literal']['divisor']
                    pos += reference['real_literal']['bytes']
                else:
                    raise ValueError(f'unsupported original literal subtype {kind}')
            if pos > size:
                raise ValueError('operand extends beyond declared code')
        except (ValueError, IndexError, struct.error) as exc:
            errors.append({'offset': start, 'error': str(exc)})
            break
        if token == 1:
            stack.append({'ref': strings.get(operand, f'<string:{operand}>')})
        elif token == 6:
            stack.append({'value': strings.get(operand)})
        elif token in (5, 0x31):
            stack.append({'value': operand})
        elif token == 4:
            stack.append({'ref': 'self'})
        elif token == 0x66:
            stack.append({'ref': 'sender-context-unqualified'})
        elif token in (0x39, 0x3a):
            stack.append({'value': token == 0x39})
        elif recipe == 'command_word':
            command, argc = operand >> 8, operand & 255
            call_key = f'{key}:0x{command:02x}'
            commands[call_key] += 1
            args = [pop() for _ in range(argc)][::-1]
            site = {'offset': start, 'command': call_key, 'argc': argc}
            sites.append(site)
            result = unknown
            if token == 0x4d and command in (0x57, 0x58):
                component = {**site, 'kind': 'create' if command == 0x57 else 'call', 'args': args}
                components.append(component)
                if command == 0x57 and args:
                    result = {'component_type': args[0].get('value'), 'creation_offset': start}
            stack.append(result)
        elif token == 0x20:
            name, owner = pop(), pop()
            stack.append({'ref': str(owner.get('ref', '?')) + '.' + str(name.get('value', name.get('ref', '?')))})
        elif token in (0x6b, 0x6c, 0x6d, 0x6e, 0x6f):
            # The following operator is a separate token in the original reader.
            operator = code[pos] if pos < size else None
            rhs = pop()
            lhs = pop() if token not in (0x6f,) and not (token == 0x6d and operator == 0x35) else unknown
            if token == 0x6b and operator == 0x22 and 'component_type' in rhs:
                bindings.append({'offset': start, 'receiver': lhs.get('ref'), **rhs})
            stack.append(rhs if token == 0x6b and operator == 0x22 else unknown)
        elif token == 0x67:
            value = pop()
            number = value.get('value')
            stack.append({'value': -number} if isinstance(number, (int, float)) else unknown)
        elif token == 0x77:
            invocations.append({'offset': start, 'kind': 'invoke', 'receiver': pop(),
                                'args': [], 'stack_base': len(stack)})
        elif token == 0x10 and invocations:
            call = invocations.pop()
            stack[:] = stack[:call.pop('stack_base')]
            components.append(call)
            stack.append({'result_of_invoke': call['offset']})
        elif token == 7 and invocations:
            base = invocations[-1]['stack_base']
            if len(stack) > base:
                invocations[-1]['args'].append(pop())
        elif token in (0, 7, 8, 0x61, 0x62, 0x63, 0x72, 0x73, 0x76) and not list_starts:
            stack.clear()
        elif token == 0x1a:
            list_starts.append(len(stack))
        elif token == 0x1b:
            if list_starts:
                base = list_starts.pop()
                stack[:] = stack[:base]
                stack.append(unknown)
            else:
                index, owner = pop(), pop()
                stack.append({'ref': str(owner.get('ref', '?')) + '[' + str(index.get('value', index.get('ref', '?'))) + ']'})
    if invocations:
        errors.append({'error': 'unclosed dynamic invocation', 'offsets': [c['offset'] for c in invocations]})
    return {'grammar': 'original 0x4018ab token reader; all declared code, no reachability claim',
            'code_bytes': size, 'decoded_bytes': pos if not errors else start,
            'tokens': dict(sorted(tokens.items())), 'literal_types': dict(sorted(literals.items())),
            'commands': dict(sorted(commands.items())), 'call_sites': sites,
            'components': components, 'component_bindings': bindings, 'errors': errors}, boundaries


def inventory(root, reference=None, ce_baseline=None):
    report = {'schema': 2, 'scope': 'Static resource and lexical opcode inventory; not runtime coverage or semantic validation',
              'files': [], 'archives': [], 'ce_command_table': ce_baseline['ce_command_table'] if ce_baseline else command_table()}
    if ce_baseline:
        report['ce_baseline_source'] = ce_baseline['source_identity']
    if reference:
        report['original_reference'] = reference
    totals = Counter()
    for path in sorted(p for p in root.rglob('*') if p.is_file()):
        data = path.read_bytes()
        name = path.relative_to(root).as_posix()
        report['files'].append({'path': name, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        if path.suffix.lower() != '.mhk':
            continue
        tags, kinds, fields, events, operations = (Counter() for _ in range(5))
        bases = []
        refs = []
        resource_issues = []
        try:
            archive_resources = list(resources(data, resource_issues))
        except (ValueError, struct.error, IndexError) as exc:
            report['archives'].append({'path': name, 'error': str(exc)})
            continue
        for tag, ident, payload in archive_resources:
            tags[tag] += 1
            if tag != 'BITL':
                continue
            bases.append(ident)
            for kind, _, _, entries in items(payload):
                kinds[f'0x{kind:04x}'] += 1
                totals['items'] += 1
                for field, value in entries:
                    fields[f'0x{field:04x}'] += 1
                    if field == 0x194:
                        refs.append(struct.unpack_from('>I', value)[0])
                    if field == 0x66:
                        event, opcode, param = struct.unpack_from('>3H', value)
                        events[f'0x{event:04x}'] += 1
                        if opcode == 0xfffe:
                            cursor = 6
                            for i in range(param):
                                length = struct.unpack_from('>H', value, cursor)[0]
                                cursor += 2
                                if length < 6 or cursor + length > len(value):
                                    raise ValueError('Invalid script subentry')
                                operations[f'0x{struct.unpack_from(">H", value, cursor)[0]:04x}'] += 1
                                refs += script_refs(value[cursor:cursor + length])
                                cursor += length
                                if i + 1 < param:
                                    cursor += cursor % 2
                        else:
                            operations[f'0x{opcode:04x}'] += 1
                            refs += script_refs(value[2:])
        bytecode = {str(ident): decode_prefixes(payload, refs)
                    for tag, ident, payload in archive_resources if tag == 'BCOD'}
        lexical = {}
        if reference:
            for tag, ident, payload in archive_resources:
                if tag != 'BCOD':
                    continue
                decoded, boundaries = lexical_inventory(payload, reference)
                decoded['invalid_roots'] = sorted(set(refs) - boundaries - {0})
                lexical[str(ident)] = decoded
        report['archives'].append({'path': name, 'page_bases': bases, 'resources': dict(sorted(tags.items())),
                                   'item_types': dict(sorted(kinds.items())), 'fields': dict(sorted(fields.items())),
                                   'events': dict(sorted(events.items())), 'script_operations': dict(sorted(operations.items())),
                                   'bytecode_prefixes': bytecode, 'bytecode_lexical': lexical,
                                   'resource_errors': resource_issues})
    report['totals'] = {'files': len(report['files']), 'archives': len(report['archives']),
                        'archive_errors': sum('error' in a or bool(a.get('resource_errors')) for a in report['archives']), **totals}
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reference', type=Path, help='Pinned original-token-dispatch.json')
    parser.add_argument('--ce-baseline', type=Path, help='Optional recorded CE command-table/source snapshot')
    args = parser.parse_args()
    if not args.game_data.is_dir():
        parser.error('--game-data must be an existing directory')
    reference = json.loads(args.reference.read_text()) if args.reference else None
    ce_baseline = json.loads(args.ce_baseline.read_text()) if args.ce_baseline else None
    report = inventory(args.game_data, reference, ce_baseline)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps(report['totals']))
