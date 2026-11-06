/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CHAIN_VALIDATION_H
#define NOX_NATIVE_CHAIN_VALIDATION_H

#include "nox/native_world.h"

namespace Nox {
inline bool readNativeChainText(Common::SeekableReadStream &input, Common::String &output) {
	const uint length = input.readUint16LE();
	if (length > 8192 || input.size() - input.pos() < int64(length)) return false;
	Common::String next; for (uint i = 0; i < length; ++i) next += char(input.readByte());
	output = next; return !input.err();
}
inline bool validateNativeChain(HdvArchive &archive, Common::SeekableReadStream &input,
		Common::String &report, Common::String &error, const byte *acquiredFields = nullptr, const Native::Inventory *acquiredInventory = nullptr, Native::Inventory *boundInventory = nullptr) {
	if (input.readUint32BE() != MKTAG('N','C','C','H') || input.readUint32LE() != 2 || input.readUint32LE() != 3) {
		error = "Invalid native pages chain oracle"; return false;
	}
	Native::ItemCatalog catalog; if (!catalog.load(archive)) return false;
	uint replies = 0;
	for (uint mode = 0; mode < 2; ++mode) {
		byte fields[256], records[1536];
		if (input.read(fields, sizeof(fields)) != sizeof(fields) || input.read(records, sizeof(records)) != sizeof(records)) return false;
		uint16 gold = input.readUint16LE(); Native::Inventory inventory;
		if (fields[62] != 253 || fields[61] || !inventory.importRecords(records, sizeof(records))) return false;
        if (acquiredFields || acquiredInventory) {
            byte actual[1536];
            if (!acquiredFields || !acquiredInventory || memcmp(acquiredFields, fields, 66) ||
                    !acquiredInventory->exportRecords(actual, sizeof(actual)) || memcmp(actual, records, sizeof(actual))) {
                error = "Acquired native pages differ from historical dialogue prerequisites"; return false;
            }
            memcpy(fields, acquiredFields, sizeof(fields)); inventory = *acquiredInventory;
        }
		for (uint stage = 0; stage < 3; ++stage) {
			Native::ConversationContext context; context.map = input.readByte(); context.mapType = input.readByte();
			const byte x = input.readByte(), y = input.readByte(); context.hour = input.readByte(); context.minute = input.readByte();
			const byte adult = input.readByte(); context.serviceAccess = input.readByte(); const byte full = input.readByte();
			if (adult > 1 || full > 1 || context.serviceAccess > 3) return false;
			context.adultMode = adult != 0; context.partyFull = full != 0;
			Common::String expectedResource;
			if (!readNativeChainText(input, expectedResource)) return false;
			const byte actor = input.readByte();
			// Explicit controlled map boundary. Only source map/position/context
			// is initialized here; fields and inventory are the retained native
			// results of the previous phase. Physical NPC admission is excluded.
			Native::World world(archive);
			if (!world.initialize(context.mapType, context.map, x, y, fields, sizeof(fields)) || world.dialogueResource() != expectedResource) {
				error = "Native map dialogue resource identity differs"; return false;
			}
			for (uint visit = 0; visit < 2; ++visit) {
				const uint length = input.readByte(); if (length > 28 || input.size() - input.pos() < int64(length)) return false;
				Common::String query; for (uint i = 0; i < length; ++i) query += char(input.readByte());
				Common::String expectedReplies[2]; bool recognized[2];
				const uint count = length ? 2 : 1;
				for (uint i = 0; i < count; ++i) {
					if (!readNativeChainText(input, expectedReplies[i])) return false;
					const byte flag = input.readByte(); if (flag > 1) return false; recognized[i] = flag != 0;
				}
				Native::Conversation conversation;
				if (!conversation.begin(archive, world.dialogueResource(), actor, world.questFields(), sizeof(fields), context)) return false;
				for (uint i = 0; i < count; ++i) {
					const Native::ConversationResult result = i ? conversation.submit(query) : conversation.opening();
					if (result != Native::kConversationReply || conversation.reply() != expectedReplies[i] || conversation.recognizedReply() != recognized[i]) {
						error = Common::String::format("Native linked pages reply differs mode%u/stage%u/visit%u/reply%u (result%u)", mode, stage, visit, i, uint(result)); return false;
					}
					++replies;
				}
				if (world.acceptConversation(conversation)) { error = "World accepted conversation before exit handoff completed"; return false; }
				if (conversation.finish(catalog, inventory, gold) != Native::kHandoffApplied || !world.acceptConversation(conversation)) {
					error = "Native linked pages exit/world commit failed"; return false;
				}
			}
			byte expectedFields[66], expectedRecords[1536];
			if (input.read(expectedFields, sizeof(expectedFields)) != sizeof(expectedFields) || input.read(expectedRecords, sizeof(expectedRecords)) != sizeof(expectedRecords)) return false;
			const uint16 expectedGold = input.readUint16LE();
			if (memcmp(world.questFields(), expectedFields, sizeof(expectedFields)) || !inventory.exportRecords(records, sizeof(records)) || memcmp(records, expectedRecords, sizeof(records)) || gold != expectedGold) {
				error = Common::String::format("Native linked pages state differs from actual M3 phase mode%u/stage%u", mode, stage); return false;
			}
			memcpy(fields, world.questFields(), sizeof(fields));
		}
        if (boundInventory) *boundInventory = inventory;
	}
	if (input.err() || input.pos() != input.size() || replies != 20) return false;
	report += "native-pages-chain\tcases=2\tstages=6\tconversations=12\tactual_replies=20\tauthored_fields=66\tfull1536_inventory=1\tworld_commit=1\tretained_native_state=1\townership=controlled-semantic-phases\n";
	return true;
}
} // namespace Nox
#endif
