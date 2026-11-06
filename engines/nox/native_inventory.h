/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INVENTORY_H
#define NOX_NATIVE_INVENTORY_H

#include "nox/records.h"
#include "nox/resources.h"

namespace Nox {
namespace Native {

// Data-only view of the original three definition tables. Offsets are file
// offsets, not instruction addresses. Keep the user's opaque definition bytes.
class ItemCatalog {
public:
	bool load(HdvArchive &archive) {
		Common::Array<byte> data;
		if (!archive.read("NA/SRTN.INVENTORY", data) || data.size() != 67130) return false;
		_data = data; return true;
	}
	bool definition(byte kind, byte id, ItemDefinition &output) const {
		static const uint starts[] = { 0x1000, 0x1cc0, 0x2c00 };
		if (kind > 2) return false;
		const uint offset = starts[kind] + uint(id) * 32;
		if (offset > _data.size() || _data.size() - offset < sizeof(output.bytes)) return false;
		memcpy(output.bytes, _data.data() + offset, sizeof(output.bytes)); return true;
	}
private:
	Common::Array<byte> _data;
};

struct InventoryItem {
	byte bytes[6];
	InventoryItem() { memset(bytes, 0, sizeof(bytes)); bytes[0] = 254; }
	bool empty() const { return bytes[0] == 254; }
	byte kind() const { return bytes[0] & 15; }
	byte id() const { return bytes[1]; }
	byte quantity() const { return bytes[2]; }
};

enum InventoryResult { kInventoryApplied, kInventoryNoSpace, kInventoryInvalid, kInventoryQuantityOverflow };

// A complete owned copy of the original256 six-byte slots. No private HDV or
// Own the original inventory records. Import is explicit for saves, source
// conversion and controlled contexts; normal creation is owned by NewGame.
class Inventory {
public:
	bool importRecords(const byte *bytes, uint size) {
		if (!bytes || size != 1536) return false;
		for (uint i = 0; i < 256; ++i) memcpy(_items[i].bytes, bytes + i * 6, 6);
		return true;
	}
	bool exportRecords(byte *bytes, uint size) const {
		if (!bytes || size != 1536) return false;
		for (uint i = 0; i < 256; ++i) memcpy(bytes + i * 6, _items[i].bytes, 6);
		return true;
	}
	const InventoryItem *item(uint slot) const { return slot < 256 ? &_items[slot] : nullptr; }
	int find(byte kind, byte id) const {
		if (kind > 2) return -1;
		for (uint i = 0; i < 256; ++i)
			if (!_items[i].empty() && _items[i].kind() == kind && _items[i].id() == id) return i;
		return -1;
	}
	byte ordinaryCapacityRemaining() const {
		// Original count is a byte; records marked32 do not consume this limit.
		byte count = 0;
		for (uint i = 0; i < 256; ++i)
			if (!_items[i].empty() && !(_items[i].bytes[5] & 32)) ++count;
		return count > 170 ? 0 : 170 - count;
	}
	InventoryResult grantOne(const ItemCatalog &catalog, byte kind, byte id) {
		ItemDefinition definition;
		if (!catalog.definition(kind, id, definition)) return kInventoryInvalid;
		const int existing = find(kind, id);
		if (existing >= 0) {
			//255 is the original unlimited quantity, including254 promoted to255.
			if (_items[existing].bytes[2] != 255) ++_items[existing].bytes[2];
			return kInventoryApplied;
		}
		for (uint i = 0; i < 256; ++i) {
			if (!_items[i].empty()) continue;
			InventoryItem added;
			added.bytes[0] = byte(kind | (definition.bytes[1] << 4));
			added.bytes[1] = id; added.bytes[2] = 1;
			added.bytes[5] = definition.bytes[12] | ((definition.bytes[8] & 128) ? 32 : 0);
			_items[i] = added; return kInventoryApplied;
		}
		return kInventoryNoSpace;
	}
	// Merchant B0DE checks ordinary capacity even for an existing stack. Buying
	// differs from awards:255 is not an unlimited-quantity shortcut here.
	InventoryResult buyQuantity(const ItemDefinition &definition, byte kind, byte id, byte quantity) {
		if (kind > 2 || !quantity) return kInventoryInvalid;
		if (!ordinaryCapacityRemaining()) return kInventoryNoSpace;
		const int existing = find(kind, id);
		if (existing >= 0) {
			const uint sum = uint(_items[existing].bytes[2]) + quantity;
			if (sum > 255) return kInventoryQuantityOverflow;
			_items[existing].bytes[2] = byte(sum); return kInventoryApplied;
		}
		for (uint i = 0; i < 256; ++i) {
			if (!_items[i].empty()) continue;
			InventoryItem added;
			added.bytes[0] = byte(kind | (definition.bytes[1] << 4));
			added.bytes[1] = id; added.bytes[2] = quantity;
			added.bytes[5] = definition.bytes[12] | ((definition.bytes[8] & 128) ? 32 : 0);
			_items[i] = added; return kInventoryApplied;
		}
		return kInventoryNoSpace;
	}
	// A1F1 leaves opaque bytes intact when the last copy marks a record empty.
	bool removeQuantity(uint slot, byte quantity) {
		if (slot >= 256 || _items[slot].empty() || !quantity || quantity > _items[slot].quantity()) return false;
		_items[slot].bytes[2] -= quantity;
		if (!_items[slot].bytes[2]) _items[slot].bytes[0] = 254;
		return true;
	}
private:
	InventoryItem _items[256];
};

// Shared conversation handoff descriptors are authored data, not a program
// dispatcher. Recruitment needs the party owner and is returned as a request.
struct DialogueHandoff {
	enum Kind { kNone, kRecruit, kGold, kItem } kind;
	byte companion, itemKind, itemId;
	uint16 gold;
	DialogueHandoff() : kind(kNone), companion(0), itemKind(0), itemId(0), gold(0) {}
};
inline bool describeDialogueHandoff(const byte *fields, uint size, DialogueHandoff &output) {
	if (!fields || size < 17) return false;
	DialogueHandoff next;
	if (fields[16] != 63) {
		if (!(fields[16] & 64)) { next.kind = DialogueHandoff::kRecruit; next.companion = fields[16]; }
		else if (fields[4] & 128) { next.kind = DialogueHandoff::kItem; next.itemKind = fields[4] & 127; next.itemId = fields[3]; }
		else { next.kind = DialogueHandoff::kGold; next.gold = READ_LE_UINT16(fields + 3); }
	}
	output = next; return true;
}
inline uint16 awardGold(uint16 balance, uint16 amount) {
	// Addition wraps as the original16-bit transaction before the9999 cap.
	const uint16 sum = uint16(balance + amount); return MIN<uint16>(sum, 9999);
}
inline bool spendGold(uint16 &balance, uint16 cost) {
	if (balance < cost) return false;
	balance -= cost; return true;
}

enum HandoffResult { kHandoffApplied, kHandoffNoSpace, kHandoffNeedsParty, kHandoffInvalid };
// Called at the conversation-exit boundary, once, after authored responses.
// A recruitment request stays explicit until a complete party owner handles it.
inline HandoffResult finishDialogueHandoff(const DialogueHandoff &request,
		const ItemCatalog &catalog, Inventory &inventory, uint16 &gold) {
	switch (request.kind) {
	case DialogueHandoff::kNone: return kHandoffApplied;
	case DialogueHandoff::kRecruit: return kHandoffNeedsParty;
	case DialogueHandoff::kGold: gold = awardGold(gold, request.gold); return kHandoffApplied;
	case DialogueHandoff::kItem: {
		const InventoryResult result = inventory.grantOne(catalog, request.itemKind, request.itemId);
		return result == kInventoryApplied ? kHandoffApplied : result == kInventoryNoSpace ? kHandoffNoSpace : kHandoffInvalid;
	}
	}
	return kHandoffInvalid;
}

} // namespace Native
} // namespace Nox
#endif
