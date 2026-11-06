/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_H
#define NOX_NATIVE_WORLD_H

#include "nox/native_gates.h"
#include "nox/native_maps.h"
#include "nox/resources.h"
#include "nox/native_conversation.h"
#include "nox/native_traversal.h"
#include "nox/native_objects.h"
#include "nox/native_actors.h"
#include "nox/native_search.h"

namespace Nox {
namespace Native {
class WorldPresentation;

// Native world owner, separate from the compatibility Runtime. Initialization
// is an explicit source-supported profile/import, not a claimed new-game flow.
// Changed maps live here as records; no external session image is authoritative.
class World {
public:
	explicit World(HdvArchive &archive) : _archive(archive), _current(-1), _type(0), _x(0), _y(0), _onFoot(true) {
		memset(_fields, 0, sizeof(_fields)); memset(_loaded, 0, sizeof(_loaded));
	}
	bool initialize(byte type, byte id, byte x, byte y, const byte *fields, uint length, bool onFoot = true) {
		if (ready() || !fields || length != sizeof(_fields) || !travel(type, id, x, y)) return false;
		memcpy(_fields, fields, sizeof(_fields)); _onFoot = onFoot; return true;
	}
	bool ready() const { return _current >= 0; }
	byte mapType() const { return _type; }
	byte mapId() const { return ready() ? _states[_current].id : 0; }
	byte x() const { return _x; }
	byte y() const { return _y; }
    bool onFoot() const { return _onFoot; }
	byte coordinateSector() const { return byte((_y & 240) | (_x >> 4)); }
	byte field(byte index) const { return _fields[index]; }
	const byte *questFields() const { return _fields; }
    bool locationContext(byte &output) const {
        if (!ready()) return false;
        const MapDefinition *definition=_resources[_states[_current].family].find(mapId());
        return definition && definition->dialogueContext(output);
    }
	Common::String dialogueResource() const {
		if (!ready()) return Common::String();
		const MapDefinition *definition = _resources[_states[_current].family].find(mapId());
		if (!definition || definition->metadata.size() < 23) return Common::String();
		Common::String name("NA/DATA.TLK.L");
		for (uint i = 20; i < 23; ++i) {
			const byte c = definition->metadata[i];
			if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return Common::String();
			name += char(c);
		}
		return name;
	}
	bool acceptConversation(const Conversation &conversation) {
		if (!ready() || !conversation.completed() || conversation.context().map != mapId() || conversation.context().mapType != mapType()) return false;
		memcpy(_fields, conversation.fields(), sizeof(_fields)); return true;
	}
	const Common::Array<byte> *currentTables() const { return ready() ? &_states[_current].tables : nullptr; }
    bool notice(byte index,Common::Array<byte> &output) const {
        return ready() && _resources[_states[_current].family].notice(mapId(),index,output);
    }
	ActorAdmission admitActor(byte bank, uint slot, bool presentation, WorldActor &output) {
		if (!ready()) return kActorInvalid;
		return admitWorldActor(_states[_current].tables.data(), 1792, _fields, sizeof(_fields), bank, slot, presentation, output);
	}
	bool terrainAt(byte x, byte y, byte &tile) const {
		return ready() && _resources[_states[_current].family].tile(mapId(), x, y, tile);
	}
	bool applyObjectPhase(uint slot, const byte *visibility, const byte *actors, byte partyObject, byte leftBoundary, ObjectScene &scene) {
		if (!ready() || !visibility || !actors) return false;
		ObjectSceneContext context; context.x = _x; context.y = _y; context.partyObject = partyObject; context.leftBoundary = leftBoundary;
		memcpy(context.visibility, visibility, 187); memcpy(context.actors, actors, 187); memcpy(context.fields, _fields, sizeof(_fields));
		return applyObjectRecord(_states[_current].tables.data() + 1024, 256, slot, context, scene);
	}
	// Direct data-owner projection avoids the original scroll/cache ABI. Local
	// edge padding and changed terrain require their own source-supported owner.
	bool projectTerrain(Common::Array<byte> &output) const {
		if (!ready()) return false;
		const MapDefinition *map = _resources[_states[_current].family].find(mapId());
		if (!map || (!map->compressed && (_x < 8 || _x >= 40 || _y < 5 || _y >= 43))) return false;
		Common::Array<byte> next; next.resize(187);
		for (uint row = 0; row < 11; ++row) for (uint column = 0; column < 17; ++column)
			if (!terrainAt(byte(int(_x) + int(column) - 8), byte(int(_y) + int(row) - 5), next[row * 17 + column])) return false;
		output = next; return true;
	}

