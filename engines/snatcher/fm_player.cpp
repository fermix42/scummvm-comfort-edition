/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fm_player.h"
#include <algorithm>
#include <limits>
#include <sstream>
namespace Snatcher {
namespace {
bool fail(std::string& error,const char* text){error=text;return false;}
uint16_t word(const uint8_t* p){return uint16_t(unsigned(p[0])<<8|p[1]);}
int16_t signedWord(uint16_t value){return value<32768?int16_t(value):int16_t(int(value)-65536);}
uint16_t plus(uint16_t a,int32_t b){return uint16_t(uint32_t(a)+uint32_t(b));}
uint8_t gateTicks(uint8_t duration,uint8_t gate){if(gate==0||gate==127)return duration;unsigned ticks=(unsigned(duration)*unsigned(gate)*2)>>8;return uint8_t(ticks?ticks:1);}
struct SynthCommand {unsigned kind,channel;uint16_t value;uint8_t levels[4];FmInstrument instrument;SynthCommand(unsigned k=0,unsigned c=0,uint16_t v=0):kind(k),channel(c),value(v),instrument(){for(unsigned i=0;i<4;++i)levels[i]=0;}};
class RecordingSink:public FmSynthSink {
public:
 std::vector<SynthCommand> commands;
 bool reset(std::string&)override{commands.push_back(SynthCommand(0));return true;}
 bool instrument(unsigned ch,const FmInstrument& i,std::string&)override{SynthCommand c(1,ch);c.instrument=i;commands.push_back(c);return true;}
 bool frequency(unsigned ch,uint16_t v,std::string&)override{commands.push_back(SynthCommand(2,ch,v));return true;}
 bool levels(unsigned ch,const uint8_t v[4],std::string& e)override{return carrierLevels(ch,v,15,e);}
 bool carrierLevels(unsigned ch,const uint8_t v[4],uint8_t mask,std::string&)override{SynthCommand c(3,ch,mask);for(unsigned i=0;i<4;++i)c.levels[i]=v[i];commands.push_back(c);return true;}
 bool fastRelease(unsigned ch,std::string&)override{commands.push_back(SynthCommand(7,ch));return true;}
 bool key(unsigned ch,bool on,std::string&)override{commands.push_back(SynthCommand(4,ch,on?1:0));return true;}
 bool pan(unsigned ch,uint8_t v,std::string&)override{commands.push_back(SynthCommand(5,ch,v));return true;}
 bool lfo(uint8_t v,std::string&)override{commands.push_back(SynthCommand(6,0,v));return true;}
 bool renderPcm(uint32_t,std::vector<int16_t>&,std::string& e)override{return fail(e,"recording sink cannot produce audio");}
 uint32_t sampleRate()const override{return 44100;}
 bool replay(FmSynthSink& sink,std::string& e)const{for(const SynthCommand& c:commands){bool ok=false;switch(c.kind){case 0:ok=sink.reset(e);break;case 1:ok=sink.instrument(c.channel,c.instrument,e);break;case 2:ok=sink.frequency(c.channel,c.value,e);break;case 3:ok=sink.carrierLevels(c.channel,c.levels,uint8_t(c.value),e);break;case 4:ok=sink.key(c.channel,c.value!=0,e);break;case 5:ok=sink.pan(c.channel,uint8_t(c.value),e);break;case 6:ok=sink.lfo(uint8_t(c.value),e);break;case 7:ok=sink.fastRelease(c.channel,e);break;}if(!ok)return false;}return true;}
};
}
bool readFmPerformanceData(const uint8_t* main,size_t size,FmPerformanceData& out,std::string& e){
 e.clear();if(!main||size<0x61ca)return fail(e,"Main IP does not contain owned FM performance tables");FmPerformanceData staged;
 for(unsigned i=0;i<256;++i)staged.frequency[i]=word(main+0x5fca+i*2);
 for(unsigned i=0;i<256;++i)staged.frequencyStep[i]=main[0x60ca+i];
 for(unsigned i=0;i<64;++i){staged.vibrato[i]=main[0x3dc0+i];staged.panWave[i]=main[0x4332+i];}
 for(unsigned i=0;i<4;++i)staged.pan[i]=main[0x4952+i];for(unsigned i=0;i<513;++i)staged.noise[i]=main[0x3e00+i];out=staged;return true;
}
bool fmPitchFrequency(const FmPerformanceData& data,uint16_t pitch,uint16_t& frequency,std::string& e){
 e.clear();unsigned note=pitch>>8;uint16_t value=uint16_t(data.frequency[note]+((unsigned(data.frequencyStep[note])*(pitch&255))>>8));frequency=uint16_t(value&0x3fff);return true;
}
uint8_t fmChannelLevel(uint8_t note,uint16_t release,uint16_t volume,int16_t loopVolume,uint8_t musicGain,uint8_t effectGain,bool effect){
 uint16_t value=uint16_t(int(note)-int(release>>8)-(127-int(volume>>8))+int(loopVolume));unsigned level=value>=0x8000?0:value>=128?127:(value>>1)+64;
 if(effectGain!=255)level=(level*(effectGain>>1))>>7;if(!effect&&musicGain!=255)level=(level*(musicGain>>1))>>7;return uint8_t(level);
}
FmTrackState::FmTrackState():pendingReset(false),active(false),percussion(false),effect(false),dirtyLevel(false),shortGate(false),slideLegato(false),command(0),priority(0),instrument(255),tempo(255),tempoFraction(1),remaining(1),duration(0),gate(0),gateRemaining(0),previousGate(0),noteLevel(127),transpose(0),pan(0),panPeriod(0),panCountdown(0),panWave(0),panIndex(0),pmsDelay(0),pmsCountdown(0),pms(0),amsDelay(0),amsCountdown(0),ams(0),vibratoDelay(0),vibratoWait(0),vibratoRate(0),vibratoPhase(0),vibratoDepth(0),vibratoRamp(0),vibratoRampLeft(0),noiseRate(0),noiseFraction(0),noiseMask(0),noiseIndex(0),vibratoDepth16(0),vibratoStep(0),pitch(0),frequencyPitch(0),slideTarget(0),slideOffset(0),slideIncrement(0),slideTicks(0),slideTicksReset(0),slideDelay(0),slideDelayReset(0),finePitch(0),volume(0x7f00),release(0),releaseStep(0),volumeStep(0),volumeRamp(0),volumeTarget(0),cachedAlgorithm(0){for(unsigned i=0;i<4;++i)cachedLevels[i]=0;}
FmPlayer::FmPlayer():_loaded(false),_muted(false),_startupHold(false),_forceMono(false),_sourceResetPublication(true),_musicGain(255),_effectGain(255),_warmup(0),_updates(0),_primaryCompletionSerial(0),_secondaryCompletionSerial(0),_completionPublicationGeneration(0),_completionPublication(0),_data(){}
bool FmPlayer::load(const uint8_t* main,size_t mainSize,const uint8_t* fm,size_t fmSize,std::string& e){
 if(active())return fail(e,"FM source replacement requires stopped native voices");FmPerformanceData performance;std::vector<FmInstrument> instruments;if(!readFmPerformanceData(main,mainSize,performance,e)||!parseFmInstruments(fm,fmSize,instruments,e))return false;
 auto assets=std::make_shared<Assets>();assets->main.assign(main,main+mainSize);assets->fm.assign(fm,fm+fmSize);assets->instruments.swap(instruments);FmPlayer staged;staged._assets=assets;staged._data=performance;staged._loaded=true;*this=staged;return true;
}
unsigned FmPlayer::physical(unsigned index)const{return index>=8?index-4:index;}
bool FmPlayer::visible(unsigned index)const{return index<6?!(index>=4&&_tracks[index+4].active):index>=8;}
bool FmPlayer::key(unsigned index,bool on,FmSynthSink& synth,std::string& e){return !visible(index)||synth.key(physical(index),on,e);}
bool FmPlayer::pan(unsigned index,FmSynthSink& synth,std::string& e){if(_forceMono)_tracks[index].pan|=0xc0;return !visible(index)||synth.pan(physical(index),_tracks[index].pan,e);}
bool FmPlayer::resetVoice(unsigned index,FmSynthSink& synth,std::string& e){
 // Main pending-reset paths (including4E58) call5A16 before57CC.
 //5A16 retains stereo bits and clears AMS/FMS, publishing through5A9C.
 if(!_sourceResetPublication)return !visible(index)||synth.fastRelease(physical(index),e);
 _tracks[index].pan&=0xc0;
 return pan(index,synth,e)&&(!visible(index)||synth.fastRelease(physical(index),e));
}
bool FmPlayer::muteMusic(FmSynthSink& synth,std::string& e){
 // Main578A visits active music descriptors5..0 and calls5E5E with level0.
 // It does not key off voices, and5E5E preserves an owning effect overlay.
 for(int index=5;index>=0;--index){const auto& t=_tracks[index];if(!t.active||!visible(index))continue;uint8_t values[4];for(unsigned op=0;op<4;++op){uint8_t value=uint8_t(unsigned(t.cachedLevels[op])+127);values[op]=value<128?value:127;}if(!synth.carrierLevels(unsigned(index),values,fmCarrierMask(t.cachedAlgorithm),e))return false;}
 return true;
}
bool FmPlayer::pitch(unsigned index,int32_t modulation,FmSynthSink& synth,std::string& e){
 if(!visible(index))return true;_tracks[index].frequencyPitch=plus(_tracks[index].pitch,modulation);uint16_t frequency;if(!fmPitchFrequency(_data,plus(_tracks[index].pitch,modulation),frequency,e)){std::ostringstream context;context<<e<<" (base "<<_tracks[index].pitch<<", modulation "<<modulation<<")";e=context.str();return false;}return synth.frequency(physical(index),frequency,e);
}
bool FmPlayer::level(unsigned index,FmSynthSink& synth,std::string& e){
 FmTrackState& t=_tracks[index];t.dirtyLevel=false;if(!visible(index)||t.instrument==255)return true;
 const FmInstrument& inst=_assets->instruments[t.instrument];uint8_t values[4];unsigned mask=fmCarrierMask(inst.algorithm);unsigned dynamic=fmChannelLevel(t.noteLevel,t.release,t.volume,t.reader.loopVolume(),_musicGain,_effectGain,t.effect);
 for(unsigned op=0;op<4;++op){unsigned value=inst.operators[op].totalLevel;if(mask&(1u<<op))value=std::min(127u,value+127-dynamic);values[op]=uint8_t(value);}return synth.carrierLevels(physical(index),values,uint8_t(mask),e);
}
void FmPlayer::setGains(uint8_t music,uint8_t effects){if(music!=_musicGain||effects!=_effectGain)for(FmTrackState& t:_tracks)t.dirtyLevel=true;_musicGain=music;_effectGain=effects;}
bool FmPlayer::active()const{for(const FmTrackState& t:_tracks)if(t.active)return true;return false;}
bool FmPlayer::musicActive()const{for(unsigned i=0;i<8;++i)if(_tracks[i].active)return true;return false;}

FmReplayOrigin FmPlayer::replayOrigin()const{
 FmReplayOrigin o;o.updates=_updates;o.primary=_primaryCompletionSerial;o.secondary=_secondaryCompletionSerial;o.publicationGeneration=_completionPublicationGeneration;o.publication=_completionPublication;o.musicGain=_musicGain;o.effectGain=_effectGain;o.warmup=_warmup;o.loaded=_loaded;o.muted=_muted;o.startupHold=_startupHold;o.forceMono=_forceMono;
 for(unsigned i=0;i<10;++i)if(_tracks[i].dirtyLevel)o.dirtyLevelMask|=uint16_t(1u<<i);
 o.valid=!active()&&!_warmup&&!_muted&&!_startupHold;if(_sourceResetPublication)for(const auto& t:_tracks){if(t.instrument!=255||t.cachedAlgorithm)o.valid=false;for(uint8_t value:t.cachedLevels)if(value)o.valid=false;}return o;
}
bool FmPlayer::restoreReplayOrigin(const FmReplayOrigin& o,std::string& e){
 e.clear();if(!o.valid||o.version!=1||active()||o.loaded!=_loaded||o.warmup||o.muted||o.startupHold||o.dirtyLevelMask>1023||o.primary>o.publicationGeneration||o.secondary>o.publicationGeneration||(o.publication&48)||(o.publicationGeneration==0?o.publication!=0:((o.publication&192)==0||(o.publication&15)!=(o.publicationGeneration&15))))return fail(e,"Invalid native FM semantic history origin");
 for(unsigned i=0;i<10;++i){_tracks[i]=FmTrackState();_tracks[i].dirtyLevel=(o.dirtyLevelMask&(1u<<i))!=0;}
 _updates=o.updates;_primaryCompletionSerial=o.primary;_secondaryCompletionSerial=o.secondary;_completionPublicationGeneration=o.publicationGeneration;_completionPublication=o.publication;_musicGain=o.musicGain;_effectGain=o.effectGain;_warmup=o.warmup;_muted=o.muted;_startupHold=o.startupHold;_forceMono=o.forceMono;return true;
}
uint16_t FmPlayer::activeVoiceMask()const{uint16_t mask=0;for(unsigned i=0;i<10;++i)if(_tracks[i].active)mask|=uint16_t(1u<<i);return mask;}
uint64_t FmPlayer::semanticDigest()const{
 uint64_t hash=UINT64_C(14695981039346656037);auto field=[&](uint64_t value){for(unsigned n=8;n;--n)hash=(hash^uint8_t(value>>((n-1)*8)))*UINT64_C(1099511628211);};
 field(uint64_t(_loaded));
 field(uint64_t(_muted));
 field(uint64_t(_startupHold));
 field(uint64_t(_forceMono));
 field(uint64_t(_musicGain));
 field(uint64_t(_effectGain));
 field(uint64_t(_warmup));
 field(uint64_t(_updates));
 field(uint64_t(_primaryCompletionSerial));
 field(uint64_t(_secondaryCompletionSerial));
 field(uint64_t(_completionPublicationGeneration));
 field(uint64_t(_completionPublication));
 for(const FmTrackState& t:_tracks){
  if(_sourceResetPublication){field(t.cachedAlgorithm);for(uint8_t value:t.cachedLevels)field(value);}
  field(uint64_t(t.pendingReset));
  field(uint64_t(t.active));
  field(uint64_t(t.percussion));
  field(uint64_t(t.effect));
  field(uint64_t(t.dirtyLevel));
  field(uint64_t(t.shortGate));
  field(uint64_t(t.slideLegato));
  field(uint64_t(t.command));
  field(uint64_t(t.priority));
  field(uint64_t(t.instrument));
  field(uint64_t(t.tempo));
  field(uint64_t(t.tempoFraction));
  field(uint64_t(t.remaining));
  field(uint64_t(t.duration));
  field(uint64_t(t.gate));
  field(uint64_t(t.gateRemaining));
  field(uint64_t(t.previousGate));
  field(uint64_t(t.noteLevel));
  field(uint64_t(t.transpose));
  field(uint64_t(t.pan));
  field(uint64_t(t.panPeriod));
  field(uint64_t(t.panCountdown));
  field(uint64_t(t.panWave));
  field(uint64_t(t.panIndex));
  field(uint64_t(t.pmsDelay));
  field(uint64_t(t.pmsCountdown));
  field(uint64_t(t.pms));
  field(uint64_t(t.amsDelay));
  field(uint64_t(t.amsCountdown));
  field(uint64_t(t.ams));
  field(uint64_t(t.vibratoDelay));
  field(uint64_t(t.vibratoWait));
  field(uint64_t(t.vibratoRate));
  field(uint64_t(t.vibratoPhase));
  field(uint64_t(t.vibratoDepth));
  field(uint64_t(t.vibratoRamp));
  field(uint64_t(t.vibratoRampLeft));
  field(uint64_t(t.noiseRate));
  field(uint64_t(t.noiseFraction));
  field(uint64_t(t.noiseMask));
  field(uint64_t(t.noiseIndex));
  field(uint64_t(t.vibratoDepth16));
  field(uint64_t(t.vibratoStep));
  field(uint64_t(t.pitch));
  field(uint64_t(t.frequencyPitch));
  field(uint64_t(t.slideTarget));
  field(uint64_t(t.slideOffset));
  field(uint64_t(t.slideIncrement));
  field(uint64_t(t.slideTicks));
  field(uint64_t(t.slideTicksReset));
  field(uint64_t(t.slideDelay));
  field(uint64_t(t.slideDelayReset));
  field(uint64_t(t.finePitch));
  field(uint64_t(t.volume));
  field(uint64_t(t.release));
  field(uint64_t(t.releaseStep));
  field(uint64_t(t.volumeStep));
  field(uint64_t(t.volumeRamp));
  field(uint64_t(t.volumeTarget));
  const FmSequenceReader& r=t.reader;field(r._bytes?r._bytes->size():0);
  field(uint64_t(r._cursor));
  field(uint64_t(r._return));
  field(uint64_t(r._phrase));
  field(uint64_t(r._phraseReturn));
  field(uint64_t(r._duration));
  field(uint64_t(r._gate));
  field(uint64_t(r._level));
  field(uint64_t(r._phraseState));
  field(uint64_t(r._percussion));
  field(uint64_t(r._ended));
  field(uint64_t(r._haveReturn));
  field(uint64_t(r._havePhrase));
  for(unsigned i=0;i<2;++i){field(r._loop[i]);field(r._repeat[i]);field(uint64_t(r._loopPitch[i]));field(uint64_t(r._loopVolume[i]));field(r._haveLoop[i]);}
 }return hash;
}
bool FmPlayer::stop(FmSynthSink& synth,std::string& e){e.clear();for(unsigned i=0;i<6;++i)if(!synth.key(i,false,e))return false;for(FmTrackState& t:_tracks)t=FmTrackState();_warmup=0;_startupHold=false;_muted=false;return true;}
bool FmPlayer::start(uint16_t command,FmSynthSink& synth,std::string& e){bool accepted;return start(command,synth,accepted,e);}
bool FmPlayer::start(uint16_t command,FmSynthSink& synth,bool& accepted,std::string& e){
 accepted=false;e.clear();if(!_loaded)return fail(e,"FM source assets not loaded");uint8_t id=uint8_t(command);
 // Source Main5058/5182 distinguishes control requests from music assets.
 if(!id||(id>=123&&id<241))return true;
 if(id>=241){
  if(id==241){if(!synth.reset(e))return false;for(FmTrackState& t:_tracks)t=FmTrackState();_musicGain=_effectGain=0;_forceMono=false;_warmup=0;_startupHold=false;_muted=false;}
  else if(id==242){if(_sourceResetPublication){if(!muteMusic(synth,e))return false;}else for(unsigned i=0;i<6;++i)if(!synth.key(i,false,e))return false;for(unsigned i=0;i<10;++i){FmTrackState& t=_tracks[i];t.active=false;t.command=t.priority=0;t.pendingReset=i<6;}_warmup=0;_startupHold=false;}
  else if(id==243)_forceMono=true;
  else if(id==244)_forceMono=false;
  // F5..FF are the verified source5304 RTS entries, never unknown no-ops.
  accepted=true;return true;
 }
 FmCommandAsset asset;if(!readFmCommandAsset(_assets->main.data(),_assets->main.size(),_assets->fm.data(),_assets->fm.size(),command,asset,e))return false;
 FmPlayer staged=*this;RecordingSink output;unsigned first=asset.firstChannel;
 if(!asset.music&&asset.command<50){if(_tracks[9].active&&_tracks[9].command==asset.command)first=9;else if(_tracks[8].active&&_tracks[8].command==asset.command)first=8;else if(asset.priority>=_tracks[9].priority)first=9;else if(asset.priority>=_tracks[8].priority)first=8;else return true;}
 if(first+asset.channelCount>10)return fail(e,"FM command exceeds native track owners");
 if(asset.music){if(_sourceResetPublication&&!staged.muteMusic(output,e))return false;for(unsigned i=0;i<10;++i){if(!_sourceResetPublication&&(i<6||i>=8)&&!staged.key(i,false,output,e))return false;FmTrackState previous=staged._tracks[i];staged._tracks[i]=FmTrackState();staged._tracks[i].pitch=previous.pitch;staged._tracks[i].release=previous.release;staged._tracks[i].cachedAlgorithm=previous.cachedAlgorithm;for(unsigned op=0;op<4;++op)staged._tracks[i].cachedLevels[op]=previous.cachedLevels[op];staged._tracks[i].pendingReset=i<6;}staged._warmup=0;staged._startupHold=asset.channelCount>=7;}
 for(unsigned i=0;i<asset.channelCount;++i){unsigned slot=first+i;uint16_t previous=staged._tracks[slot].pitch,release=staged._tracks[slot].release;FmTrackState track;track.pitch=previous;track.release=release;track.cachedAlgorithm=staged._tracks[slot].cachedAlgorithm;for(unsigned op=0;op<4;++op)track.cachedLevels[op]=staged._tracks[slot].cachedLevels[op];track.pendingReset=slot<6||slot>=8;track.active=true;track.percussion=slot==6||slot==7;track.effect=!asset.music;track.command=asset.command;track.priority=asset.priority;
  if(!track.reader.start(asset.bytes,asset.channels[i],e,track.percussion))return false;staged._tracks[slot]=track;if(!_sourceResetPublication&&!track.percussion&&!staged.key(slot,false,output,e))return false;
 }
 if(!output.replay(synth,e))return false;*this=staged;accepted=true;return true;
}
bool FmPlayer::event(unsigned index,const FmSequenceEvent& ev,FmSynthSink& synth,std::vector<FmDrumEvent>& drums,bool& timed,std::string& e){
 FmTrackState& t=_tracks[index];timed=false;const uint8_t* p=ev.operands;
 switch(ev.kind){
 case kFmTempo:t.tempo=ev.value;break;
 case kFmInstrument:
  if(t.percussion)return fail(e,"instrument command has no percussion synthesis owner");if(t.instrument!=ev.value){t.instrument=ev.value;t.cachedAlgorithm=_assets->instruments[t.instrument].algorithm;for(unsigned op=0;op<4;++op)t.cachedLevels[op]=_assets->instruments[t.instrument].source[2+op*6];t.releaseStep=0;if(visible(index)&&!synth.instrument(physical(index),_assets->instruments[t.instrument],e))return false;t.pan=uint8_t((t.pan&0x3f)|0xc0);t.panPeriod=0;if(!pan(index,synth,e))return false;}break;
 case kFmVolume:t.volume=uint16_t(ev.value)<<8;t.volumeRamp=0;t.dirtyLevel=true;break;
 case kFmVolumeRamp:{t.volumeRamp=p[0];t.volumeTarget=p[1];int difference=(int(p[1])<<8)-int(t.volume);t.volumeStep=int16_t(difference/int(p[0]?p[0]:1));break;}
 case kFmTranspose:t.transpose=ev.value;break;
 case kFmPitchOffset:t.finePitch=uint16_t(int16_t(int8_t(ev.value))*4);break;
 case kFmPan:if(ev.value>=4)return fail(e,"pan selector outside original four-entry table");t.pan=uint8_t((t.pan&0x3f)|_data.pan[ev.value]);t.panPeriod=0;if(!pan(index,synth,e))return false;break;
 case kFmPanCycle:if(p[1]>=4)return fail(e,"pan cycle outside original source table");t.panPeriod=t.panCountdown=p[0];t.panWave=p[1];t.panIndex=0;t.pan=uint8_t((t.pan&0x3f)|(p[1]==3?0x40:0x80));if(!pan(index,synth,e))return false;break;
 case kFmVibrato:t.vibratoDelay=p[0];t.vibratoRate=p[1];t.vibratoDepth=p[2];t.vibratoDepth16=uint16_t(p[2])<<8;t.vibratoRamp=0;break;
 case kFmVibratoRamp:if(ev.value)return fail(e,"vibrato depth-ramp source carry ownership is unresolved");t.vibratoRamp=0;break;
 case kFmRandomPitch:t.noiseRate=p[0];t.noiseMask=word(p+1);t.noiseFraction=0;t.noiseIndex=0;break;
 case kFmPortamento:if(ev.opcode==0xf0){t.slideLegato=true;t.slideTicksReset=p[0];t.slideDelayReset=0;}else{t.slideLegato=false;t.slideDelayReset=p[0];t.slideTicksReset=p[1];t.slideOffset=uint16_t(p[2])<<8;}break;
 case kFmSlideIncrement:t.slideIncrement=signedWord(uint16_t(p[0])|(uint16_t(p[1])<<8));break;
 case kFmRelease:t.releaseStep=uint16_t(ev.value)<<5;break;
 case kFmPitchSensitivity:t.pmsDelay=p[0];t.pms=p[1];if(!p[0]){t.pan=uint8_t((t.pan&0xf0)|t.pms);if(!pan(index,synth,e))return false;}break;
 case kFmAmplitudeModulation:t.amsDelay=p[0];t.ams=uint8_t(p[1]<<4);if(!p[0]){t.pan=uint8_t((t.pan&0xcf)|t.ams);if(!pan(index,synth,e))return false;}break;
 case kFmGlobalLfo:if(!synth.lfo(uint8_t(ev.value|8),e))return false;break;
 case kFmRest:t.remaining=t.duration=ev.duration;t.gate=0;t.previousGate=0;t.shortGate=false;t.gateRemaining=0;t.noteLevel=0;if(!t.percussion&&!key(index,false,synth,e))return false;timed=true;break;
 case kFmHold:t.remaining=t.duration=ev.duration;t.gate=t.previousGate=ev.gate;{unsigned ticks=(unsigned(ev.duration)*uint8_t(ev.gate*2))>>8;t.gateRemaining=uint8_t(ticks?ticks:1);}timed=true;break;
 case kFmPercussion:{t.remaining=t.duration=ev.duration;t.previousGate=ev.gate;unsigned level5=((unsigned(ev.level)*(t.volume>>8))>>7)>>2;drums.push_back(FmDrumEvent(uint8_t(index),ev.note,uint8_t(level5)));timed=true;break;}
 case kFmNote:{
  if(t.shortGate){if(!key(index,false,synth,e))return false;t.shortGate=false;}
  uint16_t previousPitch=t.pitch;t.remaining=t.duration=ev.duration;t.gate=ev.gate;t.gateRemaining=gateTicks(ev.duration,ev.gate);t.noteLevel=ev.level;t.shortGate=ev.gate!=0&&ev.gate!=127&&ev.duration==1;
  t.pitch=uint16_t(((uint16_t(uint8_t(ev.note+t.transpose))<<8)+uint16_t(t.reader.loopPitch())+t.finePitch)&0x7fff);
  t.vibratoWait=t.vibratoDelay;t.vibratoPhase=0;t.vibratoRampLeft=t.vibratoRamp;if(t.vibratoRamp)t.vibratoDepth16=0;t.release=0;t.pmsCountdown=t.pmsDelay;t.amsCountdown=t.amsDelay;
  if(ev.hasSlide){t.slideDelay=ev.slideDelay;t.slideTicks=ev.slideTicks;t.slideTarget=uint16_t(uint8_t(ev.slideNote+t.transpose))<<8;t.slideIncrement=ev.slideIncrement;}
  else{t.slideTicks=t.slideTicksReset;t.slideDelay=t.slideDelayReset;if(t.slideTicks){if(t.slideLegato){t.slideTarget=t.pitch;t.pitch=previousPitch;uint16_t target=t.slideTarget;if(target>=0x6000)target=target<0x8000?0x5fff:0;t.slideTarget=target;int delta=int(target)-int(t.pitch);if(!delta)delta=1;t.slideIncrement=int16_t(delta/int(t.slideTicks));}else{t.slideTarget=t.pitch;t.pitch=uint16_t(t.pitch-t.slideOffset);}}}
  if(!pitch(index,0,synth,e))return false;
  if(t.pmsDelay){t.pan&=0xf0;if(!pan(index,synth,e))return false;}if(t.amsDelay){t.pan&=0xcf;if(!pan(index,synth,e))return false;}
  if(t.volumeRamp){if(--t.volumeRamp==0)t.volume=uint16_t(t.volumeTarget)<<8;else t.volume=plus(t.volume,t.volumeStep);}
  t.dirtyLevel=true;if(!level(index,synth,e))return false;if(t.previousGate!=127&&!key(index,true,synth,e))return false;t.previousGate=t.gate;timed=true;break;
 }
 case kFmSequenceEnd:{
  bool wasVisible=visible(index);t.active=false;t.command=t.priority=0;if(!t.percussion&&wasVisible&&!synth.fastRelease(physical(index),e))return false;
  if(index>=8){unsigned base=index-4;FmTrackState& underlying=_tracks[base];underlying.release=0xffff;if(underlying.active&&underlying.instrument!=255){if(!synth.instrument(base,_assets->instruments[underlying.instrument],e)||!pan(base,synth,e)||!level(base,synth,e))return false;}}
  // Main4EB6 also retires an overlay when its underlying channel ends.
  bool droppedOverlay=false;
  if(index==4||index==5){FmTrackState& overlay=_tracks[index+4];if(overlay.active){overlay.active=false;overlay.command=overlay.priority=0;droppedOverlay=true;if(!synth.fastRelease(index,e))return false;}}
  uint8_t publication=0;
  if(index<8&&!musicActive()){if(_primaryCompletionSerial==UINT64_MAX)return fail(e,"FM primary completion serial exhausted");++_primaryCompletionSerial;publication|=128;}
  const bool overlaysEnded=!_tracks[8].active&&!_tracks[9].active;
  if((index>=8||droppedOverlay)&&overlaysEnded){if(_secondaryCompletionSerial==UINT64_MAX)return fail(e,"FM secondary completion serial exhausted");++_secondaryCompletionSerial;}
  // Main4F70/4F88 marks secondary completion even on a music FM-channel
  // terminal with no live overlay. Percussion does not call that helper.
  if((index<6||index>=8)&&overlaysEnded)publication|=64;
  if(publication){if(_completionPublicationGeneration==UINT64_MAX)return fail(e,"FM completion publication exhausted");++_completionPublicationGeneration;_completionPublication=uint8_t(publication|((_completionPublication+1)&15));}
  timed=true;break;
 }
 case kFmLoop:t.dirtyLevel=true;break;
 case kFmJump:case kFmCall:case kFmReturn:case kFmIgnoredByte:break;
 }
 return true;
}
bool FmPlayer::tickTrack(unsigned index,bool carried,FmSynthSink& synth,std::vector<FmDrumEvent>& drums,std::string& e){
 FmTrackState& t=_tracks[index];if(carried){
  --t.remaining;if(!t.remaining){for(size_t budget=0;budget<=_assets->fm.size();++budget){FmSequenceEvent ev;if(!t.reader.next(ev,e))return false;bool timed=false;if(!event(index,ev,synth,drums,timed,e))return false;if(timed)return true;}return fail(e,"FM control cycle has no timed yield");}
  if(t.percussion)return true;
  if(t.panPeriod&&!--t.panCountdown){t.panCountdown=t.panPeriod;t.pan=uint8_t((t.pan&0x3f)|_data.panWave[unsigned(t.panWave)*16+t.panIndex]);if(++t.panIndex>11)t.panIndex=0;if(!pan(index,synth,e))return false;}
  if(t.pmsCountdown&&!--t.pmsCountdown){t.pan=uint8_t((t.pan&0xf0)|t.pms);if(!pan(index,synth,e))return false;}
  if(t.amsCountdown&&!--t.amsCountdown){t.pan=uint8_t((t.pan&0xcf)|t.ams);if(!pan(index,synth,e))return false;}
  if(t.gate!=0&&t.gate!=127){if(t.gateRemaining&&!--t.gateRemaining&&!key(index,false,synth,e))return false;if(!t.gateRemaining&&t.releaseStep){uint32_t next=uint32_t(t.release)+t.releaseStep;t.release=uint16_t(std::min(next,65535u));t.dirtyLevel=true;}}
  if(t.remaining==1&&(physical(index)&1)&&t.reader.peek()==0xe2){FmSequenceEvent ev;if(!t.reader.next(ev,e))return false;bool timed;if(!event(index,ev,synth,drums,timed,e))return false;}
 }
 if(t.percussion)return true;bool pitchDirty=false;int32_t modulation=0;
 if(t.slideTicks){if(t.slideDelay){if(carried)--t.slideDelay;}else{if(!--t.slideTicks)t.pitch=t.slideTarget;else t.pitch=plus(t.pitch,t.slideIncrement);pitchDirty=true;}}
 if(t.vibratoDepth){if(t.vibratoWait){if(carried)--t.vibratoWait;}else{if(t.vibratoRamp)return fail(e,"vibrato ramp tick has unowned source arithmetic");t.vibratoPhase=uint8_t(t.vibratoPhase+t.vibratoRate);unsigned magnitude=unsigned(_data.vibrato[(t.vibratoPhase>>1)&63])*(t.vibratoDepth16>>8);if(!(t.vibratoDepth&128))magnitude>>=2;modulation=(t.vibratoPhase&128)?-int32_t(magnitude):int32_t(magnitude);pitchDirty=true;}}
 unsigned noise=unsigned(t.noiseFraction)+t.noiseRate;t.noiseFraction=uint8_t(noise);if(t.noiseRate&&noise>255){uint16_t value=word(_data.noise+t.noiseIndex)&t.noiseMask;t.noiseIndex=uint16_t((t.noiseIndex+1)&511);modulation+=signedWord(value);pitchDirty=true;}
 if(pitchDirty&&!pitch(index,modulation,synth,e))return false;
 if(t.volumeRamp){if(!--t.volumeRamp)t.volume=uint16_t(t.volumeTarget)<<8;else t.volume=plus(t.volume,t.volumeStep);t.dirtyLevel=true;}
 return !t.dirtyLevel||level(index,synth,e);
}
bool FmPlayer::step(FmSynthSink& synth,std::vector<FmDrumEvent>& drums,std::string& e){
 if(_updates==std::numeric_limits<uint64_t>::max())return fail(e,"FM update identity exhausted");++_updates;
 bool muted=(_musicGain!=255&&_musicGain<21)||(_effectGain!=255&&_effectGain<21);if(muted){if(!_muted){if(_sourceResetPublication&&!muteMusic(synth,e))return false;for(unsigned i=0;i<6;++i){if(!_sourceResetPublication&&!synth.key(i,false,e))return false;_tracks[i].active=false;_tracks[i].command=_tracks[i].priority=0;_tracks[i].pendingReset=true;}_warmup=0;}_muted=true;
  // Source523A retires six music voices, holds percussion, but the two
  // independent source effect overlays still execute through4E08.
  for(unsigned i=8;i<10;++i)if(_tracks[i].active){if(_tracks[i].pendingReset){if(!resetVoice(i,synth,e))return false;_tracks[i].pendingReset=false;}unsigned sum=unsigned(_tracks[i].tempoFraction)+_tracks[i].tempo;_tracks[i].tempoFraction=uint8_t(sum);if(!tickTrack(i,sum>255,synth,drums,e))return false;}return true;
 }_muted=false;
 bool hold=_startupHold;_startupHold=false;
 for(unsigned index=0;index<10;++index){FmTrackState& t=_tracks[index];if(t.pendingReset&&!(index>=8&&hold)){if(!resetVoice(index,synth,e))return false;t.pendingReset=false;}if(!t.active)continue;if(index<6&&(hold||(_warmup&&((index&1)==(_warmup&1)))))continue;if(index>=8&&hold)continue;
  unsigned sum=unsigned(t.tempoFraction)+t.tempo;t.tempoFraction=uint8_t(sum);if(!tickTrack(index,sum>255,synth,drums,e)){std::ostringstream message;message<<"FM command "<<unsigned(t.command)<<" track "<<index<<" offset "<<t.reader.cursor()<<": "<<e;e=message.str();return false;}
 }
 if(_warmup&&!hold)--_warmup;return true;
}
bool FmPlayer::tick(FmSynthSink& synth,std::vector<FmDrumEvent>& drums,std::string& e){
 e.clear();drums.clear();if(!_loaded)return fail(e,"FM player source assets are unowned");FmPlayer staged=*this;RecordingSink output;std::vector<FmDrumEvent> pending;if(!staged.step(output,pending,e)||!output.replay(synth,e))return false;*this=staged;drums.swap(pending);return true;
}
}
