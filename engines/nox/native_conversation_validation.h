/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CONVERSATION_VALIDATION_H
#define NOX_NATIVE_CONVERSATION_VALIDATION_H

#include "nox/native_conversation.h"
#include "common/memstream.h"

namespace Nox {
inline bool roundtripNativeConversation(HdvArchive &archive,Native::Conversation &conversation) {
	Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES);
	if (!conversation.save(before)) return false;
	Common::MemoryReadStream input(before.getData(),before.size());Native::Conversation restored;
	if (!restored.load(input,archive) || !restored.save(after) || before.size()!=after.size() ||
			memcmp(before.getData(),after.getData(),before.size())) return false;
	conversation=restored;return true;
}
inline bool validateNativeConversation(HdvArchive &archive, Common::SeekableReadStream &input,
		Common::String &report, Common::String &error) {
	if (input.readUint32BE() != MKTAG('N','C','N','V') || input.readUint32LE() != 2) { error = "Invalid native conversation oracle"; return false; }
	Native::ItemCatalog catalog; if (!catalog.load(archive)) return false;
	for (uint caseNo = 0; caseNo < 2; ++caseNo) {
		if (input.size() - input.pos() < 3404) { error = "Truncated native conversation context"; return false; }
		Native::ConversationContext context;
		context.map = input.readByte(); context.hour = input.readByte(); context.minute = input.readByte();
		const byte adult = input.readByte(); context.serviceAccess = input.readByte(); const byte full = input.readByte();
		context.adultMode = adult != 0; context.partyFull = full != 0;
		uint16 gold = input.readUint16LE(); const uint16 expectedGold = input.readUint16LE();
		byte fields[256], before[1536], expectedInventory[1536], expectedFields[66], actual[1536];
		if (adult > 1 || full > 1 || context.map != 104 || input.read(fields, sizeof(fields)) != sizeof(fields) || input.read(before, sizeof(before)) != sizeof(before) || input.read(expectedInventory, sizeof(expectedInventory)) != sizeof(expectedInventory) || input.read(expectedFields, sizeof(expectedFields)) != sizeof(expectedFields)) return false;
		Common::String replies[3]; bool recognized[3];
		for (uint i = 0; i < 3; ++i) {
			const uint length = input.readUint16LE();
			if (length > 8192 || input.size() - input.pos() < int64(length + 1)) return false;
			for (uint j = 0; j < length; ++j) replies[i] += char(input.readByte());
			const byte flag = input.readByte(); if (flag > 1) return false; recognized[i] = flag != 0;
		}
		Native::Inventory inventory; Native::Conversation conversation;
		if (!inventory.importRecords(before, sizeof(before)) || !conversation.begin(archive, "NA/DATA.TLK.L068", 16, fields, sizeof(fields), context)) return false;
		if (conversation.opening() != Native::kConversationReply || conversation.reply() != replies[0] || conversation.recognizedReply() != recognized[0] || !conversation.terminalReply()) {
			error = Common::String::format("Native Nystul binding reply differs in case%u", caseNo); return false;
		}
		if (conversation.fields()[61] != 3 || conversation.fields()[16] != 127 || !inventory.exportRecords(actual, sizeof(actual)) || memcmp(actual, before, sizeof(actual))) {
			error = "Native binding did not retain reply effects before exit reward"; return false;
		}
		if (!roundtripNativeConversation(archive,conversation)) { error="Native pending binding continuation could not roundtrip";return false; }
		if (conversation.finish(catalog, inventory, gold) != Native::kHandoffApplied || gold != expectedGold || !inventory.exportRecords(actual, sizeof(actual)) || memcmp(actual, expectedInventory, sizeof(actual))) {
			error = "Native conversation-exit award differs from actual M3 binding inventory"; return false;
		}
		// Retain native results, then perform another actual native admission and
		// response chain. No expected flags or items are initialized here.
		memcpy(fields, conversation.fields(), sizeof(fields));
		if (!conversation.begin(archive, "NA/DATA.TLK.L068", 16, fields, sizeof(fields), context) || conversation.opening() != Native::kConversationReply || conversation.reply() != replies[1] || conversation.recognizedReply() != recognized[1] || conversation.terminalReply()) {
			error = "Native retained binding repeat opening differs"; return false;
		}
		if (conversation.submit("BIND") != Native::kConversationReply || conversation.reply() != replies[2] || conversation.recognizedReply() != recognized[2]) {
			error = "Native retained binding refusal/default response differs"; return false;
		}
		if (conversation.finish(catalog, inventory, gold) != Native::kHandoffApplied || gold != expectedGold || memcmp(conversation.fields(), expectedFields, sizeof(expectedFields)) || !inventory.exportRecords(actual, sizeof(actual)) || memcmp(actual, expectedInventory, sizeof(actual))) {
			error = "Native linked binding repeat flags/inventory differ from actual M3 outcome"; return false;
		}
		if (conversation.finish(catalog, inventory, gold) != Native::kHandoffInvalid || gold != expectedGold || !inventory.exportRecords(actual, sizeof(actual)) || memcmp(actual, expectedInventory, sizeof(actual))) {
			error = "Native closed conversation replay changed award state"; return false;
		}
		if (!roundtripNativeConversation(archive,conversation) || conversation.finish(catalog,inventory,gold)!=Native::kHandoffInvalid ||
				gold!=expectedGold || !inventory.exportRecords(actual,sizeof(actual)) || memcmp(actual,expectedInventory,sizeof(actual))) {
			error="Native restored completed conversation repeated its award";return false;
		}
	}
	if (input.err() || input.pos() != input.size()) return false;
	report += "native-conversation\tcases=2\tactual_replies=6\tauthored_fields=66\tfull1536_inventory=1\texit_award=1\tlinked_repeat=1\tclosed_replay_refused=1\tpending_continuation_roundtrip=2\tcompleted_continuation_roundtrip=2\townership=controlled-semantic-phase\n";
	return true;
}
} // namespace Nox
#endif
