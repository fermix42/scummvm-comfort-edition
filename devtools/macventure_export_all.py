#!/usr/bin/env python3
"""Export MacVenture resource forks and container files for local analysis.

This is a diagnostic helper for studying installed MacVenture game data. It
does not modify game data or engine code.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import unicodedata
from pathlib import Path


FILE_PATH_IDS = {
    1: "mcvi",
    2: "title",
    3: "subdir",
    4: "object",
    5: "filter",
    6: "text",
    7: "graphic",
    8: "sound",
}

OPCODES = {
    0x80: ("GATT", "get attribute"),
    0x81: ("SATT", "set attribute"),
    0x82: ("SUCH", "sum children attribute"),
    0x83: ("PUCT", "push selected control"),
    0x84: ("PUOB", "push selected object"),
    0x85: ("PUTA", "push target"),
    0x86: ("PUDX", "push deltax"),
    0x87: ("PUDY", "push deltay"),
    0x88: ("PUIB", "push immediate.b"),
    0x89: ("PUI", "push immediate"),
    0x8A: ("GGLO", "get global"),
    0x8B: ("SGLO", "set global"),
    0x8C: ("RAND", "random"),
    0x8D: ("COPY", "copy"),
    0x8E: ("COPYN", "copyn"),
    0x8F: ("SWAP", "swap"),
    0x90: ("SWAPN", "swapn"),
    0x91: ("POP", "pop"),
    0x92: ("COPYP", "copy+1"),
    0x93: ("COPYPN", "copy+n"),
    0x94: ("SHUFF", "shuffle"),
    0x95: ("SORT", "sort"),
    0x96: ("CLEAR", "clear stack"),
    0x97: ("SIZE", "get stack size"),
    0x98: ("ADD", "add"),
    0x99: ("SUB", "subtract"),
    0x9A: ("MUL", "multiply"),
    0x9B: ("DIV", "divide"),
    0x9C: ("MOD", "mod"),
    0x9D: ("DMOD", "divmod"),
    0x9E: ("ABS", "abs"),
    0x9F: ("NEG", "neg"),
    0xA0: ("AND", "and"),
    0xA1: ("OR", "or"),
    0xA2: ("XOR", "xor"),
    0xA3: ("NOT", "not"),
    0xA4: ("LAND", "logical and"),
    0xA5: ("LOR", "logical or"),
    0xA6: ("LXOR", "logical xor"),
    0xA7: ("LNOT", "logical not"),
    0xA8: ("GTU", "gt? unsigned"),
    0xA9: ("LTU", "lt? unsigned"),
    0xAA: ("GTS", "gt? signed"),
    0xAB: ("LTS", "lt? signed"),
    0xAC: ("EQ", "eq?"),
    0xAD: ("EQS", "eq string?"),
    0xAE: ("CONT", "contains"),
    0xAF: ("CONTW", "contains word"),
    0xB0: ("BRA", "bra"),
    0xB1: ("BRAB", "bra.b"),
    0xB2: ("BEQ", "beq"),
    0xB3: ("BEQB", "beq.b"),
    0xB4: ("BNE", "bne"),
    0xB5: ("BNEB", "bne.b"),
    0xB6: ("CLAT", "call later"),
    0xB7: ("CCA", "cancel call"),
    0xB8: ("CLOW", "cancel low priority"),
    0xB9: ("CHI", "cancel high priority"),
    0xBA: ("CRAN", "cancel priority range"),
    0xBB: ("FORK", "fork"),
    0xBC: ("CALL", "call"),
    0xBD: ("FOOB", "focus object"),
    0xBE: ("SWOB", "swap objects"),
    0xBF: ("SNOB", "snap object"),
    0xC0: ("TEXI", "toggle exits"),
    0xC1: ("PTXT", "print text"),
    0xC2: ("PNEW", "print newline"),
    0xC3: ("PTNE", "print text+nl"),
    0xC4: ("PNTN", "print nl+text+nl"),
    0xC5: ("PNUM", "print number"),
    0xC6: ("P2", "push 2"),
    0xC7: ("PLBG", "play sound in background"),
    0xC8: ("PLAW", "play sound and wait"),
    0xC9: ("WAIT", "wait for sound to finish?"),
    0xCA: ("TIME", "get current time"),
    0xCB: ("DAY", "get current day"),
    0xCC: ("CHLD", "get children"),
    0xCD: ("NCHLD", "get num children"),
    0xCE: ("VERS", "get engine version"),
    0xCF: ("PSCE", "push scenario number"),
    0xD0: ("P1", "push 1"),
    0xD1: ("GOBD", "get object dimensions"),
    0xD2: ("GOVP", "get overlap percent"),
    0xD3: ("CAPC", "capture children"),
    0xD4: ("RELC", "release children"),
    0xD5: ("DLOG", "show speech dialog"),
    0xD6: ("ACMD", "activate command"),
    0xD7: ("LOSE", "lose game"),
    0xD8: ("WIN", "win game"),
    0xD9: ("SLEEP", "sleep"),
    0xDA: ("CLICK", "click to continue"),
    0xDB: ("ROBQ", "run queue"),
    0xDC: ("RSQ", "run sound queue"),
    0xDD: ("RTQ", "run text queue"),
    0xDE: ("UPSC", "update screen"),
    0xDF: ("FMAI", "flash main window"),
    0xE0: ("CHGR", "cache graphic and object"),
    0xE1: ("CHSO", "cache sound"),
    0xE2: ("MDIV", "muldiv"),
    0xE3: ("UPOB", "update object"),
    0xE4: ("PLEV", "currently playing event?"),
    0xE5: ("WEV", "wait for event to finish"),
    0xE6: ("GFIB", "get fibonacci"),
    0xE7: ("CFIB", "calc fibonacci"),
}


def be16(data: bytes, off: int) -> int:
    return struct.unpack_from(">H", data, off)[0]


def sbe16(data: bytes, off: int) -> int:
    return struct.unpack_from(">h", data, off)[0]


def be24(data: bytes, off: int) -> int:
    return (data[off] << 16) | (data[off + 1] << 8) | data[off + 2]


def be32(data: bytes, off: int) -> int:
    if off + 4 > len(data):
        data = data + b"\0" * (off + 4 - len(data))
    return struct.unpack_from(">I", data, off)[0]


def align128(value: int) -> int:
    return (value + 127) & ~127


def safe_name(name: str) -> str:
    return "".join(c if c.isalnum() or c in "._- " else "_" for c in name).strip() or "unnamed"


def compare_name(name: str) -> str:
    decomposed = unicodedata.normalize("NFKD", name)
    return "".join(c for c in decomposed if not unicodedata.combining(c)).casefold()


def child_path(parent: Path, name: str) -> Path:
    direct = parent / name
    if direct.exists():
        return direct
    wanted = compare_name(name)
    try:
        for child in parent.iterdir():
            if compare_name(child.name) == wanted:
                return child
    except FileNotFoundError:
        pass
    return direct


def read_macbinary(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 128 or data[0] != 0:
        return {
            "name": path.name,
            "type": None,
            "creator": None,
            "data": data,
            "resource": data,
            "macbinary": False,
        }

    name_len = data[1]
    data_len = be32(data, 83)
    resource_len = be32(data, 87)
    resource_start = 128 + align128(data_len)
    if resource_len <= 0 or resource_start + resource_len > len(data):
        return {
            "name": path.name,
            "type": None,
            "creator": None,
            "data": data,
            "resource": data,
            "macbinary": False,
        }

    return {
        "name": data[2 : 2 + name_len].decode("mac_roman", "replace"),
        "type": data[65:69].decode("mac_roman", "replace"),
        "creator": data[69:73].decode("mac_roman", "replace"),
        "data": data[128 : 128 + data_len],
        "resource": data[resource_start : resource_start + resource_len],
        "data_len": data_len,
        "resource_len": resource_len,
        "macbinary": True,
    }


def parse_resource_fork(data: bytes) -> list[dict]:
    if len(data) < 16:
        return []
    data_off = be32(data, 0)
    map_off = be32(data, 4)
    data_len = be32(data, 8)
    map_len = be32(data, 12)
    if data_off + data_len > len(data) or map_off + map_len > len(data):
        return []

    type_list_off = be16(data, map_off + 24)
    name_list_off = be16(data, map_off + 26)
    type_list = map_off + type_list_off
    name_list = map_off + name_list_off
    num_types = be16(data, type_list) + 1
    resources: list[dict] = []

    for type_index in range(num_types):
        entry = type_list + 2 + type_index * 8
        rtype_bytes = data[entry : entry + 4]
        rtype = rtype_bytes.decode("mac_roman", "replace")
        count = be16(data, entry + 4) + 1
        ref_off = type_list + be16(data, entry + 6)
        for res_index in range(count):
            ref = ref_off + res_index * 12
            rid = sbe16(data, ref)
            name_off = sbe16(data, ref + 2)
            attrs = data[ref + 4]
            item_off = be24(data, ref + 5)
            handle = be32(data, ref + 8)
            name = None
            if name_off != -1:
                npos = name_list + name_off
                nlen = data[npos]
                name = data[npos + 1 : npos + 1 + nlen].decode("mac_roman", "replace")
            item_pos = data_off + item_off
            item_len = be32(data, item_pos)
            payload = data[item_pos + 4 : item_pos + 4 + item_len]
            resources.append(
                {
                    "type": rtype,
                    "id": rid,
                    "name": name,
                    "attrs": attrs,
                    "handle": handle,
                    "offset": item_pos,
                    "size": item_len,
                    "data": payload,
                }
            )
    return resources


def decode_str_list(data: bytes) -> list[str]:
    if len(data) < 2:
        return []
    count = be16(data, 0)
    out = []
    pos = 2
    for _ in range(count):
        if pos >= len(data):
            break
        length = data[pos]
        pos += 1
        out.append(data[pos : pos + length].decode("mac_roman", "replace"))
        pos += length
    return out


def parse_container(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 4:
        return {"items": [], "error": "too small"}

    header = be32(data, 0)
    items = []
    if not header & 0x80000000:
        item_len = header
        if item_len == 0:
            return {"items": [], "error": "zero fixed item length"}
        count = (len(data) - 4) // item_len
        for item_id in range(count):
            start = 4 + item_id * item_len
            items.append({"id": item_id, "offset": start, "size": item_len, "data": data[start : start + item_len]})
        return {"simplified": True, "item_length": item_len, "items": items}

    table_off = header & 0x7FFFFFFF
    if table_off + 48 > len(data):
        return {"items": [], "error": f"bad table offset {table_off}"}
    count = be16(data, table_off)
    huff = [be16(data, table_off + 2 + i * 2) for i in range(15)]
    lens = list(data[table_off + 32 : table_off + 48])
    groups = (count + 63) // 64

    for group_id in range(groups):
        group_entry = table_off + 0x30 + group_id * 6
        bit_offset = be24(data, group_entry)
        group_data_off = be24(data, group_entry + 3)
        pos = table_off + (bit_offset >> 3)
        bits = bit_offset & 7
        lengths = []
        for _ in range(64):
            mask = be32(data, pos)
            mask >>= 16 - bits
            mask &= 0xFFFF
            x = 0
            while x < 16 and (huff[x] if x < 15 else 0x10000) <= mask:
                x += 1
            bit_size = lens[x]
            bits += bit_size & 0xF
            if bits & 0x10:
                bits &= 0xF
                pos += 2
            bit_size >>= 4
            length = 0
            if bit_size:
                raw = be32(data, pos)
                bit_size -= 1
                if bit_size:
                    length = raw >> ((32 - bit_size) - bits)
                    length &= (1 << bit_size) - 1
                    length |= 1 << bit_size
                    bits += bit_size
                    if bits & 0x10:
                        bits &= 0xF
                        pos += 2
            lengths.append(length)

        offset = group_data_off + 4
        for index, length in enumerate(lengths):
            item_id = group_id * 64 + index
            if item_id >= count:
                break
            items.append({"id": item_id, "offset": offset, "size": length, "data": data[offset : offset + length]})
            offset += length

    return {
        "simplified": False,
        "table_offset": table_off,
        "count": count,
        "huff": huff,
        "lens": lens,
        "items": items,
    }


def int8(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def int16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def disassemble_script(script: bytes) -> tuple[str, list[dict]]:
    lines = []
    randoms = []
    pos = 0
    while pos < len(script):
        off = pos
        op = script[pos]
        pos += 1
        if op < 0x80:
            lines.append(f"{off:04x}: {op:02x}        PUSH_SMALL {op}")
            continue
        name, desc = OPCODES.get(op, (f"OP_{op:02X}", "unknown"))
        operand = ""
        if op == 0x88 and pos < len(script):
            operand = f" {script[pos]} ; 0x{script[pos]:02x}"
            pos += 1
        elif op == 0x89 and pos + 1 < len(script):
            value = int16(be16(script, pos))
            operand = f" {value} ; 0x{be16(script, pos):04x}"
            pos += 2
        elif op in (0xB0, 0xB2, 0xB4) and pos + 1 < len(script):
            value = int16(be16(script, pos))
            operand = f" {value:+d} -> 0x{pos + 2 + value:04x}"
            pos += 2
        elif op in (0xB1, 0xB3, 0xB5) and pos < len(script):
            value = int8(script[pos])
            operand = f" {value:+d} -> 0x{pos + 1 + value:04x}"
            pos += 1
        if op == 0x8C:
            randoms.append({"offset": off})
        lines.append(f"{off:04x}: {op:02x}        {name}{operand} ; {desc}")
    return "\n".join(lines) + ("\n" if lines else ""), randoms


def write_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def export_resource_fork(game_file: Path, out_dir: Path) -> dict:
    mac = read_macbinary(game_file)
    resource_dir = out_dir / "resource-fork"
    resource_dir.mkdir(parents=True, exist_ok=True)
    write_bytes(resource_dir / "resource-fork.bin", mac["resource"])
    resources = parse_resource_fork(mac["resource"])
    index = []
    strings = {}
    for res in resources:
        item_name = f"{res['type']}_{res['id']}"
        if res["name"]:
            item_name += "_" + safe_name(res["name"])
        filename = item_name + ".bin"
        write_bytes(resource_dir / filename, res["data"])
        row = {k: v for k, v in res.items() if k != "data"}
        row["file"] = filename
        index.append(row)
        if res["type"] == "STR#":
            decoded = decode_str_list(res["data"])
            strings[str(res["id"])] = decoded
            text = "\n".join(f"{i + 1}: {value}" for i, value in enumerate(decoded)) + "\n"
            (resource_dir / f"{item_name}.txt").write_text(text, encoding="utf-8")
    (resource_dir / "index.json").write_text(json.dumps(index, indent=2), encoding="utf-8")
    return {
        "macbinary": {k: v for k, v in mac.items() if k not in ("data", "resource")},
        "resource_count": len(resources),
        "strings": strings,
    }


def export_container(path: Path, out_dir: Path, disassemble: bool) -> dict:
    container = parse_container(path)
    target = out_dir / "containers" / safe_name(path.name)
    raw_dir = target / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)
    script_dir = target / "disasm"
    random_hits = []
    index = []
    for item in container.get("items", []):
        item_data = item["data"]
        filename = f"{item['id']:04d}_{item['size']:06d}.bin"
        write_bytes(raw_dir / filename, item_data)
        row = {k: v for k, v in item.items() if k != "data"}
        row["sha1"] = hashlib.sha1(item_data).hexdigest()
        row["file"] = str(Path("raw") / filename)
        if disassemble and item["size"]:
            script_dir.mkdir(parents=True, exist_ok=True)
            text, randoms = disassemble_script(item_data)
            disasm_name = f"{item['id']:04d}.txt"
            (script_dir / disasm_name).write_text(text, encoding="utf-8")
            row["disasm"] = str(Path("disasm") / disasm_name)
            for hit in randoms:
                hit["script"] = item["id"]
                hit["script_size"] = item["size"]
                hit["disasm"] = row["disasm"]
                random_hits.append(hit)
        index.append(row)

    summary = {k: v for k, v in container.items() if k != "items"}
    summary["source"] = str(path)
    summary["item_count"] = len(index)
    summary["items"] = index
    if random_hits:
        summary["random_opcode_hits"] = random_hits
        (target / "random-opcode-hits.json").write_text(json.dumps(random_hits, indent=2), encoding="utf-8")
    (target / "index.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return summary


def find_game_file(game_dir: Path, preferred: str | None) -> Path:
    candidates = []
    if preferred:
        candidates.extend([game_dir / preferred, game_dir / (preferred + ".bin")])
    candidates.extend(sorted(game_dir.glob("*.bin")))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise SystemExit(f"could not find MacBinary game file in {game_dir}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("game_dir", type=Path)
    parser.add_argument("--game-file", help="main Macintosh game file name, with or without .bin")
    parser.add_argument("--out", type=Path, default=Path("macventure-dumps"))
    args = parser.parse_args()

    game_dir = args.game_dir
    game_file = find_game_file(game_dir, args.game_file)
    out_dir = args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    resource_summary = export_resource_fork(game_file, out_dir)
    strings = resource_summary.get("strings", {})
    filename_table = []
    for values in strings.values():
        if len(values) >= 8 and values[0].startswith("MCV"):
            filename_table = values
            break
    data_subdir = None
    container_paths = []
    for idx, name in enumerate(filename_table, start=1):
        if idx == 3:
            data_subdir = child_path(game_dir, name)
        if idx in (4, 5, 6, 7, 8):
            base = data_subdir or game_dir
            candidate = child_path(base, name)
            if candidate.exists():
                container_paths.append((idx, candidate))
    if not container_paths:
        for candidate in sorted(game_dir.rglob("*")):
            if candidate.is_file() and candidate.suffix.lower() != ".bin":
                container_paths.append((0, candidate))

    containers = []
    for path_id, container_path in container_paths:
        kind = FILE_PATH_IDS.get(path_id, "unknown")
        summary = export_container(container_path, out_dir, disassemble=(path_id == 5 or "filter" in container_path.name.lower()))
        summary["path_id"] = path_id
        summary["kind"] = kind
        containers.append(summary)

    manifest = {
        "game_dir": str(game_dir),
        "game_file": str(game_file),
        "output_dir": str(out_dir),
        "resource_summary": resource_summary,
        "container_count": len(containers),
        "containers": [
            {
                "source": c["source"],
                "path_id": c["path_id"],
                "kind": c["kind"],
                "item_count": c["item_count"],
                "random_opcode_hits": c.get("random_opcode_hits", []),
            }
            for c in containers
        ],
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
