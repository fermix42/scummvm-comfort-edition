/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nox/runtime.h"
#include <float.h>

namespace Nox {
static bool finiteState(double value) { return value >= -DBL_MAX && value <= DBL_MAX; }
static void sync64(Common::Serializer &s, uint64 &value) {
	uint32 lo=uint32(value),hi=uint32(value>>32);
	s.syncAsUint32LE(lo);s.syncAsUint32LE(hi);
	if(s.isLoading())value=uint64(lo)|(uint64(hi)<<32);
}
bool Runtime::syncState(Common::Serializer &s) {
	if (!s.syncVersion(2) || s.getVersion()<1) {failure="Unsupported development snapshot version";return false;}
	s.syncBytes(mainram,sizeof(mainram));s.syncBytes(auxram,sizeof(auxram));
	s.syncBytes(&lc[0][0][0],sizeof(lc));s.syncBytes(&high[0][0],sizeof(high));
	s.syncAsByte(a);s.syncAsByte(x);s.syncAsByte(y);s.syncAsByte(p);s.syncAsByte(sp);s.syncAsByte(key_latch);
	s.syncAsUint16LE(pc);s.syncAsUint16LE(origin);
	s.syncAsByte(alt);s.syncAsByte(bank);s.syncAsByte(ram);s.syncAsByte(read_aux);s.syncAsByte(write_aux);
	s.syncAsByte(video_text);s.syncAsByte(video_hires);s.syncAsByte(video_mixed);s.syncAsByte(video_page2);
	s.syncAsByte(video_80col);s.syncAsByte(video_80store);s.syncAsByte(video_dhgr);
	byte musicFlag=music,waitingFlag=worldWaiting;
	s.syncAsByte(musicFlag);s.syncAsByte(waitingFlag);
	music=musicFlag!=0;worldWaiting=waitingFlag!=0;
	sync64(s,cpu_cycles);sync64(s,total_steps);sync64(s,nativeCalls);
	uint32 diskSize=disk.size();s.syncAsUint32LE(diskSize);
	if(diskSize!=disk.size()){failure="Snapshot resource image size differs";return false;}
	s.syncBytes(disk.data(),disk.size());
	// Encode synthesis state field-by-field. No padding, pointer, callback,
	// audio-device handle or compiler-dependent object representation is saved.
	sync64(s,speaker.cycle);sync64(s,speaker.samples);sync64(s,speaker.toggles);
	s.syncAsUint32LE(speaker.phase);s.syncAsSint32LE(speaker.state);
	s.syncAsDoubleLE(speaker.area);s.syncAsDoubleLE(speaker.last_input);s.syncAsDoubleLE(speaker.last_output);
	sync64(s,mockingboard.cycle);s.syncAsDoubleLE(mockingboard.last_input);s.syncAsDoubleLE(mockingboard.last_output);
	for(uint i=0;i<2;++i){
		NoxAY &ay=mockingboard.ay[i];NoxVIA &via=mockingboard.via[i];
		s.syncBytes(ay.reg,16);s.syncAsByte(ay.selected);s.syncBytes(ay.tone,3);
		s.syncAsByte(ay.noise);s.syncAsByte(ay.attack);s.syncAsByte(ay.holding);s.syncAsByte(ay.env_step);
		for(uint n=0;n<3;++n)s.syncAsUint32LE(ay.tone_count[n]);
		s.syncAsUint32LE(ay.noise_count);s.syncAsUint32LE(ay.env_count);s.syncAsUint32LE(ay.lfsr);
		sync64(s,ay.tick);sync64(s,ay.writes);
		s.syncBytes(via.reg,16);s.syncAsByte(via.ifr);s.syncAsByte(via.ier);s.syncAsByte(via.running);
		s.syncAsUint16LE(via.latch);s.syncAsUint32LE(via.remaining);sync64(s,via.expirations);
		if(ay.selected>15||ay.env_step>31||via.running>1){failure="Invalid snapshot audio state";return false;}
	}
	if(s.err()||bank<1||bank>2||alt>1||read_aux>1||write_aux>1||ram>1||musicFlag>1||waitingFlag>1||
	   video_text>1||video_hires>1||video_mixed>1||video_page2>1||video_80col>1||video_80store>1||video_dhgr>1||
	   speaker.phase>=NOX_CPU_HZ||(speaker.state!=1&&speaker.state!=-1)||!finiteState(speaker.area)||
	   !finiteState(speaker.last_input)||!finiteState(speaker.last_output)||!finiteState(mockingboard.last_input)||!finiteState(mockingboard.last_output)){
		failure="Invalid or truncated Nox snapshot";return false;
	}
	return true;
}
// Local validation evidence only: raw payload after actual defeat or a
// completed refusal at an empty-key stable original player-command boundary.
// This is never a player-save envelope or a supported restoration boundary.
// Normal saveState/canSnapshot retain their original battle restrictions.
bool Runtime::campaignDiagnosticSnapshot(Common::WriteStream &out) {
    if(!_battleValidation||!inBattle())return false;
    if(campaignPartyHealth()!=0&&(!battlePlayerTurn()||!keys.empty()||key_latch||(pc!=0xd53f&&pc!=0xfde9)||!battleInputReady()))return false;
    Common::Serializer serializer(nullptr,&out);
    return syncState(serializer)&&!out.err();
}
// Private original-engine oracle data, never a restorable player save. Export
// named records only; no CPU registers, instructions, ROM or disk payload.
bool Runtime::campaignBattleReference(Common::WriteStream &out) const {
    if (!_battleValidation || !inBattle() || !battlePlayerTurn() ||
        !keys.empty() || key_latch || (pc != 0xd53f && pc != 0xfde9)) return false;
    out.write("NBREF001", 8);
    out.write(auxram + 0x1600, 0x800); // Character, enemy and companion records.
    out.write(mainram + 0xbd00, 256); // Compact actor records.
    out.write(mainram + 0xbf00, 256); // Extended actor records.
    out.write(mainram + 0xa31, 187); // Actor index by viewport cell.
    out.write(mainram + 0xaec, 187); // Actor kind by viewport cell.
    out.write(mainram + 0x7043, 20); // Readied spell IDs and INT requirements.
    const uint16 fields[] = {0x70a3,0x2efb,0x61f,0x606,0x60d4,0x64e,
        0x609c,0x33f9,0x625,0x60b3,0x60b4,0x6306,0x6307,0x6308,
        0x6309,0x630a,0x6300,0x27f8,0x27f9,0x620};
    for (uint i = 0; i < ARRAYSIZE(fields); ++i) out.writeByte(mainram[fields[i]]);
    return !out.err();
}
bool Runtime::saveState(Common::WriteStream &stream) {
	if(!canSnapshot()){failure="Save requires an idle world command or final ending boundary";return false;}
	Common::Serializer serializer(nullptr,&stream);
	return syncState(serializer) && _enhancements.write(stream);
}
bool Runtime::loadState(Common::SeekableReadStream &stream) {
	// Caller must decode into a fresh candidate Runtime and only swap it into
	// the engine on success, preserving live state if any validation fails.
	Common::Serializer serializer(&stream,nullptr);
	if(!syncState(serializer))return false;
	if(serializer.getVersion()==1)_enhancements.reset(); // Explicit legacy migration: no recorded enhancements.
	else if(!_enhancements.read(stream)){failure="Invalid Nox enhancement state";return false;}
	if(stream.eos()||stream.pos()!=stream.size()||!canSnapshot()){
		failure="Incomplete or invalid quiescent Nox snapshot";return false;
	}
	samples.clear();speaker.sink=sample;speaker.context=this;
	_activeMapResource=currentMapResource();
	return true;
}
// Controlled clone contracts: never mutate the live world or source HDV.
bool Runtime::validateOwnerContracts(HdvArchive &resources, Common::String &report) {
 Common::MemoryWriteStreamDynamic baseline(DisposeAfterUse::YES);
 if(!saveState(baseline))return false;
 Common::String difference;
 auto equal=[&](Runtime &left,Runtime &right){
  Common::MemoryWriteStreamDynamic a(DisposeAfterUse::YES),b(DisposeAfterUse::YES);
  if(!left.saveState(a)||!right.saveState(b)){difference="Cannot serialize compared boundary";return false;}
  if(a.size()!=b.size()){difference="Encoded sizes differ";return false;}
  for(uint i=0;i<a.size();++i)if(a.getData()[i]!=b.getData()[i]){difference=Common::String::format("first codec difference at %u: %u/%u",i,a.getData()[i],b.getData()[i]);return false;}
  return true;
 };
 auto clone=[&](){Runtime *v=new Runtime(*this);v->speaker.sink=sample;v->speaker.context=v;v->samples.clear();v->_dialogueValidationPolling=false;return v;};
 auto restore=[&](Runtime &source,Runtime &dest){
  Common::MemoryWriteStreamDynamic bytes(DisposeAfterUse::YES);
  if(!source.saveState(bytes)||!dest.initialize(resources))return false;
  Common::MemoryReadStream input(bytes.getData(),bytes.size());
  dest._captureEnhancements=source._captureEnhancements;
  return dest.loadState(input)&&equal(source,dest);
 };
 // These are opaque storage domains, not fabricated earned quest/party states.
 // Every byte is changed, including unused fields which semantic accessors omit.
 uint owners=0;
 for(uint domain=0;domain<5;++domain){
  Runtime *left=clone(),*right=new Runtime();
  byte *data=domain==0?left->mainram:domain==1?left->auxram:domain==2?&left->lc[0][0][0]:domain==3?&left->high[0][0]:left->disk.data();
  const uint size=domain<2?65536:domain==2?sizeof(lc):domain==3?sizeof(high):left->disk.size();
  for(uint i=0;i<size;++i)data[i]^=byte(1+i%255);
  const bool ok=restore(*left,*right);delete left;delete right;
  if(!ok){failure="Opaque state owner preservation differs";return false;}++owners;
 }
 Runtime *left=clone(),*right=new Runtime();
 bool ok=restore(*left,*right);
 // Reference starts as an in-memory copy, not a second serializer decode.
 // Thus an omitted mutable field can diverge during continuation.
 auto sameAudio=[&](){return left->samples.size()==right->samples.size()&&(left->samples.empty()||!memcmp(left->samples.data(),right->samples.data(),left->samples.size()*sizeof(int16)));};
 auto invoke=[](Runtime &v,uint16 entry){
  const uint16 ret=v.pc;const byte stack=v.sp;v.push16(ret-1);v.pc=entry;
  for(uint i=0;i<100000;++i){if(!v.step())return false;if(v.pc==ret&&v.sp==stack)return true;}
  return false;
 };
 uint32 signature=2166136261u;for(uint i=0;i<12;++i)signature=(signature^left->mainram[0x1089+i])*16777619u;
 ok=ok&&signature==2909809865u;
 byte rngBefore[5];memcpy(rngBefore,left->mainram+0x6306,5);
 left->mainram[0x630b]=right->mainram[0x630b]=0;
 left->mainram[0x630c]=right->mainram[0x630c]=255;
 for(uint i=0;ok&&i<64;++i)ok=invoke(*left,0x1089)&&invoke(*right,0x1089)&&left->a==right->a&&equal(*left,*right)&&sameAudio();
 ok=ok&&memcmp(rngBefore,left->mainram+0x6306,5)!=0;
 const byte clockPrefix[]={0xf8,0xee,0xf8,0x2b,0xad,0xf8,0x2b,0xc9,2,0xd0,0x41};
 ok=ok&&!memcmp(left->high[0]+0xfbe6-0xe000,clockPrefix,sizeof(clockPrefix));
 // Exercise minute/hour/day rollover and extinguishing light with a pure BCD oracle.
 const uint16 fields[]={0x2bf8,0x3878,0x3879,0x3ffc,0x367f,0x0414};
 byte expected[]={0,0x23,0x59,2,1,2};
 for(uint i=0;i<6;++i)left->mainram[fields[i]]=right->mainram[fields[i]]=expected[i];
 for(uint n=0;ok&&n<124;++n){
  if(++expected[0]==2){expected[0]=0;
   if(!expected[3])expected[4]=0;else if((--expected[3])&128)expected[4]=0;
   uint minute=(expected[2]>>4)*10+(expected[2]&15)+1;
   if(minute==60){minute=0;if(expected[5])--expected[5];uint hour=((expected[1]>>4)*10+(expected[1]&15)+1)%24;expected[1]=byte((hour/10)*16+hour%10);}
   expected[2]=byte((minute/10)*16+minute%10);
  }
  ok=invoke(*left,0xfbe6)&&invoke(*right,0xfbe6)&&equal(*left,*right)&&sameAudio();
  for(uint i=0;i<6;++i)ok=ok&&left->mainram[fields[i]]==expected[i];
 }
 const uint audioCount=left->samples.size();delete left;delete right;
 if(!ok){failure="Original RNG/clock continuation differs from restored state or BCD oracle";return false;}
 left=clone();right=new Runtime();ok=restore(*left,*right);
 // Arbitrary continuation is compared without relaxing saveState's idle guard.
 // A direction may open a tutorial/modal; its pending state is still deterministic.
 auto sameContinuation=[&](){
  Common::MemoryWriteStreamDynamic a(DisposeAfterUse::YES),b(DisposeAfterUse::YES);
  Common::Serializer sa(nullptr,&a),sb(nullptr,&b);
  if(!left->syncState(sa)||!right->syncState(sb)||!left->_enhancements.write(a)||!right->_enhancements.write(b))return false;
  return left->keys.empty()&&right->keys.empty()&&a.size()==b.size()&&!memcmp(a.getData(),b.getData(),a.size())&&sameAudio();
 };
 const byte inputs[]={21,8};
 for(uint n=0;ok&&n<ARRAYSIZE(inputs);++n){
  left->key(inputs[n]);right->key(inputs[n]);
  for(uint i=0;ok&&i<32;++i)ok=left->advance(20000)&&right->advance(20000)&&sameContinuation();
 }
 const uint movementAudio=left->samples.size();
 const Common::String detail=Common::String::format("left_pc=%04x right_pc=%04x left_idle=%u right_idle=%u audio=%u/%u routine_audio=%u %s",left->pc,right->pc,left->canSnapshot()?1:0,right->canSnapshot()?1:0,left->samples.size(),right->samples.size(),audioCount,difference.c_str());
 delete left;delete right;
 Common::MemoryWriteStreamDynamic unchanged(DisposeAfterUse::YES);
 ok=ok&&saveState(unchanged)&&baseline.size()==unchanged.size()&&!memcmp(baseline.getData(),unchanged.getData(),baseline.size());
 if(!ok||!audioCount||!movementAudio){failure="Input/audio continuation differs or live fixture was modified: "+detail;return false;}
 report=Common::String::format("PASS\topaque_domains=%u\trng_calls=64\tclock_calls=124\tinput_replays=2\troutine_pcm=%u\tinput_pcm=%u\tlive_unchanged=1\tprovenance=controlled_clones\n",owners,audioCount,movementAudio);
 return true;
}
} // namespace Nox
