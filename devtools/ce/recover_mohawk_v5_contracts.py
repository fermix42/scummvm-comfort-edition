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
    'eval_operator_errors': (0x404053, 0x40498c),
    'eval_operator_branch_tables': (0x40522a, 0x405338),
    'value_increment_decrement': (0x40f050, 0x40f114),
    'comparison_predicates': (0x405d70, 0x405ef0),
    'key_compare': (0x40f7d4, 0x40f869),
    'value_to_key': (0x4100a2, 0x4101a9),
    'value_to_rectangle': (0x4101a9, 0x41052a),
    'value_to_point': (0x41052a, 0x4107b7),
    'packed_geometry_helpers': (0x40de40, 0x40dfb0),
    'packed_key_helpers': (0x44a67a, 0x44a79f),
    'key_event_value': (0x422050, 0x42206f),
    'page_key_events_and_match': (0x420144, 0x42041b),
    'general_key_consumers': (0x40a4ae, 0x40a639),
    'filesystem_callback_set': (0x453ef3, 0x453f05),
    'filesystem_callback_install': (0x4298de, 0x4298ff),
    'filesystem_error_retry': (0x454102, 0x45424a),
    'filesystem_error_callback': (0x4548ef, 0x454991),
    'filesystem_error_map': (0x454991, 0x454a65),
    'filesystem_open_operation': (0x454ab1, 0x454c2f),
    'filesystem_write_exact': (0x4554b8, 0x45559b),
    'filesystem_operation_retry': (0x4563d1, 0x456508),
    'filesystem_native_operations': (0x45652f, 0x4566ad),
    'filesystem_retry_dialog': (0x42db7e, 0x42de02),
    'dialog_native_result_mapping': (0x451550, 0x4516eb),
    'filesystem_initial_error_policy': (0x45424a, 0x4542b5),
    'filesystem_callback_scope': (0x4320e2, 0x432103),
    'filesystem_callback_scope_restore': (0x4321aa, 0x4321c0),
    'item_destroy': (0x4189c5, 0x418d09),
    'item_value_watchers': (0x418d09, 0x418e13),
    'value_item_invalidation': (0x40f2cd, 0x40f382),
    'script_entry_destroy': (0x41e056, 0x41e14e),
    'general_destroy_queued': (0x40735f, 0x4073c4),
    'target_queue_event': (0x406c48, 0x406d62),
    'page_queue_append': (0x4206e7, 0x420818),
    'page_queue_drain': (0x41fecb, 0x41fff1),
    'page_target_event': (0x420c02, 0x420c98),
    'item_delete_event_7': (0x41a500, 0x41a656),
    'resource_unload': (0x4268d3, 0x426945),
    'page_teardown': (0x421475, 0x4215a2),
    'vm_error_throw': (0x402fc6, 0x40347d),
    'eval_stack_reset_empty': (0x4061b0, 0x4061ec),
    'crt_real_to_int64': (0x477860, 0x477887),
    'list_insert': (0x4167b2, 0x4168fc),
    'selector_location_factory': (0x442530, 0x4425dc),
    'selector_shared_deck_lookup': (0x442660, 0x442710),
    'profile_shutdown_register': (0x42974a, 0x4297c5),
    'crt_onexit_register': (0x47791a, 0x4779aa),
    'crt_exit_callbacks': (0x4782f2, 0x478397),
    'file_exact_read': (0x45559b, 0x45567e),
    'database_destroy': (0x43afba, 0x43b032),
    'eval_exception_metadata': (0x48c470, 0x48c4dc),

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
    'page_load_entry': (0x42041b, 0x420520),
    'page_item_factory': (0x42620b, 0x426590),
    'page_resource_load_passes': (0x4265fd, 0x4268d3),
    'item_construct_register': (0x4185d0, 0x4189c5),
    'item_store_fields_event_5': (0x41a098, 0x41a0bd),
    'item_apply_fields_event_23': (0x41b336, 0x41b475),
    'item_phase_events_3a_3b': (0x41bbc9, 0x41bd63),
    'item_load_event_2f': (0x41b5d2, 0x41b6c7),
    'item_timer_phase_setup': (0x418e13, 0x418ecf),
    'proxy_event_dispatch': (0x42555f, 0x425872),
    'proxy_external_load': (0x4258e7, 0x425ba2),
    'database_open_or_create': (0x43bb60, 0x43be70),
    'database_setpaths': (0x43be70, 0x43beb4),
    'database_create_index': (0x43beb4, 0x43c535),
    'database_record_and_field': (0x43c535, 0x43c921),
    'database_deck_and_unit': (0x43c921, 0x43cd97),
    'database_deck_list_and_counts': (0x43cd97, 0x43d325),
    'database_unit_classification': (0x43d325, 0x43da12),
    'database_index_write': (0x43da12, 0x43e20e),
    'database_index_read': (0x43e20e, 0x43eb21),
    'database_index_endian_fields': (0x43eb21, 0x43eddf),
    'database_source_reads': (0x43edf7, 0x43f258),
    'database_text_and_list_fields': (0x43f258, 0x43f6ae),
    'selector_users_database_level': (0x440c1c, 0x440e29),
    'selector_constructor_defaults': (0x4404ba, 0x440636),
    'selector_subject_graph': (0x440e29, 0x4410f5),
    'selector_history_filter': (0x4410f5, 0x44136c),
    'selector_mission_build': (0x44136c, 0x441959),
    'selector_mission_reorder': (0x441959, 0x441b77),
    'selector_get_deck': (0x441b88, 0x441d84),
    'selector_pool_remove': (0x441e8b, 0x441fd8),
    'selector_pool_draw': (0x441fd8, 0x44218a),
    'selector_pool_load': (0x44218a, 0x4423e9),
    'selector_configuration': (0x4435f0, 0x4436b0),
    'selector_history_iterator': (0x4437a0, 0x443854),
    'history_state_and_files': (0x445a0d, 0x445d1e),
    'history_get_load_save': (0x445d1e, 0x446136),
    'history_add_and_deck_list': (0x446136, 0x44620b),
    'history_front_insert': (0x446270, 0x44629c),
    'history_sequence_reset': (0x446300, 0x44631f),
    'history_profile_open': (0x439680, 0x4397db),
    'history_profile_write': (0x4397db, 0x439941),
    'history_profile_read': (0x439b9d, 0x439bc9),
    'datafile_destructor': (0x446491, 0x4464f1),
    'datafile_sections': (0x446ac3, 0x446e57),
    'datafile_write_value': (0x446e57, 0x447188),
    'datafile_delete_read_value': (0x447188, 0x4472b8),
    'datafile_load_vars_section': (0x4472b8, 0x447642),
    'datafile_section_exists': (0x447642, 0x44780b),
    'profile_read': (0x457b01, 0x457d90),
    'profile_cache_open': (0x457f14, 0x45819c),
    'profile_cache_remove': (0x458202, 0x458289),
    'profile_key_scan': (0x458289, 0x458328),
    'profile_section_and_key_lookup': (0x458328, 0x4584d3),
    'profile_cache_clear': (0x458570, 0x458592),
    'profile_edit': (0x4585be, 0x45876c),
    'profile_flush': (0x45876c, 0x45881d),
    'profile_buffer_mutate': (0x45881d, 0x458a58),
    'profile_write_through_switch': (0x458a58, 0x458ac8),
    'encryptor_encrypt': (0x4480f5, 0x4483bb),
    'encryptor_decrypt_validate': (0x4483bb, 0x4485ca),
    'encryptor_key_access': (0x448640, 0x448680),
    'path_relative_construct': (0x453268, 0x45337b),
    'path_normalize_resolve': (0x453695, 0x453b3e),
    'path_set_current_and_defaults': (0x453dfc, 0x453f16),
    'path_initial_directories': (0x4543eb, 0x45447c),
    'crt_casefold_compare': (0x483f40, 0x484034),
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
    result['packed_conversion_handlers'] = {
        name: {str(tag): hex(image.u32(table + 4 * (tag - 2))) for tag in range(2, 16)}
        for name, table in (('string', 0x40fd79), ('rectangle', 0x4104f2), ('point', 0x41077f))}
    result['packed_conversion_handlers']['key'] = {
        str(tag): hex(image.u32(0x41017f + 4 * image.read(0x41019b + tag - 2, 1)[0]))
        for tag in range(2, 16)}
    result['binary_operator_handlers'] = {
        hex(token): hex(image.u32(0x405296 + 4 * image.read(0x4052b6 + token - 11, 1)[0]))
        for token in range(11, 53)}
    result['packed_formats'] = {hex(a): image.cstring(a) for a in (
        0x493760, 0x493764, 0x49376c, 0x493778, 0x493780, 0x493788,
        0x4937c0, 0x4937e4, 0x4937e8, 0x4937f8)}
    result['item_field_handlers'] = {hex(0x192 + n): hex(image.u32(0x41d232 + 4 * n)) for n in range(5)}
    result['item_event_handlers'] = {hex(n): hex(image.u32(0x41d042 + 4 * n))
                                      for n in (1, 5, 7, 0x23, 0x2f, 0x3a, 0x3b, 0x4a, 0x4e, 0x5e)}
    result['phase_script_handlers'] = {str(n - 2): hex(image.u32(0x41d246 + 4 * n)) for n in range(5)}
    result['component_case_handlers'] = {
        name: {str(n + 1): hex(image.u32(table + 4 * n)) for n in range(count)}
        for name, table, count in (('aaDatabase', 0x43bb34, 11), ('aaQSelector', 0x440bdc, 16),
                                   ('aaUserHistory', 0x4459dd, 12), ('encryptor', 0x4480e5, 4))}
    result['storage_and_component_strings'] = {hex(a): image.cstring(a) for a in (
        0x496e64, 0x497020, 0x497034, 0x497048, 0x497064, 0x49706c, 0x497074, 0x497794, 0x49779c,
        0x497e50, 0x497e58, 0x497e60, 0x497e64, 0x497e68, 0x497e6c, 0x497e70,
        0x49817c, 0x498180, 0x49818c, 0x498198, 0x49819c,
        0x49839c, 0x4983a0, 0x4983b0, 0x4983b4, 0x4983d0,
        0x489620, 0x48974c, 0x48976c, 0x489770, 0x489774)}
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
