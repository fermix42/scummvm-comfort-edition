/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nox/runtime.h"

namespace Nox {
static bool observerCodeMatches(const byte *data, uint length, uint32 expected) {
	uint32 hash = 2166136261u;
	for (uint i = 0; i < length; ++i) hash = (hash ^ data[i]) * 16777619u;
	return hash == expected;
}
// Keep this diagnostic predicate separate after MSVC14.44 whole-program
// internal compiler errors in these keyboard-loop builds.
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool Runtime::battleInputReady() const {
    if(!_battleValidation||!keys.empty()||key_latch)return false;
    // The growing Vortex cone performs damage during these apparent animation
    // iterations; no acknowledgement may truncate those gameplay transitions.
    if(pc==0x9c26&&_vortexEffects&&mainram[0x32fd]==12)return false;
    if(pc==0x9c26&&_ringEffectValidation)return false;
    return peek(pc)==0xad&&peek(uint16(pc+1))==0&&peek(uint16(pc+2))==0xc0;
}
// Original final book-page waiting loop has a read and a post-read branch.
// Both are safe empty-key test boundaries; no original instruction is skipped.
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool Runtime::campaignBookInputReady() const {
    if(!_spellValidationObservation||!keys.empty()||key_latch||alt||read_aux||write_aux)return false;
    if(pc!=0xaf4e&&(pc!=0xaf51||(p&0x80)))return false;
    const byte loop[]={0xad,0,0xc0,0x10,0xfb};
    return !memcmp(mainram+0xaf4e,loop,sizeof(loop));
}
bool Runtime::atEndingInput() const {
    if(pc!=0x1324||mainram[0x2af9]!=8||mainram[0x31fb]!=133||!mainram[0x60e4])return false;
    if(mainram[0x99cb]!=0x20||mainram[0x99cc]!=0x24||mainram[0x99cd]!=0x13)return false;
    const byte *stack=alt?auxram:mainram;
    return (stack[0x100+byte(sp+1)]|uint(stack[0x100+byte(sp+2)])<<8)==0x99cd;
}
#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool Runtime::campaignConversationInputReady() const {
    return _dialogueValidationPolling&&((_strictCampaignWords&&observerCodeMatches(mainram+0xb7a1,0xb88d-0xb7a1,3042273437u))||(mainram[0x2af9]==74&&mainram[0x267d]==6))&&keys.empty()&&!key_latch&&!alt&&!read_aux&&!write_aux&&pc==0xb9d4&&mainram[0xb9d4]==0xad&&mainram[0xb9d5]==0&&mainram[0xb9d6]==0xc0;
}
byte Runtime::battleDirectionKey() const {
    const int partyX=mainram[0xbd00+mainram[0x620]],partyY=mainram[0xbd01+mainram[0x620]];
    uint best=65535;int dx=0,dy=-1;
    for(uint i=0;i<128;i+=8)if(mainram[0xbf02+i]&&mainram[0xbf03+i]!=255&&mainram[0xbf07+i]<2){
        const int deltaX=int(mainram[0xbf00+i])-partyX,deltaY=int(mainram[0xbf01+i])-partyY;
        const uint distance=ABS(deltaX)+ABS(deltaY);
        if(distance<best){best=distance;dx=deltaX;dy=deltaY;}
    }
    return ABS(dx)>ABS(dy)?(dx>0?21:8):(dy>0?10:11);
}
Common::String Runtime::currentMapResource() const {
	if (!_resourceArchive) return Common::String();
	// Existing LocationResources family identities, checked against the actual
	// original filename pointer. Never merge same coordinates across resources.
	static const char *families[] = {"SURF", nullptr, "UM", "RUIN", "UM", "TOWN2", "TOWN", "CSTL", "KEEP"};
	const byte type = mainram[0x267d];
	if (type >= ARRAYSIZE(families) || !families[type]) return Common::String();
	const uint name = mainram[0x6bb0] | uint(mainram[0x6bb1]) << 8;
	const uint length = mainram[name];
	if (!length || length > 47 || name + length + 1 > sizeof(mainram)) return Common::String();
	const Common::String resource = "NA/" + Common::String(reinterpret_cast<const char *>(mainram + name + 1), length);
	const Common::String expected = "NA/DATA.MAP." + Common::String(families[type]);
	if (resource != expected && !(type == 0 && (resource == expected + "1" || resource == expected + "2"))) return Common::String();
	const HdvArchive::Entry *entry = _resourceArchive->entry(resource);
	return entry && entry->storage != 13 ? resource : Common::String();
}
Common::String Runtime::observedText(uint16 address, uint max) const {
	Common::String text;
	for (uint at = address; at < 0xc000 && mainram[at] && text.size() < max; ++at) {
		const byte ch = mainram[at] & 127;
		if (ch == 13) text += '\n';
		else if (ch >= 32 && ch < 127) text += char(ch);
	}
	return text;
}
bool Runtime::observeEnhancements() {
    if (_battleControlObservation && inBattle() && pc==0x1089 && peek(pc)==0x8a && _battleControlTrace.size()<16384) {
        const byte *stack=alt?auxram:mainram;
        const uint16 caller=uint16(stack[0x100+byte(sp+1)]|uint16(stack[0x100+byte(sp+2)])<<8);
        _battleControlTrace+=Common::String::format("rng[caller=%04X alt=%u bank=%u side=%u actor=%u bounds=%u/%u seed=%08X party=%u/%u enemy=%u/%u target=%u/%u navigation=%u];",caller,uint(alt),uint(bank),uint(mainram[0x33f9]),uint(mainram[0x620]),uint(mainram[0x630b]),uint(mainram[0x630c]),READ_LE_UINT32(mainram+0x6307),uint(mainram[0xbd00]),uint(mainram[0xbd01]),uint(mainram[0xbf00]),uint(mainram[0xbf01]),uint(mainram[0x644]),uint(mainram[0x645]),uint(mainram[0x6093]));
    }
    if(_dialogueValidationPolling&&!alt&&!read_aux&&!write_aux&&pc==0x9ac5&&mainram[0x9600]==0x98&&mainram[0x9601]==0x30&&peek(pc)==0xce&&peek(pc+1)==0x21&&peek(pc+2)==4)++_pagePickAttempts;
    if(_battleValidation){
        if(_ringEffectValidation&&mainram[0x2af9]==32&&mainram[0x33f9]){
            // Observe the next original action after the protected spell;
            // ordinary spells/weapon attacks count too, not just death spells.
            const bool nextSpell=pc==0x9cc4&&peek(pc)==0xc9&&peek(pc+1)==11;
            const bool nextWeapon=pc==0xf0b9&&mainram[0x32fd]==255&&peek(pc)==0xad&&peek(pc+1)==0xf9&&peek(pc+2)==0x33;
            if((nextSpell||nextWeapon)&&_ringProtectedSpells&&!_ringSurvivedSpellHP){_ringSurvivedSpellHP=campaignPartyHealth();_ringHealthTrace+=Common::String::format("after-completed-protected-spell-before-%s HP=%u actor=%u;",nextSpell?"spell":"weapon",campaignPartyHealth(),mainram[0x620]);}
            if(nextSpell&&(a==11||a==139)){
                if(_ringHealthTrace.size()<4096)_ringHealthTrace+=Common::String::format("death-cast=%u actor=%u original=%u HP=%u prior_protected=%u;",_ringDeathSpells+1,mainram[0x620],a,campaignPartyHealth(),_ringProtectedSpells);
                ++_ringDeathSpells;
            }
            if(pc==0x9cd5&&peek(pc)==0xad&&peek(pc+1)==0xcb&&peek(pc+2)==0x60)++_ringProtectedSpells;
        }
        if(pc==0x9600&&peek(pc)==0x20&&peek(pc+1)==0x87&&peek(pc+2)==0x89)_battleAdmission=true;
        if(pc==0x9c01&&mainram[0x32fd]==12&&!mainram[0x33f9]&&peek(pc)==0xa9&&peek(pc+1)==1&&peek(pc+2)==0x8d&&peek(pc+3)==0xb3&&peek(pc+4)==0x0b){++_vortexEffects;for(uint i=0;i<16;++i)_vortexEnemyHP[i]=auxram[0x1902+i*32]|uint(auxram[0x1903+i*32])<<8;}
        if(pc==0x9a57&&peek(pc)==0x20&&peek(pc+1)==0x51&&peek(pc+2)==0x9a&&mainram[0x9600]==0x20&&mainram[0x9601]==0x14&&mainram[0x9602]==0x7f)_battleTargeting=true;
        if(_vortexEffects&&mainram[0x32fd]==12&&pc==0x9c89&&peek(pc)==0x20&&peek(pc+1)==0x2d&&peek(pc+2)==0x13)_vortexDirectionPending=true;
        if(pc==0x9c8c)_vortexDirectionPending=false;
        if(mainram[0x32fd]==255&&_weaponTrace.size()<4096){
            if(!mainram[0x33f9]&&alt&&pc==0xe16c&&peek(pc)==0x8d&&peek(pc+1)==0x1c&&peek(pc+2)==6)_weaponTrace+=Common::String::format("hit-roll[side0 chance%u roll%u];",mainram[0x61b],a);
            if(!mainram[0x33f9]&&alt&&(pc==0xdecb||pc==0xdf06||pc==0xdf1b||pc==0xdf28||pc==0xdf39||pc==0xdf59))_weaponTrace+=Common::String::format("physical-path[%04X kind%u special%u targetFlags%02X geometry%u weapon%u range%u];",pc,mainram[0x600],mainram[0x625],mainram[0x288],mainram[0x6093],mainram[0x604],mainram[0x61d]);
            if(pc==0xf0b9&&peek(pc)==0xad&&peek(pc+1)==0xf9&&peek(pc+2)==0x33)_weaponTrace+=Common::String::format("weapon[side%u kind%u special%u atk%u def%u flags%02X rangeSkill%u];",mainram[0x33f9],mainram[0x600],mainram[0x625],mainram[0x28a],mainram[0x285],mainram[0x288],auxram[0x161c]);
            if(pc==0xf108&&peek(pc)==0xa9&&peek(pc+1)==3)_weaponTrace+="intelligence-refused;";
            if(pc==0xf2ad&&peek(pc)==0xac&&peek(pc+1)==7&&peek(pc+2)==6)_weaponTrace+=Common::String::format("computedDamage=%u result%u;",mainram[0xbeb]|uint(mainram[0xbec])<<8,mainram[0x62c]);
        }
        if(pc==0xf843&&peek(pc)==0x8d&&peek(pc+1)==0xd9&&peek(pc+2)==5)_battleTargeting=true;
        if(pc==0xf9c5&&peek(pc)==0x68&&peek(pc+1)==0xa8)_battleTargeting=false;
        if(_vortexEffects&&mainram[0x32fd]==12&&_vortexTargetTrace.size()<4096){
            if(pc==0x9faa&&peek(pc)==0xc9&&peek(pc+1)==2)_vortexTargetTrace+=Common::String::format("class[y=%u]=%u actor=%u,%u;",y,a,mainram[0x682],mainram[0x683]);
            if(pc==0x9fc9&&peek(pc)==0x8d&&peek(pc+1)==0xff)_vortexTargetTrace+=Common::String::format("geometry=%u u=%u v=%u dir=%02X;",a,mainram[0x367b],mainram[0x2afa],mainram[0xba7]);
            if(pc==0x9ac4&&peek(pc)==0xad&&peek(pc+1)==0x7d&&peek(pc+2)==0x26)_vortexTargetTrace+=Common::String::format("completed-list=%u;",mainram[0x240]);
        }
        if(alt&&(pc==0xd887||pc==0xdda4))_battleTargeting=false;
    }

	if(_spellValidationObservation&&!alt&&!read_aux&&!write_aux&&pc==0x9d3a&&mainram[0x32fd]==12&&mainram[0x267d]!=255&&mainram[0x9d3a]==0xa9&&mainram[0x9d3b]==68)++_worldCastRefusals;
	if(_spellValidationObservation&&!alt&&!read_aux&&!write_aux&&pc==0xadd5&&a==5&&mainram[0x25a]==4&&mainram[0xac6d]==0x20&&mainram[0xac6e]==0x62&&mainram[0xac6f]==0xa1){
		const uint ret=mainram[0x100+byte(sp+1)]|uint(mainram[0x100+byte(sp+2)])<<8;
		if(ret==0xace6)++_spellReadyRefusals;
	}
	if (!_captureEnhancements || alt || read_aux || write_aux || side(pc, false) != 0) return true;
	auto failed = [&]() { failure = "Nox enhancement observation exceeded its state contract"; return false; };
	auto conversationCode = [&]() { return observerCodeMatches(mainram + 0xb7a1, 0xb88d - 0xb7a1, 3042273437u); };
	// Adapted from conversation_observer.inc: commit a reply only after the
	// resolved text renderer returns with the expected stack and resource code.
	if (_conversationPending && pc == _conversationReturn && sp == _conversationSP && conversationCode()) {
		bool spoken = false;
		for (uint i = 0; i < _conversationSegment.size(); ++i)
			if (_conversationSegment[i] != ' ' && _conversationSegment[i] != '\n' && _conversationSegment[i] != '\r') spoken = true;
		if (spoken) {
			if (!_conversationResponse.empty()) _conversationResponse += '\n';
			if (_conversationResponse.size() + _conversationSegment.size() > 8192) return failed();
			_conversationResponse += _conversationSegment;
		}
		if (_conversationReturn == 0xaf4e || _conversationReturn == 0xaf6d) {
			if (!_enhancements.recordReply(_conversationOpening, _conversationInput, _conversationSelected, _conversationResponse)) return failed();
			_conversationQuery = false;
		}
		_conversationPending = false;
	}
	if (pc == 0xb0d8 && conversationCode()) {
		const uint length = mainram[0x99f6] | uint(mainram[0x99f7]) << 8;
		const uint nameLength = mainram[0x6cda];
		if (length && length <= 0x13fd && nameLength && nameLength <= 47) {
			const Common::String resource = "NA/" + Common::String(reinterpret_cast<const char *>(mainram + 0x6cdb), nameLength);
			if (_resourceArchive && _resourceArchive->entry(resource)) {
				if (!_enhancements.beginConversation(resource, mainram[0x67d], mainram[0x67a], mainram + 0x9a00, length)) return failed();
				_conversationActive = true; _conversationInput.clear();
			}
		}
	}
	if (pc == 0xaf9c && (a == 1 || a == 2) && !mainram[0x6d7] && _conversationActive && conversationCode()) {
		_conversationOpening = a == 1; _conversationQuery = true;
		_conversationResponse.clear(); _conversationSelected.clear();
		_conversationInput = a == 2 ? observedText(0x615, 64) : Common::String();
	}
	if (pc == 0xb216 && _conversationQuery && !_conversationOpening && _conversationSelected.empty() && conversationCode()) {
		const uint selected = mainram[0x6ca] | uint(mainram[0x6cb]) << 8;
		if (selected >= 0x9a00 && selected < 0xadfd) {
			for (uint i = selected; i > selected - 32; --i) if (mainram[i - 1] == 0xa4 && mainram[i] == 0xa1) {
				Common::String label;
				for (uint at = i + 1; at < selected && label.size() < 39; ++at) label += char(mainram[at] & 127);
				if (!label.empty() && label[0] != '%' && label != "DFL") _conversationSelected = label;
				break;
			}
		}
	}
	if (pc == 0x7cec && _conversationQuery && conversationCode()) {
		const uint16 ret = uint16((mainram[0x100 + byte(sp + 1)] | uint(mainram[0x100 + byte(sp + 2)]) << 8) + 1);
		const uint response = mainram[0xe4] | uint(mainram[0xe5]) << 8;
		if ((ret == 0xaf4e || ret == 0xaf6d || ret == 0xb279) && response >= 0x200 && response < 0xc000) {
			_conversationSegment = observedText(response, 8192); _conversationReturn = ret; _conversationSP = byte(sp + 2); _conversationPending = true;
		}
	}
	if (pc == 0x9b06 && _conversationActive && observerCodeMatches(mainram + 0x9b06, 0x9d4d - 0x9b06, 2977316293u)) _conversationJoinCount = mainram[0x70a3];
	if (atWorldInput()) {
		if (_conversationJoinCount && mainram[0x70a3] == _conversationJoinCount + 1 && mainram[0x70a3] <= 6) {
			Common::String name; const uint at = 0x1600 + (mainram[0x70a3] - 1) * 128 + 75;
			for (uint n = 0; n < 31 && auxram[at + n]; ++n) name += char(auxram[at + n] & 127);
			if (!_enhancements.nameCharacter(name)) return failed();
		}
		_conversationJoinCount = 0;
		if (observerCodeMatches(mainram + 0x7102, 0x72a7 - 0x7102, 4183333227u)) {
			// A completed exit/Bye may return without a printed reply. Its
			// transient query is over; do not invent a journal response for it.
			_conversationQuery = false; _conversationPending = false;
			_activeMapResource = currentMapResource();
			if (!_activeMapResource.empty() && !_enhancements.discover(_activeMapResource, mainram[0x267d], mainram[0x2af9], positionX(), positionY(), true, false)) return failed();
		}
	}
	return true;
}
} // namespace Nox
