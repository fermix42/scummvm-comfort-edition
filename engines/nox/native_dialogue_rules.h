/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_RULES_H
#define NOX_NATIVE_DIALOGUE_RULES_H

#include "nox/native_dialogue.h"
#include "nox/native_gates.h"

namespace Nox {
namespace Native {

struct DialogueFieldPair {
	byte field, value;
	DialogueFieldPair() : field(255), value(0) {}
};
struct DialogueTimeWindow {
	bool present;
	byte startHour, startMinute, endHour, endMinute;
	DialogueTimeWindow() : present(false), startHour(0), startMinute(0), endHour(0), endMinute(0) {}
};
struct DialogueResponse {
	uint headerOffset, bodyOffset;
	DialogueFieldPair exact, bitConditions[2], assignments[2], bitChanges[3];
	DialogueTimeWindow time;
	bool normalOnly, whisperOnly, yellOnly, adultOnly, substituteInput;
	uint16 inputPrefixLength;
	byte control;
	DialogueResponse() : headerOffset(0), bodyOffset(0), normalOnly(false), whisperOnly(false), yellOnly(false),
		adultOnly(false), substituteInput(false), inputPrefixLength(0), control(255) {}
};

inline byte foldDialogueByte(byte value) { return value >= 0xe1 && value <= 0xfa ? value & 0xdf : value; }
inline byte dialogueByte(const Common::Array<byte> &text, uint offset) {
	// A bounded packet has a virtual end marker and then a zero terminator.
	return offset < text.size() ? text[offset] : offset == text.size() ? 252 : 0;
}

// Semantic text matching, including the original four-letter authored stems.
// Returned offsets refer to resource data. There is no machine execution state.
inline bool findDialogueText(const Common::Array<byte> &text, uint start,
		const byte *pattern, uint length, byte terminator, bool allowShort, uint &after) {
	if (!pattern || !length || start > text.size()) return false;
	for (uint candidate = start; candidate <= text.size() + 1; ++candidate) {
		const byte first = foldDialogueByte(dialogueByte(text, candidate));
		if (first != foldDialogueByte(pattern[0])) {
			if (first == terminator) return false;
			continue;
		}
		for (uint matched = 1; matched <= 6; ++matched) {
			const uint position = candidate + matched;
			if (matched == 6) { after = position; return true; }
			const byte source = foldDialogueByte(dialogueByte(text, position));
			const byte input = matched < length ? foldDialogueByte(pattern[matched]) : 0;
			if (source == 0xba || source == 0xae || (!input && allowShort)) { after = position; return true; }
			if (!input || source != input) break;
		}
	}
	return false;
}

inline bool findDialogueTag(const Common::Array<byte> &text, uint start,
		byte tag, byte number, byte terminator, uint &after) {
	const byte pattern[] = { 0xa4, tag, number };
	return findDialogueText(text, start, pattern, number ? 3 : 2, terminator, true, after);
}
inline bool dialoguePair(const Common::Array<byte> &text, uint offset, DialogueFieldPair &output) {
	if (offset > text.size() || text.size() - offset < 2) return false;
	output.field = text[offset]; output.value = text[offset + 1]; return true;
}

inline bool parseDialogueResponse(const Common::Array<byte> &text, uint start,
		const Common::String &query, byte mode, DialogueResponse &output, bool opening = false) {
	if (text.empty() || text.size() > 0x13fd || (!opening && query.empty()) || query.size() > 28 || mode > 2) return false;
	Common::Array<byte> pattern; pattern.push_back(0xa4); pattern.push_back(opening ? 0xc9 : 0xa1);
	if (!opening) for (uint i = 0; i < query.size(); ++i) pattern.push_back(byte(query[i]) | 128);
	DialogueResponse next;
	uint header = 0;
	if (!findDialogueText(text, start, pattern.data(), pattern.size(), 252, mode < 2, header)) return false;
	next.headerOffset = header;
	if (dialogueByte(text, header) == 0xba) {
		uint value = 0;
		if (findDialogueTag(text, header, 0xd3, 0xb1, 0xae, value) && !dialoguePair(text, value, next.exact)) return false;
		uint changesStart = header;
		if (findDialogueTag(text, header, 0xd3, 0xb2, 0xae, value)) {
			if (!dialoguePair(text, value, next.bitConditions[0])) return false;
			changesStart = value;
			if (findDialogueTag(text, value, 0xd3, 0xb2, 0xae, value) && !dialoguePair(text, value, next.bitConditions[1])) return false;
		}
		// Preserve source ordering: bit-change scanning starts after the first
		// bit predicate when present; direct assignments restart at the header.
		for (uint i = 0; i < 3; ++i) {
			if (!findDialogueTag(text, changesStart, 0xd0, 0xb2, 0xae, value)) break;
			if (!dialoguePair(text, value, next.bitChanges[i])) return false;
			changesStart = value;
		}
		uint assignmentsStart = header;
		for (uint i = 0; i < 2; ++i) {
			if (!findDialogueTag(text, assignmentsStart, 0xd0, 0xb1, 0xae, value)) break;
			if (!dialoguePair(text, value, next.assignments[i])) return false;
			assignmentsStart = value;
		}
		next.yellOnly = findDialogueTag(text, header, 0xd9, 0, 0xae, value);
		next.whisperOnly = findDialogueTag(text, header, 0xd7, 0, 0xae, value);
		next.normalOnly = findDialogueTag(text, header, 0xce, 0, 0xae, value);
		next.adultOnly = findDialogueTag(text, header, 0xc1, 0, 0xae, value);
		if (findDialogueTag(text, header, 0xd4, 0, 0xae, value)) {
			if (value >= text.size()) return false;
			next.control = text[value];
		}
		if (findDialogueTag(text, header, 0xc8, 0, 0xae, value)) {
			if (value > text.size() || text.size() - value < 4) return false;
			next.time.present = true; next.time.startHour = text[value]; next.time.startMinute = text[value + 1];
			next.time.endHour = text[value + 2]; next.time.endMinute = text[value + 3];
		}
		if (findDialogueTag(text, header, 0xcb, 0, 0xae, value)) {
			const byte period = 0xae;
			uint body = 0, end = 0;
			if (!findDialogueText(text, header, &period, 1, 0, true, body) || !findDialogueTag(text, body, 0xcb, 0, 0, end) || end < body + 2) return false;
			next.substituteInput = true; next.inputPrefixLength = end - body - 2;
		}
	}
	const byte period = 0xae;
	if (!findDialogueText(text, header, &period, 1, 252, true, next.bodyOffset)) return false;
	output = next; return true;
}

inline bool dialogueEligible(const DialogueResponse &response, QuestFields fields,
		byte voice, bool adultMode, byte hour, byte minute) {
	if (response.normalOnly ? voice != 0 : response.whisperOnly ? voice != 2 : response.yellOnly && voice != 1) return false;
	if (response.exact.field != 255 && (!fields.contains(response.exact.field) || fields.data[response.exact.field] != response.exact.value)) return false;
	for (uint i = 0; i < 2; ++i) {
		const DialogueFieldPair &condition = response.bitConditions[i];
		if (condition.field == 255) continue;
		const uint bit = condition.value & 0xbf;
		if (!fields.contains(condition.field) || bit > 7) return false;
		const bool isSet = (fields.data[condition.field] & (1U << bit)) != 0;
		if (isSet != ((condition.value & 64) != 0)) return false;
	}
	if (response.adultOnly && !adultMode) return false;
	if (response.time.present) {
		if (hour < response.time.startHour || (hour == response.time.startHour && minute < response.time.startMinute)) return false;
		if (hour > response.time.endHour || (hour == response.time.endHour && minute >= response.time.endMinute)) return false;
	}
	return true;
}

// Original conversation response order: a failed condition resumes searching
// from that response's body, retaining the same submitted keyword and context.
// Selection is read-only; authored effects and exit services happen later.
inline bool selectDialogueResponse(const Common::Array<byte> &text, uint start,
		const Common::String &query, byte mode, QuestFields fields, byte voice,
		bool adultMode, byte hour, byte minute, DialogueResponse &output, bool opening = false) {
	uint cursor = start;
	DialogueResponse candidate;
	while (parseDialogueResponse(text, cursor, query, mode, candidate, opening)) {
		if (dialogueEligible(candidate, fields, voice, adultMode, hour, minute)) { output = candidate; return true; }
		if (candidate.bodyOffset <= cursor || candidate.bodyOffset > text.size()) return false;
		cursor = candidate.bodyOffset;
	}
	return false;
}

enum DialogueCommitResult { kDialogueRefused, kDialogueApplied, kDialogueInvalidEffects };

// This commits only the authored quest-field transaction. Recruitment, items,
// gold, map changes and other post-conversation services have separate owners.
// Conditions observe the original state; all assignments precede bit changes.
inline DialogueCommitResult commitDialogueFields(const DialogueResponse &response,
		byte *fields, uint count, byte voice, bool adultMode, byte hour, byte minute) {
	if (!fields || !dialogueEligible(response, QuestFields(fields, count), voice, adultMode, hour, minute))
		return kDialogueRefused;
	for (uint i = 0; i < 2; ++i)
		if (response.assignments[i].field != 255 && response.assignments[i].field >= count) return kDialogueInvalidEffects;
	for (uint i = 0; i < 3; ++i) {
		const DialogueFieldPair &change = response.bitChanges[i];
		if (change.field != 255 && (change.field >= count || (change.value & 0xbf) > 7)) return kDialogueInvalidEffects;
	}
	for (uint i = 0; i < 2; ++i) {
		const DialogueFieldPair &assignment = response.assignments[i];
		if (assignment.field != 255) fields[assignment.field] = assignment.value;
	}
	for (uint i = 0; i < 3; ++i) {
		const DialogueFieldPair &change = response.bitChanges[i];
		if (change.field == 255) continue;
		const byte mask = byte(1U << (change.value & 0xbf));
		if (change.value & 64) fields[change.field] |= mask;
		else fields[change.field] &= ~mask;
	}
	return kDialogueApplied;
}

} // namespace Native
} // namespace Nox
#endif
