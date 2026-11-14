/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CONVERSATION_H
#define NOX_NATIVE_CONVERSATION_H

#include "nox/native_dialogue_rules.h"
#include "nox/native_dialogue_input.h"
#include "nox/native_dialogue_control.h"
#include "nox/native_inventory.h"
#include "nox/enhancements.h"
#include "engines/achievements.h"
#include "common/stream.h"

namespace Nox {
namespace Native {

enum ConversationResult { kConversationReply, kConversationNoReply, kConversationUnsupported, kConversationInvalid };
struct ConversationContext {
	byte map, mapType, voice, hour, minute, serviceAccess;
	bool adultMode, partyFull;
	ConversationContext() : map(0), mapType(255), voice(0), hour(0), minute(0), serviceAccess(0), adultMode(false), partyFull(false) {}
};

// Authored bodies, optionally replacing the one $K marker selected by the
// response parser. Insert the original submitted input, not its lexical alias;
// inserted text is literal and never interpreted as another presentation tag.
inline bool plainDialogueBody(const Common::Array<byte> &text, uint start, Common::String &output,
        const Common::String *input=nullptr,uint inputPrefix=0) {
	Common::String next;bool inserted=false;
	for (uint i = start; i < text.size(); ++i) {
		if (!text[i]) {if (input && !inserted) return false;output = next; return true;}
		const byte value = text[i] & 127;
        if (value=='$' && input && !inserted && i-start==inputPrefix && i+1<text.size() && (text[i+1]&127)=='K') {
            next+=*input;inserted=true;++i;
        }
		else if (value == 13) next += '\n';
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
	Conversation() : _active(false), _terminal(false), _recognized(false), _completed(false), _journalPending(false), _journalOpening(false),_paymentPhase(0),_paymentPresentation(false) { memset(_fields, 0, sizeof(_fields)); }
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
		_paymentPhase=0;_paymentPresentation=false;_paymentQuery.clear();
		_active = true; _terminal = false; _recognized = false; _completed = false; _reply.clear(); return true;
	}
	ConversationResult opening(GameplayRandom *random=nullptr) { return respond(Common::String(), true,false,random); }
	ConversationResult submit(const Common::String &query,GameplayRandom *random=nullptr) { return respond(query, false,false,random); }
	ConversationResult closing(GameplayRandom *random=nullptr) { return respond("%END",false,true,random); }
	const Common::String &reply() const { return _reply; }
	const byte *fields() const { return _fields; }
	bool terminalReply() const { return _terminal; }
	bool recognizedReply() const { return _recognized; }
	bool completed() const { return _completed; }
	const ConversationContext &context() const { return _context; }
	const DialoguePacket &packet() const { return _packet; }
    bool recordSpokenReply(Enhancements &owner) {
        if (!_journalPending) return true;
        if (_reply.empty()) {_journalPending=_journalOpening=false;_journalInput.clear();_journalSelected.clear();return true;}
        if (!owner.beginConversation(_resource,_packet.record,_packet.actor,_packet.text.data(),_packet.text.size()) ||
            !owner.recordReply(_journalOpening,_journalInput,_journalSelected,_reply)) return false;
        _journalPending=_journalOpening=false;_journalInput.clear();_journalSelected.clear();
        AchMan.noteCEGameEvent("nox-archaist","apple2-v119","dialogue_reply");
        return true;
    }
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
		const byte flags=byte((_active?1:0)|(_terminal?2:0)|(_recognized?4:0)|(_completed?8:0)|(_journalPending?16:0)|(_journalOpening?32:0));
		if ((_active && _completed) || _resource.size()>31 || _reply.size()>8192) return false;
		out.writeUint32BE(MKTAG('N','C','O','N'));out.writeUint16LE(_paymentPhase?3:2);out.writeByte(flags);
		if (_resource.empty()) return !flags && _packet.text.empty() && !out.err();
		out.writeByte(byte(_resource.size()));out.write(_resource.c_str(),_resource.size());
		out.writeByte(_packet.actor);out.writeByte(_packet.record);out.writeUint32LE(_packet.offset);
		out.writeByte(_context.map);out.writeByte(_context.mapType);out.writeByte(_context.voice);
		out.writeByte(_context.hour);out.writeByte(_context.minute);out.writeByte(_context.serviceAccess);
		out.writeByte(_context.adultMode?1:0);out.writeByte(_context.partyFull?1:0);
		out.write(_fields,256);out.writeUint16LE(uint16(_reply.size()));out.write(_reply.c_str(),_reply.size());
        out.writeByte(_journalInput.size());out.write(_journalInput.c_str(),_journalInput.size());
        out.writeByte(_journalSelected.size());out.write(_journalSelected.c_str(),_journalSelected.size());
        if (_paymentPhase) {out.writeByte(_paymentPhase);out.writeByte(_paymentPresentation);out.writeByte(_paymentQuery.size());out.write(_paymentQuery.c_str(),_paymentQuery.size());}
		return !out.err();
	}
	bool load(Common::SeekableReadStream &in,HdvArchive &archive) {
		if (in.readUint32BE()!=MKTAG('N','C','O','N')) return false;
        const uint version=in.readUint16LE();if (version!=1 && version!=2 && version!=3) return false;
		const byte flags=in.readByte();if (flags>(version==1?15:63) || ((flags&1) && (flags&8))) return false;
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
		const uint size=in.readUint16LE();if (size>8192 || in.size()-in.pos()<size) return false;
		for (uint i=0;i<size;++i) next._reply+=char(in.readByte());
        if (version>=2) {
            Common::String *texts[]={&next._journalInput,&next._journalSelected};
            for (uint j=0;j<2;++j) {const uint count=in.readByte();if (count>28 || count>in.size()-in.pos()) return false;for (uint i=0;i<count;++i) *texts[j]+=char(in.readByte());}
            next._journalPending=(flags&16)!=0;next._journalOpening=(flags&32)!=0;
            if (next._journalOpening && !next._journalPending) return false;
        }
        if (version==3) {
            next._paymentPhase=in.readByte();const byte presentation=in.readByte(),queryLength=in.readByte();
            if (!next._paymentPhase || next._paymentPhase>4 || presentation>1 || queryLength>28 || queryLength>in.size()-in.pos() || !(flags&1)) return false;
            next._paymentPresentation=presentation!=0;for(uint i=0;i<queryLength;++i) next._paymentQuery+=char(in.readByte());
            if(next._paymentPhase<4 && !READ_LE_UINT16(next._fields+18)) return false;
        }
		if (in.err() || in.pos()!=in.size()) return false;
		next._active=(flags&1)!=0;next._terminal=(flags&2)!=0;next._recognized=(flags&4)!=0;next._completed=(flags&8)!=0;
		*this=next;return true;
	}
	HandoffResult finish(const ItemCatalog &catalog, Inventory &inventory, uint16 &gold) {
		if (!_active || _paymentPhase) return kHandoffInvalid;
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
    bool finishRecruitment() {
        DialogueHandoff request;
        if (!_active || callbacksPending(_fields) || !describeDialogueHandoff(_fields,sizeof(_fields),request) || request.kind!=DialogueHandoff::kRecruit) return false;
        _active=false;_completed=true;return true;
    }
    bool trainingPending() const {return (_fields[21]&3)==2 && _fields[22] && _fields[22]<64 && READ_LE_UINT16(_fields+18);}
    bool completeTraining() {if (!_active || !trainingPending()) return false;_fields[18]=_fields[19]=0;return true;}
    byte paymentPhase() const {return _paymentPhase;}
    bool paymentPresentationPending() const {return _paymentPresentation;}
    bool paymentPresentation(HdvArchive &archive,Common::String &text) {
        if (!_paymentPresentation || !_paymentPhase) return false;_paymentPresentation=false;
        if (_paymentPhase==2) return tavernText(archive,0x9760,text);
        if (_paymentPhase==3) {
            Common::String prefix,suffix;if(!tavernText(archive,0x981b,prefix) || !tavernText(archive,0x983f,suffix)) return false;
            text=prefix+Common::String::format("%u",uint(READ_LE_UINT16(_fields+18)))+suffix;return true;
        }
        return false;
    }
    // 0 ignores input,1 presents an authored result,2 resumes keyword input.
    int paymentInput(HdvArchive &archive,byte key,uint16 &gold,Common::String &text,GameplayRandom *random=nullptr) {
        if (!_active || !_paymentPhase) return -1;
        key&=127;if(key>='a' && key<='z') key-=32;
        if (_paymentPhase==4) {_paymentPhase=0;_paymentQuery.clear();return 2;}
        if (_paymentPhase==1) {
            if(key=='Y') {_paymentPhase=2;return tavernText(archive,0x9760,text)?1:-1;}
            if(key!='N' && key!=27) return 0;
            return paymentResult(archive,0x9897,text);
        }
        if (_paymentPhase==2) {
            if(key==27) return paymentResult(archive,0x9897,text);
            if(key!='1' && key!='2') return 0;
            const uint16 price=key=='2'?5:READ_LE_UINT16(_fields+18);
            if (!(_fields[42]&2) && !spendGold(gold,price)) return paymentResult(archive,0x98b6,text);
            if(key=='2') _fields[17]|=16;
            _fields[20]=byte(_fields[20]+1);_fields[17]|=2;
            return paymentResult(archive,key=='2'?0x97a0:0x97fe,text);
        }
        if(key!='Y' && key!='N' && key!=27) return 0;
        if(key!='Y') return paymentResult(archive,0x9866,text);
        if(!spendGold(gold,READ_LE_UINT16(_fields+18))) return paymentResult(archive,0x98b6,text);
        _fields[17]|=4;_fields[18]=_fields[19]=0;_paymentPhase=0;
        const Common::String query=_paymentQuery;_paymentQuery.clear();
        const ConversationResult result=respond(query,query.empty(),false,random);_fields[17]&=~4;
        if(result!=kConversationReply || _paymentPhase) return -1;
        text=_reply;return 1;
    }
private:
    static bool tavernText(HdvArchive &archive,uint address,Common::String &text) {
        Common::Array<byte> source;return archive.read("NA/SRTN.NPC.TALK",source) && address>=0x9600 && plainDialogueBody(source,0x1269+address-0x9600,text);
    }
    int paymentResult(HdvArchive &archive,uint address,Common::String &text) {
        _fields[18]=_fields[19]=0;_paymentPhase=4;_paymentPresentation=false;
        return tavernText(archive,address,text)?1:-1;
    }
	bool callbacksPending(const byte *fields) const {
		// Field17 bit5 is a typed NPC battle request consumed by WorldInput
		// after the displayed reply and ordinary exit handoff have completed.
		return (fields[12] & 32) ||
			((fields[21] & 3) && !(fields[21]&1) && (fields[18] || fields[19]) && !(fields[22] && fields[22]<64)) ||
			(fields[42] & 128) || (fields[46] & 178) || (_context.map == 68 && (fields[58] & 32));
	}
	ConversationResult respond(const Common::String &query, bool opening,bool ending=false,GameplayRandom *random=nullptr) {
		if (!_active || (_terminal && !ending)) return kConversationInvalid;
		if (!opening && query.empty()) return kConversationUnsupported;
		Common::String normalized;
		if (!opening && !_lexicon.normalize(query, normalized)) return kConversationInvalid;
		DialogueResponse response;
		bool recognized = selectDialogueResponse(_packet.text, 0, normalized, 0, QuestFields(_fields, sizeof(_fields)), _context.voice, _context.adultMode, _context.hour, _context.minute, response, opening);
		if (!recognized && ending) { _reply.clear();_terminal=true;_recognized=false;return kConversationNoReply; }
		if (!recognized && (opening || !selectDialogueResponse(_packet.text, 0, "DFL", 0, QuestFields(_fields, sizeof(_fields)), _context.voice, _context.adultMode, _context.hour, _context.minute, response))) return kConversationNoReply;
		// Resolve source control once, using the explicitly supplied session RNG.
		Common::String reply;
		if (!plainDialogueBody(_packet.text, response.bodyOffset, reply,
                response.substituteInput?&query:nullptr,response.inputPrefixLength)) return kConversationUnsupported;
		byte next[256]; memcpy(next, _fields, sizeof(next));
		const DialogueCommitResult result = commitDialogueFields(response, next, sizeof(next), _context.voice, _context.adultMode, _context.hour, _context.minute);
		if (result == kDialogueInvalidEffects) return kConversationUnsupported;
		if (result != kDialogueApplied) return kConversationInvalid;
		if (callbacksPending(next)) return kConversationUnsupported;
        GameplayRandom after;if (random) after=*random;bool terminal=false;
        if (!DialogueControl::resolve(response.control,random?&after:nullptr,terminal)) return kConversationUnsupported;
		memcpy(_fields, next, sizeof(_fields));if (random) *random=after;
        if ((_fields[21]&1) && READ_LE_UINT16(_fields+18)) {
            _paymentPhase=(_fields[17]&8)?(opening?1:2):3;
            _paymentPresentation=_paymentPhase!=1;_paymentQuery=opening?Common::String():query;
        }
		// Journal recognition denotes a submitted keyword, not an opening.
		_reply = reply; _terminal = ending || terminal; _recognized = recognized && !opening && !ending;
        _journalPending=true;_journalOpening=opening;_journalInput=opening?Common::String():query;
        _journalSelected=_recognized?normalized:Common::String("DFL");return kConversationReply;
	}
    byte _paymentPhase;bool _paymentPresentation;Common::String _paymentQuery;
	DialoguePacket _packet;
	Common::String _resource;
	DialogueLexicon _lexicon;
	ConversationContext _context;
	byte _fields[256];
	bool _active, _terminal, _recognized, _completed, _journalPending, _journalOpening;
    Common::String _journalInput,_journalSelected;
	Common::String _reply;
};

} // namespace Native
} // namespace Nox
#endif
