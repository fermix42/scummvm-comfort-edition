/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_INPUT_VALIDATION_H
#define NOX_NATIVE_DIALOGUE_INPUT_VALIDATION_H
#include "nox/native_dialogue_input.h"
namespace Nox {
inline bool validateNativeDialogueInput(HdvArchive &archive, Common::SeekableReadStream &input,
		Common::SeekableReadStream &expected, Common::String &report, Common::String &error) {
	if (input.readUint32BE() != MKTAG('N','K','O','R') || expected.readUint32BE() != MKTAG('N','K','E','S')) return false;
	const uint count = input.readUint32LE(), size = input.readUint16LE();
	if (!count || count > 100000 || expected.readUint32LE() != count || size != 70) return false;
	byte lexical[70]; Common::Array<byte> source;
	if (input.read(lexical, sizeof(lexical)) != sizeof(lexical) || !archive.read("NA/SRTN.NPC.TALK", source) || source.size() < 0xca4 + size || memcmp(lexical, source.data() + 0xca4, size)) {
		error = "Lexical oracle does not match original NPC resource data"; return false;
	}
	Native::DialogueLexicon lexicon; if (!lexicon.load(archive)) return false;
	for (uint i = 0; i < count; ++i) {
		const uint length = input.readByte(); if (!length || length > 28 || input.size() - input.pos() < int64(length)) return false;
		Common::String query; for (uint j = 0; j < length; ++j) query += char(input.readByte());
		const uint resultLength = expected.readByte(); if (resultLength > 28 || expected.size() - expected.pos() < int64(resultLength)) return false;
		Common::String golden; for (uint j = 0; j < resultLength; ++j) golden += char(expected.readByte());
		Common::String actual;
		if (!lexicon.normalize(query, actual) || actual != golden) {
			error = Common::String::format("Native lexical normalization differs in case%u", i); return false;
		}
	}
	if (input.err() || expected.err() || input.pos() != input.size() || expected.pos() != expected.size()) return false;
	report += Common::String::format("native-dialogue-input\tcases=%u\tresource_lexical_bytes=70\townership=semantic-unit-only\n", count);
	return true;
}
}
#endif
