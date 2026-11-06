/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_INPUT_H
#define NOX_NATIVE_DIALOGUE_INPUT_H

#include "nox/native_dialogue_rules.h"

namespace Nox {
namespace Native {

// Named lexical data is read from the user's resource, never embedded here as
// extracted assets or executed as a program. The thirteen bounded strings are
// canonical job, two aliases, curse response key and nine trigger patterns.
class DialogueLexicon {
public:
	DialogueLexicon() : _ready(false) {}
	bool load(HdvArchive &archive) {
		Common::Array<byte> data;
		if (!archive.read("NA/SRTN.NPC.TALK", data) || data.size() != 32831) return false;
		Common::Array<Common::Array<byte> > words; uint cursor = 0xca4;
		for (uint i = 0; i < 13; ++i) {
			Common::Array<byte> word;
			while (cursor < data.size() && data[cursor] && word.size() < 28) word.push_back(data[cursor++]);
			if (word.empty() || cursor >= data.size() || data[cursor++]) return false;
			words.push_back(word);
		}
		_words = words; _ready = true; return true;
	}
	bool normalize(const Common::String &input, Common::String &output) const {
		if (!_ready || input.empty() || input.size() > 28) return false;
		Common::String next = input;
		if (contains(next, _words[1]) || contains(next, _words[2])) next = plain(_words[0]);
		for (uint i = 4; i < 13; ++i)
			if (contains(next, _words[i])) { next = plain(_words[3]); break; }
		output = next; return true;
	}
private:
	static Common::String plain(const Common::Array<byte> &bytes) {
		Common::String result; for (uint i = 0; i < bytes.size(); ++i) result += char(bytes[i] & 127); return result;
	}
	static bool contains(const Common::String &input, const Common::Array<byte> &word) {
		Common::Array<byte> text; text.push_back(0xa4); text.push_back(0xa1);
		for (uint i = 0; i < input.size(); ++i) text.push_back(byte(input[i]) | 128);
		text.push_back(0); uint after = 0;
		return findDialogueText(text, 0, word.data(), word.size(), 0, true, after);
	}
	bool _ready;
	Common::Array<Common::Array<byte> > _words;
};

} // namespace Native
} // namespace Nox
#endif
