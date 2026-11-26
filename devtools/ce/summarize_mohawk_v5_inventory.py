# SPDX-License-Identifier: GPL-3.0-or-later
"""Produce shareable metadata and tables from a local ThinkQuick inventory.

Excludes game scripts, binary payloads and arbitrary component argument values.
Preserves call offsets, receiver identifiers, selector IDs and counts for audit.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path


def scope(path):
    if '/Questions/' in path:
        return 'question-content'
    if path.startswith('Custom Questions/'):
        return 'custom-editor'
    if path.endswith('/Startup.MHK'):
        return 'startup'
    if path.endswith('/Main.MHK'):
        return 'main-core'
    return 'other-runtime'


def summarize(report):
    tokens, commands, events, operations, fields, kinds = (Counter() for _ in range(6))
    examples = defaultdict(list)
    counts = Counter()
    scopes = defaultdict(Counter)
    components, invocations, bindings, anomalies = [], [], [], []
    for archive in report['archives']:
        path = archive['path']
        group = scope(path)
        events.update(archive.get('events', {}))
        operations.update(archive.get('script_operations', {}))
        fields.update(archive.get('fields', {}))
        kinds.update(archive.get('item_types', {}))
        for issue in archive.get('resource_errors', []):
            anomalies.append({'archive': path, **issue})
        if 'error' in archive:
            anomalies.append({'archive': path, 'error': archive['error']})
        for ident, code in archive.get('bytecode_lexical', {}).items():
            if code['errors'] or code['invalid_roots']:
                raise ValueError(f'Incomplete lexical inventory: {path}:{ident}')
            counts['bcod_resources'] += 1
            counts['code_bytes'] += code['code_bytes']
            counts['decoded_bytes'] += code['decoded_bytes']
            tokens.update(code['tokens'])
            commands.update(code['commands'])
            scopes[group]['code_bytes'] += code['code_bytes']
            scopes[group]['native_calls'] += sum(code['commands'].values())
            for site in code['call_sites']:
                key = site['command']
                if len(examples[key]) < 3:
                    examples[key].append({'archive': path, 'bcod': ident, 'offset': site['offset']})
            bindings += [{'archive': path, 'bcod': ident, **binding} for binding in code['component_bindings']]
            for site in code['components']:
                loc = {'archive': path, 'bcod': ident, 'offset': site['offset'], 'scope': group}
                if site['kind'] == 'invoke':
                    receiver = site['receiver']
                    # Literal callable names and object-valued receivers take
                    # different branches in original 0x40498c.
                    literal = receiver.get('value')
                    method = site['args'][0].get('value') if site['args'] else None
                    invocations.append({**loc, 'receiver': receiver.get('ref'),
                                        'literal_callable': literal if isinstance(literal, str) else None,
                                        'receiver_result_of_invoke': receiver.get('result_of_invoke'),
                                        'first_argument_selector': method if isinstance(method, (str, int)) else None,
                                        'argc': len(site['args'])})
                    scopes[group]['dynamic_invocations'] += 1
                else:
                    args = site['args']
                    row = {**loc, 'kind': site['kind'], 'argc': site['argc']}
                    if site['kind'] == 'create':
                        row['type_selector'] = args[0].get('value') if args else None
                    else:
                        row['receiver'] = args[0].get('ref') if args else None
                        row['method_id'] = args[1].get('value') if len(args) > 1 else None
                    components.append(row)
                    scopes[group]['component_' + site['kind']] += 1
    reference = report['original_reference']
    token_rows = {key: {'count': n, **reference['tokens'][key]} for key, n in sorted(tokens.items())}
    command_rows = {}
    for key, n in sorted(commands.items()):
        row = {'count': n, **report['ce_command_table'].get(key, {'name': None, 'handler_present': False}),
               'examples': examples[key]}
        if key.startswith('0x4d:'):
            row['original_handler'] = reference['general_command_handlers'].get(key[-4:])
        command_rows[key] = row
    return {'schema': 1, 'scope': 'Complete declared BCOD lexical inventory; static presence, not runtime reachability',
            'ce_baseline_source': report.get('ce_baseline_source'),
            'data_manifest_sha256': hashlib.sha256(json.dumps(report['files'], sort_keys=True).encode()).hexdigest(),
            'totals': {**report['totals'], **counts}, 'scopes': dict(sorted(scopes.items())),
            'original_reference': reference, 'tokens': token_rows, 'commands': command_rows,
            'item_types': dict(sorted(kinds.items())), 'item_fields': dict(sorted(fields.items())),
            'events': dict(sorted(events.items())), 'script_operations': dict(sorted(operations.items())),
            'component_sites': components, 'component_bindings': bindings,
            'dynamic_invocations': invocations, 'resource_exceptions': anomalies}


def markdown(summary):
    lines = ['# ThinkQuick step-1 inventory tables', '',
             'Generated metadata; no original interpreter execution or game-support claim.', '',
             'Counts cover the extracted installation including Custom Questions. Every declared',
             'BCOD byte is decoded using the pinned original token reader; expression roots align.',
             'Operator bytes are tokens here, so these counts differ from the old prefix scanner.', '',
             '## Opcodes', '', '| Token | Count | Inline operand | Original evaluator handler |',
             '| --- | ---: | --- | --- |']
    for key, row in summary['tokens'].items():
        lines.append(f"| {key} | {row['count']} | {row['operand']} | {row['execution_handler']} |")
    lines += ['', '## Native command IDs', '',
              'Names are CE table labels. A handler being present does not establish v5 semantics.',
              'Namespaces 0x4f (notify), 0x70 (collection), 0x71 (rectangle) lack v5 dispatch.', '',
              '| Namespace:ID | CE label | Count | CE handler | First archive:BCOD offset |',
              '| --- | --- | ---: | --- | --- |']
    for key, row in summary['commands'].items():
        example = row['examples'][0]
        status = 'stub' if row.get('explicit_stub') else 'present' if row['handler_present'] else 'missing'
        lines.append(f"| {key} | {row.get('name') or 'unmapped'} | {row['count']} | {status} | {example['archive']}:{example['offset']} |")
    lines += ['', '## Component constructors', '',
              '| Selector | Original registered class | Sites | First archive:BCOD offset |',
              '| --- | --- | ---: | --- |']
    classes = summary['original_reference']['component_factory']['classes']
    grouped = defaultdict(list)
    for site in summary['component_sites']:
        if site['kind'] == 'create':
            grouped[str(site['type_selector'])].append(site)
    for selector, sites in sorted(grouped.items()):
        klass = next((c['name'] for c in classes if str(c['id']) == selector or c['name'].lower() == selector.lower()), 'unmapped')
        lines.append(f"| {selector} | {klass} | {len(sites)} | {sites[0]['archive']}:{sites[0]['offset']} |")
    lines += ['', '## Original component method tables', '',
              'Name/ID pairs are read directly from the pinned original executable. Table/class',
              'affiliations are the initial interface map; argument, return and storage contracts',
              'still require step-2 recovery and step-3 probes. All tables include uncalled methods.', '']
    for name, table in summary['original_reference']['component_method_tables'].items():
        methods = ', '.join(f"{m['id']}={m['name']}" for m in table['methods'])
        lines += [f"- **{name}**, table {table['address']}: {methods}"]
    lines += ['', '## Static scope partition', '',
              'These are asset groups, not proven user-flow reachability. Main and other-runtime',
              'contain code shared by menus, rounds and persistence.', '',
              '| Asset group | Code bytes | Native calls | Dynamic invokes | Constructors | Numeric method calls |',
              '| --- | ---: | ---: | ---: | ---: | ---: |']
    for key, c in summary['scopes'].items():
        lines.append(f"| {key} | {c.get('code_bytes', 0)} | {c.get('native_calls', 0)} | {c.get('dynamic_invocations', 0)} | {c.get('component_create', 0)} | {c.get('component_call', 0)} |")
    lines += ['', 'All constructor, numeric-method and dynamic-call offsets are retained in',
              '`THINKQUICK-INVENTORY.json`, together with item/event/field/script-operation counts.',
              'Late-bound receivers and script callable names are kept distinct; no guessed',
              'receiver type is promoted to a confirmed component requirement.', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--json', type=Path, required=True)
    parser.add_argument('--markdown', type=Path, required=True)
    args = parser.parse_args()
    summary = summarize(json.loads(args.input.read_text()))
    with args.json.open('x', encoding='utf-8') as stream:
        json.dump(summary, stream, indent=2)
        stream.write('\n')
    with args.markdown.open('x', encoding='utf-8') as stream:
        stream.write(markdown(summary))
