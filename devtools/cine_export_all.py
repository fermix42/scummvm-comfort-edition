#!/usr/bin/env python3
"""Export Cine resource bundles for local investigation.

This is intentionally standalone so variant-specific game data can be dumped
without changing the engine or relying on runtime script coverage.
"""

from __future__ import annotations

import argparse
import csv
import json
import shutil
from dataclasses import dataclass
from pathlib import Path


def be16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "big")


def be32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 4], "big")


class CineUnpacker:
    def __init__(self, src: bytes, dst_len: int):
        self.src = src
        self.src_pos = len(src) - 4
        self.dst = bytearray(dst_len)
        self.dst_pos = dst_len - 1
        self.crc = 0
        self.chunk32b = 0
        self.error = False

    def read_source(self) -> int:
        if self.src_pos < 0 or self.src_pos + 4 > len(self.src):
            self.error = True
            return 0
        value = be32(self.src, self.src_pos)
        self.src_pos -= 4
        return value

    def rcr(self, input_carry: bool) -> int:
        output_carry = self.chunk32b & 1
        self.chunk32b >>= 1
        if input_carry:
            self.chunk32b |= 0x80000000
        return output_carry

    def next_bit(self) -> int:
        carry = self.rcr(False)
        if self.chunk32b == 0:
            self.chunk32b = self.read_source()
            self.crc ^= self.chunk32b
            carry = self.rcr(True)
        return carry

    def get_bits(self, count: int) -> int:
        value = 0
        for _ in range(count):
            value = (value << 1) | self.next_bit()
        return value

    def unpack_raw_bytes(self, count: int) -> None:
        if self.dst_pos >= len(self.dst) or self.dst_pos - count + 1 < 0:
            self.error = True
            return
        for _ in range(count):
            self.dst[self.dst_pos] = self.get_bits(8)
            self.dst_pos -= 1

    def copy_relocated_bytes(self, offset: int, count: int) -> None:
        if self.dst_pos + offset >= len(self.dst) or self.dst_pos - count + 1 < 0:
            self.error = True
            return
        for _ in range(count):
            self.dst[self.dst_pos] = self.dst[self.dst_pos + offset]
            self.dst_pos -= 1

    def unpack(self) -> bytes:
        if len(self.src) == len(self.dst):
            return bytes(self.src)

        unpacked_len = self.read_source()
        if unpacked_len > len(self.dst):
            self.error = True
            unpacked_len = len(self.dst)
        self.dst_pos = unpacked_len - 1
        self.crc = self.read_source()
        self.chunk32b = self.read_source()
        self.crc ^= self.chunk32b

        while self.dst_pos >= 0 and not self.error:
            if not self.next_bit():
                if not self.next_bit():
                    self.unpack_raw_bytes(self.get_bits(3) + 1)
                else:
                    self.copy_relocated_bytes(self.get_bits(8), 2)
            else:
                code = self.get_bits(2)
                if code == 3:
                    self.unpack_raw_bytes(self.get_bits(8) + 9)
                elif code < 2:
                    self.copy_relocated_bytes(self.get_bits(code + 9), code + 3)
                else:
                    count = self.get_bits(8) + 1
                    self.copy_relocated_bytes(self.get_bits(12), count)

        if self.error or self.crc != 0:
            raise ValueError(f"unpack failed: error={self.error} crc=0x{self.crc:08x}")
        return bytes(self.dst)


@dataclass
class BundleEntry:
    bundle: str
    index: int
    name: str
    offset: int
    packed_size: int
    unpacked_size: int


def safe_name(name: str) -> str:
    return "".join(c if c not in '<>:"/\\|?*' and ord(c) >= 32 else "_" for c in name)


