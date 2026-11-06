/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VORTEX_TARGETS_H
#define NOX_NATIVE_VORTEX_TARGETS_H
#include "nox/native_traversal.h"
namespace Nox { namespace Native {
// Caller-resolved battle scene data. Actor admission/record ownership must
// supply these cells; this helper has no machine-memory or CPU dependency.
struct BattleActorCell {
    bool present; byte x, y, state, kindFlags;
    BattleActorCell() : present(false), x(0), y(0), state(0), kindFlags(0) {}
};
struct SpellTarget {
    byte cell, x, y, distance;
    SpellTarget(byte c, byte column, byte row, byte d) : cell(c), x(column), y(row), distance(d) {}
};
inline bool collectVortexTargets(byte casterX, byte casterY, Direction direction, const BattleActorCell *cells,
        uint length, Common::Array<SpellTarget> &output) {
    if (!cells || length != 187 || casterX > 10 || casterY > 10 || uint(direction) > uint(kWest)) return false;
    Common::Array<SpellTarget> next;
    for (uint i = 0; i < length; ++i) {
        const BattleActorCell &actor = cells[i]; if (!actor.present) continue;
        if (actor.x > 10 || actor.y > 10) return false;
        if ((actor.x == casterX && actor.y == casterY) || ((actor.state & 8) && !(actor.kindFlags & 64))) continue;
        const uint dx = actor.x > casterX ? actor.x - casterX : casterX - actor.x;
        const uint dy = actor.y > casterY ? actor.y - casterY : casterY - actor.y;
        bool selected = false;
        switch (direction) {
        case kNorth: selected = actor.y < casterY && dx <= dy; break;
        case kSouth: selected = actor.y >= casterY && dx <= dy; break;
        case kEast: selected = actor.x >= casterX && dy < dx; break;
        case kWest: selected = actor.x < casterX && dy < dx; break;
        }
        if (!selected) continue;
        // Original collector's target38 exit reads beyond its viewport. Do
        // not silently invent that behavior or claim a reachable battle bound.
        // Battle ownership must resolve this unsupported context before release.
        if (next.size() >= 37) return false;
        next.push_back(SpellTarget(byte(i), actor.x, actor.y, byte(dx + dy)));
    }
    output = next; return true;
}
} }
#endif
