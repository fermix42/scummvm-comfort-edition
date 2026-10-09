#!/usr/bin/env python3
"""Extract Operation Stealth Cine scripts from local DOS VGA part archives.

This is a small workspace helper for analysis. It unpacks every entry in the
specified PROCS*/PROC* part files, decompiles PRC global scripts and REL object
scripts using the same textual style as ScummVM's DUMP_SCRIPTS block, and writes
a manifest with hashes for reproducibility.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def be16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 2], "big", signed=False)


def sbe16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 2], "big", signed=True)


def be32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "big", signed=False)


class CineUnpacker:
    def __init__(self, src: bytes, dst_len: int):
        self.src = src
        self.src_begin = 0
        self.src_end = len(src)
        self.src_pos = len(src) - 4
        self.dst = bytearray(dst_len)
        self.dst_begin = 0
        self.dst_end = dst_len
        self.dst_pos = 0
        self.crc = 0
        self.chunk32b = 0
        self.error = False

    def read_source(self) -> int:
        if self.src_pos < self.src_begin or self.src_pos + 4 > self.src_end:
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
        if self.dst_pos >= self.dst_end or self.dst_pos - count + 1 < self.dst_begin:
            self.error = True
            return
        for _ in range(count):
            self.dst[self.dst_pos] = self.get_bits(8)
            self.dst_pos -= 1

    def copy_relocated_bytes(self, offset: int, count: int) -> None:
        if self.dst_pos + offset >= self.dst_end or self.dst_pos - count + 1 < self.dst_begin:
            self.error = True
            return
        for _ in range(count):
            self.dst[self.dst_pos] = self.dst[self.dst_pos + offset]
            self.dst_pos -= 1

    def unpack(self) -> bytes:
        if len(self.src) == len(self.dst):
            return self.src

        unpacked_len = self.read_source()
        if unpacked_len > len(self.dst):
            self.error = True
            unpacked_len = len(self.dst)
        self.dst_pos = unpacked_len - 1
        self.crc = self.read_source()
        self.chunk32b = self.read_source()
        self.crc ^= self.chunk32b

        while self.dst_pos >= self.dst_begin and not self.error:
            if not self.next_bit():
                if not self.next_bit():
                    self.unpack_raw_bytes(self.get_bits(3) + 1)
                else:
                    self.copy_relocated_bytes(self.get_bits(8), 2)
            else:
                c = self.get_bits(2)
                if c == 3:
                    self.unpack_raw_bytes(self.get_bits(8) + 9)
                elif c < 2:
                    self.copy_relocated_bytes(self.get_bits(c + 9), c + 3)
                else:
                    count = self.get_bits(8) + 1
                    self.copy_relocated_bytes(self.get_bits(12), count)

        if self.error or self.crc != 0:
            raise ValueError("Cine unpack failed")
        return bytes(self.dst)


def c_string(data: bytes, offset: int) -> tuple[str, int]:
    end = data.find(b"\x00", offset)
    if end == -1:
        end = len(data)
    return data[offset:end].decode("latin-1", errors="replace"), end - offset + 1


def obj_param_name(param_idx: int) -> str:
    return {
        1: ".X",
        2: ".Y",
        3: ".mask",
        4: ".frame",
        5: ".status",
        6: ".costume",
    }.get(param_idx, f".param{param_idx}")


class Decompiler:
    def __init__(
        self,
        script: bytes,
        idx: int,
        max_instructions: int | None = None,
        continue_after_break: bool = False,
        show_offsets: bool = False,
    ):
        self.script = script
        self.idx = idx
        self.pos = 0
        self.max_instructions = max_instructions
        self.continue_after_break = continue_after_break
        self.show_offsets = show_offsets
        self.compare1 = ""
        self.compare2 = ""
        self.lines = [f"--------- SCRIPT {idx} ---------\n"]

    def byte(self) -> int:
        value = self.script[self.pos] if self.pos < len(self.script) else 0
        self.pos += 1
        return value

    def word(self) -> int:
        value = sbe16(self.script, self.pos) if self.pos + 2 <= len(self.script) else 0
        self.pos += 2
        return value

    def uword(self) -> int:
        value = be16(self.script, self.pos) if self.pos + 2 <= len(self.script) else 0
        self.pos += 2
        return value

    def string(self, advance_nul: bool = True) -> str:
        value, size = c_string(self.script, self.pos)
        self.pos += size if advance_nul else max(size - 1, 0)
        return value

    def decompile(self) -> str:
        instruction_count = 0
        while self.pos < len(self.script):
            instruction_start = self.pos
            opcode = self.byte()
            if self.pos == len(self.script):
                opcode = 0
            op = opcode - 1
            line = ""

            if op == -1:
                pass
            elif op == 0x00:
                p1, p2, p3 = self.byte(), self.byte(), self.word()
                line = f"obj[{p1}]{obj_param_name(p2)} = {p3}\n"
            elif op == 0x01:
                p1, p2, p3 = self.byte(), self.byte(), self.byte()
                line = f"var[{p3}]=obj[{p1}]{obj_param_name(p2)}\n"
            elif op in (0x02, 0x03, 0x04, 0x05, 0x06):
                p1, p2, p3 = self.byte(), self.byte(), self.word()
                if op == 0x02:
                    line = f"obj[{p1}]{obj_param_name(p2)}+={p3}\n"
                elif op == 0x03:
                    line = f"obj[{p1}]{obj_param_name(p2)}-={p3}\n"
                elif op == 0x04:
                    line = f"obj[{p1}]{obj_param_name(p2)}+=obj[{p3}]{obj_param_name(p2)}\n"
                elif op == 0x05:
                    line = f"obj[{p1}]{obj_param_name(p2)}-=obj[{p3}]{obj_param_name(p2)}\n"
                else:
                    self.compare1 = f"obj[{p1}]{obj_param_name(p2)}"
                    self.compare2 = f"{p3}"
            elif op in (0x07, 0x08):
                p1 = self.byte()
                p2, p3, p4, p5 = self.word(), self.word(), self.word(), self.word()
                line = (f"setupObject(Idx:{p1},X:{p2},Y:{p3},mask:{p4},frame:{p5})\n"
                        if op == 0x07 else f"checkCollision({p1},{p2},{p3},{p4},{p5})\n")
            elif op == 0x09:
                p1, p2 = self.byte(), self.byte()
                if p2:
                    p3 = self.byte()
                    if p2 == 1:
                        line = f"var[{p1}]=var[{p3}]\n"
                    elif p2 == 2:
                        line = f"var[{p1}]=globalVar[{p3}]\n"
                    elif p2 == 3:
                        line = f"var[{p1}]=mouse.X\n"
                    elif p2 == 4:
                        line = f"var[{p1}]=mouse.Y\n"
                    elif p2 == 5:
                        line = f"var[{p1}]=rand() mod {p3}\n"
                    elif p2 == 8:
                        line = f"var[{p1}]=file[{p3}].packedSize\n"
                    elif p2 == 9:
                        line = f"var[{p1}]=file[{p3}].unpackedSize\n"
                    else:
                        line = f"Unsupported var source {p2}\n"
                else:
                    line = f"var[{p1}]={self.word()}\n"
            elif op in (0x0A, 0x0B, 0x0C, 0x0D):
                p1, p2 = self.byte(), self.byte()
                sym = {0x0A: "+=", 0x0B: "-=", 0x0C: "*=", 0x0D: "/="}[op]
                rhs = f"var[{self.byte()}]" if p2 else str(self.word())
                line = f"var[{p1}]{sym}{rhs}\n"
            elif op == 0x0E:
                p1, p2 = self.byte(), self.byte()
                if p2:
                    p3 = self.byte()
                    self.compare1 = f"var[{p1}]"
                    self.compare2 = f"{'var' if p2 == 1 else 'globalVar'}[{p3}]"
                else:
                    self.compare1 = f"var[{p1}]"
                    self.compare2 = f"{self.word()}"
            elif op == 0x0F:
                p1, p2, p3 = self.byte(), self.byte(), self.byte()
                line = f"obj[{p1}]{obj_param_name(p2)}=var[{p3}]\n"
            elif op in (0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19):
                p = self.byte()
                name = {
                    0x13: "loadMask0", 0x14: "unloadMask0", 0x15: "OP_15",
                    0x16: "loadMask1", 0x17: "unloadMask0", 0x18: "loadMask4",
                    0x19: "unloadMask4",
                }[op]
                line = f"{name}({p})\n"
            elif op == 0x1A:
                line = f"OP_1A({self.byte()})\n"
            elif op == 0x1B:
                line = "bgIncrustList.clear()\n"
            elif op == 0x1D:
                line = f"label({self.byte()})\n"
            elif op == 0x1E:
                line = f"goto({self.byte()})\n"
            elif op in (0x1F, 0x20, 0x21, 0x22, 0x23, 0x24):
                rel = {0x1F: ">", 0x20: ">=", 0x21: "<", 0x22: "<=", 0x23: "==", 0x24: "!="}[op]
                line = f"if({self.compare1}{rel}{self.compare2}) goto({self.byte()})\n"
            elif op == 0x25:
                line = f"removeLabel({self.byte()})\n"
            elif op == 0x26:
                line = f"loop(--var[{self.byte()}]) -> label({self.byte()})\n"
            elif op in (0x31, 0x32):
                line = f"{'startGlobalScript' if op == 0x31 else 'endGlobalScript'}({self.byte()})\n"
            elif op in (0x3B, 0x3C, 0x3D, 0x3F):
                name = {0x3B: "loadResource", 0x3C: "loadBg", 0x3D: "loadCt", 0x3F: "loadPart"}[op]
                line = f"{name}({self.string()})\n"
            elif op == 0x40:
                line = "closePart()\n"
            elif op == 0x41:
                p = self.byte()
                line = f"loadPrc({p},{self.string()})\n"
            elif op == 0x42:
                line = "requestCheckPendingDataLoad()\n"
            elif op == 0x45:
                line = "blitAndFade()\n"
            elif op == 0x46:
                line = "fadeToBlack()\n"
            elif op == 0x47:
                line = f"transformPaletteRange({self.byte()},{self.byte()},{self.word()},{self.word()},{self.word()})\n"
            elif op == 0x49:
                line = f"setDefaultMenuBgColor({self.byte()})\n"
            elif op == 0x4A:
                line = f"palRotate({self.byte()},{self.byte()},{self.byte()})\n"
            elif op == 0x4F:
                line = "break()\n"
                if not self.continue_after_break:
                    self.pos = len(self.script)
            elif op == 0x50:
                line = "endScript()\n\n"
            elif op == 0x51:
                line = f"message({self.byte()},{self.word()},{self.word()},{self.word()},{self.word()})\n"
            elif op in (0x52, 0x53):
                p1, p2 = self.byte(), self.byte()
                if p2:
                    p3 = self.byte()
                    src = f"{'var' if p2 == 1 else 'globalVar'}[{p3}]"
                else:
                    src = str(self.word())
                if op == 0x52:
                    line = f"globalVar[{p1}] = {src}\n"
                else:
                    self.compare1 = f"globalVar[{p1}]"
                    self.compare2 = src
            elif op == 0x59:
                line = f"comment: {self.string(False)}\n"
            elif op == 0x5A:
                line = f"freePartRang({self.byte()},{self.byte()})\n"
            elif op == 0x5B:
                line = "unloadAllMasks()\n"
            elif op == 0x65:
                line = "setupTableUnk1()\n"
            elif op == 0x66:
                line = f"tableUnk1[{self.byte()}] = {self.word()}\n"
            elif op == 0x68:
                line = f"setPlayerCommandPosY({self.byte()})\n"
            elif op == 0x69:
                line = "allowPlayerInput()\n"
            elif op == 0x6A:
                line = "disallowPlayerInput()\n"
            elif op == 0x6B:
                line = f"changeDataDisk({self.byte()})\n"
            elif op == 0x6D:
                line = f"loadDat({self.string()})\n"
            elif op == 0x6E:
                line = "updateDat()\n"
            elif op == 0x6F:
                line = "OP_6F() -> dat related\n"
            elif op == 0x70:
                line = "stopSample()\n"
            elif op in (0x77, 0x78):
                line = f"{'playSample' if op == 0x77 else 'playSampleSwapped'}({self.byte()},{self.byte()},{self.word()},{self.byte()},{self.word()},{self.word()})\n"
            elif op == 0x79:
                line = f"disableSystemMenu({self.byte()})\n"
            elif op in (0x7A, 0x7B, 0x8B, 0x8C, 0x8F, 0x91, 0x9D):
                names = {0x7A: "OP_7A", 0x7B: "OP_7B", 0x8B: "OP_8B", 0x8C: "OP_8C", 0x8F: "OP_8F", 0x91: "OP_91", 0x9D: "OP_9D"}
                suffix = " -> flip img idx" if op == 0x9D else ""
                line = f"{names[op]}({self.byte()}){suffix}\n"
            elif op == 0x7F:
                vals = [self.byte(), self.byte(), self.byte(), self.byte(), self.word(), self.word(), self.word()]
                line = f"OP_7F({','.join(map(str, vals))})\n"
            elif op == 0x80:
                line = f"OP_80({self.byte()},{self.byte()})\n"
            elif op == 0x82:
                line = f"OP_82({self.byte()},{self.byte()},{self.uword()},{self.uword()},{self.byte()})\n"
            elif op == 0x83:
                line = f"OP_83({self.byte()},{self.byte()})\n"
            elif op in (0x84, 0x85, 0x86, 0x87, 0x88):
                rel = {0x84: ">", 0x85: ">=", 0x86: "<", 0x87: "<=", 0x88: "=="}[op]
                line = f"if({self.compare1}{rel}{self.compare2}) goto next label({self.byte()})\n"
            elif op == 0x89:
                line = f"if({self.compare1}!={self.compare2}) goto next label({self.byte()})\n"
            elif op == 0x8D:
                vals = [self.word() for _ in range(8)]
                self.compare1 = f"obj[{vals[0]}]"
                self.compare2 = "{" + ",".join(map(str, vals[1:])) + "}"
            elif op == 0x8E:
                line = f"ADDBG({self.byte()},{self.string(False)})\n"
            elif op == 0x90:
                line = f"loadABS({self.byte()},{self.string(False)})\n"
            elif op == 0x9E:
                p = self.byte()
                p2 = self.byte() if p else self.word()
                line = f"OP_9E({p},{p2})\n"
            elif op in (0xA0, 0xA1, 0xA2):
                line = f"OP_{op:02X}({self.uword()},{self.uword()})\n"
            elif op == 0xA3:
                line = f"OP_A3({self.uword()},{self.uword()})\n"
            elif op in (0xA4, 0xA5):
                line = f"{'ADD_OVERLAY' if op == 0xA4 else 'REMOVE_OVERLAY'} object={self.byte()}, type=22\n"
            elif op == 0x9A:
                line = f"o2_wasZoneChecked({self.byte()})\n"
            else:
                line = f"Unsupported opcode {op:X} in decompileScript\n\n"
                self.pos = len(self.script)

            if self.show_offsets and line:
                line = f"{instruction_start:04d}: {line}"
            self.lines.append(line)
            instruction_count += 1
            if self.max_instructions is not None and instruction_count >= self.max_instructions:
                break
        return "".join(self.lines)


def parse_part(path: Path) -> list[dict]:
    data = path.read_bytes()
    count = be16(data, 0)
    entry_size = be16(data, 2)
    entries = []
    pos = 4
    for _ in range(count):
        raw_name = data[pos:pos + 14]
        name = raw_name.split(b"\x00", 1)[0].decode("latin-1", errors="replace")
        entries.append({
            "name": name,
            "offset": be32(data, pos + 14),
            "packed": be32(data, pos + 18),
            "unpacked": be32(data, pos + 22),
        })
        pos += entry_size
    return entries


def unpack_entry(part_path: Path, entry: dict, unpacker_exe: Path | None = None, raw_out: Path | None = None) -> bytes:
    if unpacker_exe:
        if raw_out is None:
            raise ValueError("raw_out is required with unpacker_exe")
        raw_out.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [str(unpacker_exe), str(part_path), entry["name"], str(raw_out)],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return raw_out.read_bytes()

    data = part_path.read_bytes()
    packed = data[entry["offset"]:entry["offset"] + entry["packed"]]
    return CineUnpacker(packed, entry["unpacked"]).unpack()


def extract_scripts(resource: bytes, is_rel: bool) -> list[dict]:
    count = be16(resource, 0)
    pos = 2
    headers = []
    for idx in range(count):
        size = be16(resource, pos)
        pos += 2
        meta = {}
        if is_rel:
            meta = {"p1": be16(resource, pos), "p2": be16(resource, pos + 2), "p3": be16(resource, pos + 4)}
            pos += 6
        headers.append((idx, size, meta))
    scripts = []
    for idx, size, meta in headers:
        body = resource[pos:pos + size]
        pos += size
        if size:
            scripts.append({"idx": idx, "size": size, "meta": meta, "body": body})
    return scripts


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-dir", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--unpacker-exe", type=Path)
    parser.add_argument(
        "--stop-at-break",
        action="store_true",
        help="stop each script at the first break() opcode, matching the older concise dumps",
    )
    parser.add_argument(
        "--no-offsets",
        action="store_true",
        help="omit byte offsets from decompiled script lines",
    )
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "prc").mkdir(exist_ok=True)
    (args.out / "rel").mkdir(exist_ok=True)

    part_files = sorted(
        p for p in args.game_dir.iterdir()
        if p.is_file() and (p.name.upper().startswith("PROCS") or p.name.upper() in {"PROCEGOU", "PROCLABY"})
    )
    manifest = {
        "game_dir": str(args.game_dir),
        "parts": [],
        "resources": [],
        "total_scripts": 0,
    }

    for part in part_files:
        part_hash = hashlib.sha256(part.read_bytes()).hexdigest()
        part_info = {"name": part.name, "size": part.stat().st_size, "sha256": part_hash}
        manifest["parts"].append(part_info)
        for entry in parse_part(part):
            upper = entry["name"].upper()
            if not (upper.endswith(".PRC") or upper.endswith(".REL")):
                continue
            try:
                raw_out = args.out / "raw" / part.name / f"{entry['name']}.bin"
                resource = unpack_entry(part, entry, args.unpacker_exe, raw_out)
                scripts = extract_scripts(resource, upper.endswith(".REL"))
            except Exception as exc:
                manifest["resources"].append({"part": part.name, "name": entry["name"], "error": str(exc)})
                continue

            subdir = "rel" if upper.endswith(".REL") else "prc"
            resource_dir = args.out / subdir / part.name / entry["name"]
            resource_dir.mkdir(parents=True, exist_ok=True)
            (resource_dir / f"{entry['name']}.bin").write_bytes(resource)
            for script in scripts:
                text = Decompiler(
                    script["body"],
                    script["idx"],
                    continue_after_break=not args.stop_at_break,
                    show_offsets=not args.no_offsets,
                ).decompile()
                if script["meta"]:
                    text = (
                        f"// REL params: p1={script['meta']['p1']} p2={script['meta']['p2']} p3={script['meta']['p3']}\n"
                        + text
                    )
                (resource_dir / f"{entry['name']}_{script['idx']:03}.txt").write_text(text, encoding="utf-8")

            manifest["resources"].append({
                "part": part.name,
                "name": entry["name"],
                "type": "REL" if upper.endswith(".REL") else "PRC",
                "packed_size": entry["packed"],
                "unpacked_size": entry["unpacked"],
                "sha256_unpacked": hashlib.sha256(resource).hexdigest(),
                "script_count": len(scripts),
            })
            manifest["total_scripts"] += len(scripts)

    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"wrote {manifest['total_scripts']} scripts from {len(manifest['resources'])} resources to {args.out}")


if __name__ == "__main__":
    main()
