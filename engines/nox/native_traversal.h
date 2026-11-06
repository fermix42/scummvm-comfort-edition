/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TRAVERSAL_H
#define NOX_NATIVE_TRAVERSAL_H
#include "nox/native_graphics.h"
namespace Nox {
namespace Native {

struct TraversalProbe {
	byte terrain, objectTile, objectState, movementFlag;
	bool actorPresent, objectPresent;
	TraversalProbe() : terrain(0), objectTile(0), objectState(0), movementFlag(0), actorPresent(false), objectPresent(false) {}
};
enum TraversalReason { kTraversalActor, kTraversalBlockingObject, kTraversalPassableObject, kTraversalMovementFlag,
	kTraversalInteraction, kTraversalRange0, kTraversalRange1, kTraversalRange2, kTraversalCastleRestriction, kTraversalClear };
struct TraversalDecision {
	bool blocked, requiresInteraction;
	byte effectiveTile;
	TraversalReason reason;
	TraversalDecision() : blocked(false), requiresInteraction(false), effectiveTile(0), reason(kTraversalClear) {}
};

// Reuses the original independently recovered traversal semantics. Rules are
// asset data; actor presence/movement mode must come from their owning phase.
class TraversalRules {
public:
	TraversalRules() : _ready(false), _type(255) {}
	bool load(HdvArchive &archive, byte type) {
		Common::Array<byte> game, ranges; MapAssetRange assets;
		if (!archive.read("NA/GAME", game) || !resolveMapAssets(game, type, assets) || !archive.readRange(assets.resource, assets.offset + assets.size, 6, ranges)) return false;
		memcpy(_ranges, ranges.data(), sizeof(_ranges));
		// Original lookup examines eight overlapping two-byte windows, from
		// last to first. Preserve that data interpretation, including inactive
		// entries, rather than invent a different script dispatch table.
		for (uint i = 0; i < 8; ++i) { _scripts[i][0] = game[0x102a + i]; _scripts[i][1] = game[0x102b + i]; }
		_type = type; _ready = true; return true;
	}
	byte mapType() const { return _type; }
	const byte *ranges() const { return _ranges; }
	const byte *scripts() const { return &_scripts[0][0]; }
	bool probe(const TraversalProbe &input, TraversalDecision &output) const {
		if (!_ready) return false;
		TraversalDecision result; result.effectiveTile = input.terrain;
		if (input.actorPresent) { result.blocked = true; result.reason = kTraversalActor; }
		else if (input.objectPresent && input.objectState >= 16 && input.objectState < 32) {
			result.effectiveTile = input.objectTile; result.blocked = true; result.reason = kTraversalBlockingObject;
		} else if (input.objectPresent && ((input.objectState >= 33 && input.objectState < 42) || input.objectState == 254)) {
			result.effectiveTile = input.objectTile; result.reason = kTraversalPassableObject;
		} else if (input.objectPresent && input.movementFlag == 208) {
			result.effectiveTile = input.objectTile; result.blocked = true; result.reason = kTraversalMovementFlag;
		} else {
			if (input.objectPresent) result.effectiveTile = input.objectTile;
			for (int i = 7; i >= 0; --i)
				if (_scripts[i][0] == _type && _scripts[i][1] == result.effectiveTile) { result.requiresInteraction = true; result.reason = kTraversalInteraction; output = result; return true; }
			for (uint i = 0; i < 3; ++i)
				if (result.effectiveTile >= _ranges[i * 2] && result.effectiveTile < _ranges[i * 2 + 1]) { result.blocked = true; result.reason = TraversalReason(kTraversalRange0 + i); output = result; return true; }
			if (_type == 7 && result.effectiveTile >= 119 && result.effectiveTile < 121) { result.blocked = true; result.reason = kTraversalCastleRestriction; }
		}
		output = result; return true;
	}
private:
	bool _ready; byte _type, _ranges[6], _scripts[8][2];
};

enum Direction { kNorth, kSouth, kEast, kWest };
enum PositionMoveResult { kPositionMoved, kPositionBlocked, kPositionNeedsInteraction, kPositionOutsideMap, kPositionUnsupported };

} // namespace Native
} // namespace Nox
#endif
