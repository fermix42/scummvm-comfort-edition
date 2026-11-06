/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_SCHEDULE_H
#define NOX_NATIVE_NPC_SCHEDULE_H
#include "nox/native_world.h"
namespace Nox { namespace Native {
// Tagged data reference for the original castle15 reserved-marker quirk.
// This is not a CPU address or an instruction execution escape.
enum { kNpcCastleMarkerData = 65534 };
struct NpcScheduleSelection {
    byte hour,minute; uint16 offset;
    NpcScheduleSelection() : hour(255),minute(255),offset(65535) {}
};
class NpcScheduler;
// Pointer-free schedule selections refer to data offsets in the active map.
// Global goal coordinates and request records persist across map changes.
class NpcScheduleState {
    friend class NpcRouteFollower;
    friend class NpcRouteRecovery;
    friend class NpcNavigation;
    friend class NpcScheduler;
    friend class NpcRouteStore;
public:
    NpcScheduleState() : _ready(false),_approachCount(0),_pathBusy(0) {
        memset(_x,0,160); memset(_y,0,160); memset(_requests,255,56); memset(_recent,255,60);
    }
    bool initialize(HdvArchive &archive) {
        if (_ready) return false;
        Common::Array<byte> game; if (!archive.read("NA/GAME",game) || game.size() != 13824) return false;
        memcpy(_x,game.data() + 0xed9,160); memcpy(_y,game.data() + 0xf79,160); _ready = true; return true;
    }
    bool ready() const { return _ready; }
    const byte *goalX() const { return _x; } const byte *goalY() const { return _y; }
    const byte *requests() const { return _requests; } const byte *recent() const { return _recent; }
    byte approachCount() const { return _approachCount; } byte pathBusy() const { return _pathBusy; }
    const NpcScheduleSelection *selection(uint actor) const { return _ready && actor < 32 ? &_selected[actor] : nullptr; }
    bool save(Common::WriteStream &output) const {
        if (!_ready) return false;
        output.writeUint32BE(MKTAG('N','N','P','S')); output.writeUint16LE(2); output.write(_x,160); output.write(_y,160);
        for (uint i = 0; i < 32; ++i) { output.writeByte(_selected[i].hour); output.writeByte(_selected[i].minute); output.writeUint16LE(_selected[i].offset); }
        output.write(_requests,56); output.write(_recent,60); output.writeByte(_approachCount); output.writeByte(_pathBusy); return !output.err();
    }
    bool load(Common::SeekableReadStream &input) {
        if (input.size() - input.pos() != 572 || input.readUint32BE() != MKTAG('N','N','P','S')) return false;
        const uint16 version = input.readUint16LE(); if (version != 1 && version != 2) return false;
        NpcScheduleState next;
        if (input.read(next._x,160) != 160 || input.read(next._y,160) != 160) return false;
        for (uint i = 0; i < 32; ++i) {
            NpcScheduleSelection &entry = next._selected[i]; entry.hour = input.readByte(); entry.minute = input.readByte(); entry.offset = input.readUint16LE();
            if (entry.offset != 65535 && !(version == 2 && i == 0 && entry.offset == kNpcCastleMarkerData) &&
                (entry.offset >= 768 || (entry.offset & 7))) return false;
        }
        if (input.read(next._requests,56) != 56 || input.read(next._recent,60) != 60) return false;
        next._approachCount = input.readByte(); next._pathBusy = input.readByte(); if (input.err()) return false;
        next._ready = true; *this = next; return true;
    }
private:
    bool _ready; byte _x[160],_y[160],_requests[56],_recent[60],_approachCount,_pathBusy;
    NpcScheduleSelection _selected[32];
};
struct NpcSchedulePolicy { bool keepSpecialActors,keepSpecialBehavior; };
enum NpcScheduleResult { kNpcScheduleApplied,kNpcScheduleSkipped,kNpcScheduleUnsupported };
class NpcScheduler {
public:
    // Queue authored upcoming route segments. Records keep logical NPC slots,
    // start/end goals and coordinates; no bank addresses or CPU pointers.
    // Path generation consumes this queue in a subsequent phase.
    static NpcScheduleResult prefetchAtClock(const World &world, byte hour, byte minute, NpcScheduleState &state) {
        if (!world.ready() || !state.ready()) return kNpcScheduleUnsupported;
        if (world.mapType() < 4 || world.mapId() == 69 || state._pathBusy) return kNpcScheduleSkipped;
        if (hour >= 0x24 || (hour & 15) > 9 || minute >= 0x60 || (minute & 15) > 9) return kNpcScheduleUnsupported;
        const uint currentHour = (hour >> 4) * 10 + (hour & 15),currentMinute = (minute >> 4) * 10 + (minute & 15);
        const byte *table = world.currentTables()->data(); NpcScheduleState next = state; bool terminated = false;
        for (uint offset = 0; offset < 768; offset += 8) {
            const byte *row = table + offset;
            if (offset && (row[0] & 128)) { terminated = true; break; }
            if ((row[0] & 128) && row[0] != 255) return kNpcScheduleUnsupported;
            if (row[2] >= 0x24 || (row[2] & 15) > 9 || row[3] >= 0x60 || (row[3] & 15) > 9) return kNpcScheduleUnsupported;
            const uint futureHour = (row[2] >> 4) * 10 + (row[2] & 15),futureMinute = (row[3] >> 4) * 10 + (row[3] & 15);
            const uint distance = (futureHour + 24 - currentHour) % 24;
            if (!((distance == 0 && futureMinute > currentMinute) || distance == 1)) continue;
            int free = -1; bool handled = false;
            for (int slot = 9; slot >= 0; --slot) {
                byte *cached = next._recent + slot * 6;
                if (!(cached[5] & 128)) {
                    if (cached[1] & 7) return kNpcScheduleUnsupported;
                    if (table[1280 + cached[1] + 3] == 255) cached[5] = 255;
                }
                if (cached[5] & 128) free = slot;
                // Source sentinel skips after this slot's stale-entry cleanup.
                if (row[0] == 255 || (cached[1] == row[1] && !(cached[5] & 128))) { handled = true; break; }
            }
            if (handled) continue;
            // A full queue invokes an original error presentation. Keep this
            // unqualified boundary explicit until that presentation is native.
            if (free < 0) return kNpcScheduleUnsupported;
            if (row[0] == row[4]) continue;
            if (row[1] & 7) return kNpcScheduleUnsupported;
            byte *cached = next._recent + free * 6;
            cached[0] = row[0]; cached[1] = row[1]; cached[2] = row[5]; cached[3] = row[6]; cached[4] = row[4]; cached[5] = 0;
        }
        if (!terminated) return kNpcScheduleUnsupported;
        state = next; return kNpcScheduleApplied;
    }
    // Authored clock-boundary activation only. Route prefetch and route
    // generation run after this phase and retain their own native owners.
    static NpcScheduleResult activateAtClock(World &world, byte hour, byte minute) {
        if (!world.ready()) return kNpcScheduleUnsupported;
        if (world.mapType() < 4 || world.mapId() == 69) return kNpcScheduleSkipped;
        if (hour >= 0x24 || (hour & 15) > 9 || minute >= 0x60 || (minute & 15) > 9) return kNpcScheduleUnsupported;
        World next(world); byte *owned = next._states[next._current].tables.data(); bool terminated = false;
        for (uint offset = 0; offset < 768; offset += 8) {
            const byte *row = owned + offset;
            if (offset && (row[0] & 128)) { terminated = true; break; }
            if ((row[0] & 128) && row[0] != 255) return kNpcScheduleUnsupported;
            if (row[1] & 7) {
                const byte padding[8] = {0,1,0,0,0,0,0,0};
                if (world.mapType() != 7 || world.mapId() != 15 || offset < 224 || offset > 248 || memcmp(row,padding,8)) return kNpcScheduleUnsupported;
                // Source padding overlaps the reserved marker's identity and
                // flags, then the following NPC's X coordinate. These are
                // bounded authored data effects, not machine pointer writes.
                if (row[2] == hour && row[3] == minute && owned[1285] != row[0]) {
                    owned[1285] = row[0]; owned[1287] = 3; owned[1288] = row[7];
                }
            } else if (row[0] != 255 && row[2] == hour && row[3] == minute) {
                byte *actor = owned + 1280 + row[1];
                if (actor[4] != row[0]) { actor[4] = row[0]; actor[6] = 3; actor[7] = row[7]; }
            }
        }
        if (!terminated) return kNpcScheduleUnsupported;
        world.adoptState(next); return kNpcScheduleApplied;
    }
    static NpcScheduleResult apply(World &world, byte hour, byte minute, const NpcSchedulePolicy &policy, NpcScheduleState &state) {
        if (!world.ready() || !state.ready()) return kNpcScheduleUnsupported;
        // Actual resident caller excludes nonlocal families before invoking the
        // initializer; its own ceremonial-map guard returns without any reset.
        if (world.mapType() < 4 || world.mapId() == 69) return kNpcScheduleSkipped;
        if (hour >= 0x24 || (hour & 15) > 9 || minute >= 0x60 || (minute & 15) > 9) return kNpcScheduleUnsupported;
        World nextWorld(world); NpcScheduleState next = state; bool previousDay[32] = {},terminated = false;
        next._approachCount = next._pathBusy = 0; memset(next._requests,255,56); memset(next._recent,255,60);
        for (uint i = 0; i < 32; ++i) next._selected[i] = NpcScheduleSelection();
        const byte *table = nextWorld.currentTables()->data();
        for (uint offset = 0; offset < 768; offset += 8) {
            const byte *row = table + offset;
            if (offset && (row[0] & 128)) { terminated = true; break; }
            if ((row[0] & 128) && row[0] != 255) return kNpcScheduleUnsupported;
            if (row[1] & 7) {
                // Four authored castle15 padding rows overlap the reserved
                // marker's selection descriptor. Preserve their observed data
                // effect with a typed reference; do not reproduce RAM pointers.
                const byte padding[8] = {0,1,0,0,0,0,0,0};
                if (world.mapType() != 7 || world.mapId() != 15 || offset < 224 || offset > 248 ||
                    memcmp(row,padding,8) || (table[1285] != 255 && table[1285] != 0)) return kNpcScheduleUnsupported;
                NpcScheduleSelection &marker = next._selected[0];
                if (marker.minute & 128) {
                    marker.hour = 1; marker.minute = 0; marker.offset = kNpcCastleMarkerData; previousDay[0] = false;
                }
                next._x[0] = next._y[0] = 0; continue;
            }
            const uint actor = row[1] / 8; NpcScheduleSelection &selected = next._selected[actor];
            const bool future = row[2] > hour || (row[2] == hour && row[3] > minute);
            bool consider = true;
            if (future && !previousDay[actor]) {
                if (selected.hour & 128) previousDay[actor] = true;
                else consider = false;
            }
            if (consider && ((selected.hour & 128) || selected.hour < row[2] || (selected.hour == row[2] && selected.minute < row[3]))) {
                selected.hour = row[2]; selected.minute = row[3]; selected.offset = uint16(offset);
            }
            if (row[0] == 255) { terminated = true; break; }
            if (row[0] >= 160) return kNpcScheduleUnsupported;
            next._x[row[0]] = row[5]; next._y[row[0]] = row[6];
        }
        if (!terminated) return kNpcScheduleUnsupported;
        Common::Array<byte> markerData;
        if (next._selected[0].offset == kNpcCastleMarkerData &&
            (!world._archive.read("NA/BS.ROUTINES.BK2",markerData) || markerData.size() < 0x1008)) return kNpcScheduleUnsupported;
        byte *owned = nextWorld._states[nextWorld._current].tables.data();
        for (uint i = 0; i < 32; ++i) {
            const NpcScheduleSelection &entry = next._selected[i]; if (entry.minute == 255) continue;
            byte *record = owned + 1280 + i * 8;
            if (policy.keepSpecialActors && (record[6] & 128)) continue;
            const byte *row = entry.offset == kNpcCastleMarkerData ? markerData.data() + 0x1000 : owned + entry.offset;
            record[4] = row[0]; record[0] = row[5]; record[1] = row[6];
            if (!policy.keepSpecialBehavior || !(record[6] & 128)) record[6] = 0;
            record[7] = row[7];
        }
        world.adoptState(nextWorld); state = next; return kNpcScheduleApplied;
    }
};
} }
#endif
