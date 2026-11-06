/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTORS_H
#define NOX_NATIVE_ACTORS_H
#include "common/scummsys.h"
namespace Nox { namespace Native {
enum ActorAdmission { kActorAbsent, kActorPresent, kActorRemoved, kActorInvalid };
struct WorldActor {
    byte x, y, tile, state, capability, flags, kind;
    bool large;
    WorldActor() : x(0), y(0), tile(0), state(0), capability(0), flags(0), kind(0), large(false) {}
};
// World-only actor admission and quest-driven removal. The source's compact
// and extended records remain in the owning map; no machine frame is needed.
inline ActorAdmission admitWorldActor(byte *tables, uint size, const byte *fields, uint fieldSize,
        byte bank, uint slot, bool presentation, WorldActor &output) {
    if (!tables || size != 1792 || !fields || fieldSize != 256 || bank > 1 || slot > 252 || (slot & 3)) return kActorInvalid;
    byte *compact = tables + 768 + slot;
    byte *record = bank ? tables + 1280 + slot : compact;
    if (!record[2] || record[3] == 255) return kActorAbsent;
    if (presentation) {
        if (!bank && (record[3] & 4) && (fields[record[5]] & record[7])) { record[2] = 0; return kActorRemoved; }
        if (bank) {
            const byte identity = record[5] & 127;
            if (identity >= 34 && identity < 66) {
                const uint index = identity - 34;
                if (fields[34 + index / 8] & (1 << (index % 8))) { record[3] = 255; return kActorRemoved; }
            }
            if (record[5] == 126 && (compact[0] & 128) && (fields[compact[1]] & compact[3])) {
                record[3] = 255; record[7] = 255; return kActorRemoved;
            }
        }
    }
    WorldActor next; next.x = record[0]; next.y = record[1]; next.tile = record[2]; next.state = record[3];
    if (bank) { next.capability = record[6]; next.flags = record[7]; next.kind = 4; next.large = (record[3] & 16) != 0; }
    else next.large = (record[3] & 8) != 0;
    output = next; return kActorPresent;
}
} }
#endif
