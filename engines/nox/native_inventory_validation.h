/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INVENTORY_VALIDATION_H
#define NOX_NATIVE_INVENTORY_VALIDATION_H

#include "nox/native_inventory.h"

namespace Nox {
inline bool validateNativeInventory(HdvArchive &archive, Common::SeekableReadStream &input,
		Common::String &report, Common::String &error) {
	Native::ItemCatalog catalog;
	if (!catalog.load(archive) || input.readUint32LE() != MKTAG('V','N','I','N')) {
		error = "Invalid native inventory catalog/comparison input"; return false;
	}
	const uint cases = input.readUint32LE();
	if (!cases || cases > 1000 || input.size() - input.pos() != int64(cases) * 3074) {
		error = "Invalid native inventory comparison count"; return false;
	}
	for (uint i = 0; i < cases; ++i) {
		const byte kind = input.readByte(), id = input.readByte();
		byte before[1536], expected[1536], after[1536];
		if (input.read(before, sizeof(before)) != sizeof(before) || input.read(expected, sizeof(expected)) != sizeof(expected)) return false;
		Native::Inventory inventory;
		// Controlled authored handoff request BEFORE the native exit operation.
		// This does not pretend the request was earned through native dialogue.
		byte fields[256]; memset(fields, 0, sizeof(fields)); fields[16] = 64; fields[3] = id; fields[4] = kind | 128;
		Native::DialogueHandoff request; uint16 gold = 123;
		if (!Native::describeDialogueHandoff(fields, sizeof(fields), request) || !inventory.importRecords(before, sizeof(before)) || Native::finishDialogueHandoff(request, catalog, inventory, gold) != Native::kHandoffApplied || gold != 123 || !inventory.exportRecords(after, sizeof(after))) {
			error = "Native historical inventory handoff could not execute"; return false;
		}
		if (memcmp(after, expected, sizeof(after))) {
			uint difference = 0; while (difference < sizeof(after) && after[difference] == expected[difference]) ++difference;
			error = Common::String::format("Native inventory differs from actual M3 handoff case%u byte%u: %u versus%u", i, difference, after[difference], expected[difference]); return false;
		}
		// Retain the genuinely produced item for a second inventory operation.
		const int slot = inventory.find(kind, id);
		if (slot < 0 || inventory.item(slot)->quantity() != 1 || inventory.grantOne(catalog, kind, id) != Native::kInventoryApplied || inventory.item(slot)->quantity() != 2) {
			error = "Native linked item stack transaction differs"; return false;
		}
	}
	if (input.err() || input.pos() != input.size()) return false;
	// Explicit controlled quantity boundary. No NPC progression is claimed.
	byte records[1536], after[1536];
	memset(records, 254, sizeof(records)); memset(records, 0, 6); records[0] = 2; records[1] = 149; records[2] = 254;
	Native::Inventory inventory;
	if (!inventory.importRecords(records, sizeof(records))) return false;
	if (inventory.grantOne(catalog, 3, 0) != Native::kInventoryInvalid || !inventory.exportRecords(after, sizeof(after)) || memcmp(records, after, sizeof(records))) {
		error = "Native inventory invalid-definition rejection mutated records"; return false;
	}
	if (inventory.grantOne(catalog, 2, 149) != Native::kInventoryApplied || inventory.item(0)->quantity() != 255 || inventory.grantOne(catalog, 2, 149) != Native::kInventoryApplied || inventory.item(0)->quantity() != 255) {
		error = "Native unlimited item quantity boundary differs"; return false;
	}
	uint16 gold = Native::awardGold(9900, 250);
	if (gold != 9999 || !Native::spendGold(gold, 9998) || gold != 1 || Native::spendGold(gold, 2) || gold != 1) {
		error = "Native linked gold cap/payment/refusal differs"; return false;
	}
	byte fields[256]; memset(fields, 0, sizeof(fields)); fields[16] = 63;
	Native::DialogueHandoff request;
	if (!Native::describeDialogueHandoff(fields, sizeof(fields), request) || request.kind != Native::DialogueHandoff::kNone) return false;
	fields[16] = 64; fields[4] = 130; fields[3] = 157;
	if (!Native::describeDialogueHandoff(fields, sizeof(fields), request) || request.kind != Native::DialogueHandoff::kItem || request.itemKind != 2 || request.itemId != 157) return false;
	fields[4] = 1; fields[3] = 244;
	if (!Native::describeDialogueHandoff(fields, sizeof(fields), request) || request.kind != Native::DialogueHandoff::kGold || request.gold != 500) return false;
	fields[16] = 4;
	if (!Native::describeDialogueHandoff(fields, sizeof(fields), request) || request.kind != Native::DialogueHandoff::kRecruit || request.companion != 4) return false;
	if (!inventory.exportRecords(records, sizeof(records)) || Native::finishDialogueHandoff(request, catalog, inventory, gold) != Native::kHandoffNeedsParty || gold != 1 || !inventory.exportRecords(after, sizeof(after)) || memcmp(records, after, sizeof(records))) return false;
	fields[16] = 64; fields[4] = 1; fields[3] = 244;
	if (!Native::describeDialogueHandoff(fields, sizeof(fields), request) || Native::finishDialogueHandoff(request, catalog, inventory, gold) != Native::kHandoffApplied || gold != 501 || !inventory.exportRecords(after, sizeof(after)) || memcmp(records, after, sizeof(records))) return false;
	report += Common::String::format("native-inventory\tactual_m3_handoffs=%u\tfull1536_comparison=1\tlinked_stacks=1\tunlimited=1\tatomic_refusal=1\tgold=1\texit_handoff=1\townership=semantic-unit-only\n", cases);
	return true;
}
} // namespace Nox
#endif