def trim_c_string(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("ascii", errors="replace").strip()


def parse_bundle(path: Path) -> list[BundleEntry]:
    data = path.read_bytes()
    if len(data) < 4:
        raise ValueError("too small")
    count = be16(data, 0)
    entry_size = be16(data, 2)
    if entry_size < 26 or 4 + count * entry_size > len(data):
        raise ValueError(f"bad bundle header: count={count} entry_size={entry_size}")
    entries: list[BundleEntry] = []
    for index in range(count):
        pos = 4 + index * entry_size
        name = trim_c_string(data[pos : pos + 14])
        offset = be32(data, pos + 14)
        packed_size = be32(data, pos + 18)
        unpacked_size = be32(data, pos + 22)
        if not name or offset < 4 + count * entry_size or packed_size == 0 or unpacked_size == 0:
            raise ValueError("not a Cine bundle")
        if offset + packed_size > len(data):
            raise ValueError("bundle entry exceeds file size")
        entries.append(
            BundleEntry(
                bundle=path.name,
                index=index,
                name=name,
                offset=offset,
                packed_size=packed_size,
                unpacked_size=unpacked_size,
            )
        )
    return entries


def unpack_entry(bundle_data: bytes, entry: BundleEntry) -> bytes:
    packed = bundle_data[entry.offset : entry.offset + entry.packed_size]
    return CineUnpacker(packed, entry.unpacked_size).unpack()


def split_prc(data: bytes) -> list[tuple[int, bytes]]:
    count = be16(data, 0)
    sizes_pos = 2
    body_pos = sizes_pos + count * 2
    scripts = []
    for index in range(count):
        size = be16(data, sizes_pos + index * 2)
        scripts.append((index, data[body_pos : body_pos + size]))
        body_pos += size
    return scripts


def split_rel(data: bytes) -> list[tuple[int, tuple[int, int, int], bytes]]:
    count = be16(data, 0)
    table_pos = 2
    body_pos = table_pos + count * 8
    scripts = []
    for index in range(count):
        pos = table_pos + index * 8
        size = be16(data, pos)
        params = (be16(data, pos + 2), be16(data, pos + 4), be16(data, pos + 6))
        scripts.append((index, params, data[body_pos : body_pos + size]))
        body_pos += size
    return scripts


def decode_msg(data: bytes) -> list[str]:
    count = be16(data, 0)
    lengths_pos = 2
    body_pos = lengths_pos + count * 2
    messages = []
    for index in range(count):
        length = be16(data, lengths_pos + index * 2)
        chunk = data[body_pos : body_pos + length] if body_pos < len(data) else b""
        messages.append(chunk.rstrip(b"\0").decode("cp437", errors="replace"))
        body_pos += length
    return messages


def decode_vol_cnf(path: Path) -> bytes:
    data = path.read_bytes()
    if data.startswith(b"ABASECP"):
        unpacked_size = be32(data, 8)
        packed_size = be32(data, 12)
        return CineUnpacker(data[16 : 16 + packed_size], unpacked_size).unpack()
    return data


def fixed_vol_name(raw: bytes, name_len: int) -> str:
    if name_len == 13:
        return trim_c_string(raw)
    tmp = bytearray(raw)
    tmp = bytearray(0 if b == 0x20 else b for b in tmp)
    base = tmp[:8].split(b"\0", 1)[0].decode("ascii", errors="replace")
    ext = tmp[8:].split(b"\0", 1)[0].decode("ascii", errors="replace")
    return f"{base}.{ext}" if ext else base


def parse_vol_cnf(data: bytes) -> dict:
    count = be16(data, 0)
    entry_size = be16(data, 2)
    pos = 4
    volumes = []
    for _ in range(count):
        entry = data[pos : pos + entry_size]
        volumes.append(
            {
                "name": trim_c_string(entry[:10]),
                "names_offset": be32(entry, 10),
                "disk": int.from_bytes(entry[14:16], "big", signed=True),
                "names_size": be32(entry, 16),
            }
        )
        pos += entry_size

    sizes_pos = pos
    block_pos = sizes_pos
    blocks: list[bytes] = []
    mod11 = True
    mod13 = True
    for _ in range(count):
        size = be32(data, block_pos)
        block_pos += 4
        mod11 = mod11 and size % 11 == 0
        mod13 = mod13 and size % 13 == 0
        blocks.append(data[block_pos : block_pos + size])
        block_pos += size
    name_len = 11 if mod11 and not mod13 else 13

    for volume, block in zip(volumes, blocks):
        volume["files"] = [
            fixed_vol_name(block[i : i + name_len], name_len)
            for i in range(0, len(block), name_len)
        ]
    return {"entry_count": count, "entry_size": entry_size, "name_length": name_len, "volumes": volumes}


def write_script_splits(entry_path: Path, entry: BundleEntry, data: bytes) -> None:
    suffix = entry.name.upper().rsplit(".", 1)[-1] if "." in entry.name else ""
    if suffix == "PRC":
        scripts = split_prc(data)
        split_dir = entry_path.with_suffix(entry_path.suffix + ".scripts")
        split_dir.mkdir(exist_ok=True)
        index_rows = []
        for script_index, script_data in scripts:
            script_path = split_dir / f"{safe_name(entry.name)}_{script_index:03d}.bin"
            script_path.write_bytes(script_data)
            index_rows.append({"script": script_index, "size": len(script_data), "file": script_path.name})
        (split_dir / "index.json").write_text(json.dumps(index_rows, indent=2), encoding="utf-8")
    elif suffix == "REL":
        scripts = split_rel(data)
        split_dir = entry_path.with_suffix(entry_path.suffix + ".scripts")
        split_dir.mkdir(exist_ok=True)
        index_rows = []
        for script_index, params, script_data in scripts:
            script_path = split_dir / f"{safe_name(entry.name)}_{script_index:03d}.bin"
            script_path.write_bytes(script_data)
            index_rows.append(
                {
                    "script": script_index,
                    "size": len(script_data),
                    "p1": params[0],
                    "p2": params[1],
                    "p3": params[2],
                    "file": script_path.name,
                }
            )
        (split_dir / "index.json").write_text(json.dumps(index_rows, indent=2), encoding="utf-8")
    elif suffix == "MSG":
        messages = decode_msg(data)
        text_path = entry_path.with_suffix(entry_path.suffix + ".txt")
        with text_path.open("w", encoding="utf-8", newline="\n") as handle:
            for index, message in enumerate(messages):
                handle.write(f"[{index:03d}] {message}\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("game_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    args = parser.parse_args()

    game_dir = args.game_dir
    out_dir = args.out_dir
    raw_dir = out_dir / "raw-files"
    bundle_dir = out_dir / "bundles"
    raw_dir.mkdir(parents=True, exist_ok=True)
    bundle_dir.mkdir(parents=True, exist_ok=True)

    bundle_names = []
    manifest_rows = []
    for path in sorted(p for p in game_dir.iterdir() if p.is_file()):
        shutil.copy2(path, raw_dir / path.name)
        try:
            entries = parse_bundle(path)
        except ValueError:
            entries = []
        if not entries:
            continue
        bundle_names.append(path.name)
        data = path.read_bytes()
        this_bundle_dir = bundle_dir / safe_name(path.name)
        this_bundle_dir.mkdir(exist_ok=True)
        for entry in entries:
            entry_name = safe_name(entry.name or f"entry_{entry.index:03d}.bin")
            entry_path = this_bundle_dir / entry_name
            try:
                unpacked = unpack_entry(data, entry)
                entry_path.write_bytes(unpacked)
                write_script_splits(entry_path, entry, unpacked)
                status = "ok"
            except Exception as exc:
                status = f"failed: {exc}"
            manifest_rows.append(
                {
                    "bundle": entry.bundle,
                    "index": entry.index,
                    "name": entry.name,
                    "offset": entry.offset,
                    "packed_size": entry.packed_size,
                    "unpacked_size": entry.unpacked_size,
                    "status": status,
                }
            )

    vol_path = game_dir / "VOL.CNF"
    if vol_path.exists():
        vol_data = decode_vol_cnf(vol_path)
        (out_dir / "VOL.CNF.unpacked").write_bytes(vol_data)
        (out_dir / "VOL.CNF.json").write_text(json.dumps(parse_vol_cnf(vol_data), indent=2), encoding="utf-8")

    with (out_dir / "manifest.csv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["bundle", "index", "name", "offset", "packed_size", "unpacked_size", "status"],
        )
        writer.writeheader()
        writer.writerows(manifest_rows)

    summary = {
        "game_dir": str(game_dir),
        "out_dir": str(out_dir),
        "bundle_count": len(bundle_names),
        "entry_count": len(manifest_rows),
        "failed_count": sum(1 for row in manifest_rows if row["status"] != "ok"),
        "bundles": bundle_names,
    }
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if summary["failed_count"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
