/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_NAVIGATION_H
#define NOX_NATIVE_NPC_NAVIGATION_H
#include "nox/native_npc_path.h"
#include "nox/native_npc_route_store.h"
#include "common/memstream.h"
namespace Nox { namespace Native {
enum NpcNavigationResult { kNpcNavigationIdle, kNpcNavigationSearching, kNpcNavigationStored, kNpcNavigationNoRoute, kNpcNavigationCapacity, kNpcNavigationInvalid };
// Background path owner: pending authored queue -> cooperative search -> route
// allocation. The actual caller owns clock/prefetch, error UI and actor turns.
class NpcNavigation {
public:
    NpcNavigation() : _active(false),_slot(255),_type(255),_map(255) {}
    bool active() const { return _active; }
    NpcNavigationResult advance(HdvArchive &archive, const World &world, const WorldStory &window,
            NpcScheduleState &schedule, NpcRouteStore &routes, GameplayRandom &random) {
        if (!world.ready() || !window.state() || !schedule.ready()) return kNpcNavigationInvalid;
        bool stored = false;
        for (uint budget = 0; budget < 10; ++budget) {
            if (!_active) {
                int pending = -1;
                for (uint i = 0; i < 10; ++i) if (schedule._recent[i * 6 + 5] == 0) { pending = int(i); break; }
                if (pending < 0) { schedule._pathBusy = 0; return stored ? kNpcNavigationStored : kNpcNavigationIdle; }
                const byte *entry = schedule._recent + pending * 6;
                if ((entry[1] & 7) || entry[4] >= 160 || entry[2] >= 48 || entry[3] >= 48) return kNpcNavigationInvalid;
                NpcPathSearch search;
                if (!search.configure(archive,world.mapType()) || !search.start(schedule._x[entry[4]],schedule._y[entry[4]],entry[2],entry[3])) return kNpcNavigationInvalid;
                _search = search; _active = true; _slot = byte(pending); _type = world.mapType(); _map = world.mapId();
            }
            if (_type != world.mapType() || _map != world.mapId() || _slot >= 10 || schedule._recent[_slot * 6 + 5] != 0) return kNpcNavigationInvalid;
            if (_search.status() == kNpcPathSearching) {
                schedule._pathBusy = 0;
                const NpcPathStatus result = _search.advance(window,random);
                if (result == kNpcPathSearching) { schedule._pathBusy = 1; return kNpcNavigationSearching; }
                if (result == kNpcPathUnsupported) return kNpcNavigationInvalid;
            }
            if (_search.status() == kNpcPathError) return kNpcNavigationNoRoute;
            Common::Array<Common::Point> points;
            if (!_search.routePoints(points) || points.size() < 2 || points.size() > 128) return kNpcNavigationInvalid;
            byte payload[256]; memset(payload,170,256); payload[0] = byte((points.size() - 1) * 2);
            for (uint i = 1; i < points.size(); ++i) {
                const Common::Point &point = points[points.size() - i]; payload[i * 2] = byte(point.x); payload[i * 2 + 1] = byte(point.y);
            }
            const byte *entry = schedule._recent + _slot * 6;
            const NpcRouteAllocation result = routes.allocate(world,schedule,entry[0],entry[1],payload,256);
            if (result == kNpcRouteFull) return kNpcNavigationCapacity;
            if (result == kNpcRouteInvalid) return kNpcNavigationInvalid;
            schedule._recent[_slot * 6 + 5] = 255; _active = false; _slot = 255; stored = true;
        }
        return stored ? kNpcNavigationStored : kNpcNavigationSearching;
    }
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','N','A','V')); out.writeUint16LE(1); out.writeByte(_active); out.writeByte(_slot); out.writeByte(_type); out.writeByte(_map);
        Common::MemoryWriteStreamDynamic search(DisposeAfterUse::YES); if (_active && !_search.save(search)) return false;
        out.writeUint32LE(search.size()); if (search.size()) out.write(search.getData(),search.size()); return !out.err();
    }
    bool load(Common::SeekableReadStream &in, HdvArchive &archive, const World &world, const NpcScheduleState &schedule) {
        if (in.size() - in.pos() < 14 || in.readUint32BE() != MKTAG('N','N','A','V') || in.readUint16LE() != 1) return false;
        NpcNavigation next; const byte active = in.readByte(); next._slot = in.readByte(); next._type = in.readByte(); next._map = in.readByte(); const uint32 size = in.readUint32LE();
        if (active > 1 || size != in.size() - in.pos()) return false; next._active = active != 0;
        if (next._active) {
            if (!size || next._slot >= 10 || !world.ready() || !schedule.ready() || next._type != world.mapType() || next._map != world.mapId() || schedule._recent[next._slot * 6 + 5] != 0) return false;
            if (!next._search.load(in,archive) || next._search.referenceVisitProfile()) return false;
        } else if (size || next._slot != 255) return false;
        if (in.err()) return false; *this = next; return true;
    }
private:
    bool _active; byte _slot,_type,_map; NpcPathSearch _search;
};
} }
#endif
