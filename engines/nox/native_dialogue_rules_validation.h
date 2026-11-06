/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_RULES_VALIDATION_H
#define NOX_NATIVE_DIALOGUE_RULES_VALIDATION_H

#include "nox/native_dialogue_rules.h"

namespace Nox {
// Comparison encoding lives only in the validator. Native production rules use
// named conditions/effects and resource offsets, never the legacy64K layout.
inline void nativeDialogueComparison(const Native::DialogueResponse &r, byte *bytes) {
	memset(bytes, 0, 32);
	bytes[0] = r.exact.field; bytes[1] = r.exact.value;
	bytes[2] = r.assignments[0].field; bytes[3] = r.assignments[0].value;
	bytes[4] = r.yellOnly; bytes[5] = r.whisperOnly; bytes[6] = r.adultOnly;
	WRITE_LE_UINT16(bytes + 7, r.inputPrefixLength); bytes[9] = r.control;
	bytes[10] = r.time.present ? r.time.startHour : 255;
	bytes[11] = r.time.present ? r.time.startMinute : 255;
	bytes[12] = r.time.endHour; bytes[13] = r.time.endMinute; bytes[14] = r.normalOnly;
	bytes[15] = r.bitConditions[0].field; bytes[16] = r.bitConditions[0].value;
	bytes[17] = r.bitChanges[0].field; bytes[18] = r.bitChanges[0].value;
	bytes[19] = r.assignments[1].field; bytes[20] = r.assignments[1].value;
	bytes[21] = r.bitChanges[1].field; bytes[22] = r.bitChanges[1].value;
	bytes[23] = r.bitConditions[1].field; bytes[24] = r.bitConditions[1].value;
	bytes[25] = r.bitChanges[2].field; bytes[26] = r.bitChanges[2].value;
}

inline bool validateNativeDialogueRules(Common::SeekableReadStream &input,
		Common::SeekableReadStream &expected, Common::String &report, Common::String &error,
		Common::SeekableReadStream *effects = nullptr, bool selection = false, bool opening = false) {
	if (input.readUint32LE() != 0x524f444e || expected.readUint32LE() != 0x5253444e) {
		error = "Invalid native dialogue comparison signature"; return false;
	}
	const uint count = input.readUint32LE();
	if (!count || count > 100000 || expected.readUint32LE() != count) {
		error = "Invalid native dialogue comparison count"; return false;
	}
	if (effects && (effects->readUint32LE() != 0x4645444e || effects->readUint32LE() != count)) {
		error = "Invalid native dialogue effects signature/count"; return false;
	}
	uint matched = 0, eligible = 0, effectCases = 0, unsupportedEffects = 0;
	for (uint i = 0; i < count; ++i) {
		if (input.size() - input.pos() < 270 || expected.size() - expected.pos() < 42) {
			error = "Truncated native dialogue comparison case"; return false;
		}
		const uint id = input.readUint32LE(), size = input.readUint16LE(), start = input.readUint16LE();
		const byte length = input.readByte(), mode = input.readByte(), voice = input.readByte(), active = input.readByte(), hour = input.readByte(), minute = input.readByte();
		if (!size || size > 0x13fd || start >= size || length > 28 || mode > 2 || voice > 2 || active > 1 || input.size() - input.pos() < int64(256 + size + length)) {
			error = "Invalid native dialogue comparison bounds"; return false;
		}
		byte fields[256]; if (input.read(fields, sizeof(fields)) != sizeof(fields)) return false;
		Common::Array<byte> text; text.resize(size); if (input.read(text.data(), size) != size) return false;
		Common::String query; for (uint q = 0; q < length; ++q) query += char(input.readByte());
		const uint expectedId = expected.readUint32LE(); const bool expectedFound = expected.readByte() != 0, expectedEligible = expected.readByte() != 0;
		const uint expectedHeader = expected.readUint16LE(), expectedBody = expected.readUint16LE();
		byte metadata[32]; if (expected.read(metadata, sizeof(metadata)) != sizeof(metadata) || expectedId != id) return false;
		Native::DialogueResponse response;
		const bool found = selection ? Native::selectDialogueResponse(text, start, query, mode, Native::QuestFields(fields, sizeof(fields)), voice, active != 0, hour, minute, response, opening) : Native::parseDialogueResponse(text, start, query, mode, response, opening);
		if (found != expectedFound) {
			error = Common::String::format("Native dialogue match differs in case%u", id); return false;
		}
		if (effects) {
			if (effects->size() - effects->pos() < 261 || effects->readUint32LE() != id) {
				error = "Truncated/misordered native dialogue effects case"; return false;
			}
			const byte expectedDisposition = effects->readByte();
			byte expectedFields[256];
			if (effects->read(expectedFields, 256) != 256) return false;
			const Native::DialogueCommitResult committed = found ? Native::commitDialogueFields(response, fields, sizeof(fields), voice, active != 0, hour, minute) : Native::kDialogueRefused;
			if (expectedDisposition > 2 || (expectedDisposition == 2 ? committed != Native::kDialogueInvalidEffects : committed == Native::kDialogueInvalidEffects || (committed == Native::kDialogueApplied) != (expectedDisposition == 1)) || memcmp(fields, expectedFields, 256)) {
				error = Common::String::format("Native dialogue field transaction differs in case%u", id); return false;
			}
			if (expectedDisposition == 2) ++unsupportedEffects; else ++effectCases;
			// Eligibility below must still observe the original pre-transaction
			// context, including cases whose own assignment changes a condition.
			const int64 saved = input.pos(); input.seek(saved - length - size - 256);
			if (input.read(fields, 256) != 256 || !input.seek(saved)) return false;
		}
		if (!found) continue;
		++matched;
		byte actual[32]; nativeDialogueComparison(response, actual);
		if (response.headerOffset != expectedHeader || response.bodyOffset != expectedBody) {
			error = Common::String::format("Native dialogue data offsets differ in case%u: %u/%u versus%u/%u", id, response.headerOffset, response.bodyOffset, expectedHeader, expectedBody); return false;
		}
		for (uint value = 0; value < 32; ++value)
			if (actual[value] != metadata[value]) {
				error = Common::String::format("Native dialogue metadata differs in case%u field%u: %u versus%u", id, value, actual[value], metadata[value]); return false;
			}
		const bool allowed = Native::dialogueEligible(response, Native::QuestFields(fields, sizeof(fields)), voice, active != 0, hour, minute);
		if (allowed != expectedEligible) { error = Common::String::format("Native dialogue eligibility differs in case%u", id); return false; }
		eligible += allowed ? 1 : 0;
	}
	if (input.err() || expected.err() || input.pos() != input.size() || expected.pos() != expected.size()) {
		error = "Native dialogue comparison has unread/truncated data"; return false;
	}
	if (effects && (effects->err() || effects->pos() != effects->size())) {
		error = "Native dialogue effects comparison has unread/truncated data"; return false;
	}
	// A malformed later effect must not leave an earlier assignment committed.
	byte fields[256]; memset(fields, 0, sizeof(fields));
	Native::DialogueResponse invalid; invalid.assignments[0].field = 7; invalid.assignments[0].value = 89;
	invalid.bitChanges[2].field = 7; invalid.bitChanges[2].value = 8;
	if (Native::commitDialogueFields(invalid, fields, sizeof(fields), 0, false, 0, 0) != Native::kDialogueInvalidEffects || fields[7]) {
		error = "Native dialogue invalid effects partially committed"; return false;
	}
	report += Common::String::format("%s\tcases=%u\tmatched=%u\teligible=%u\townership=semantic-unit-only\n", opening ? "native-dialogue-openings" : selection ? "native-dialogue-selection" : "native-dialogue-rules", count, matched, eligible);
	if (effects) report += Common::String::format("native-dialogue-effects\tcompared=%u\tunsupported=%u\tfull_field_comparison=1\tatomic_rejection=1\townership=semantic-unit-only\n", effectCases, unsupportedEffects);
	return true;
}
} // namespace Nox
#endif
