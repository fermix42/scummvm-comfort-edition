#!/usr/bin/env python3
"""Disassemble one script inside an extracted Cine PRC/REL resource.

This is an analysis helper for matching CINE_TRACE_BUILD byte offsets back to
script instructions. Unlike the normal dump, it keeps walking after break()
opcodes and prefixes every instruction with its byte offset.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from cine_extract_os_scripts import Decompiler, be16


def extract_script(resource: bytes, script_index: int, is_rel: bool) -> bytes:
    count = be16(resource, 0)
    if script_index < 0 or script_index >= count:
        raise SystemExit(f"script index {script_index} outside 0..{count - 1}")

    pos = 2
    headers = []
    for idx in range(count):
        size = be16(resource, pos)
        pos += 2
        if is_rel:
            pos += 6
        headers.append((idx, size))

    for idx, size in headers:
        body = resource[pos:pos + size]
        pos += size
        if idx == script_index:
            return body

    raise SystemExit(f"script index {script_index} was not found")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("resource", type=Path)
    parser.add_argument("script_index", type=int)
    parser.add_argument("--rel", action="store_true", help="resource is a REL object-script file")
    args = parser.parse_args()

    script = extract_script(args.resource.read_bytes(), args.script_index, args.rel)
    print(Decompiler(script, args.script_index, continue_after_break=True, show_offsets=True).decompile(), end="")


if __name__ == "__main__":
    main()
