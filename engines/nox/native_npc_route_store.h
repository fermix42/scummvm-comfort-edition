/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ROUTE_STORE_H
#define NOX_NATIVE_NPC_ROUTE_STORE_H
#include "nox/native_npc_schedule.h"
namespace Nox { namespace Native {
enum NpcRouteAllocation { kNpcRouteAllocated, kNpcRouteExisting, kNpcRouteFull, kNpcRouteInvalid };
// Owned route pages. Request keys retain logical actor slots and page IDs.
// Scanning order is observable: an existing key returns before lower stale slots.
class NpcRouteStore {
    friend class NpcRouteFollower;
    friend class NpcRouteRecovery;
public:
    NpcRouteStore() : _readyMask(0) { memset(_data,0,sizeof(_data)); }
    const byte *data() const { return _data; }
    bool hasPage(byte page) const { return page < 14 && (_readyMask & (1U << page)); }
    bool importPages(const byte *data, uint size) {
        if (!data || size != sizeof(_data)) return false;
        memcpy(_data,data,sizeof(_data)); _readyMask = 0x3fff; return true;
    }
    NpcRouteAllocation allocate(const World &world, NpcScheduleState &state, byte goal, byte actorSlot, const byte *payload, uint size) {
        if (!world.ready() || !state.ready() || (actorSlot & 7) || !payload || size != 256) return kNpcRouteInvalid;
        NpcScheduleState next = state; int free = -1;
        const byte *actors = world.currentTables()->data() + 1280;
        for (int slot = 13; slot >= 0; --slot) {
            byte *request = next._requests + slot * 4;
            if (!(request[2] & 128)) {
                if (request[1] & 7) return kNpcRouteInvalid;
                if (actors[request[1] + 3] == 255) request[2] = 255;
            }
            if (request[2] & 128) free = slot;
            else if (request[0] == goal && request[1] == actorSlot) {
                state = next; return kNpcRouteExisting;
            }
        }
        if (free < 0) return kNpcRouteFull;
        byte *request = next._requests + free * 4;
        request[0] = goal; request[1] = actorSlot; request[2] = 1; request[3] = byte(free);
        memcpy(_data + free * 256,payload,256); _readyMask |= uint16(1U << free); state = next;
        return kNpcRouteAllocated;
    }
    bool save(Common::WriteStream &output) const {
        output.writeUint32BE(MKTAG('N','R','T','S')); output.writeUint16LE(1);
        output.writeUint16LE(_readyMask); output.write(_data,sizeof(_data)); return !output.err();
    }
    bool load(Common::SeekableReadStream &input) {
        if (input.size() - input.pos() != 3592 || input.readUint32BE() != MKTAG('N','R','T','S') || input.readUint16LE() != 1) return false;
        NpcRouteStore next; next._readyMask = input.readUint16LE();
        if (next._readyMask & 0xc000 || input.read(next._data,sizeof(next._data)) != sizeof(next._data) || input.err()) return false;
        *this = next; return true;
    }
private:
    uint16 _readyMask; byte _data[3584];
};
} }
#endif
