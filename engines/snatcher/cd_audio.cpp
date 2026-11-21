/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <atomic>
#include "cd_audio.h"
#include <algorithm>
#include <limits>
namespace Snatcher {
namespace {bool fail(std::string& e,const char *s){e=s;return false;}struct Inside {bool& value;Inside(bool& v):value(v){value=true;}~Inside(){value=false;}};}
CdAudioPump::CdAudioPump(CdFrameSource& source,CdQueueBackend& backend,uint32_t chunk,size_t maxChunks):_source(source),_backend(backend),_chunk(chunk),_nextFrame(0),_total(0),_maxChunks(maxChunks),_nextToken(1),_track(0),_lastConsumed(0),_positionKnown(false),_active(false),_paused(false),_inputFinished(false),_completionPending(false),_inside(false){}
bool validateCdIdleSnapshot(const CdIdleSnapshot& s,std::string& error){
 error.clear();if(s.version!=1||!s.nextToken)return fail(error,"Invalid idle CD owner version or token cursor");
 if(!s.positionKnown){if(s.track||s.total||s.nextFrame||s.lastConsumed||s.inputFinished)return fail(error,"Unknown idle CD owner contains a fabricated position");return true;}
 if(!s.track||!s.total||s.nextFrame>s.total||s.lastConsumed>s.nextFrame||s.inputFinished!=(s.nextFrame==s.total))return fail(error,"Invalid retained idle CD source bounds");return true;
}
bool CdAudioPump::captureIdle(CdIdleSnapshot& output,std::string& error)const{
 error.clear();if(_inside||_active||_completionPending)return fail(error,"CD snapshot requires an inactive owner with delivered completion");
 CdIdleSnapshot s;s.track=_track;s.total=_total;s.nextFrame=_nextFrame;s.lastConsumed=_lastConsumed;s.nextToken=_nextToken;s.positionKnown=_positionKnown;s.inputFinished=_inputFinished;s.paused=_paused;
 if(!validateCdIdleSnapshot(s,error))return false;output=s;return true;
}
bool CdAudioPump::validateIdleSource(const CdIdleSnapshot& s,std::string& error){
 error.clear();if(_inside)return fail(error,"Reentrant idle CD source validation");if(!validateCdIdleSnapshot(s,error))return false;Inside guard(_inside);
 if(s.positionKnown){uint32_t total=0;if(!_source.frameCount(s.track,total,error)){if(error.empty())error="Idle CD track metadata is unavailable";return false;}if(total!=s.total)return fail(error,"Idle CD track identity or length differs from original source");}return true;
}
bool CdAudioPump::restoreIdle(const CdIdleSnapshot& s,std::string& error){
 error.clear();if(_inside||_active||_completionPending)return fail(error,"Idle CD restore requires no active or pending owner");if(!validateIdleSource(s,error))return false;Inside guard(_inside);
 commitValidatedIdle(s);return true;
}
void CdAudioPump::commitValidatedIdle(const CdIdleSnapshot& s){
 _backend.stop();_resourceReadPaused=false;_sampleOrigin=0;_track=s.track;_total=s.total;_nextFrame=s.nextFrame;_lastConsumed=s.lastConsumed;_nextToken=s.nextToken;_positionKnown=s.positionKnown;_inputFinished=s.inputFinished;_paused=s.paused;_token=AudioToken();_owner=AudioOwner();_completion=CdAudioCompletion();
}
bool encodeCdRelativeClock(uint64_t frames,uint32_t& packed,std::string& e){
 e.clear();packed=0;if(frames>=uint64_t(100)*60*75)return fail(e,"CD relative clock exceeds two-digit minutes");
 const unsigned m=unsigned(frames/(60*75)),s=unsigned(frames/75%60),f=unsigned(frames%75);
 packed=uint32_t((m/10)*16+m%10)<<24|uint32_t((s/10)*16+s%10)<<16|uint32_t((f/10)*16+f%10)<<8;return true;
}
bool CdAudioPump::consumedPosition(unsigned& track,uint64_t& frames)const{
 track=_track;frames=0;if(_active){uint64_t position;if(!_backend.consumedSamples(position))return false;frames=std::min((position+_sampleOrigin)/588,uint64_t(_total));return true;}
 frames=_lastConsumed;return _positionKnown;
}
bool CdAudioPump::presentationPosition(unsigned& track,uint64_t& frames)const{
 track=0;frames=0;
 if(!_active||paused()||!_backend.active())return true;
 return consumedPosition(track,frames);
}
CdAudioPump::~CdAudioPump(){_backend.stop();}
bool CdAudioPump::fill(std::string& e){
 for(size_t reads=0;reads<_maxChunks&&_nextFrame<_total&&_backend.queuedChunks()<_maxChunks;++reads){
  const uint32_t count=std::min(_chunk,_total-_nextFrame);std::vector<uint8_t> pcm;
  if(!_source.readFrames(_track,_nextFrame,count,pcm,e))return false;
  if(pcm.size()!=size_t(count)*2352)return fail(e,"CD-DA source returned an incomplete frame range");
  if(!_backend.queue(pcm,e))return false;_nextFrame+=count;
 }
 if(_nextFrame==_total&&!_inputFinished){_backend.finishInput();_inputFinished=true;}return true;
}
bool CdAudioPump::start(unsigned track,uint8_t volume,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& e){
 token=AudioToken();e.clear();if(_inside)return fail(e,"Reentrant CD-DA pump operation");
 if(_active||_completionPending)return fail(e,"CD-DA track or undelivered completion already owned");
 if(!_chunk||_chunk>4096||!_maxChunks||_maxChunks>8)return fail(e,"Invalid CD-DA queue limits");
 if(!_nextToken||_nextToken==std::numeric_limits<uint64_t>::max())return fail(e,"CD-DA token space exhausted");
 Inside guard(_inside);uint32_t total=0;if(!_source.frameCount(track,total,e)){if(e.empty())e="Cannot read CD-DA track metadata";return false;}if(!total)return fail(e,"CD-DA track contains no playable frames");
 if(!_backend.begin(volume,e)){_backend.stop();if(e.empty())e="Cannot start CD-DA mixer queue";return false;}if(_paused)_backend.pause(true);
 _resourceReadPaused=false;_volume=volume;_sampleOrigin=0;_track=track;_lastConsumed=0;_positionKnown=false;_nextFrame=0;_total=total;_inputFinished=false;
 if(!fill(e)){_backend.stop();_nextFrame=_total=0;if(e.empty())e="Cannot buffer initial CD-DA frames";return false;}
 _token=AudioToken(_nextToken++,generation);_owner=owner;_active=true;token=_token;
 return true;
}
void CdAudioPump::finish(CdAudioResult result,const std::string& error){
 uint64_t position;_positionKnown=_backend.consumedSamples(position);_lastConsumed=_positionKnown?std::min((position+_sampleOrigin)/588,uint64_t(_total)):0;
 _backend.stop();_completion.token=_token;_completion.owner=_owner;_completion.result=result;_completion.error=error;_completionPending=true;_active=false;_resourceReadPaused=false;
}
bool CdAudioPump::update(std::string& e){
 e.clear();if(_inside)return fail(e,"Reentrant CD-DA pump operation");if(!_active||paused())return true;Inside guard(_inside);
 if(!_backend.active()){
  if(_inputFinished){finish(kCdAudioFinished,std::string());return true;}
  e="CD-DA mixer stream stopped before all input was queued";finish(kCdAudioFailed,e);return false;
 }
 if(!fill(e)){if(e.empty())e="CD-DA refill failed";finish(kCdAudioFailed,e);return false;}
 if(_inputFinished&&!_backend.active())finish(kCdAudioFinished,std::string());return true;
}
bool CdAudioPump::cancel(AudioToken token,std::string& e){
 e.clear();if(_inside)return fail(e,"Reentrant CD-DA pump operation");
 if(!_active||token.id!=_token.id||token.generation!=_token.generation)return fail(e,"CD-DA token is stale or completed");Inside guard(_inside);finish(kCdAudioCancelled,std::string());return true;
}
bool CdAudioPump::reset(std::string& error){error.clear();if(_inside){error="reentrant CD audio reset";return false;}_inside=true;if(_active)_backend.stop();_active=false;_resourceReadPaused=false;_completionPending=false;_inputFinished=false;_nextFrame=_total=0;_track=0;_lastConsumed=0;_positionKnown=false;_token=AudioToken();_owner=AudioOwner();_inside=false;return true;}
void CdAudioPump::cancelGeneration(uint32_t generation){if(!_inside&&_active&&_token.generation==generation){Inside guard(_inside);finish(kCdAudioCancelled,std::string());}}
void CdAudioPump::setPaused(bool value){if(_inside||_paused==value)return;bool previous=paused();_paused=value;if(_active&&previous!=paused())_backend.pause(paused());}
void CdAudioPump::pauseForResourceRead(){if(_inside||!_active||_resourceReadPaused)return;bool previous=paused();_resourceReadPaused=true;if(!previous)_backend.pause(true);}
bool CdAudioPump::releaseForResourceRead(std::string& error){
 error.clear();if(_inside)return fail(error,"reentrant resource-read transport handoff");if(!_active)return true;
 if(!_resourceReadPaused)return fail(error,"resource read requires its prior transport pause");
 Inside guard(_inside);finish(kCdAudioCancelled,std::string());return true;
}
bool CdAudioPump::take(CdAudioCompletion& completion){if(_inside||!_completionPending)return false;completion=_completion;_completionPending=false;return true;}
}
#ifndef SNATCHER_AUDIO_CORE_ONLY
#include "snatcher/resource.h"
#include "audio/audiostream.h"
#include "audio/mixer.h"
#include "audio/decoders/raw.h"
#include <cstdlib>
#include <cstring>
namespace Snatcher {
bool ResourceCdFrameSource::frameCount(unsigned track,uint32_t& count,std::string& e)const{return _resource.cdAudioFrameCount(track,count,e);}
bool ResourceCdFrameSource::readFrames(unsigned track,uint32_t first,uint32_t count,std::vector<uint8_t>& pcm,std::string& e)const{return _resource.readCdAudioFrames(track,first,count,pcm,e);}
class ConsumedCdStream:public Audio::AudioStream {
public:
 Audio::QueuingAudioStream& queue;std::atomic<uint64_t> samples;
 explicit ConsumedCdStream(Audio::QueuingAudioStream& q):queue(q),samples(0){}
 int readBuffer(int16* buffer,const int count)override{const int n=queue.readBuffer(buffer,std::min(count,2));if(n>0)samples.fetch_add(uint64_t(n),std::memory_order_relaxed);return n;}
 bool isStereo()const override{return true;}int getRate()const override{return 44100;}
 bool endOfData()const override{return queue.endOfData();}bool endOfStream()const override{return queue.endOfStream();}
};
struct CeCdQueueBackend::Impl {Audio::Mixer& mixer;Audio::QueuingAudioStream *stream;ConsumedCdStream *counted;Audio::SoundHandle handle;bool paused=false;Impl(Audio::Mixer& m):mixer(m),stream(nullptr),counted(nullptr){}};
CeCdQueueBackend::CeCdQueueBackend(Audio::Mixer& mixer):_impl(new Impl(mixer)){}
CeCdQueueBackend::~CeCdQueueBackend(){stop();delete _impl;}
bool CeCdQueueBackend::begin(uint8_t volume,std::string& e){
 e.clear();stop();_impl->paused=false;_impl->stream=Audio::makeQueuingAudioStream(44100,true);if(!_impl->stream)return fail(e,"Cannot create CD-DA queued mixer stream");
 // Backend owns the queue until stop() has removed its mixer handle. This
 // avoids a finished/drained queue pointer becoming dangling on the mixer thread.
 _impl->counted=new ConsumedCdStream(*_impl->stream);
 _impl->mixer.playStream(Audio::Mixer::kMusicSoundType,&_impl->handle,_impl->counted,-1,volume,0,DisposeAfterUse::NO);return true;
}
bool CeCdQueueBackend::queue(const std::vector<uint8_t>& pcm,std::string& e){
 e.clear();if(!_impl->stream||pcm.empty()||pcm.size()%4||pcm.size()>size_t(4096)*2352)return fail(e,"Invalid CD-DA queued buffer");
 byte *copy=static_cast<byte *>(std::malloc(pcm.size()));if(!copy)return fail(e,"Cannot allocate CD-DA queue buffer");std::memcpy(copy,pcm.data(),pcm.size());
 _impl->stream->queueBuffer(copy,uint32(pcm.size()),DisposeAfterUse::YES,Audio::FLAG_16BITS|Audio::FLAG_LITTLE_ENDIAN|Audio::FLAG_STEREO);return true;
}
size_t CeCdQueueBackend::queuedChunks()const{return _impl->stream?_impl->stream->numQueuedStreams():0;}
void CeCdQueueBackend::finishInput(){if(_impl->stream)_impl->stream->finish();}
bool CeCdQueueBackend::active()const{return _impl->stream&&_impl->mixer.isSoundHandleActive(_impl->handle);}
bool CeCdQueueBackend::consumedFrames(uint64_t& frames)const{frames=0;if(!_impl->counted)return false;frames=_impl->counted->samples.load(std::memory_order_relaxed)/1176;return true;}
bool CeCdQueueBackend::consumedSamples(uint64_t& frames)const{frames=0;if(!_impl->counted)return false;frames=_impl->counted->samples.load(std::memory_order_relaxed)/2;return true;}
void CeCdQueueBackend::stop(){if(!_impl->stream)return;_impl->mixer.stopHandle(_impl->handle);delete _impl->counted;_impl->counted=nullptr;delete _impl->stream;_impl->stream=nullptr;}
void CeCdQueueBackend::pause(bool paused){if(_impl->stream&&_impl->paused!=paused){_impl->paused=paused;_impl->mixer.pauseHandle(_impl->handle,paused);}}
}
#endif

