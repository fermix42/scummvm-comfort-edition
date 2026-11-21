/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fm_synth.h"
#include <limits>
#include <algorithm>
#include <deque>
#include <mutex>
#include <type_traits>
#include "nuked_opn2/ym3438.h"
namespace Snatcher {
namespace {
bool channelValid(unsigned channel,std::string& error){error.clear();if(channel>=6){error="FM generator channel outside six native voices";return false;}return true;}
const uint8_t operatorOffsets[4]={0,8,4,12};
}
int64_t FmOutputCoupling::process(int64_t input,uint32_t rate){
 // One-pole high-pass transfer H(z)=(1-z^-1)/(1-r*z^-1), with
 // r=1-44100/(512*rate). Q16 state avoids a signal-dependent DC fit.
 const int64_t next=input-_input+_output-(_output*44100)/(int64_t(512)*rate);_input=input;_output=next;return next;
}
uint8_t fmCarrierMask(uint8_t algorithm){return algorithm>7?0:algorithm<4?8:algorithm==4?10:algorithm<7?14:15;}
bool encodeFmInstrument(unsigned channel,const FmInstrument& instrument,std::vector<FmSynthWrite>& writes,std::string& error){
 if(!channelValid(channel,error))return false;
 std::vector<FmSynthWrite> next;uint8_t part=uint8_t(channel/3),voice=uint8_t(channel%3);next.push_back(FmSynthWrite(part,uint8_t(0xb0+voice),instrument.source[0]));
 uint8_t carriers=fmCarrierMask(instrument.source[0]&7);
 for(unsigned op=0;op<4;++op)for(unsigned parameter=0;parameter<6;++parameter){uint8_t value=instrument.source[1+6*op+parameter];if(parameter==1&&(carriers&(1u<<op)))value=255;next.push_back(FmSynthWrite(part,uint8_t(0x30+operatorOffsets[op]+voice+16*parameter),value));}
 writes.swap(next);return true;
}
bool encodeFmFrequency(unsigned channel,uint16_t frequency,std::vector<FmSynthWrite>& writes,std::string& error){
 if(!channelValid(channel,error))return false;if(frequency>0x3fff){error="FM F-number/block outside native generator field";return false;}
 std::vector<FmSynthWrite> next;uint8_t part=uint8_t(channel/3),voice=uint8_t(channel%3);next.push_back(FmSynthWrite(part,uint8_t(0xa4+voice),uint8_t(frequency>>8)));next.push_back(FmSynthWrite(part,uint8_t(0xa0+voice),uint8_t(frequency)));writes.swap(next);return true;
}
bool encodeFmLevels(unsigned channel,const uint8_t levels[4],std::vector<FmSynthWrite>& writes,std::string& error){
 // Explicit full-level setup retains its existing ascending operator contract.
 // Source dynamic carrier updates below have their own descending chronology.
 std::vector<FmSynthWrite> next;if(!encodeFmCarrierLevels(channel,levels,15,next,error))return false;std::reverse(next.begin(),next.end());writes.swap(next);return true;
}
bool encodeFmCarrierLevels(unsigned channel,const uint8_t levels[4],uint8_t mask,std::vector<FmSynthWrite>& writes,std::string& error){
 if(!channelValid(channel,error))return false;if(!mask||mask>15){error="FM source carrier mask outside four operators";return false;}if(!levels){error="FM source operator levels missing";return false;}for(unsigned i=0;i<4;++i)if(levels[i]>127){error="FM source operator attenuation outside7bit field";return false;}
 // Main5E5E walks its source operator index3 down to0. Register order matters
 // even when the final latched carrier values and write count are identical.
 std::vector<FmSynthWrite> next;for(int i=3;i>=0;--i)if(mask&(1u<<i))next.push_back(FmSynthWrite(uint8_t(channel/3),uint8_t(0x40+operatorOffsets[i]+channel%3),levels[i]));writes.swap(next);return true;
}
bool encodeFmKey(unsigned channel,bool on,std::vector<FmSynthWrite>& writes,std::string& error){
 if(!channelValid(channel,error))return false;std::vector<FmSynthWrite> next(1,FmSynthWrite(0,0x28,uint8_t((on?0xf0:0)+(channel<3?channel:channel+1))));writes.swap(next);return true;
}
bool encodeFmFastRelease(unsigned channel,std::vector<FmSynthWrite>& writes,std::string& error){
 if(!channelValid(channel,error))return false;std::vector<FmSynthWrite> next;
 for(unsigned op=0;op<4;++op)next.push_back(FmSynthWrite(uint8_t(channel/3),uint8_t(0x80+4*op+channel%3),0xff));
 next.push_back(FmSynthWrite(0,0x28,uint8_t(channel<3?channel:channel+1)));writes.swap(next);return true;
}
bool encodeFmPan(unsigned channel,uint8_t value,std::vector<FmSynthWrite>& writes,std::string& error){
 if(!channelValid(channel,error))return false;std::vector<FmSynthWrite> next(1,FmSynthWrite(uint8_t(channel/3),uint8_t(0xb4+channel%3),value));writes.swap(next);return true;
}
}
namespace Snatcher {
const char* fmCoreName(FmCore c){return c==kFmGens?"gens":"nuked";}
bool parseFmCore(const std::string& name,FmCore& c){if(name=="gens")c=kFmGens;else if(name=="nuked")c=kFmNuked;else return false;return true;}
struct NativeFmSynthRenderer::Impl {
 uint32_t rate;bool coupledOutput,discard;ym3438_t generator;std::deque<FmSynthWrite> pending;
 FmSynthWrite current;unsigned phase,delay;uint32_t cycleRemaining;int16_t pin[2];FmOutputCoupling coupling[2];uint64_t frames;
 explicit Impl(uint32_t r,bool coupled):rate(r),coupledOutput(coupled),discard(false),phase(0),delay(0),cycleRemaining(0),frames(0){
  static std::once_flag configured;std::call_once(configured,[]{OPN2_SetChipType(ym3438_mode_ym2612);});
  OPN2_Reset(&generator);pin[0]=pin[1]=0;
 }
 void clock(){
  if(phase&&!--delay){if(phase==1){OPN2_Write(&generator,current.part*2+1,current.value);phase=2;delay=2;}else phase=0;}
  if(!phase&&!pending.empty()&&!(OPN2_Read(&generator,0)&0x80)){
   current=pending.front();pending.pop_front();OPN2_Write(&generator,current.part*2,current.address);phase=1;delay=2;
  }
  Bit16s values[2];OPN2_Clock(&generator,values);
  // The YM2612-mode output's documented idle pin value is3. Remove this fixed
  // digital idle baseline, not a fitted signal-dependent gain or filter.
  pin[0]=int16_t(values[0]-3);pin[1]=int16_t(values[1]-3);
 }
};
NativeFmSynthRenderer::NativeFmSynthRenderer(uint32_t rate,bool coupled):_impl(new Impl(rate,coupled)){}
NativeFmSynthRenderer::~NativeFmSynthRenderer(){delete _impl;}
bool NativeFmSynthRenderer::init(std::string& error){error.clear();if(_impl->rate<8000||_impl->rate>192000){error="FM render rate outside supported native audio range";return false;}return true;}
bool NativeFmSynthRenderer::reset(std::string& error){if(!init(error))return false;OPN2_Reset(&_impl->generator);_impl->pending.clear();_impl->phase=_impl->delay=_impl->cycleRemaining=0;_impl->pin[0]=_impl->pin[1]=0;_impl->frames=0;for(unsigned i=0;i<2;++i)_impl->coupling[i].reset();return true;}
bool NativeFmSynthRenderer::apply(const std::vector<FmSynthWrite>& writes,std::string& error){
 if(!init(error))return false;if(_impl->discard)return true;if(writes.size()>4096-_impl->pending.size()){error="FM generator write queue capacity exhausted";return false;}
 for(const FmSynthWrite& write:writes)_impl->pending.push_back(write);return true;
}
bool NativeFmSynthRenderer::instrument(unsigned channel,const FmInstrument& value,std::string& error){std::vector<FmSynthWrite> writes;return encodeFmInstrument(channel,value,writes,error)&&apply(writes,error);}
bool NativeFmSynthRenderer::frequency(unsigned channel,uint16_t value,std::string& error){std::vector<FmSynthWrite> writes;return encodeFmFrequency(channel,value,writes,error)&&apply(writes,error);}
bool NativeFmSynthRenderer::levels(unsigned channel,const uint8_t values[4],std::string& error){std::vector<FmSynthWrite> writes;return encodeFmLevels(channel,values,writes,error)&&apply(writes,error);}
bool NativeFmSynthRenderer::carrierLevels(unsigned channel,const uint8_t values[4],uint8_t mask,std::string& error){std::vector<FmSynthWrite> writes;return encodeFmCarrierLevels(channel,values,mask,writes,error)&&apply(writes,error);}
bool NativeFmSynthRenderer::key(unsigned channel,bool on,std::string& error){std::vector<FmSynthWrite> writes;return encodeFmKey(channel,on,writes,error)&&apply(writes,error);}
bool NativeFmSynthRenderer::fastRelease(unsigned channel,std::string& error){std::vector<FmSynthWrite> writes;return encodeFmFastRelease(channel,writes,error)&&apply(writes,error);}
bool NativeFmSynthRenderer::pan(unsigned channel,uint8_t value,std::string& error){std::vector<FmSynthWrite> writes;return encodeFmPan(channel,value,writes,error)&&apply(writes,error);}
bool NativeFmSynthRenderer::lfo(uint8_t value,std::string& error){std::vector<FmSynthWrite> writes(1,FmSynthWrite(0,0x22,value));return apply(writes,error);}
bool NativeFmSynthRenderer::renderPcm(uint32_t frames,std::vector<int16_t>& pcm,std::string& error){
 error.clear();if(!frames||frames>1000000){error="FM render request outside bounded native frame domain";return false;}if(!init(error))return false;
 if(std::numeric_limits<uint64_t>::max()-_impl->frames<frames){error="FM rendered frame counter overflow";return false;}
 std::vector<int16_t> next(size_t(frames)*2);
 if(_impl->discard){_impl->frames+=frames;pcm.swap(next);return true;}
 // Exact NTSC source oscillator53693175Hz; OPN2 clock step is42 source
 // oscillator periods (YM divider7, generator divider6). Integer integration
 // keeps sample boundaries and write timing independent of callback chunks.
 const uint32_t sourceClock=53693175,cycleDuration=_impl->rate*42;
 for(uint32_t frame=0;frame<frames;++frame){uint32_t remaining=sourceClock;int64_t sum[2]={0,0};
  while(remaining){if(!_impl->cycleRemaining){_impl->clock();_impl->cycleRemaining=cycleDuration;}uint32_t span=remaining<_impl->cycleRemaining?remaining:_impl->cycleRemaining;sum[0]+=int64_t(_impl->pin[0])*span;sum[1]+=int64_t(_impl->pin[1])*span;remaining-=span;_impl->cycleRemaining-=span;}
  for(unsigned channel=0;channel<2;++channel){// A complete OPN2 frame contains24 pin samples. The declared oracle
   // output domain normalizes their sum by11:264 times the pin average.
   // This fixed component conversion is not fitted to game music.
   const int64_t input=sum[channel]*264*65536/sourceClock;
   int64_t value=(_impl->coupledOutput?_impl->coupling[channel].process(input,_impl->rate):input)/65536;if(value>32767)value=32767;if(value<-32768)value=-32768;next[size_t(frame)*2+channel]=int16_t(value);}
 }
 _impl->frames+=frames;pcm.swap(next);return true;
}
uint8_t NativeFmSynthRenderer::keyOnMask()const{uint8_t mask=0;for(unsigned channel=0;channel<6;++channel)for(unsigned op=0;op<4;++op)if(_impl->generator.mode_kon[channel+op*6])mask|=uint8_t(1u<<channel);return mask;}
uint32_t NativeFmSynthRenderer::sampleRate()const{return _impl->rate;}
uint64_t NativeFmSynthRenderer::renderedFrames()const{return _impl->frames;}
size_t NativeFmSynthRenderer::pendingWrites()const{return _impl->pending.size()+size_t(_impl->phase!=0);}
#include "fm_synth_state.inc"
}
#ifndef SNATCHER_FM_CORE_ONLY
#include "fm_gens.h"
#include "audio/mixer_intern.h"
#include "audio/audiostream.h"
#include "audio_runtime.h"
namespace Snatcher {
namespace {
class NativeFmGeneratorStream:public Audio::AudioStream {
public:
 explicit NativeFmGeneratorStream(FmSynthSink& source):_source(source),_failed(false),_budget(0),_capture(nullptr){}
 int readBuffer(int16* buffer,const int samples) override {if(samples<=0||samples%2||_failed||!_budget)return 0;unsigned count=std::min(unsigned(samples/2),_budget);std::string error;if(!_source.renderPcm(count,_scratch,error)){_failed=true;return 0;}for(unsigned i=0;i<count*2;++i)buffer[i]=_scratch[size_t(i)];if(_capture)_capture->insert(_capture->end(),_scratch.begin(),_scratch.end());_budget-=count;return int(count*2);}
 void capture(std::vector<int16_t>* output){_capture=output;}
 bool isStereo()const override{return true;}bool endOfData()const override{return _failed;}int getRate()const override{return int(_source.sampleRate());}
 void resetFailure(){_failed=false;_budget=0;}
 void beginPull(unsigned frames){_budget=frames;}
 bool pullComplete()const{return !_budget&&!_failed;}
private:FmSynthSink& _source;std::vector<int16_t> _scratch;bool _failed;unsigned _budget;std::vector<int16_t>* _capture;
};
}
struct CeFmSynthRenderer::Impl {
 uint32_t rate;NativeFmSynthRenderer nuked;GensFmSynthRenderer gens;FmCore core;FmSynthSink& generator;Audio::MixerImpl mixer;NativeFmGeneratorStream stream;CeAudioBackend percussion;Audio::SoundHandle handle;bool ready;
 explicit Impl(uint32_t r,bool coupled,FmCore c):rate(r),nuked(r,coupled),gens(r,coupled),core(c),generator(c==kFmGens?static_cast<FmSynthSink&>(gens):static_cast<FmSynthSink&>(nuked)),mixer(r>=8000&&r<=192000?r:44100),stream(generator),percussion(mixer),ready(false){}
 ~Impl(){if(ready)mixer.stopHandle(handle);}
};
CeFmSynthRenderer::CeFmSynthRenderer(uint32_t rate,bool coupled,FmCore core):_impl(new Impl(rate,coupled,core)){}
CeFmSynthRenderer::~CeFmSynthRenderer(){delete _impl;}
bool CeFmSynthRenderer::init(std::string& error){if(!(_impl->core==kFmGens?_impl->gens.init(error):_impl->nuked.init(error)))return false;if(_impl->ready)return true;_impl->mixer.setReady(true);_impl->mixer.playStream(Audio::Mixer::kPlainSoundType,&_impl->handle,&_impl->stream,-1,255,0,DisposeAfterUse::NO,true,false);if(!_impl->mixer.isSoundHandleActive(_impl->handle)){error="Cannot initialize private FM mixing stream";return false;}_impl->ready=true;return true;}
bool CeFmSynthRenderer::reset(std::string& error){if(_impl->ready){_impl->mixer.stopHandle(_impl->handle);_impl->ready=false;}if(!_impl->generator.reset(error))return false;_impl->stream.resetFailure();return init(error);}
bool CeFmSynthRenderer::instrument(unsigned channel,const FmInstrument& value,std::string& error){return init(error)&&_impl->generator.instrument(channel,value,error);}
bool CeFmSynthRenderer::frequency(unsigned channel,uint16_t value,std::string& error){return init(error)&&_impl->generator.frequency(channel,value,error);}
bool CeFmSynthRenderer::levels(unsigned channel,const uint8_t values[4],std::string& error){return init(error)&&_impl->generator.levels(channel,values,error);}
bool CeFmSynthRenderer::carrierLevels(unsigned channel,const uint8_t values[4],uint8_t mask,std::string& error){return init(error)&&_impl->generator.carrierLevels(channel,values,mask,error);}
bool CeFmSynthRenderer::key(unsigned channel,bool on,std::string& error){return init(error)&&_impl->generator.key(channel,on,error);}
bool CeFmSynthRenderer::fastRelease(unsigned channel,std::string& error){return init(error)&&_impl->generator.fastRelease(channel,error);}
bool CeFmSynthRenderer::pan(unsigned channel,uint8_t value,std::string& error){return init(error)&&_impl->generator.pan(channel,value,error);}
bool CeFmSynthRenderer::lfo(uint8_t value,std::string& error){return init(error)&&_impl->generator.lfo(value,error);}
bool CeFmSynthRenderer::renderPcm(uint32_t frames,std::vector<int16_t>& pcm,std::string& error){
 error.clear();if(!frames||frames>1000000){error="FM render request outside bounded native frame domain";return false;}if(!init(error))return false;std::vector<int16_t> next(size_t(frames)*2);uint32_t offset=0;while(offset<frames){uint32_t count=frames-offset;if(count>4096)count=4096;_impl->stream.beginPull(count);if(_impl->mixer.mixCallback(reinterpret_cast<byte*>(next.data()+size_t(offset)*2),count*4)!=int(count)||!_impl->stream.pullComplete()){error="FM private mixer did not produce requested stereo frames";return false;}offset+=count;}pcm.swap(next);return true;
}
bool CeFmSynthRenderer::snapshotGeneratorState(std::vector<uint8_t>& bytes,std::string& error)const{return _impl->core==kFmGens?_impl->gens.snapshotState(bytes,error):_impl->nuked.snapshotState(bytes,error);}
bool CeFmSynthRenderer::renderPcmSeparated(uint32_t frames,std::vector<int16_t>& mixed,std::vector<int16_t>& fm,std::string& error){
 std::vector<int16_t> lane;_impl->stream.capture(&lane);bool ready=renderPcm(frames,mixed,error);_impl->stream.capture(nullptr);
 if(!ready)return false;if(lane.size()!=size_t(frames)*2){error="FM lane capture disagrees with delivered mixer frames";return false;}fm.swap(lane);return true;
}
bool CeFmSynthRenderer::restoreGeneratorState(const std::vector<uint8_t>& bytes,std::string& error){return _impl->core==kFmGens?_impl->gens.restoreState(bytes,error,_impl->gens.renderedFrames()):_impl->nuked.restoreState(bytes,error,_impl->nuked.renderedFrames());}
void CeFmSynthRenderer::setReplayDiscard(bool discard){if(_impl->core==kFmGens)_impl->gens.setReplayDiscard(discard);else _impl->nuked.setReplayDiscard(discard);}
uint8_t CeFmSynthRenderer::keyOnMask()const{return _impl->core==kFmGens?_impl->gens.keyOnMask():_impl->nuked.keyOnMask();}
uint32_t CeFmSynthRenderer::sampleRate()const{return _impl->rate;}
AudioBackend& CeFmSynthRenderer::percussionBackend(){return _impl->percussion;}
}
#endif
