#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent bounded reader for save-associated enhancement acceptance checks."""
import hashlib
import struct


class Reader:
    def __init__(self, data):
        self.data, self.position = data, 0

    def number(self, fmt):
        size = struct.calcsize(fmt)
        assert size <= len(self.data) - self.position
        value = struct.unpack_from(fmt, self.data, self.position)[0]
        self.position += size
        return value

    def text(self, maximum):
        size = self.number("<I")
        assert size <= maximum and size <= len(self.data) - self.position
        data = self.data[self.position:self.position + size]
        self.position += size
        assert b"\0" not in data
        return data.decode("utf-8")

    def flag(self):
        value = self.number("<B")
        assert value in (0, 1)
        return bool(value)


def read_save_enhancements(path):
    raw = path.read_bytes()
    assert raw[:4] == b"NXSV"
    version, sequence, length = struct.unpack_from("<III", raw, 4)
    assert version == 2 and sequence > 0 and length <= 20 * 1024 * 1024
    payload, footer = raw[32:32 + length], raw[32 + length:]
    assert len(payload) == length and footer == b"NXOK"
    assert hashlib.md5(raw[:16] + payload + footer).digest() == raw[16:32]
    assert payload[:4] == b"NSM2"
    metadata_size = struct.unpack_from("<I", payload, 4)[0]
    assert metadata_size <= 65536 and metadata_size + 8 <= len(payload)
    metadata, offset, texts = payload[8:8 + metadata_size], 0, []
    for _ in range(3):
        size = struct.unpack_from("<H", metadata, offset)[0]
        assert offset + 2 + size <= len(metadata)
        texts.append(metadata[offset + 2:offset + 2 + size].decode("utf-8"))
        offset += 2 + size
    slot, _playtime = struct.unpack_from("<II", metadata, offset)
    offset += 8
    chunks = {}
    while offset < len(metadata):
        tag, flags, size = struct.unpack_from("<4sBI", metadata, offset)
        offset += 9
        assert flags in (0, 1) and size <= len(metadata) - offset and tag not in chunks
        chunks[tag] = metadata[offset:offset + size]
        offset += size
    assert chunks[b"MODE"] in (b"\0", b"\1") and slot <= 999
    runtime = payload[8 + metadata_size:]
    assert struct.unpack_from("<I", runtime)[0] == 2
    # Opaque compatibility bytes can contain any values. Require one terminal,
    # structurally complete chunk rather than trusting the first magic match.
    candidates, cursor = [], 0
    while True:
        cursor = runtime.find(b"NXEH", cursor)
        if cursor < 0:
            break
        if cursor + 12 <= len(runtime):
            chunk_version, size = struct.unpack_from("<II", runtime, cursor + 4)
            if chunk_version == 1 and size <= 16 * 1024 * 1024 and cursor + 12 + size == len(runtime):
                candidates.append(runtime[cursor + 12:])
        cursor += 4
    assert len(candidates) == 1
    reader = Reader(candidates[0])
    result = {"target": texts[0], "session": texts[1], "description": texts[2], "sequence": reader.number("<Q"), "characters": [], "cells": []}
    result["slot"] = slot
    result["saved_mode"] = "Classic" if chunks[b"MODE"] == b"\1" else "Modern"
    assert result["sequence"] <= 16384
    count = reader.number("<I")
    assert count <= 1024
    identities, reply_sequences = set(), []
    for _ in range(count):
        character = {"resource": reader.text(128), "record": reader.number("<H"), "actor": reader.number("<H"), "name": reader.text(128), "default_response": reader.text(8192), "topics": [], "replies": []}
        identity = (character["resource"], character["record"])
        assert identity not in identities
        identities.add(identity)
        topic_count = reader.number("<I")
        assert topic_count <= 2048
        for _ in range(topic_count):
            character["topics"].append({"key": reader.text(4), "label": reader.text(8192), "used": reader.flag()})
        reply_count = reader.number("<I")
        assert reply_count + len(reply_sequences) <= 16384
        for _ in range(reply_count):
            reply = {"sequence": reader.number("<Q"), "input": reader.text(64), "keyword": reader.text(4), "response": reader.text(8192), "opening": reader.flag(), "recognized": reader.flag()}
            character["replies"].append(reply)
            reply_sequences.append(reply["sequence"])
        result["characters"].append(character)
    assert sorted(reply_sequences) == list(range(1, result["sequence"] + 1))
    count = reader.number("<I")
    assert count <= 65536
    identities = set()
    for _ in range(count):
        cell = {"resource": reader.text(128), "map_type": reader.number("<B"), "map_id": reader.number("<B"), "x": reader.number("<B"), "y": reader.number("<B"), "observed": reader.flag(), "revealed": reader.flag(), "annotation": reader.text(1024)}
        identity = tuple(cell[key] for key in ("resource", "map_type", "map_id", "x", "y"))
        assert identity not in identities and (cell["observed"] or cell["revealed"])
        identities.add(identity)
        result["cells"].append(cell)
    assert reader.position == len(reader.data)
    return result
