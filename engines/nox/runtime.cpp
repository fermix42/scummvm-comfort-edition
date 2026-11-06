/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nox/runtime.h"
#include "graphics/font.h"
#include "graphics/fontman.h"

namespace Nox {
static bool interfaceCodeMatches(const byte *data,uint length,uint32 expected) {
	uint32 hash=2166136261u;
	for(uint i=0;i<length;++i)hash=(hash^data[i])*16777619u;
	return hash==expected;
}
#include "nox/cycles.inc"
#include "nox/cpu.inc"

Runtime::Runtime() {
	memset(mainram, 0, sizeof(mainram)); memset(auxram, 0, sizeof(auxram));
	memset(lc, 0, sizeof(lc)); memset(high, 0, sizeof(high)); memset(rom, 0, sizeof(rom));
	rom[0x2ffe] = 0x81;
	speaker_init(&speaker, sample, this); mock_init(&mockingboard);
	// Use ScummVM's redistributable console font for the hardware text plane.
	// Game-rendered hires text still comes from the original game resources.
	Graphics::Surface glyph;
	glyph.create(8, 8, Graphics::PixelFormat::createFormatCLUT8());
	const Graphics::Font *console = FontMan.getFontByUsage(Graphics::FontManager::kConsoleFont);
	for (uint ch = 0; ch < 64; ++ch) {
		glyph.fillRect(Common::Rect(8, 8), 0);
		console->drawChar(&glyph, ch < 32 ? ch + 64 : ch, 0, 0, 1);
		for (uint row = 0; row < 8; ++row) {
			font[ch][row] = 0;
			for (uint col = 0; col < 7; ++col)
				if (*(const byte *)glyph.getBasePtr(col, row)) font[ch][row] |= 0x40 >> col;
		}
	}
	glyph.free();
}

bool Runtime::initialize(HdvArchive &resources) {
	_resourceArchive = &resources;
	Common::Array<byte> loader, system, driver;
	if (!resources.isBaseV119() || !resources.read("NA/LOADER.P", loader) || !resources.read("NOXARCH.SYSTEM", system) || !resources.read("NA/MOCK.DRIVER", driver)) { failure = resources.error(); return false; }
	if (loader.size() != 10514 || system.size() != 2385 || driver.size() != 637) { failure = "Unsupported bootstrap resources"; return false; }
	disk = resources.image();
	memcpy(mainram + 0x9600, loader.data(), loader.size());
	// Reuse nox_prepare_legacy_driver's transformation, obtaining bytes only
	// from the user's SYSTEM resource. The patched driver targets host block I/O.
	const uint base = 0x6ee;
	if (system[base] != 0x4c || system[base+0x223] != 0xd1 || system[base+0x23d] != 0xb1 || !resources.entry("NA")) { failure = "Unsupported SYSTEM bootstrap layout"; return false; }
	memcpy(lc[0][0], system.data() + base, 611);
	const uint16 directory = resources.entry("NA")->key;
	lc[0][0][0x11] = directory; lc[0][0][0x13] = directory >> 8;
	lc[0][0][0x223] = 0x10; lc[0][0][0x224] = 0xc7; lc[0][0][0x23d] = 0x91;
	memset(lc[0][0] + 0x25b, 0, 8);
	memcpy(mainram + 0x9000, driver.data(), driver.size());
	mainram[0x920b] = 2; mainram[0xaf] = auxram[0xaf] = 0x82; mainram[0xbf] = 0xc4;
	pc = 0x9000; push16(0x8fff);
	uint count = 0;
	do {
		if (++count > 10000 || !step()) { failure = "Music bootstrap failed"; return false; }
	} while (pc != 0x9000);
	pc = 0x9bcf; a=x=y=0; p=B|U; sp=255; alt=read_aux=write_aux=0; bank=ram=1;
	mainram[0x7c] = auxram[0x7c] = 7;
	samples.clear();
	return true;
}

void Runtime::switches(uint16 q) {
	if (q == 0xc008) alt=0; else if (q == 0xc009) alt=1;
	else if (q == 0xc002) read_aux=0; else if (q == 0xc003) read_aux=1;
	else if (q == 0xc004) write_aux=0; else if (q == 0xc005) write_aux=1;
	else if (q >= 0xc080 && q <= 0xc08f) { bank=(q&8)?1:2; ram=((q&3)==0||(q&3)==3); }
}
int Runtime::side(uint16 q, bool writing) const {
	if (video_80store && ((q>=0x400 && q<0x800) || (video_hires && q>=0x2000 && q<0x4000))) return video_page2;
	return writing ? write_aux : read_aux;
}
byte Runtime::peek(uint16 q) const {
	if (q < 0x200) return (alt ? auxram : mainram)[q];
	if (q < 0xc000) return (side(q, false) ? auxram : mainram)[q];
	if (q < 0xd000) return mainram[q];
	if (!ram) return rom[q-0xd000];
	if (q < 0xe000) return lc[alt][bank-1][q-0xd000];
	return high[alt][q-0xe000];
}
void Runtime::io(uint16 q, bool writing) {
	if (q == 0xc030) speaker_toggle(&speaker, cpu_cycles);
	if (q == 0xc010) key_latch=0;
	if (writing) {
		if(q==0xc000)video_80store=0; else if(q==0xc001)video_80store=1;
		else if(q==0xc00c)video_80col=0; else if(q==0xc00d)video_80col=1;
	}
	if(q==0xc050)video_text=0;else if(q==0xc051)video_text=1;
	else if(q==0xc052)video_mixed=0;else if(q==0xc053)video_mixed=1;
	else if(q==0xc054)video_page2=0;else if(q==0xc055)video_page2=1;
	else if(q==0xc056)video_hires=0;else if(q==0xc057)video_hires=1;
	else if(q==0xc05e)video_dhgr=1;else if(q==0xc05f)video_dhgr=0;
}
byte Runtime::rd(uint16 q) {
	if (q>=0xc400 && q<=0xc48f && (q&0x7f)<=15) return mock_read(&mockingboard, q-0xc400);
	if (q>=0xc000 && q<0xc100) {
		io(q, false);
		if(q==0xc000) {
			if(_dialogueValidationPolling && !key_latch && keys.empty()) ++_emptyKeyboardPolls;
			if (!key_latch && !keys.empty()) {
				key_latch=keys.pop();worldWaiting=false;
				// Consume Q only at the verified v119 world-command keyboard
				// service. Dialog text input and combat keys keep their semantics.
				// Returning an empty latch leaves the original command loop idle;
				// it never enters the legacy private-disk save controller.
				if(origin==0x71dc&&!alt&&!read_aux&&!write_aux&&!mainram[0x6052]&&
				   (key_latch==('Q'|128)||key_latch==('q'|128))&&
				   interfaceCodeMatches(mainram+0x7102,0x1a5,4183333227u)){
					key_latch=0;keys.clear();worldWaiting=true;interfaceRequest=kSaveInterfaceRequest;
				}
			}
			return key_latch;
		}
		if(q==0xc013)return read_aux?128:0;if(q==0xc014)return write_aux?128:0;
		if(q==0xc018)return video_80store?128:0;if(q==0xc019)return (total_steps%17000)<14000?128:0;
		if(q==0xc01a)return video_text?128:0;if(q==0xc01b)return video_mixed?128:0;
		if(q==0xc01c)return video_page2?128:0;if(q==0xc01d)return video_hires?128:0;if(q==0xc01f)return video_80col?128:0;
		switches(q);
		if(q==0xc011)return bank==2?128:0;if(q==0xc012)return ram?128:0;if(q==0xc016)return alt?128:0;
		return 0;
	}
	return peek(q);
}
void Runtime::wr(uint16 q, byte value) {
    if (_battleControlObservation && q==0xbf03 && !side(q,true) && mainram[q]!=value && _battleControlTrace.size()<16384)
        _battleControlTrace+=Common::String::format("enemy-state[origin=%04X alt=%u bank=%u type=%u old=%u new=%u];",origin,uint(alt),uint(bank),uint(mainram[0x267d]),uint(mainram[q]),uint(value));
	if(q>=0xc400 && q<=0xc48f && (q&0x7f)<=15) { speaker_advance(&speaker,cpu_cycles);mock_write(&mockingboard,q-0xc400,value,cpu_cycles);return; }
	if(q>=0xc000 && q<0xc100) {io(q,true);switches(q);return;}
	if(q<0x200)(alt?auxram:mainram)[q]=value;
	else if(q<0xc000)(side(q,true)?auxram:mainram)[q]=value;
	else if(q<0xd000)mainram[q]=value;
	else if(q<0xe000)lc[alt][bank-1][q-0xd000]=value;
	else high[alt][q-0xe000]=value;
}

void Runtime::arithmetic(byte v, int subtract) {
	unsigned result;byte old=a;
	if(p&D){
		unsigned n0,n1,hc,adjust0=0,adjust1=0,alu,dc;
		if(!subtract){
			n0=(old&15)+(v&15)+(p&C);hc=n0>9;if(hc)adjust0=6;
			n1=(old>>4)+(v>>4)+hc;dc=n1>9;if(dc)adjust1=6;
			alu=((n1&15)<<4)+(n0&15);
			p=(p&~(C|V|N|Z))|(dc?C:0)|((~(old^v)&(old^alu)&128)?V:0)|(alu&N)|(alu?0:Z);
			a=(((n1+adjust1)&15)<<4)|((n0+adjust0)&15);
		}else{
			n0=(old&15)+((v^255)&15)+(p&C);hc=n0>15;if(!hc)adjust0=10;
			n1=(old>>4)+((v^255)>>4)+hc;if(n1<=15)adjust1=160;
			result=unsigned(old)+(v^255)+(p&C);dc=result>255;alu=result&255;
			p=(p&~(C|V|N|Z))|(dc?C:0)|(((old^v)&(old^alu)&128)?V:0)|(alu&N)|(alu?0:Z);
			a=((((alu+adjust1)>>4)&15)<<4)|((alu+adjust0)&15);
		}return;
	}
	result=subtract?unsigned(old)+(v^255)+(p&C):unsigned(old)+v+(p&C);
	p=(p&~(C|V))|(result>255?C:0)|(((subtract?(old^v):~(old^v))&(old^result)&128)?V:0);
	a=result;nz(a);
}

uint32 Runtime::instructionCycles() const {
	if(pc==0xfe89||pc==0xf3f2||pc==0xc311||pc==0xc710)return 6;
	byte op=peek(pc),lo=peek(pc+1);uint16 addr=lo|uint16(peek(pc+2))<<8;
	uint cycles=audio_base_cycles[op],index=0;
	if(audio_page_kind[op]==1)index=x;if(audio_page_kind[op]==2)index=y;
	if(audio_page_kind[op]==3){addr=peek(lo)|uint16(peek(byte(lo+1)))<<8;index=y;}
	if(index&&((addr&255)+index>255))cycles++;
	if((op&31)==16||op==0x80){
		bool take=op==0x80?true:op==0x10?!(p&N):op==0x30?(p&N):op==0x50?!(p&V):op==0x70?(p&V):op==0x90?!(p&C):op==0xb0?(p&C):op==0xd0?!(p&Z):(p&Z);
		if(take){uint16 next=pc+2,target=next+int8(lo);if(op!=0x80)cycles++;if((next&0xff00)!=(target&0xff00))cycles++;}
	}
	if((p&D)&&((op&0xe0)==0x60||(op&0xe0)==0xe0)&&((op&3)==1||op==0x72||op==0xf2))cycles++;
	return cycles;
}

bool Runtime::step() {
	if(_observeCallers&&pc==0x9600&&!alt&&!read_aux&&!write_aux){
		uint32 identity=2166136261u;for(uint i=0;i<0x1b1;++i)identity=(identity^mainram[0x9600+i])*16777619u;
		if(identity==1297297889u){++_saveEntries;if(_saveCallerTrace.size()<512)_saveCallerTrace+=Common::String::format("return:%04X,a:%u,x:%u,map:%u;",mainram[0x100+byte(sp+1)]|uint16(mainram[0x100+byte(sp+2)])<<8,a,x,mainram[0x267d]);}
		else{++_otherEntries;_otherFingerprint=identity;_otherReturn=mainram[0x100+byte(sp+1)]|uint16(mainram[0x100+byte(sp+2)])<<8;}
	}
	if (!observeEnhancements()) return false;
	// Continue Game enters this resource-qualified menu operation before the
	// original disk-save availability check. CE owns the authoritative saves.
	// Cancellation resumes the original menu redraw, as its own Escape path does.
	if(pc==0xa2b6&&!alt&&!read_aux&&!write_aux&&
	   interfaceCodeMatches(mainram+0xa2b6,0x34,3833385143u)&&
	   interfaceCodeMatches(mainram+0x9614,0x74,3442984570u)){
		pc=0xa00d;key_latch=0;keys.clear();interfaceRequest=kLoadInterfaceRequest;return true;
	}
	if (atWorldInput()) worldWaiting=true;
	mock_advance(&mockingboard,cpu_cycles);
	if (music && mock_irq(&mockingboard) && !(p&I)) {
		push16(pc);push((p|U)&~B);p=(p|I)&~D;pc=r16(0xfffe);cpu_cycles+=7;
		speaker_advance(&speaker,cpu_cycles);return true;
	}
	origin=pc;cpu_cycles+=instructionCycles();++total_steps;
	bool ok=true;
	if(pc==0xfe89||pc==0xf3f2)pc=pop16()+1;
	else if(pc==0xc311){
		const uint start=r16(0x3c),stop=r16(0x3e),dest=r16(0x42);
		if(stop<start||dest+stop-start+1>65536)ok=false;
		else {memmove(((p&C)?auxram:mainram)+dest,((p&C)?mainram:auxram)+start,stop-start+1);pc=pop16()+1;}
	}else if(pc==0xc710){
		const uint block=r16(0x46),dest=r16(0x44);const byte command=rd(0x42);
		if(block*512+512>disk.size())ok=false;
		else if(command==1){for(uint j=0;j<512;++j)wr(dest+j,disk[block*512+j]);}
		else if(command==2){for(uint j=0;j<512;++j)disk[block*512+j]=rd(dest+j);}
		else ok=false;
		if(ok){a=0;p&=~C;pc=pop16()+1;}
	}else if(!nativeMovement())ok=fallbackStep()!=0;
	mock_advance(&mockingboard,cpu_cycles);speaker_advance(&speaker,cpu_cycles);
	if(!ok)failure=Common::String::format("Temporary Nox bridge stopped at %04X (opcode %02X)",origin,peek(origin));
	return ok;
}
bool Runtime::inputBoundary() const {
    if(atEncounterReference())return true;
    if(campaignSceneInputReady())return true;
    if(_battleValidation)return battleInputReady()||atWorldInput();
	if(_spellValidationObservation)return atKeyboardPoll()&&(pc==0xa9a1||pc==0x9966||pc==0x1324||pc==0x1371||(pc==0xd560&&_spellCastBoundary)||atWorldInput());
	if(_dialogueValidationPolling)return campaignConversationInputReady()||(atKeyboardPoll() && atWorldInput());
	// A shared address in another bank is not a keyboard boundary. Diagnose
	// only the actual LDA of the standard Apple II keyboard latch.
	return atKeyboardPoll() && (pc==0xa06b || pc==0x1324 || pc==0x1371 ||
		(pc==0xd560 && !inWorldScene()) || atWorldInput());
}
bool Runtime::advance(uint32 clocks, bool stopAtBoundary, bool worldBoundaryOnly) {
	const uint64 end=cpu_cycles+clocks;
	while(cpu_cycles<end) {
		if(interfaceRequest!=kNoInterfaceRequest)return true;
        // This private source observer captures a semantic operation input,
        // including a possible held key. It is not a quiescent player save.
        if(stopAtBoundary && atFormationReference())return true;
		if(stopAtBoundary && (worldBoundaryOnly?atWorldInput():inputBoundary()) && keys.empty() && !key_latch){if(atWorldInput())worldWaiting=true;return observeEnhancements();}
		if(!step())return false;
	}
	return true;
}
void Runtime::sample(int16 value, void *context) {
	Runtime *self=static_cast<Runtime *>(context);
	int mixed=value+4*mock_sample(&self->mockingboard,(self->speaker.samples+1)*NOX_CPU_HZ/NOX_AUDIO_HZ);
	if(self->samples.size()<48000)self->samples.push_back(CLIP(mixed,-32767,32767));
}
} // namespace Nox
