/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_STEP_H
#define NOX_NATIVE_ACTOR_STEP_H
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
struct ActorStepProbe {
    byte terrain,actorTile,objectTile,objectState,previousTerrain;
    bool occupied,objectPresent,large,restrictedTerrain,unrestricted;
};
struct ActorStepDecision {
    bool blocked; byte effectiveTerrain;
};
// Complete world-actor destination probe, distinct from the player's rules.
// The AI owner supplies actual occupancy, actor traits and prior candidate tile.
// Navigation choice, movement/attack commit and battle contexts are separate.
class ActorStepRules {
public:
    ActorStepRules() : _ready(false),_type(255) {}
    bool load(HdvArchive &archive, byte type) {
        Common::Array<byte> game,footer; MapAssetRange range;
        if (!archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset + range.size,72,footer)) return false;
        memcpy(_footer,footer.data(),72); _type = type; _ready = true; return true;
    }
    bool probe(const ActorStepProbe &input, ActorStepDecision &output,bool tactical=false) const {
        if (!_ready) return false;
        ActorStepDecision result; result.blocked = false; result.effectiveTerrain = input.previousTerrain;
        if (input.unrestricted) { output = result; return true; }
        if (input.occupied) { result.blocked = true; output = result; return true; }
        const byte tile = input.objectPresent ? input.objectTile : input.terrain;
        result.effectiveTerrain = tile;
        if (input.objectPresent) {
            if (in(tile,52) || (input.objectState >= 17 && input.objectState < 32)) {
                result.blocked = true; output = result; return true;
            }
            if (input.objectState == 16 || (input.objectState >= 33 && input.objectState < 42) || input.objectState == 254) {
                output = result; return true;
            }
        }
        if (!input.large && input.restrictedTerrain) {
            const byte terrain = input.terrain;
            result.blocked = !(terrain == 232 || terrain == 92 || terrain == 70 || (terrain >= 93 && terrain < 117) || (tactical && terrain==64));
        } else if (in(input.actorTile,60)) {
            result.blocked = input.objectPresent || input.terrain < 229 || input.terrain >= 233;
        } else {
            result.blocked = (in(tile,6) && !(!tactical && _type == 5 && (tile == 69 || tile == 70))) ||
                in(tile,8) || in(tile,10) || in(tile,26) || (!tactical && _type == 7 && tile >= 119 && tile < 121) || tile == _footer[67];
        }
        output = result; return true;
    }
    bool probeTacticalLarge(byte terrain,byte actorTile,bool occupied,bool objectPresent,ActorStepDecision &output) const {
        if (!_ready) return false;ActorStepDecision result;result.effectiveTerrain=terrain;
        if (in(actorTile,60)) result.blocked=occupied || objectPresent || terrain<229 || terrain>=233;
        else result.blocked=occupied || objectPresent || in(terrain,6) || in(terrain,8) || in(terrain,10) || in(terrain,26) || terrain==_footer[67];
        output=result;return true;
    }
private:
    bool in(byte value, uint offset) const { return value >= _footer[offset] && value < _footer[offset + 1]; }
    bool _ready; byte _type,_footer[72];
};
} }
#endif
