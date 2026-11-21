/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio_output.h"
#include <algorithm>
#include <limits>
namespace Snatcher {
bool NativeAudioOutput::restore(const NativeAudioOutputState& state,std::string& error){
 error.clear();if(state.version!=1||state.rate!=44100){error="Unsupported native final audio-output state";return false;}
 // 32 source channels of S16 fit within 2^36 Q16. The high-pass impulse
 // response has absolute sum2; leave only its bounded integer rounding tail.
 const int64_t inputBound=INT64_C(1)<<36,outputBound=(INT64_C(1)<<37)+1024;
 for(unsigned i=0;i<2;++i)if(state.inputQ16[i]<-inputBound||state.inputQ16[i]>inputBound||state.inputQ16[i]%65536||state.outputQ16[i]<-outputBound||state.outputQ16[i]>outputBound||(!state.frames&&(state.inputQ16[i]||state.outputQ16[i]))){error="Invalid native audio coupling history";return false;}
 _state=state;return true;
}
bool NativeAudioOutput::process(const int32_t* source,int16_t* output,uint32_t frames,std::string& error){
 error.clear();if(!source||!output||!frames||frames>1000000||std::numeric_limits<uint64_t>::max()-_state.frames<frames){error="Invalid native final audio-output extent";return false;}
 for(size_t i=0;i<size_t(frames)*2;++i)if(source[i]<-1048576||source[i]>1048576){error="Native audio sum exceeds owned32-channel domain";return false;}
 for(uint32_t frame=0;frame<frames;++frame)for(unsigned ch=0;ch<2;++ch){
  const int64_t input=int64_t(source[size_t(frame)*2+ch])*65536;
  const int64_t coupled=input-_state.inputQ16[ch]+_state.outputQ16[ch]-_state.outputQ16[ch]/512;
  _state.inputQ16[ch]=input;_state.outputQ16[ch]=coupled;
  const int64_t sample=coupled/65536;output[size_t(frame)*2+ch]=int16_t(std::max(INT64_C(-32768),std::min(INT64_C(32767),sample)));
 }
 _state.frames+=frames;return true;
}
}
#ifndef SNATCHER_AUDIO_CORE_ONLY
#include "audio/mixer_intern.h"
#include "audio/audiostream.h"
#include "common/mutex.h"
namespace Snatcher {
struct CeAudioOutputOwner::Impl:public Audio::AudioStream {
 Audio::Mixer& outer;Audio::MixerImpl inner;Audio::SoundHandle handle;mutable Common::Mutex mutex;
 NativeAudioOutput output;bool attached,paused;std::vector<int32_t> scratch;std::string failure;
 explicit Impl(Audio::Mixer& mixer):outer(mixer),inner(44100,true,0,4,false),attached(false),paused(false){inner.setReady(true);}
 bool isStereo()const override{return true;}int getRate()const override{return 44100;}bool endOfData()const override{return false;}
 bool pull(uint32_t frames,int16_t* pcm,std::string& error){
  // Preserve CE user controls at the individual source roles. Plain volume is
  // applied once by the outer stream, rather than twice by the inner mixer.
  for(unsigned i=1;i<4;++i){Audio::Mixer::SoundType type=Audio::Mixer::SoundType(i);inner.setVolumeForSoundType(type,outer.getVolumeForSoundType(type));inner.muteSoundType(type,outer.isSoundTypeMuted(type));}
  uint32_t offset=0;while(offset<frames){uint32_t count=std::min(uint32_t(4096),frames-offset);scratch.assign(size_t(count)*2,0);inner.mixCallback(reinterpret_cast<byte*>(scratch.data()),count*8);if(!output.process(scratch.data(),pcm+size_t(offset)*2,count,error))return false;offset+=count;}return true;
 }
 int readBuffer(int16* buffer,const int samples)override{if(samples<=0)return 0;Common::StackLock lock(mutex);if(samples%2)failure="Unpaired native final audio pull";if(paused||!failure.empty())std::fill(buffer,buffer+samples,0);else{std::string error;if(!pull(uint32_t(samples/2),buffer,error)){failure=error;std::fill(buffer,buffer+samples,0);}}return samples;}
};
CeAudioOutputOwner::CeAudioOutputOwner(Audio::Mixer& mixer):_impl(new Impl(mixer)){}
CeAudioOutputOwner::~CeAudioOutputOwner(){stop();delete _impl;}
Audio::Mixer& CeAudioOutputOwner::mixer(){return _impl->inner;}
bool CeAudioOutputOwner::start(std::string& error){{Common::StackLock lock(_impl->mutex);error=_impl->failure;}if(!error.empty())return false;if(_impl->attached)return true;_impl->outer.playStream(Audio::Mixer::kPlainSoundType,&_impl->handle,_impl,-1,255,0,DisposeAfterUse::NO,true,false);if(!_impl->outer.isSoundHandleActive(_impl->handle)){error="Cannot attach native final audio output";return false;}_impl->attached=true;if(_impl->paused)_impl->outer.pauseHandle(_impl->handle,true);return true;}
void CeAudioOutputOwner::stop(){if(_impl->attached){_impl->outer.stopHandle(_impl->handle);_impl->attached=false;}}
void CeAudioOutputOwner::reset(){bool wasPaused;{Common::StackLock lock(_impl->mutex);wasPaused=_impl->paused;}setPaused(true);std::string ignored;restore(NativeAudioOutputState(),ignored);if(!wasPaused)setPaused(false);}
void CeAudioOutputOwner::setPaused(bool value){bool changed;{Common::StackLock lock(_impl->mutex);changed=_impl->paused!=value;_impl->paused=value;}if(changed&&_impl->attached)_impl->outer.pauseHandle(_impl->handle,value);}
std::string CeAudioOutputOwner::error()const{Common::StackLock lock(_impl->mutex);return _impl->failure;}
bool CeAudioOutputOwner::snapshot(NativeAudioOutputState& state,std::string& error)const{Common::StackLock lock(_impl->mutex);error=_impl->failure;if(!error.empty())return false;if(!_impl->paused){error="Final audio-output snapshot requires paused owner";return false;}state=_impl->output.snapshot();return true;}
bool CeAudioOutputOwner::restore(const NativeAudioOutputState& state,std::string& error){
 NativeAudioOutput staged;if(!staged.restore(state,error))return false;bool attached;
 {Common::StackLock lock(_impl->mutex);if(!_impl->paused){error="Final audio-output restore requires paused owner";return false;}attached=_impl->attached;}
 // Discard the old outer rate converter's prefetched samples. Device converter
 // phase is not native story/audio state and must not leak across a load/reset.
 // Outer mixer calls stay outside the callback's state mutex.
 if(attached)stop();
 {Common::StackLock lock(_impl->mutex);_impl->output=staged;_impl->failure.clear();}
 if(attached&&!start(error)){Common::StackLock lock(_impl->mutex);_impl->failure=error;return false;}return true;
}
bool CeAudioOutputOwner::renderOffline(uint32_t frames,std::vector<int16_t>& pcm,std::string& error){error.clear();if(!frames||frames>1000000){error="Native final audio offline extent invalid";return false;}const bool temporaryPause=_impl->attached;if(temporaryPause)_impl->outer.pauseHandle(_impl->handle,true);bool success=false;{Common::StackLock lock(_impl->mutex);error=_impl->failure;if(error.empty()){std::vector<int16_t> next(size_t(frames)*2);success=_impl->pull(frames,next.data(),error);if(success)pcm.swap(next);else _impl->failure=error;}}if(temporaryPause)_impl->outer.pauseHandle(_impl->handle,false);return success;}
}
#endif