	// Position/collision phase only. The actor/visibility owner must resolve the
	// destination occupancy first; -1 means no visible object, otherwise this is
	// a byte offset into the current map's unchanged-format object records.
	// No clock, actors, encounters, camera or scripted interaction runs here.
	PositionMoveResult movePositionPhase(Direction direction, const TraversalRules &rules,
			bool actorPresent, int visibleObjectSlot, TraversalDecision &decision) {
		if (!ready() || !_onFoot || rules.mapType() != _type) return kPositionUnsupported;
		byte nextX = _x, nextY = _y;
		switch (direction) {
		case kNorth: --nextY; break;
		case kSouth: ++nextY; break;
		case kEast: ++nextX; break;
		case kWest: --nextX; break;
		default: return kPositionUnsupported;
		}
		const MapResource &resource = _resources[_states[_current].family];
		const MapDefinition *definition = resource.find(mapId());
		if (!definition || (!definition->compressed && (nextX >= 48 || nextY >= 48))) return kPositionOutsideMap;
		TraversalProbe probe; probe.actorPresent = actorPresent;
		if (!resource.tile(mapId(), nextX, nextY, probe.terrain)) return kPositionUnsupported;
		return commitPositionProbe(nextX,nextY,probe,visibleObjectSlot,rules,decision);
	}

	GateResult open(byte targetX, byte targetY) {
		if (!ready() || distance(_x, targetX) + distance(_y, targetY) != 1) return kGateUnsupported;
		const int slot = objectAt(targetX, targetY);
		if (slot < 0) return kGateUnsupported;
		Common::Array<byte> &tables = _states[_current].tables;
		byte *object = tables.data() + 1024 + slot;
		const byte kind = object[3] & 0xbf;
		GateResult result = kGateUnsupported;
		if (kind == 18) result = keyDoor(object, 256 - slot, QuestFields(_fields, sizeof(_fields)));
		else if (kind == 19) result = compoundDoor(object, 256 - slot, QuestFields(_fields, sizeof(_fields)));
		else if ((kind == 48 || kind == 49) && slot >= 4) {
			ControlPair before(object[-1], object[3], object[2]), after = before;
			result = toggleControl(before, after);
			if (result == kGateAllowed) {
				object[-1] = after.wallState; object[3] = after.controlState; object[2] = after.controlPicture;
			}
			return result;
		}
		if (result == kGateAllowed) {
			// Original Open commits an opened record then presentation settles its
			// transient marker. Keep the actual opened record, not a guessed tile.
			object[2] = byte(object[2] - 1); object[3] = 41;
		}
		return result;
	}

