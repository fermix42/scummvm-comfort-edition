/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nox/enhancements.h"
#include "common/algorithm.h"
#include "common/endian.h"
#include "common/memstream.h"

namespace Nox {
namespace {
bool alphaNumeric(byte c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
Common::String upper(Common::String text) { text.toUppercase(); return text; }
Common::String topicKey(const Common::String &text) { return upper(text).substr(0, 4); }
bool internal(const Common::String &text) { return text.empty() || text == "DFL" || text[0] == '%'; }
bool validResource(const Common::String &resource) {
	if (!resource.hasPrefix("NA/") || resource.size() < 4 || resource.size() > 128) return false;
	for (uint i = 3; i < resource.size(); ++i)
		if (!alphaNumeric(resource[i]) && resource[i] != '.' && resource[i] != '_') return false;
	return true;
}
int findTopic(const Common::Array<JournalTopic> &topics, const Common::String &key) {
	for (uint i = 0; i < topics.size(); ++i) if (topics[i].key == key) return i;
	return -1;
}
void putTopic(Common::Array<JournalTopic> &topics, const Common::String &key, const Common::String &label, bool used) {
	const int found = findTopic(topics, key);
	JournalTopic topic; topic.key = key; topic.label = label; topic.used = used;
	if (found < 0) topics.push_back(topic); else topics[found] = topic;
}
void writeText(Common::WriteStream &out, const Common::String &text) {
	out.writeUint32LE(text.size()); out.write(text.c_str(), text.size());
}
bool readText(Common::SeekableReadStream &in, Common::String &text, uint max) {
	const uint32 length = in.readUint32LE();
	if (in.err() || length > max || int64(length) > in.size() - in.pos()) return false;
	Common::Array<byte> data; data.resize(length);
	if (length && (in.read(data.data(), length) != length || memchr(data.data(), 0, length))) return false;
	text = length ? Common::String(reinterpret_cast<const char *>(data.data()), length) : Common::String();
	return !in.err();
}
void write64(Common::WriteStream &out, uint64 value) { out.writeUint32LE(uint32(value)); out.writeUint32LE(uint32(value >> 32)); }
uint64 read64(Common::ReadStream &in) { const uint32 lo = in.readUint32LE(); const uint32 hi = in.readUint32LE(); return uint64(lo) | (uint64(hi) << 32); }
bool readFlag(Common::ReadStream &in, bool &flag) { const byte value = in.readByte(); flag = value != 0; return value <= 1 && !in.err(); }
Common::String cellKey(const Common::String &resource, byte type, byte id, byte x, byte y) {
	return resource + Common::String::format("#%u:%u:%u:%u", type, id, x, y);
}
}

void Enhancements::reset() { _sequence = 0; _characters.clear(); _cells.clear(); _cellIndex.clear(); _active = -1; _authored.clear(); }
bool Enhancements::beginConversation(const Common::String &resource, uint16 record, uint16 actor, const byte *packet, uint size) {
	if (!validResource(resource) || !packet || size > 0x1400) return false;
	int active = -1;
	for (uint i = 0; i < _characters.size(); ++i)
		if (_characters[i].resource == resource && _characters[i].record == record) active = i;
	if (active < 0) {
		if (_characters.size() >= 1024) return false;
		JournalCharacter character; character.resource = resource; character.record = record;
		_characters.push_back(character); active = _characters.size() - 1;
	}
	_active = active; _characters[active].actor = actor; _authored.clear();
	for (uint i = 0; i + 2 < size; ++i) if ((packet[i] & 127) == '$' && (packet[i + 1] & 127) == '!') {
		Common::String label; uint j = i + 2;
		for (; j < size && j < i + 34; ++j) {
			const byte ch = packet[j] & 127;
			if (ch == ':' || ch == '.') break;
			if (!alphaNumeric(ch) && ch != '%' && ch != ' ') { label.clear(); break; }
			label += char(ch);
		}
		if (j < size && ((packet[j] & 127) == ':' || (packet[j] & 127) == '.') && !internal(upper(label)))
			putTopic(_authored, topicKey(label), upper(label), false);
	}
	return true;
}
bool Enhancements::nameCharacter(const Common::String &name) {
	if (name.size() > 128) return false;
	if (_active >= 0 && !name.empty()) _characters[_active].name = name;
	return true;
}
bool Enhancements::recordReply(bool opening, const Common::String &input, const Common::String &selected, const Common::String &response) {
	if (input.size() > 64 || selected.size() > 40 || response.size() > 8192) return false;
	if (_active < 0 || response.empty()) return true;
	JournalCharacter &character = _characters[_active];
	uint totalReplies = 0;
	for (const auto &c : _characters) totalReplies += c.replies.size();
	if (totalReplies >= 16384 || _sequence == uint64(-1)) return false;
	JournalReply reply; reply.sequence = ++_sequence; reply.input = input; reply.response = response; reply.opening = opening;
	const Common::String selectedKey = topicKey(selected);
	const int authored = findTopic(_authored, selectedKey);
	if (!opening && !input.empty() && !internal(selected) && authored >= 0) {
		reply.keyword = selectedKey; reply.recognized = true;
		putTopic(character.topics, selectedKey, _authored[authored].label, true);
	}
	for (uint i = 0; i < response.size();) {
		if (!alphaNumeric(response[i])) { ++i; continue; }
		const uint start = i;
		while (i < response.size() && alphaNumeric(response[i])) ++i;
		const Common::String word = response.substr(start, i - start), key = topicKey(word);
		if (findTopic(_authored, key) >= 0 && findTopic(character.topics, key) < 0) putTopic(character.topics, key, upper(word), false);
	}
	if (!opening && reply.keyword.empty() && !input.empty()) {
		const int found = findTopic(character.topics, topicKey(input));
		if (found >= 0) { character.topics[found].used = true; reply.keyword = character.topics[found].key; }
	}
	if (opening) character.defaultResponse = response;
	character.replies.push_back(reply);
	return true;
}
bool Enhancements::discover(const Common::String &resource, byte mapType, byte mapId, byte x, byte y, bool observed, bool revealed, const Common::String &annotation) {
	if (!validResource(resource) || annotation.size() > 1024 || (!observed && !revealed)) return false;
	const Common::String key = cellKey(resource, mapType, mapId, x, y);
	if (_cellIndex.contains(key)) {
		DiscoveryCell &cell = _cells[_cellIndex[key]];
		cell.observed |= observed; cell.revealed |= revealed;
		if (!annotation.empty()) cell.annotation = annotation;
		return true;
	}
	if (_cells.size() >= 65536) return false;
	DiscoveryCell cell; cell.resource = resource; cell.mapType = mapType; cell.mapId = mapId; cell.x = x; cell.y = y;
	cell.observed = observed; cell.revealed = revealed; cell.annotation = annotation; _cellIndex[key] = _cells.size(); _cells.push_back(cell);
	return true;
}
bool Enhancements::write(Common::WriteStream &out) const {
	Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);
	write64(body, _sequence); body.writeUint32LE(_characters.size());
	for (const auto &c : _characters) {
		writeText(body, c.resource); body.writeUint16LE(c.record); body.writeUint16LE(c.actor); writeText(body, c.name); writeText(body, c.defaultResponse);
		body.writeUint32LE(c.topics.size());
		for (const auto &topic : c.topics) { writeText(body, topic.key); writeText(body, topic.label); body.writeByte(topic.used); }
		body.writeUint32LE(c.replies.size());
		for (const auto &reply : c.replies) {
			write64(body, reply.sequence); writeText(body, reply.input); writeText(body, reply.keyword); writeText(body, reply.response);
			body.writeByte(reply.opening); body.writeByte(reply.recognized);
		}
		if (body.size() > kMaxBytes) return false;
	}
	body.writeUint32LE(_cells.size());
	for (const auto &cell : _cells) {
		writeText(body, cell.resource); body.writeByte(cell.mapType); body.writeByte(cell.mapId); body.writeByte(cell.x); body.writeByte(cell.y);
		body.writeByte(cell.observed); body.writeByte(cell.revealed); writeText(body, cell.annotation);
		if (body.size() > kMaxBytes) return false;
	}
	if (body.err() || body.size() > kMaxBytes) return false;
	out.writeUint32BE(MKTAG('N','X','E','H')); out.writeUint32LE(1); out.writeUint32LE(body.size()); out.write(body.getData(), body.size());
	return !out.err();
}
bool Enhancements::readBody(Common::SeekableReadStream &in) {
	_sequence = read64(in); const uint32 count = in.readUint32LE();
	if (count > 1024) return false;
	Common::Array<uint64> sequences;
	for (uint i = 0; i < count; ++i) {
		JournalCharacter c;
		if (!readText(in, c.resource, 128) || !validResource(c.resource)) return false;
		c.record = in.readUint16LE(); c.actor = in.readUint16LE();
		for (const auto &prior : _characters) if (prior.resource == c.resource && prior.record == c.record) return false;
		if (!readText(in, c.name, 128) || !readText(in, c.defaultResponse, 8192)) return false;
		const uint32 topics = in.readUint32LE(); if (topics > 2048) return false;
		for (uint j = 0; j < topics; ++j) {
			JournalTopic topic;
			if (!readText(in, topic.key, 4) || topic.key.empty() || topicKey(topic.key) != topic.key || internal(topic.key) || !readText(in, topic.label, 8192) || topic.label.empty() || !readFlag(in, topic.used) || findTopic(c.topics, topic.key) >= 0) return false;
			c.topics.push_back(topic);
		}
		const uint32 replies = in.readUint32LE(); if (replies > 16384 || sequences.size() + replies > 16384) return false;
		uint64 previous = 0;
		for (uint j = 0; j < replies; ++j) {
			JournalReply reply; reply.sequence = read64(in);
			if (!reply.sequence || reply.sequence <= previous || reply.sequence > _sequence || !readText(in, reply.input, 64) || !readText(in, reply.keyword, 4) || !readText(in, reply.response, 8192) || reply.response.empty() || !readFlag(in, reply.opening) || !readFlag(in, reply.recognized)) return false;
			if ((!reply.keyword.empty() && findTopic(c.topics, reply.keyword) < 0) || (reply.recognized && reply.keyword.empty())) return false;
			previous = reply.sequence; sequences.push_back(reply.sequence); c.replies.push_back(reply);
		}
		_characters.push_back(c);
	}
	Common::sort(sequences.begin(), sequences.end());
	if (_sequence != sequences.size()) return false;
	for (uint i = 0; i < sequences.size(); ++i) if (sequences[i] != uint64(i) + 1) return false;
	const uint32 cells = in.readUint32LE(); if (cells > 65536) return false;
	for (uint i = 0; i < cells; ++i) {
		DiscoveryCell cell;
		if (!readText(in, cell.resource, 128) || !validResource(cell.resource)) return false;
		cell.mapType = in.readByte(); cell.mapId = in.readByte(); cell.x = in.readByte(); cell.y = in.readByte();
		if (!readFlag(in, cell.observed) || !readFlag(in, cell.revealed) || (!cell.observed && !cell.revealed) || !readText(in, cell.annotation, 1024)) return false;
		const Common::String key = cellKey(cell.resource, cell.mapType, cell.mapId, cell.x, cell.y);
		if (_cellIndex.contains(key)) return false;
		_cellIndex[key] = _cells.size(); _cells.push_back(cell);
	}
	return !in.err() && !in.eos() && in.pos() == in.size();
}
bool Enhancements::read(Common::SeekableReadStream &in) {
	if (in.readUint32BE() != MKTAG('N','X','E','H') || in.readUint32LE() != 1) return false;
	const uint32 size = in.readUint32LE();
	if (size > kMaxBytes || int64(size) > in.size() - in.pos()) return false;
	Common::Array<byte> bytes; bytes.resize(size);
	if (size && in.read(bytes.data(), size) != size) return false;
	Common::MemoryReadStream body(bytes.data(), bytes.size()); Enhancements candidate;
	if (!candidate.readBody(body)) return false;
	*this = candidate; return true;
}
} // namespace Nox