namespace Snatcher {
bool CdAudioPump::capturePlayback(CdPlaybackState& s,std::string& error)const{
 if(_inside){error="Reentrant CD snapshot";return false;}s.idle.track=_track;s.idle.total=_total;s.idle.nextFrame=_nextFrame;s.idle.lastConsumed=_lastConsumed;s.idle.nextToken=_nextToken;s.idle.positionKnown=_positionKnown;s.idle.inputFinished=_inputFinished;s.idle.paused=_paused;s.active=_active;s.resourcePaused=_resourceReadPaused;s.completionPending=_completionPending;s.volume=_volume;s.token=_token;s.owner=_owner;s.completion=_completion;
 s.samples=0;if(_active){if(!_backend.consumedSamples(s.samples)){error="CD consumer position unavailable";return false;}s.samples+=_sampleOrigin;}return true;
}
bool CdAudioPump::preparePlayback(CdPlaybackState& s,std::string& error){
 auto& i=s.idle;if(!i.nextToken||i.track>99||i.nextFrame>i.total||s.samples>uint64_t(i.nextFrame)*588||i.lastConsumed>i.total||s.completion.result>kCdAudioFailed){error="Invalid CD continuation bounds";return false;}
 if(i.track){uint32_t total=0;if(!_source.frameCount(i.track,total,error)||total!=i.total){error="CD continuation source length differs";return false;}}
 s.queued.clear();if(s.active){if(!s.token.id||!i.track||!i.total){error="Active CD continuation has no owner";return false;}uint32_t first=uint32_t(s.samples/588),count=i.nextFrame-first;if(count>_chunk*_maxChunks+1){error="CD continuation exceeds its queue window";return false;}if(count){if(!_source.readFrames(i.track,first,count,s.queued,error))return false;if(s.queued.size()!=size_t(count)*2352){error="CD continuation source returned an incomplete queue";return false;}size_t skip=size_t(s.samples%588)*4;if(skip>s.queued.size())return false;s.queued.erase(s.queued.begin(),s.queued.begin()+skip);}}
 return true;
}
bool CdAudioPump::commitPlayback(const CdPlaybackState& s,std::string& error){
 _backend.stop();const auto& i=s.idle;_track=i.track;_total=i.total;_nextFrame=i.nextFrame;_lastConsumed=i.lastConsumed;_nextToken=i.nextToken;_positionKnown=i.positionKnown;_inputFinished=i.inputFinished;_paused=i.paused;_active=s.active;_resourceReadPaused=s.resourcePaused;_completionPending=s.completionPending;_volume=s.volume;_token=s.token;_owner=s.owner;_completion=s.completion;_sampleOrigin=s.samples;
 if(_active){if(!_backend.begin(_volume,error))return false;size_t at=0;uint64_t sample=s.samples;while(at<s.queued.size()){size_t count=std::min(s.queued.size()-at,size_t(uint64_t(_chunk)*588-sample%(uint64_t(_chunk)*588))*4);std::vector<uint8_t> chunk(s.queued.begin()+at,s.queued.begin()+at+count);if(!_backend.queue(chunk,error))return false;at+=count;sample+=count/4;}if(_inputFinished)_backend.finishInput();_backend.pause(paused());}return true;
}
}