    // One semantic Search attempt on a caller-resolved visible object. Keep
    // retries explicit so input/scheduling can run the required turn boundary.
    SearchResult searchQuestItem(byte targetX, byte targetY, int slot, byte selectedMember, bool cancelled,
            const SearchRules &rules, const ItemCatalog &catalog, SearchState &search,
            GameplayRandom &random, Party &party, Inventory &inventory, bool &promoted) {
        if (!ready() || rules.mapType() != _type || slot < 0 || slot > 244 || (slot & 3) ||
                distance(_x, targetX) + distance(_y, targetY) > 1) return kSearchUnsupported;
        byte *object = _states[_current].tables.data() + 1024 + slot;
        if (object[0] != targetX || object[1] != targetY || !object[2] || object[2] == 255) return kSearchUnsupported;
        if (object[3] & 64) return kSearchUnsupported; // Disarm/confirmation owner still required.
        if ((object[3] & 191) != 53) { promoted = false; return kSearchNothing; }
        if ((object[7] & 192) != 64 || object[4] > 2) return kSearchUnsupported;
        ItemDefinition definition; if (!catalog.definition(object[4], object[5], definition)) return kSearchUnsupported;
        byte nextObject[12], fields[256]; memcpy(nextObject, object, 12); memcpy(fields, _fields, 256);
        Party nextParty = party; Inventory nextInventory = inventory; GameplayRandom nextRandom = random; SearchState nextSearch = search;
        bool advanced = false;
        if (rules.locked(nextObject[2])) {
            if (!fields[33]) { promoted = false; return kSearchNoTools; }
            if (cancelled) { promoted = false; return kSearchCancelled; }
            const CharacterRecord *member = nextParty.member(selectedMember ? selectedMember - 1 : 6);
            if (!member || selectedMember > nextParty.memberCount()) return kSearchUnsupported;
            LockContext context; context.difficulty = nextObject[7] & 63; context.lockSkill = member->bytes[37]; context.trapSkill = member->bytes[31];
            context.map = mapId(); context.selectedMember = selectedMember; context.partyCount = nextParty.memberCount(); context.forceOpen = false; context.cancelled = cancelled;
            LockState state = { fields[33], nextSearch.priorLockAttempts }; Common::Array<LockEffect> effects;
            const LockResult access = rules.locks().attempt(context, state, nextRandom, effects);
            if (access == kLockUnsupported || !applyLockPartyEffects(effects, nextParty, advanced)) return kSearchUnsupported;
            if (access == kLockNoTools) { promoted = false; return kSearchNoTools; }
            if (access == kLockCancelled) { promoted = false; return kSearchCancelled; }
            fields[33] = state.picks; nextSearch.priorLockAttempts = state.priorAttempts;
            if (access != kLockOpened) {
                memcpy(_fields, fields, 256); party = nextParty; random = nextRandom; search = nextSearch; promoted = advanced; return kSearchRetry;
            }
            ++nextObject[2];
        }
        const SearchRewardResult reward = searchQuestReward(nextObject, 12, fields, 256, catalog, nextInventory);
        if (reward == kSearchRewardUnsupported) return kSearchUnsupported;
        memcpy(object, nextObject, 12); memcpy(_fields, fields, 256); party = nextParty; inventory = nextInventory; random = nextRandom; search = nextSearch; promoted = advanced;
        return reward == kSearchRewardAwarded ? kSearchAwarded : reward == kSearchRewardCapacity ? kSearchCapacity : kSearchRepeated;
    }

	GateResult activatePlate() {
		if (!ready()) return kGateUnsupported;
		const int slot = objectAt(_x, _y); if (slot < 0) return kGateUnsupported;
		const byte *object = _states[_current].tables.data() + 1024 + slot;
		byte next = 0;
		const GateResult result = pressurePlate(object, 256 - slot, QuestFields(_fields, sizeof(_fields)), next);
		if (result == kGateAllowed) _fields[object[4]] = next;
		return result;
	}

	GateResult enterPortal() {
		if (!ready()) return kGateUnsupported;
		const int slot = objectAt(_x, _y); if (slot < 0) return kGateUnsupported;
		const byte *object = _states[_current].tables.data() + 1024 + slot;
		PortalDestination destination = { 0, 0, 0, 0 };
		const GateResult result = portal(object, 256 - slot, QuestFields(_fields, sizeof(_fields)), _onFoot, destination);
		if (result != kGateAllowed) return result;
		return travel(destination.mapType, destination.map, destination.x, destination.y) ? kGateAllowed : kGateInvalid;
	}

	bool save(Common::WriteStream &output) const {
		if (!ready() || _states.size() > 76) return false;
		output.writeUint32BE(MKTAG('N','W','L','D')); output.writeUint16LE(1);
		output.writeByte(_type); output.writeByte(mapId()); output.writeByte(_x); output.writeByte(_y); output.writeByte(_onFoot ? 1 : 0);
		output.write(_fields, sizeof(_fields)); output.writeUint16LE(_states.size());
		for (uint i = 0; i < _states.size(); ++i) {
			output.writeByte(_states[i].family); output.writeByte(_states[i].id);
			output.write(_states[i].tables.data(), _states[i].tables.size());
		}
		return !output.err();
	}

