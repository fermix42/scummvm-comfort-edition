/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_STORY_PATCHES_H
#define NOX_NATIVE_STORY_PATCHES_H
#include "nox/resources.h"
namespace Nox { namespace Native {
struct StoryMapState {
    byte terrain[2304], objects[256], actors[256], spawns[256];
};
// The original authored map-change data has four typed effect kinds. These
// are resource rules, never CPU instructions or arbitrary memory writes.
class StoryMapPatches {
public:
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;
        if (!archive.read("NA/BS.ROUTINES.BK2",data) || data.size() != 12286) return false;
        memcpy(_neighbors,data.data() + 0x28d3,9);
        _rules.clear(); uint cursor = 0x28f2;
        while (cursor < data.size() && !(data[cursor] & 128)) {
            if (data.size() - cursor < 3 || data[cursor + 2] < 7 || data[cursor + 2] > data.size() - cursor) return false;
            Rule rule; rule.bytes.resize(data[cursor + 2]); memcpy(rule.bytes.data(),data.data() + cursor,rule.bytes.size());
            _rules.push_back(rule); cursor += rule.bytes.size();
        }
        return cursor < data.size() && data[cursor] == 255 && _rules.size() == 11;
    }
    bool apply(byte type, byte map, byte centerSector, const byte *fields, uint size, StoryMapState &state) const {
        if (_rules.size() != 11 || !fields || size != 256) return false;
        byte sectors[9];
        for (uint i = 0; i < 9; ++i)
            sectors[i] = type >= 3 ? byte((i / 3) * 16 + i % 3) :
                byte((_neighbors[i] & 128) ? centerSector + (_neighbors[i] & 127) : centerSector - _neighbors[i]);
        StoryMapState next = state;
        for (uint i = 0; i < _rules.size(); ++i) {
            const Common::Array<byte> &r = _rules[i].bytes;
            if (r[0] != map) continue;
            uint slot = 0; while (slot < 9 && sectors[slot] != r[1]) ++slot;
            if (slot == 9) continue;
            const byte value = (r[4] & 128) ? fields[r[3]] : byte(fields[r[3]] ^ 255);
            if (!(value & r[5])) continue;
            uint at = 6;
            while (at < r.size() && !(r[at] & 128)) {
                const byte kind = r[at++];
                if (!kind) {
                    if (r.size() - at < 3) return false;
                    const uint offset = (slot % 3) * 16 + (slot / 3) * 768 + uint(r[at]) + uint(r[at + 1]) * 256;
                    if (offset >= sizeof(next.terrain)) return false;
                    next.terrain[offset] = r[at + 2]; at += 3;
                } else {
                    if ((kind != 1 && kind != 3 && kind != 4) || r.size() - at < 5) return false;
                    const uint index = r[at++];
                    if (index > 252) return false;
                    byte *record = (kind == 1 ? next.objects : kind == 3 ? next.actors : next.spawns) + index;
                    memcpy(record,r.data() + at,4); at += 4;
                    if (kind == 3) { if (index > 250) return false; record[5] = 127; }
                }
            }
            if (at >= r.size() || r[at] != 255) return false;
        }
        state = next; return true;
    }
private:
    struct Rule { Common::Array<byte> bytes; };
    Common::Array<Rule> _rules;
    byte _neighbors[9];
};
} }
#endif
