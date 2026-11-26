# SPDX-License-Identifier: GPL-3.0-or-later
"""Recover reproducible step-2 evidence metadata from pinned ThinkQuick PE32.

Does not execute the original interpreter or implement VM behavior. The default
output contains table addresses, values and evidence-region hashes, not code.
Optional disassembly is for local research and requires Capstone.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

from inspect_mohawk_v5_reference import inspect


REGIONS = {
    'literal_signed_short': (0x401946, 0x401968),
    'value_numeric_predicate': (0x40fdb1, 0x40fe5d),
    'value_to_real': (0x40fe5d, 0x40ff69),
    'value_to_integer': (0x40ffa9, 0x41007c),
    'value_to_string': (0x40f869, 0x40fd79),
    'value_storage_identity': (0x40f382, 0x40f521),
    'value_compare': (0x40f559, 0x40f7d4),
    'value_copy_assignment': (0x401f60, 0x401f9b),
    'value_initialize': (0x40f1b6, 0x40f2a9),
    'value_release': (0x40eed4, 0x40f030),
    'value_reset': (0x410f40, 0x410f62),
    'reference_create': (0x40f176, 0x40f1b4),
    'reference_target': (0x4107b7, 0x4107e5),
    'reference_unwrap_once': (0x4107e5, 0x410812),
    'list_retain_release': (0x416e7e, 0x416ebf),
    'list_resize': (0x4168fc, 0x41697d),
    'list_element': (0x416a83, 0x416abc),
    'eval_index_and_property': (0x403d71, 0x403f93),
    'eval_entry_reentrant_result': (0x40375e, 0x403ac0),
    'eval_dynamic_invoke': (0x40498c, 0x404d70),
    'eval_build_arguments_63': (0x404d70, 0x404dda),
    'eval_bind_arguments_62': (0x404dda, 0x404eca),
    'eval_declare_locals_61': (0x404eca, 0x404f33),
    'eval_nested_expression_64': (0x404f33, 0x404f7b),
    'identifier_resolve': (0x4035e5, 0x40363a),
    'symbol_lookup': (0x4149a0, 0x414b81),
    'symbol_cache_lookup': (0x414b81, 0x414c47),
    'symbol_frame_push_pop': (0x414c47, 0x414d85),
    'call_argument_frame_push': (0x40e490, 0x40e510),
    'call_argument_frame_pop': (0x40e650, 0x40e740),
    'script_named_call_sender': (0x4089f3, 0x408b2a),
    'target_resolve': (0x40347d, 0x40357e),
    'target_direct_dispatch': (0x4068f0, 0x406965),
    'target_list_dispatch': (0x406965, 0x406c3d),
    'eval_selection_72': (0x404f7b, 0x405067),
    'eval_negation': (0x404053, 0x404096),
    'numeric_negation': (0x40f114, 0x40f176),
    'logical_and': (0x405ef0, 0x405f40),
    'logical_or': (0x405f40, 0x405f90),
    'general_lbxfunc': (0x40b417, 0x40b6ae),
    'general_scriptlink': (0x40c9aa, 0x40cad6),
    'item_scriptlink_field': (0x419d08, 0x419eb2),
    'item_initial_code_field': (0x41a04c, 0x41a069),
    'item_initial_code_helper': (0x41d38d, 0x41d3e2),
    'page_code_eval_bridge': (0x41fdf2, 0x41fe16),
    'item_script_event_entry': (0x41e14e, 0x41e174),
    'item_script_dispatch_links': (0x41e174, 0x41e4f8),
    'object_named_dispatch': (0x448d61, 0x448f81),
    'object_common_dispatch': (0x448f81, 0x4490f7),
    'datafile_numeric_dispatch': (0x44652b, 0x446ac3),
    'datafile_open': (0x44780b, 0x447936),
    'datafile_open_readonly': (0x447936, 0x447ad6),
    'datafile_close': (0x447ad6, 0x447b01),
    'datafile_delete': (0x447b01, 0x447b29),
    'datafile_write_through': (0x447baa, 0x447c34),
    'clock_calendar': (0x449583, 0x449659),
    'stringlib_numeric_dispatch': (0x444eb3, 0x4452a0),
    'directory_numeric_dispatch': (0x43feca, 0x440298),
    'database_numeric_dispatch': (0x43b06c, 0x43bb60),
    'selector_numeric_dispatch': (0x4406f1, 0x440c1c),
    'history_numeric_dispatch': (0x4455bd, 0x445a0d),
    'encryptor_numeric_dispatch': (0x447e73, 0x4480f5),
    'font_numeric_dispatch': (0x448897, 0x448a60),
}


class Image:
    def __init__(self, data):
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        count, optional_size = struct.unpack_from('<H12xH', data, pe + 6)
        optional = pe + 24
        self.base = struct.unpack_from('<I', data, optional + 28)[0]
        size = struct.unpack_from('<I', data, optional + 56)[0]
        self.memory = bytearray(size)
        for i in range(count):
            _, rva, length, offset = struct.unpack_from('<4I', data, optional + optional_size + 40 * i + 8)
            self.memory[rva:rva + length] = data[offset:offset + length]

    def read(self, address, length):
        offset = address - self.base
        if offset < 0 or offset + length > len(self.memory):
            raise ValueError('Evidence address outside image')
        return bytes(self.memory[offset:offset + length])

    def u32(self, address):
        return struct.unpack('<I', self.read(address, 4))[0]

    def cstring(self, address):
        end = self.memory.index(0, address - self.base)
        return self.read(address, end - (address - self.base)).decode('ascii')


def recover(path):
    reference = inspect(path)  # Reject unrecognized images before address use.
    image = Image(path.read_bytes())
    result = {'schema': 1, 'exe_sha256': reference['exe_sha256'],
              'evidence_kind': 'static original-executable observations; no runtime equivalence claim',
              'regions': {}, 'value_tags': {}, 'components': {}}
    for name, (start, end) in REGIONS.items():
        result['regions'][name] = {'start': hex(start), 'end_exclusive': hex(end),
                                   'sha256': hashlib.sha256(image.read(start, end - start)).hexdigest()}
    for tag in range(16):
        result['value_tags'][str(tag)] = {
            'to_real_handler': hex(image.u32(0x40ff69 + 4 * tag)),
            'initialize_handler': hex(image.u32(0x40f2a9 + 4 * (tag - 7))) if 7 <= tag <= 15 else 'raw payload',
            'release_handler': hex(image.u32(0x40f030 + 4 * (tag - 7))) if 7 <= tag <= 14 else 'reset only'}
    result['numeric_string_formats'] = {hex(a): image.cstring(a) for a in (0x49379c, 0x4937a4, 0x4937a8, 0x4937b0, 0x4937b4, 0x4937bc)}
    result['numeric_constants'] = {hex(a): struct.unpack('<d', image.read(a, 8))[0]
                                   for a in (0x4885e0, 0x488700, 0x488708, 0x488710)}
    result['item_field_handlers'] = {hex(0x192 + n): hex(image.u32(0x41d232 + 4 * n)) for n in range(5)}
    # Derive each numeric virtual method via the wrapper that references its
    # named-method table. The pinned image has one unambiguous wrapper/vtable.
    for name, table in reference['component_method_tables'].items():
        table_address = int(table['address'], 16)
        needle = b'\x68' + struct.pack('<I', table_address)
        xref = image.memory.find(needle)
        if xref < 0 or image.memory.find(needle, xref + 1) >= 0:
            raise ValueError(f'Ambiguous method-table xref: {name}')
        wrapper_offset = image.memory.rfind(b'\x55\x8b\xec', max(0, xref - 100), xref)
        if wrapper_offset < 0:
            raise ValueError(f'No named wrapper prologue: {name}')
        named = image.base + wrapper_offset
        locations = []
        needle = struct.pack('<I', named)
        offset = 0
        while True:
            offset = image.memory.find(needle, offset)
            if offset < 0:
                break
            if offset >= 8:
                destructor, numeric = struct.unpack_from('<II', image.memory, offset - 8)
                if 0x401000 <= numeric < 0x488000:
                    locations.append((image.base + offset - 8, numeric, destructor))
            offset += 1
        if len(locations) != 1:
            raise ValueError(f'Ambiguous component vtable: {name}')
        vtable, numeric, destructor = locations[0]
        constructor_names = []
        offset = 0
        while True:
            offset = image.memory.find(struct.pack('<I', vtable), offset)
            if offset < 0:
                break
            if 0x401000 <= image.base + offset < 0x488000:
                # Following the constructor's vtable store, the pinned image
                # initializes its descriptive class name with this sequence.
                match = re.search(rb'\x68\xff\xff\x00\x00\x68(.{4})',
                                  bytes(image.memory[offset + 4:offset + 104]), re.S)
                if match:
                    address = struct.unpack('<I', match[1])[0]
                    label = image.cstring(address)
                    if label.lower() == ('lbx' + name).lower():
                        constructor_names.append({'vtable_store_operand': hex(image.base + offset),
                                                  'class_name_address': hex(address), 'class_name': label})
            offset += 1
        if len(constructor_names) != 1:
            raise ValueError(f'Class affiliation not uniquely confirmed: {name}')
        result['components'][name] = {'method_table': table['address'], 'table_xref': hex(image.base + xref),
                                      'vtable': hex(vtable), 'numeric_dispatch': hex(numeric),
                                      'named_dispatch': hex(named), 'destructor': hex(destructor),
                                      'constructor_identity': constructor_names[0]}
    arities = {0x4465fa: 0, 0x44660a: 1, 0x44661b: 2}
    result['datafile_methods'] = {}
    for method in reference['component_method_tables']['datafile']['methods']:
        ident = method['id']
        arity_handler = image.u32(0x446a33 + 4 * (ident - 1))
        if arity_handler not in arities:
            raise ValueError('Unrecognized datafile argument validator')
        result['datafile_methods'][str(ident)] = {
            'name': method['name'], 'arguments_excluding_receiver_and_selector': arities[arity_handler],
            'arity_handler': hex(arity_handler), 'operation_handler': hex(image.u32(0x446a7b + 4 * (ident - 1)))}
    return result, image


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--disassembly-root', type=Path, help='New local-only evidence directory')
    parser.add_argument('--module-dir', type=Path, help='Existing directory containing Capstone, if needed')
    args = parser.parse_args()
    result, image = recover(args.exe)
    if args.disassembly_root:
        if args.module_dir:
            sys.path.insert(0, str(args.module_dir.resolve()))
        import capstone
        decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        args.disassembly_root.mkdir(parents=True, exist_ok=False)
        for name, (start, end) in REGIONS.items():
            lines = [f'{i.address:08x} {i.mnemonic:8} {i.op_str}' for i in decoder.disasm(image.read(start, end - start), start)]
            (args.disassembly_root / (name + '.txt')).write_text('\n'.join(lines) + '\n', encoding='utf-8')
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