	bool load(Common::SeekableReadStream &input) {
		World candidate(_archive);
		if (input.size() - input.pos() < 269 || input.readUint32BE() != MKTAG('N','W','L','D') || input.readUint16LE() != 1) return false;
		candidate._type = input.readByte(); const byte id = input.readByte();
		candidate._x = input.readByte(); candidate._y = input.readByte(); const byte foot = input.readByte();
		if (foot > 1 || input.read(candidate._fields, sizeof(candidate._fields)) != sizeof(candidate._fields)) return false;
		candidate._onFoot = foot != 0;
		const uint count = input.readUint16LE();
		if (!count || count > 76 || input.size() - input.pos() != int64(count) * 1794) return false;
		const int activeFamily = familyFor(candidate._type); if (activeFamily < 0) return false;
		for (uint i = 0; i < count; ++i) {
			ChangedMap map; map.family = input.readByte(); map.id = input.readByte();
			if (map.family >= 7 || !candidate.ensureResource(map.family) || !candidate._resources[map.family].find(map.id)) return false;
			for (uint j = 0; j < candidate._states.size(); ++j)
				if (candidate._states[j].family == map.family && candidate._states[j].id == map.id) return false;
			map.tables.resize(1792);
			if (input.read(map.tables.data(), map.tables.size()) != map.tables.size()) return false;
			candidate._states.push_back(map);
			if (map.family == activeFamily && map.id == id) candidate._current = i;
		}
		if (!candidate.ready() || input.err() || input.pos() != input.size()) return false;
		const MapDefinition *definition = candidate._resources[activeFamily].find(id);
		if (!definition || (!definition->compressed && (candidate._x >= 48 || candidate._y >= 48))) return false;
		_states = candidate._states; _current = candidate._current; _type = candidate._type;
		_x = candidate._x; _y = candidate._y; _onFoot = candidate._onFoot;
		memcpy(_fields, candidate._fields, sizeof(_fields));
		for (uint i = 0; i < 7; ++i) { _resources[i] = candidate._resources[i]; _loaded[i] = candidate._loaded[i]; }
		return true;
	}

private:
	friend class ActorSpatialRules;
	friend class NpcWorldAction;
	friend class CompactWorldAction;
	friend class WorldActorTurn;
	friend class WorldWait;
	friend class ActorMovementEffects;
	friend class WorldPresentation;
	friend class WorldStory;
    friend class WorldTravel;
    friend class WorldInput;
    friend class WorldConversation;
    friend class InventoryActions;
	friend class WorldScenePhase;
	friend class WorldCommand;
	friend class WorldOpen;
	friend class WorldMove;
	friend class WorldViewport;
	friend class NpcScheduler;
	friend class NpcActorActions;
    friend class NpcRouteFollower;
    friend class NpcRouteRecovery;
    friend class NpcActionDispatcher;
	friend class NewGame;
	friend class WorldCheckpointFixture;
    friend class BattleAftermath;
	PositionMoveResult commitPositionProbe(byte nextX, byte nextY, TraversalProbe probe, int visibleObjectSlot,
			const TraversalRules &rules, TraversalDecision &decision) {
		if (visibleObjectSlot != -1) {
			if (visibleObjectSlot < 0 || visibleObjectSlot > 252 || (visibleObjectSlot & 3)) return kPositionUnsupported;
			const byte *object = _states[_current].tables.data() + 1024 + visibleObjectSlot;
			if (object[0] != nextX || object[1] != nextY || !object[2] || object[2] == 255) return kPositionUnsupported;
			probe.objectPresent = true; probe.objectTile = object[2]; probe.objectState = object[3];
		}
		TraversalDecision result;
		if (!rules.probe(probe, result)) return kPositionUnsupported;
		decision = result;
		if (result.blocked) return kPositionBlocked;
		if (result.requiresInteraction) return kPositionNeedsInteraction;
		_x = nextX; _y = nextY; return kPositionMoved;
	}

