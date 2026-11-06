/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CONVERSATION_H
#define NOX_NATIVE_CONVERSATION_H

#include "nox/native_dialogue_rules.h"
#include "nox/native_dialogue_input.h"
#include "nox/native_inventory.h"
#include "common/stream.h"

namespace Nox {
namespace Native {

enum ConversationResult { kConversationReply, kConversationNoReply, kConversationUnsupported, kConversationInvalid };
struct ConversationContext {
	byte map, mapType, voice, hour, minute, serviceAccess;
	bool adultMode, partyFull;
	ConversationContext() : map(0), mapType(255), voice(0), hour(0), minute(0), serviceAccess(0), adultMode(false), partyFull(false) {}
};

// Plain authored bodies need no substitution/presentation program. Structured
// text controls stay explicitly unsupported until their presentation owner runs.
inline bool plainDialogueBody(const Common::Array<byte> &text, uint start, Common::String &output) {
	Common::String next;
	for (uint i = start; i < text.size(); ++i) {
		if (!text[i]) { output = next; return true; }
		const byte value = text[i] & 127;
		if (value == 13) next += '\n';
		else if (value == 10 || (value >= 32 && value < 127 && value != '$')) next += char(value);
		else return false;
	}
	return false;
}

// Owned semantic conversation phase. Physical NPC admission/scheduling, UI,
// journal events and world/commerce/battle callbacks remain outside this owner.
// Context.serviceAccess is the source-supported admission result supplied by
// the caller, not invented here from a guessed NPC picture or map geometry.
class Conversation {
public:
	Conversation() : _active(false), _terminal(false), _recognized(false), _completed(false) { memset(_fields, 0, sizeof(_fields)); }
	bool begin(HdvArchive &archive, const Common::String &resource, byte actor,
			const byte *fields, uint size, const ConversationContext &context) {
		if (_active || !fields || size != sizeof(_fields) || context.voice > 2 || context.serviceAccess > 3) return false;
		DialoguePacket packet; Common::String error;
		if (!loadDialoguePacket(archive, resource, actor, packet, error) || !_lexicon.load(archive)) return false;
		_packet = packet; _resource = resource; _context = context; memcpy(_fields, fields, sizeof(_fields));
		// Source admission resets per-conversation requests, preserving earned
		// campaign flags and all other request fields until authored effects.
		_fields[16] = 63; _fields[22] = 0;
		_fields[21] = (_fields[21] & ~3) | context.serviceAccess;
		_fields[17] = (_fields[17] & ~33) | (context.partyFull ? 1 : 0);
		_active = true; _terminal = false; _recognized = false; _completed = false; _reply.clear(); return true;
	}
	ConversationResult opening() { return respond(Common::String(), true); }
	ConversationResult submit(const Common::String &query) { return respond(query, false); }
	ConversationResult closing() { return respond("%END",false,true); }
	const Common::String &reply() const { return _reply; }
	const byte *fields() const { return _fields; }
	bool terminalReply() const { return _terminal; }
	bool recognizedReply() const { return _recognized; }
	bool completed() const { return _completed; }
	const ConversationContext &context() const { return _context; }
	const DialoguePacket &packet() const { return _packet; }
	static bool initializeFields(byte *fields,uint size,const ConversationContext &context) {
		if (!fields || size!=256 || context.voice>2 || context.serviceAccess>3) return false;
		fields[16]=63;fields[22]=0;
		fields[21]=(fields[21]&~3)|context.serviceAccess;
		fields[17]=(fields[17]&~33)|(context.partyFull?1:0);return true;
	}
	// Persist owned continuation data, not a parser/CPU return address. Loading
	// reopens the immutable packet and lexicon without repeating admission,
	// field effects, an opening response or an already completed exit award.
	bool save(Common::WriteStream &out) const {
		const byte flags=byte((_active?1:0)|(_terminal?2:0)|(_recognized?4:0)|(_completed?8:0));
		if ((_active && _completed) || _resource.size()>31 || _reply.size()>8192) return false;
		out.writeUint32BE(MKTAG('N','C','O','N'));out.writeUint16LE(1);out.writeByte(flags);
		if (_resource.empty()) return !flags && _packet.text.empty() && !out.err();
		out.writeByte(byte(_resource.size()));out.write(_resource.c_str(),_resource.size());
		out.writeByte(_packet.actor);out.writeByte(_packet.record);out.writeUint32LE(_packet.offset);
		out.writeByte(_context.map);out.writeByte(_context.mapType);out.writeByte(_context.voice);
		out.writeByte(_context.hour);out.writeByte(_context.minute);out.writeByte(_context.serviceAccess);
		out.writeByte(_context.adultMode?1:0);out.writeByte(_context.partyFull?1:0);
		out.write(_fields,256);out.writeUint16LE(uint16(_reply.size()));out.write(_reply.c_str(),_reply.size());
		return !out.err();
	}
	bool load(Common::SeekableReadStream &in,HdvArchive &archive) {
		if (in.readUint32BE()!=MKTAG('N','C','O','N') || in.readUint16LE()!=1) return false;
		const byte flags=in.readByte();if (flags>15 || ((flags&1) && (flags&8))) return false;
		Conversation next;
		if (in.pos()==in.size()) { if (flags || in.err()) return false;*this=next;return true; }
		const byte length=in.readByte();if (!length || length>31 || (!flags || !(flags&9))) return false;
		for (uint i=0;i<length;++i) next._resource+=char(in.readByte());
		const byte actor=in.readByte(),record=in.readByte();const uint32 offset=in.readUint32LE();
		Common::String error;
		if (!loadDialoguePacket(archive,next._resource,actor,next._packet,error) || !next._lexicon.load(archive) ||
				next._packet.record!=record || next._packet.offset!=offset) return false;
		next._context.map=in.readByte();next._context.mapType=in.readByte();next._context.voice=in.readByte();
		next._context.hour=in.readByte();next._context.minute=in.readByte();next._context.serviceAccess=in.readByte();
		const byte adult=in.readByte(),full=in.readByte();if (adult>1 || full>1 || next._context.voice>2 || next._context.serviceAccess>3) return false;
		next._context.adultMode=adult!=0;next._context.partyFull=full!=0;
		if (in.read(next._fields,256)!=256) return false;
		const uint size=in.readUint16LE();if (size>8192 || in.size()-in.pos()!=size) return false;
		for (uint i=0;i<size;++i) next._reply+=char(in.readByte());
		if (in.err() || in.pos()!=in.size()) return false;
		next._active=(flags&1)!=0;next._terminal=(flags&2)!=0;next._recognized=(flags&4)!=0;next._completed=(flags&8)!=0;
		*this=next;return true;
	}
	HandoffResult finish(const ItemCatalog &catalog, Inventory &inventory, uint16 &gold) {
		if (!_active) return kHandoffInvalid;
		// Never silently bypass services or world callbacks that this phase does
		// not yet own. Source executes these before the shared exit handoff.
		if (callbacksPending(_fields)) return kHandoffInvalid;
		_fields[17]&=~2; // Talk AF7F: clear transient exit request before handoff.
		DialogueHandoff request;
		if (!describeDialogueHandoff(_fields, sizeof(_fields), request)) return kHandoffInvalid;
		const HandoffResult result = finishDialogueHandoff(request, catalog, inventory, gold);
		if (result == kHandoffApplied) { _active = false; _completed = true; }
		return result;
	}
private:
	bool callbacksPending(const byte *fields) const {
		// Field17 bit5 is a typed NPC battle request consumed by WorldInput
		// after the displayed reply and ordinary exit handoff have completed.
		return (fields[12] & 32) ||
			((fields[21] & 3) && (fields[18] || fields[19])) ||
			(fields[42] & 128) || (fields[46] & 178) || (_context.map == 68 && (fields[58] & 32));
	}
	ConversationResult respond(const Common::String &query, bool opening,bool ending=false) {
		if (!_active || (_terminal && !ending)) return kConversationInvalid;
		if (!opening && query.empty()) return kConversationUnsupported;
		Common::String normalized;
		if (!opening && !_lexicon.normalize(query, normalized)) return kConversationInvalid;
		DialogueResponse response;
		bool recognized = selectDialogueResponse(_packet.text, 0, normalized, 0, QuestFields(_fields, sizeof(_fields)), _context.voice, _context.adultMode, _context.hour, _context.minute, response, opening);
		if (!recognized && ending) { _reply.clear();_terminal=true;_recognized=false;return kConversationNoReply; }
		if (!recognized && (opening || !selectDialogueResponse(_packet.text, 0, "DFL", 0, QuestFields(_fields, sizeof(_fields)), _context.voice, _context.adultMode, _context.hour, _context.minute, response))) return kConversationNoReply;
		// Positive probability controls and input substitutions require their
		// complete source-supported owners; do not fake a successful response.
		Common::String reply;
		if (response.substituteInput || (response.control != 255 && response.control != 0) || !plainDialogueBody(_packet.text, response.bodyOffset, reply)) return kConversationUnsupported;
		byte next[256]; memcpy(next, _fields, sizeof(next));
		const DialogueCommitResult result = commitDialogueFields(response, next, sizeof(next), _context.voice, _context.adultMode, _context.hour, _context.minute);
		if (result == kDialogueInvalidEffects) return kConversationUnsupported;
		if (result != kDialogueApplied) return kConversationInvalid;
		if (callbacksPending(next)) return kConversationUnsupported;
		memcpy(_fields, next, sizeof(_fields));
		// Journal recognition denotes a submitted keyword, not an opening.
		_reply = reply; _terminal = ending || response.control == 0; _recognized = recognized && !opening && !ending; return kConversationReply;
	}
	DialoguePacket _packet;
	Common::String _resource;
	DialogueLexicon _lexicon;
	ConversationContext _context;
	byte _fields[256];
	bool _active, _terminal, _recognized, _completed;
	Common::String _reply;
};

} // namespace Native
} // namespace Nox
#endif
