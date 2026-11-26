# SPDX-License-Identifier: GPL-3.0-or-later
"""Run pinned original x86 leaf functions under the Windows debugger.

Stops at loader breakpoint, redirects the initial thread to a bounded stub,
reads results at its own breakpoint, and terminates the disposable process.
Does not run the PE entry point or application startup. Not a full VM harness.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import time

from inspect_mohawk_v5_reference import inspect


def u32(n):
    return struct.pack('<I', n & 0xffffffff)


def value(tag, payload):
    return bytes([tag]) + bytes(7) + payload.ljust(8, b'\0')


def cases():
    result = []
    for number, expected in [(1.75, 1), (-1.75, -1), (0.5, 0), (-0.5, 0),
                             (2147483648.0, -2147483648), (4294967297.0, 1)]:
        result.append(dict(name=f'toInt({number})', function=0x40ffa9,
                           receiver=value(4, struct.pack('<d', number)).hex(),
                           arguments=[], output='int32', expected=expected))
    for number in [-32768, 0, 32767]:
        result.append(dict(name=f'toReal(short {number})', function=0x40fe5d,
                           receiver=value(2, struct.pack('<h', number)).hex(),
                           arguments=[], output='double', expected=float(number)))
    result.append(dict(name='toReal(Boolean TRUE)', function=0x40fe5d,
                       receiver=value(8, b'\1').hex(), arguments=[], output='double', expected=1.0))
    for number in [0, -1, 2, -32768]:
        for function, output in [(0x40fe5d, 'double'), (0x40ffa9, 'int32')]:
            result.append(dict(name=f'{output}(Boolean payload {number})', function=function,
                               receiver=value(8, struct.pack('<h', number)).hex(), arguments=[],
                               output=output, expected=float(number) if output=='double' else number))
    for a, b, expected in [([65, 0, 97, 0], [66, 1, 98, 1], 1),
                            ([65, 0, 97, 0], [66, 1, 97, 9], 0),
                            ([65, 0, 0, 0], [65, 0, 0, 9], 0),
                            ([65, 0, 0, 0], [66, 0, 0, 0], 1),
                            ([66, 0, 0, 0], [65, 0, 0, 0], 1)]:
        result.append(dict(name=f'keyCompare({a},{b})', function=0x40f559,
                           receiver=value(11, bytes(a)).hex(),
                           arguments=[value(11, bytes(b)).hex()], output='double', expected=float(expected)))
    for key in [[65, 1, 97, 2], [65, 0, 0, 0]]:
        result.append(dict(name=f'keyToInt({key})', function=0x40ffa9,
                           receiver=value(11, bytes(key)).hex(), arguments=[], output='int32', expected=0))
        result.append(dict(name=f'keyToReal({key})', function=0x40fe5d,
                           receiver=value(11, bytes(key)).hex(), arguments=[], output='double',
                           expected=float(key[2] or key[0])))
    # Include reverse/mismatch controls: character matching and virtual-key matching differ.
    for a,b,expected in [([65,0,97,0],[66,1,97,9],1),
                         ([65,0,0,0],[65,0,0,9],1),
                         ([65,0,0,0],[65,1,0,0],0),
                         ([0,0,0,0],[0,0,0,0],0)]:
        result.append(dict(name=f'keyTrigger({a},{b})', function=0x4203ae,
                           receiver=None, arguments=[bytes(a).hex(),bytes(b).hex()],
                           calling_convention='cdecl', output='int16', expected=expected))
    return result


class DebugEvent(C.Structure):
    _fields_ = [('code', W.DWORD), ('pid', W.DWORD), ('tid', W.DWORD),
                ('payload', C.c_uint64 * 20)]


class Startup(C.Structure):
    _fields_ = [('cb', W.DWORD), ('reserved', W.LPWSTR), ('desktop', W.LPWSTR),
                ('title', W.LPWSTR), ('x', W.DWORD), ('y', W.DWORD),
                ('width', W.DWORD), ('height', W.DWORD), ('charsX', W.DWORD),
                ('charsY', W.DWORD), ('fill', W.DWORD), ('flags', W.DWORD),
                ('show', W.WORD), ('reservedSize', W.WORD), ('reservedData', C.c_void_p),
                ('stdin', W.HANDLE), ('stdout', W.HANDLE), ('stderr', W.HANDLE)]


class ProcessInfo(C.Structure):
    _fields_ = [('process', W.HANDLE), ('thread', W.HANDLE), ('pid', W.DWORD), ('tid', W.DWORD)]


def windows_api():
    if os.name != 'nt' or C.sizeof(C.c_void_p) != 8:
        raise RuntimeError('Requires 64-bit Python on Windows with WOW64 debug support')
    k = C.WinDLL('kernel32', use_last_error=True)
    signatures = {
        'CreateProcessW': ([W.LPCWSTR, W.LPWSTR, C.c_void_p, C.c_void_p, W.BOOL, W.DWORD,
                            C.c_void_p, W.LPCWSTR, C.POINTER(Startup), C.POINTER(ProcessInfo)], W.BOOL),
        'WaitForDebugEvent': ([C.POINTER(DebugEvent), W.DWORD], W.BOOL),
        'DebugSetProcessKillOnExit': ([W.BOOL], W.BOOL),
        'ContinueDebugEvent': ([W.DWORD, W.DWORD, W.DWORD], W.BOOL),
        'ReadProcessMemory': ([W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)], W.BOOL),
        'WriteProcessMemory': ([W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)], W.BOOL),
        'VirtualAllocEx': ([W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD, W.DWORD], C.c_void_p),
        'Wow64GetThreadContext': ([W.HANDLE, C.c_void_p], W.BOOL),
        'Wow64SetThreadContext': ([W.HANDLE, C.c_void_p], W.BOOL),
        'TerminateProcess': ([W.HANDLE, W.UINT], W.BOOL),
        'CloseHandle': ([W.HANDLE], W.BOOL),
        'FlushInstructionCache': ([W.HANDLE, C.c_void_p, C.c_size_t], W.BOOL),
    }
    for name, (args, result) in signatures.items():
        f = getattr(k, name); f.argtypes = args; f.restype = result
    return k


def checked(ok):
    if not ok:
        raise C.WinError(C.get_last_error())
    return ok


def run_case(exe, case, timeout):
    k = windows_api(); startup = Startup(); startup.cb = C.sizeof(startup)
    startup.flags = 1; startup.show = 0
    pi = ProcessInfo(); event = DebugEvent(); pending = False
    checked(k.CreateProcessW(str(exe), C.create_unicode_buffer('"' + str(exe) + '"'),
                            None, None, False, 2, None, str(exe.parent), C.byref(startup), C.byref(pi)))
    phase = 'loader'; start = time.monotonic(); trap = None
    def read(address, size):
        b = C.create_string_buffer(size); n = C.c_size_t()
        checked(k.ReadProcessMemory(pi.process, address, b, size, C.byref(n)))
        if n.value != size: raise RuntimeError('Short process read')
        return b.raw
    def write(address, data):
        b = C.create_string_buffer(bytes(data)); n = C.c_size_t()
        checked(k.WriteProcessMemory(pi.process, address, b, len(data), C.byref(n)))
        if n.value != len(data): raise RuntimeError('Short process write')
    try:
        checked(k.DebugSetProcessKillOnExit(True))
        while time.monotonic() - start < timeout:
            if not k.WaitForDebugEvent(C.byref(event), 100):
                if C.get_last_error() == 121: continue
                raise C.WinError(C.get_last_error())
            pending = True; raw = bytes(event.payload)
            # Debugger owns handles in create-process/load-DLL debug events.
            if event.code in (3, 6):
                file_handle = struct.unpack_from('<Q', raw)[0]
                if file_handle: k.CloseHandle(file_handle)
            if event.code == 3:
                image_base = struct.unpack_from('<Q', raw, 24)[0]
                if image_base != 0x400000: raise RuntimeError('Unexpected image base')
            if event.code == 5:
                return dict(status='process_exit', exit_code=struct.unpack_from('<I', raw)[0], phase=phase)
            if event.code == 1:
                code = struct.unpack_from('<I', raw)[0]
                address = struct.unpack_from('<Q', raw, 16)[0]
                if code not in (0x80000003, 0x4000001f):
                    return dict(status='exception', exception=hex(code), address=hex(address), phase=phase)
                if phase == 'probe':
                    if address != trap: raise RuntimeError(f'Unexpected breakpoint {address:x}')
                    data = read(output, 8)
                    fmt = {'int32':'<i', 'int16':'<h', 'double':'<d'}[case['output']]
                    actual = struct.unpack_from(fmt, data)[0]
                    return dict(status='observed', actual=actual, expected=case['expected'],
                                matches_static_contract=actual == case['expected'],
                                process_id=pi.pid, phase=phase, result_hex=data.hex(),
                                loader_eip=hex(original_eip), probe_trap=hex(trap))
                if code == 0x80000003:
                    checked(k.ContinueDebugEvent(event.pid, event.tid, 0x10002)); pending = False
                    continue  # Native loader break precedes the WOW64 loader break.
                if event.tid != pi.tid: raise RuntimeError('Loader breakpoint on unexpected thread')
                remote = checked(k.VirtualAllocEx(pi.process, None, 0x2000, 0x3000, 0x40))
                if remote + 0x2000 > 0x100000000: raise RuntimeError('Allocation exceeds x86 address space')
                output = remote + 0x1000; next_data = remote + 0x1100
                addresses = []
                for block in ([case['receiver']] if case['receiver'] else []) + case['arguments']:
                    addresses.append(next_data); data = bytes.fromhex(block)
                    write(next_data, data); next_data += max(32, len(data))
                receiver = addresses.pop(0) if case['receiver'] else None
                stub = bytearray(b'\xdb\xe3')  # FNINIT: explicit x87 control word 0x037f.
                for address_arg in reversed(addresses): stub += b'\x68' + u32(address_arg)
                if receiver is not None: stub += b'\xb9' + u32(receiver)
                stub += b'\xb8' + u32(case['function']) + b'\xff\xd0'
                if case.get('calling_convention') == 'cdecl': stub += b'\x83\xc4' + bytes([4 * len(addresses)])
                stub += (b'\xdd\x1d' if case['output'] == 'double' else b'\xa3') + u32(output)
                trap = remote + len(stub); stub += b'\xcc'
                write(remote, stub); checked(k.FlushInstructionCache(pi.process, remote, len(stub)))
                context = C.create_string_buffer(716); struct.pack_into('<I', context, 0, 0x10007)
                checked(k.Wow64GetThreadContext(pi.thread, context))
                original_eip = struct.unpack_from('<I', context, 184)[0]
                struct.pack_into('<I', context, 184, remote)
                checked(k.Wow64SetThreadContext(pi.thread, context)); phase = 'probe'
            checked(k.ContinueDebugEvent(event.pid, event.tid, 0x10002)); pending = False
        return dict(status='timeout', phase=phase)
    finally:
        # Never resume the untouched application startup after the probe.
        k.TerminateProcess(pi.process, 0)
        if pending: k.ContinueDebugEvent(event.pid, event.tid, 0x10002)
        # Drain exit notifications so debug children cannot remain frozen.
        until = time.monotonic() + 2
        while time.monotonic() < until and k.WaitForDebugEvent(C.byref(event), 100):
            k.ContinueDebugEvent(event.pid, event.tid, 0x10002)
            if event.code == 5: break
        k.CloseHandle(pi.thread); k.CloseHandle(pi.process)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--output-root', required=True, type=Path, help='New disposable directory')
    parser.add_argument('--timeout', type=float, default=5)
    parser.add_argument('--limit', type=int)
    args = parser.parse_args()
    if not 0 < args.timeout <= 20: parser.error('timeout must be in (0, 20]')
    if args.limit is not None and not 1 <= args.limit <= len(cases()): parser.error('invalid case limit')
    source = args.exe.resolve(); identity = inspect(source)
    windows_api()  # Reject unsupported hosts before creating outputs.
    root = args.output_root.resolve(); root.mkdir(parents=True, exist_ok=False)
    run_exe = root / 'CarmenTQ.exe'; shutil.copyfile(source, run_exe)
    if hashlib.sha256(run_exe.read_bytes()).hexdigest() != identity['exe_sha256']:
        raise RuntimeError('Copy identity mismatch')
    plan = cases()[:args.limit]; (root/'plan.json').write_text(json.dumps(plan, indent=2)+'\n')
    results = []
    for case in plan:
        try:
            observed = run_case(run_exe, case, args.timeout)
        except (OSError, RuntimeError) as error:
            observed = dict(status='harness_error', error=str(error))
        result = {'name':case['name'], 'function':hex(case['function']), **observed}
        results.append(result); print(json.dumps(result), flush=True)
        (root/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    receipt = dict(schema=1, kind='original executable direct-function observations; not VM/gameplay acceptance',
                   original_sha256=identity['exe_sha256'], harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                   os=platform.platform(), python=platform.python_version(), x87_control_word='0x037f',
                   application_entry_executed=False, initialization='Windows loader only; original CRT/application initialization skipped',
                   original_runtime_settings='No application startup; no original application file/registry operations invoked',
                   scope='Fresh process per case; only allowlisted function addresses; main thread redirected at loader breakpoint',
                   plan_sha256=hashlib.sha256((root/'plan.json').read_bytes()).hexdigest(),
                   count=len(results), observed=sum(r['status']=='observed' for r in results),
                   mismatches=sum(r.get('matches_static_contract') is False for r in results), results=results)
    (root/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    return 0 if receipt['observed']==receipt['count'] and not receipt['mismatches'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