	void adoptState(const World &source) {
		_states = source._states; _current = source._current; _type = source._type;
		_x = source._x; _y = source._y; _onFoot = source._onFoot;
		memcpy(_fields, source._fields, sizeof(_fields));
		for (uint i = 0; i < 7; ++i) { _resources[i] = source._resources[i]; _loaded[i] = source._loaded[i]; }
	}
	void syncObjectFields(const byte *lookup) {
		// Authored byte-field IDs80..255 alias the first176 object lookup
		// cells in the source format. Keep that explicit derived-field view,
		// rather than retaining a general machine-memory alias.
		memcpy(_fields + 80, lookup, sizeof(_fields) - 80);
	}
	void faceActor(byte bank, uint slot, byte facing, WorldActor &actor) {
		byte *record = _states[_current].tables.data() + (bank ? 1280 : 768) + slot;
		record[3] = byte((record[3] & 63) | (facing << 6)); actor.state = record[3];
	}
	struct ChangedMap { byte family, id; Common::Array<byte> tables; };
	static uint distance(byte a, byte b) { return a > b ? a - b : b - a; }
	static int familyFor(byte type) {
		switch (type) {
		case 0: return 0;
		case 2: case 4: return 1;
		case 3: return 2;
		case 6: return 3;
		case 5: return 4;
		case 7: return 5;
		case 8: return 6;
		default: return -1;
		}
	}
	bool ensureResource(uint family) {
		if (_loaded[family]) return true;
		const char *names[] = { "SURF", "UM", "RUIN", "TOWN", "TOWN2", "CSTL", "KEEP" };
		Common::Array<byte> data;
		if (!_archive.read(Common::String("NA/DATA.MAP.") + names[family], data) ||
			!_resources[family].open(data, family == 0, family == 1)) return false;
		_loaded[family] = true; return true;
	}
	// Original new-game initialization seeds Surface's virgin actor/object
	// registry from GAMEN, independently of the current Shipwreck location.
	// Keep generic resource imports and existing saved registries unchanged.
	bool initializeNewGameSurface(const byte *records, uint length) {
		if (!ready() || !records || length != 1536 || !ensureResource(0)) return false;
		for (uint i = 0; i < _states.size(); ++i)
			if (_states[i].family == 0 && _states[i].id == 0) return false;
		ChangedMap surface; surface.family = 0; surface.id = 0;
		if (!_resources[0].stateTables(0, surface.tables) || surface.tables.size() != 1792) return false;
		memcpy(surface.tables.data(), records, length);
		_states.push_back(surface); return true;
	}
	bool travel(byte type, byte id, byte x, byte y) {
		const int family = familyFor(type);
		if (family < 0 || !ensureResource(family)) return false;
		const MapDefinition *definition = _resources[family].find(id);
		if (!definition || (!definition->compressed && (x >= 48 || y >= 48))) return false;
		int index = -1;
		for (uint i = 0; i < _states.size(); ++i)
			if (_states[i].family == family && _states[i].id == id) index = i;
		if (index < 0) {
			ChangedMap map; map.family = family; map.id = id;
			if (!_resources[family].stateTables(id, map.tables)) return false;
			index = _states.size(); _states.push_back(map);
		}
		_current = index; _type = type; _x = x; _y = y; return true;
	}
	int objectAt(byte x, byte y) const {
		const byte *objects = _states[_current].tables.data() + 1024;
		for (uint slot = 0; slot <= 252; slot += 4)
			if (objects[slot] == x && objects[slot + 1] == y && objects[slot + 2] && objects[slot + 2] != 255)
				return slot;
		return -1;
	}
	HdvArchive &_archive;
	MapResource _resources[7];
	bool _loaded[7];
	Common::Array<ChangedMap> _states;
	byte _fields[256];
	int _current;
	byte _type, _x, _y;
	bool _onFoot;
};

} // namespace Native
} // namespace Nox
#endif
