/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio_fm.h"
namespace Snatcher {
namespace {
bool fail(std::string& e,const char *message){e=message;return false;}
uint32_t word32(const uint8_t *p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
uint16_t word16(const uint8_t *p){return uint16_t(uint16_t(p[0])<<8|p[1]);}
bool extent(size_t offset,size_t length,size_t size){return offset<=size&&length<=size-offset;}
}
bool parseFmInstruments(const uint8_t *data,size_t size,std::vector<FmInstrument>& out,std::string& e){
 e.clear();if(!data||size<7680)return fail(e,"Truncated FM instrument directory");std::vector<FmInstrument> result;
 for(unsigned i=0;i<256;++i){size_t offset=word32(data+i*4);if(offset!=1024+i*26||!extent(offset,26,size))return fail(e,"Unsupported FM instrument record layout");const uint8_t *p=data+offset;if(p[25])return fail(e,"Unclassified FM instrument alignment byte");FmInstrument inst={};for(unsigned j=0;j<25;++j)inst.source[j]=p[j];inst.algorithm=p[0]&7;inst.feedback=(p[0]>>3)&7;
  for(unsigned op=0;op<4;++op){const uint8_t *q=p+1+6*op;FmOperator& o=inst.operators[op];o.detune=(q[0]>>4)&7;o.multiple=q[0]&15;o.totalLevel=q[1]&127;o.rateScale=q[2]>>6;o.attackRate=q[2]&31;o.amplitudeModulation=q[3]>>7;o.decayRate=q[3]&31;o.sustainRate=q[4]&31;o.sustainLevel=q[5]>>4;o.releaseRate=q[5]&15;}result.push_back(inst);
 }out.swap(result);return true;
}
bool readFmCommandAsset(const uint8_t *main,size_t mainSize,const uint8_t *fm,size_t fmSize,uint16_t value,FmCommandAsset& out,std::string& e){
 e.clear();unsigned command=value&255;size_t record=0x6142+command*8;if(!main||!fm||!command||command>=0xf1||!extent(record,8,mainSize))return fail(e,"FM command has no owned source route");const uint8_t *p=main+record;
 if(p[0]>1||p[1]>9||p[2]>9||unsigned(p[1])+1+p[2]>10||(!p[0]&&!p[1]&&!p[2]&&!p[3]&&!word32(p+4)))return fail(e,"FM command route is unclassified or empty");
 FmCommandAsset asset;asset.command=uint8_t(command);asset.music=(p[0]&15)!=0;asset.channelCount=uint8_t(p[1]+1);asset.firstChannel=p[2];asset.priority=p[3];asset.sequenceIndex=word32(p+4);size_t base=asset.music?0x2846:0x1e00,regionEnd=asset.music?fmSize:0x2846;
 if(regionEnd>fmSize||!extent(base,4,regionEnd))return fail(e,"Truncated FM sequence directory");size_t first=word32(fm+base);if(first%4||first<4||!extent(base,first,regionEnd)||asset.sequenceIndex>=first/4)return fail(e,"FM sequence index is outside source directory");size_t relative=word32(fm+base+asset.sequenceIndex*4),length=0;
 if(relative<first||!extent(base,relative,regionEnd))return fail(e,"FM sequence pointer is outside owned resource");
 if(asset.music){if(command<63)return fail(e,"FM music route has no source length owner");size_t at=0x5586+(command-63)*2;if(!extent(at,2,mainSize))return fail(e,"Truncated FM music length table");length=word16(main+at);}
 else {size_t end=asset.sequenceIndex+1<first/4?word32(fm+base+(asset.sequenceIndex+1)*4):regionEnd-base;if(end<relative)return fail(e,"FM effect sequence extents overlap backwards");length=end-relative;}
 size_t start=base+relative;if(!length||!extent(start,length,regionEnd)||length<size_t(asset.channelCount)*4)return fail(e,"Invalid source FM sequence extent");asset.sourceOffset=start;asset.bytes.assign(fm+start,fm+start+length);
 for(unsigned ch=0;ch<asset.channelCount;++ch){uint32_t offset=word32(asset.bytes.data()+ch*4);if(offset<size_t(asset.channelCount)*4||offset>=asset.bytes.size())return fail(e,"FM channel sequence is outside selected asset");asset.channels.push_back(offset);}out=asset;return true;
}

FmSequenceReader::FmSequenceReader():_cursor(0),_return(0),_phrase(0),_phraseReturn(0),_duration(0),_gate(0),_level(127),_phraseState(0),_percussion(false),_ended(false),_haveReturn(false),_havePhrase(false){for(unsigned i=0;i<2;++i){_loop[i]=0;_repeat[i]=0;_loopPitch[i]=_loopVolume[i]=0;_haveLoop[i]=false;}}
bool FmSequenceReader::start(const std::vector<uint8_t>& bytes,size_t offset,std::string& e,bool percussion){e.clear();if(bytes.empty()||offset>=bytes.size())return fail(e,"Invalid FM channel start");*this=FmSequenceReader();_bytes=std::make_shared<const std::vector<uint8_t>>(bytes);_cursor=offset;_percussion=percussion;_phrase=offset;_havePhrase=true;for(unsigned i=0;i<2;++i){_loop[i]=offset;_haveLoop[i]=true;}return true;}
bool FmSequenceReader::next(FmSequenceEvent& out,std::string& e){
 e.clear();if(!_bytes||_bytes->empty()||_ended)return fail(e,"FM channel is not running");FmSequenceReader next=*this;FmSequenceEvent event;bool emitted=false;
 for(size_t budget=0;budget<=_bytes->size();++budget){
  if(next._cursor>=next._bytes->size())return fail(e,"FM sequence ended without a terminal command");event.offset=next._cursor;uint8_t op=(*next._bytes)[next._cursor++];event.opcode=op;
  if(!op)continue;
  if(next._percussion&&op<0x60)continue;
  if(op<0xe0){
   if(next._percussion){if(!extent(next._cursor,3,next._bytes->size()))return fail(e,"Truncated FM percussion note");event.kind=kFmPercussion;event.note=uint8_t(op-0x60);event.duration=(*next._bytes)[next._cursor++];event.gate=(*next._bytes)[next._cursor++];event.level=(*next._bytes)[next._cursor++];next._duration=event.duration;emitted=true;break;}
   uint8_t note=op&127;if(note>=96){event.kind=kFmIgnoredByte;emitted=true;break;}event.kind=kFmNote;event.note=note;event.reusedDuration=(op&128)!=0;
   if(!(op&128)){if(next._cursor>=next._bytes->size())return fail(e,"Missing FM note duration");next._duration=(*next._bytes)[next._cursor++];}
   if(next._cursor>=next._bytes->size())return fail(e,"Missing FM note gate");uint8_t gate=(*next._bytes)[next._cursor++];event.reusedGate=(gate&128)!=0;if(!(gate&128))next._gate=gate;
   if(gate&128)next._level=gate&127;else{if(next._cursor>=next._bytes->size())return fail(e,"Missing FM note level");next._level=(*next._bytes)[next._cursor++]&127;}
   event.duration=next._duration;event.gate=next._gate;event.level=next._level;
   // The note owner consumes one optional zero and its attached F3 slide
   // before programming the current note. FC is a source inline increment.
   if(next._cursor<next._bytes->size()&&!(*next._bytes)[next._cursor])++next._cursor;
   if(next._cursor<next._bytes->size()&&(*next._bytes)[next._cursor]==0xf3){
    ++next._cursor;if(!extent(next._cursor,3,next._bytes->size()))return fail(e,"Truncated FM note-attached slide");event.hasSlide=true;event.slideDelay=(*next._bytes)[next._cursor++];event.slideTicks=(*next._bytes)[next._cursor++];event.slideNote=(*next._bytes)[next._cursor++];
    if(event.slideTicks){if(!extent(next._cursor,3,next._bytes->size())||(*next._bytes)[next._cursor]!=0xfc)return fail(e,"FM attached slide has no owned FC increment");++next._cursor;uint16_t increment=(*next._bytes)[next._cursor++];increment|=uint16_t((*next._bytes)[next._cursor++])<<8;event.slideIncrement=int16_t(increment);}
   }
   emitted=true;break;
  }
  unsigned need=0;switch(op){case 0xe0:case 0xe2:case 0xe3:case 0xea:case 0xec:case 0xed:case 0xee:case 0xf0:case 0xf2:case 0xf9:case 0xfb:need=1;break;case 0xe1:case 0xeb:case 0xef:case 0xf4:case 0xf5:case 0xfc:need=2;break;case 0xe4:case 0xe5:case 0xe7:case 0xe9:case 0xf1:need=3;break;case 0xfd:case 0xfe:need=4;break;default:break;}
  if(!extent(next._cursor,need,next._bytes->size()))return fail(e,"Truncated FM semantic operand");event.operandCount=uint8_t(need);for(unsigned i=0;i<need;++i)event.operands[i]=(*next._bytes)[next._cursor+i];if(need)event.value=event.operands[0];
  if(op==0xe0){event.kind=kFmRest;next._duration=(*next._bytes)[next._cursor++];event.duration=next._duration;next._gate=next._level=0;emitted=true;break;}
  if(op==0xe1){event.kind=kFmHold;next._duration=event.duration=(*next._bytes)[next._cursor++];next._gate=event.gate=(*next._bytes)[next._cursor++];emitted=true;break;}
  if(op==0xe6||op==0xe8){unsigned slot=op==0xe6?0:1;next._loop[slot]=next._cursor;if(slot==1){next._repeat[slot]=0;next._loopPitch[slot]=next._loopVolume[slot]=0;}event.kind=kFmLoop;emitted=true;break;}
  if(op==0xe7||op==0xe9){unsigned slot=op==0xe7?0:1;uint8_t count=event.value;next._cursor+=3;
   if(count&&++next._repeat[slot]==count){if(slot==0){next._repeat[slot]=0;next._loopPitch[slot]=next._loopVolume[slot]=0;}}
   else{next._loopVolume[slot]=int16_t(uint16_t(next._loopVolume[slot]+int8_t(event.operands[1])));next._loopPitch[slot]=int16_t(uint16_t(next._loopPitch[slot]+int16_t(int8_t(event.operands[2]))*8));next._cursor=next._loop[slot];}
   event.kind=kFmLoop;emitted=true;break;
  }
  if(op==0xf6){next._phrase=next._cursor;next._havePhrase=true;next._phraseState=0;event.kind=kFmLoop;emitted=true;break;}
  if(op==0xf7){if(!next._havePhrase){next._phrase=next._loop[0];next._havePhrase=true;}if(next._phraseState==0)next._phraseState=1;else if(next._phraseState==1){next._phraseState=2;next._phraseReturn=next._cursor;next._cursor=next._phrase;}else{next._phraseState=1;next._cursor=next._phraseReturn;}event.kind=kFmLoop;emitted=true;break;}
  if(op==0xfd||op==0xfe){uint32_t encoded=word32(next._bytes->data()+next._cursor);int64_t relative=encoded>=0x80000000u?int64_t(encoded)-0x100000000LL:int64_t(encoded);int64_t target=int64_t(next._cursor)+relative;if(target<0||uint64_t(target)>=next._bytes->size())return fail(e,"FM relative branch is outside owned asset");if(op==0xfe){next._return=next._cursor+4;next._haveReturn=true;}next._cursor=size_t(target);event.kind=op==0xfd?kFmJump:kFmCall;emitted=true;break;}
  if(op==0xff){if(next._haveReturn){next._haveReturn=false;next._cursor=next._return;event.kind=kFmReturn;}else{next._ended=true;event.kind=kFmSequenceEnd;}emitted=true;break;}
  switch(op){case 0xe2:event.kind=kFmInstrument;break;case 0xe3:event.kind=kFmPan;break;case 0xe4:event.kind=kFmVibrato;break;case 0xe5:event.kind=kFmRandomPitch;break;case 0xea:event.kind=kFmTempo;break;case 0xeb:event.kind=kFmPanCycle;break;case 0xec:event.kind=kFmTranspose;break;case 0xed:event.kind=kFmGlobalLfo;break;case 0xee:event.kind=kFmVolume;break;case 0xef:event.kind=kFmVolumeRamp;break;case 0xf0:case 0xf1:event.kind=kFmPortamento;break;case 0xf2:event.kind=kFmPitchOffset;break;case 0xf4:event.kind=kFmPitchSensitivity;break;case 0xf5:event.kind=kFmAmplitudeModulation;break;case 0xf9:event.kind=kFmVibratoRamp;break;case 0xfb:event.kind=kFmRelease;break;case 0xfc:event.kind=kFmSlideIncrement;break;case 0xf3:case 0xf8:case 0xfa:event.kind=kFmIgnoredByte;break;default:return fail(e,"FM control family has no source semantic owner");}
  next._cursor+=need;emitted=true;break;
 }
 if(!emitted)return fail(e,"FM control-only cycle exceeds owned byte budget");*this=next;out=event;return true;
}
}
