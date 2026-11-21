/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <deque>
#include "fm_audio.h"
#include "fm_publication.h"
#include "audio_percussion.h"
#include "audio/audiostream.h"
#include "audio/mixer.h"
#include "common/mutex.h"
namespace Snatcher {
namespace {
struct FmHistoryEvent {uint64_t frame;uint8_t kind,a,b;FmHistoryEvent(uint64_t f,uint8_t k,uint8_t x=0,uint8_t y=0):frame(f),kind(k),a(x),b(y){}};
// Preserve the historical one-hour V1/V2 full-synthesis replay bound. V3
// retains a finite one-day semantic-reconstruction domain after measured
// 21-minute speedup. V4 uses its larger bounded event representation without
// truncating, coalescing or replacing the semantic replay origin.
uint64_t assetHash(const std::vector<uint8_t>& data){uint64_t value=UINT64_C(14695981039346656037);for(uint8_t byte:data)value=(value^byte)*UINT64_C(1099511628211);return value;}
void putHistory(std::vector<uint8_t>& out,uint64_t value,unsigned width){for(unsigned i=width;i;--i)out.push_back(uint8_t(value>>((i-1)*8)));}
uint64_t getHistory(const std::vector<uint8_t>& in,size_t& at,unsigned width){uint64_t value=0;while(width--)value=(value<<8)|in[at++];return value;}
}
struct CeFmAudioOwner::Impl:public Audio::AudioStream {
 Audio::Mixer& mixer;mutable Common::Mutex mutex;FmPlayer player;CeFmSynthRenderer synth;AudioController controller;PcmPercussionAssets assets;PcmPercussionOwner percussion;
 FmReplayOrigin historyOrigin;std::vector<uint8_t> sources[6],gainPublication;std::vector<FmHistoryEvent> history;std::deque<uint8_t> commands;FmPublicationQueue publication;uint8_t publicationVersion;bool historyOverflow;
 Audio::SoundHandle handle;bool pendingPercussion=false;bool attached,loaded,paused,mono,fmMono,driverSuspended,coupledOutput;uint8_t musicGain,effectGain,baseMusic,baseEffect;bool baseMono,baseFmMono,baseDriver;uint64_t frames,nonzero,drumEvents,remainder,updatesBase;uint32_t generation;std::string failure;std::vector<int16_t> pending,pendingFm;size_t offset;
 FmCore core;
 explicit Impl(Audio::Mixer& m,bool coupled,FmCore c):mixer(m),synth(44100,coupled,c),controller(synth.percussionBackend(),4,64),percussion(controller),publicationVersion(c==kFmGens?8:7),historyOverflow(false),attached(false),loaded(false),paused(false),mono(false),fmMono(false),driverSuspended(false),coupledOutput(coupled),musicGain(255),effectGain(255),baseMusic(255),baseEffect(255),baseMono(false),baseFmMono(false),baseDriver(false),frames(0),nonzero(0),drumEvents(0),remainder(0),updatesBase(0),generation(1),offset(0),core(c){}
 void remember(uint8_t kind,uint8_t a=0,uint8_t b=0){if(!loaded)return;if(historyOverflow)return;if(!fmHistoryEventsAllowed(4,history.size()+1)){historyOverflow=true;return;}history.push_back(FmHistoryEvent(frames,kind,a,b));}
 uint8_t fmGain(uint8_t logical)const{return gainPublication.empty()?logical:gainPublication[logical];}
 bool isStereo()const override{return true;}int getRate()const override{return 44100;}bool endOfData()const override{return false;}
 bool applyCommand(uint8_t command,std::string& error){bool accepted;if(!player.start(command,synth,accepted,error))return false;if(command==241){musicGain=effectGain=0;fmMono=false;if(publicationVersion>=5)publication.clear();}else if(command==243)fmMono=true;else if(command==244)fmMono=false;return true;}
 bool chunk(std::string& error){
  if(!driverSuspended){if(publicationVersion>=5){uint8_t command;if(publication.take(command)&&!applyCommand(command,error))return false;}else if(!commands.empty()){if(!applyCommand(commands.front(),error))return false;commands.pop_front();}}
  std::vector<FmDrumEvent> drums;if(!driverSuspended&&!player.tick(synth,drums,error))return false;
  for(const FmDrumEvent& event:drums){PcmPercussionRequest request;request.track=event.track;request.id=event.id;request.level5=event.level5;request.gain=musicGain;request.mono=mono;request.attenuate=effectGain==0x70;PreparedPcmEffect effect;bool sound=false;
   if(!assets.prepare(request,effect,sound,error))return false;if(sound){bool accepted;if(!percussion.start(request,effect,AudioOwner(event.id,event.track),generation,accepted,error))return false;if(accepted)++drumEvents;}}
  // Exact source sound update rate599227434/10000000Hz, never host FPS.
  remainder+=uint64_t(44100)*10000000;uint32_t count=uint32_t(remainder/599227434);remainder%=599227434;
  pendingPercussion=percussion.activeCount()!=0;if(!synth.renderPcmSeparated(count,pending,pendingFm,error))return false;offset=0;controller.poll();AudioCompletion done;while(controller.take(done))percussion.consume(done);return true;
 }
 bool pull(uint32_t samples,int16_t* out,bool obeyPause,std::string& error){
  if(!loaded){std::fill(out,out+samples,0);return true;}if(obeyPause&&paused){std::fill(out,out+samples,0);return true;}
  if(!failure.empty()){error=failure;return false;}
  for(uint32_t at=0;at<samples;){if(offset==pending.size()&&!chunk(error)){failure=error;return false;}size_t count=std::min(size_t(samples-at),pending.size()-offset);for(size_t i=0;i<count;++i){int16_t v=pending[offset+i];out[at+i]=v;if(((offset+i)&1)==0){++frames;if(v||pending[offset+i+1])++nonzero;}}offset+=count;at+=uint32_t(count);}
  return true;
 }
 int readBuffer(int16* buffer,const int samples) override {
  if(samples<=0)return 0;
  // CE's equal-rate copy converter still caches512 samples per input read.
  // Yield one owned stereo frame, so its cache cannot advance this semantic
  // owner beyond frames actually delivered to the master. Native735/736-frame
  // chunks remain in our own pending buffer and are reconstructed by replay.
  const int count=std::min(samples,2);Common::StackLock lock(mutex);std::string error;
  if(!pull(uint32_t(count),buffer,true,error))std::fill(buffer,buffer+count,0);return count;
 }
 void clear(){percussion.reset();controller.reset();std::string ignored;player.stop(synth,ignored);synth.reset(ignored);pending.clear();pendingFm.clear();pendingPercussion=false;offset=0;frames=nonzero=drumEvents=remainder=0;updatesBase=player.updates();++generation;failure.clear();history.clear();commands.clear();publication.clear();publicationVersion=core==kFmGens?8:7;player.setSourceResetPublication(true);historyOverflow=false;baseMusic=musicGain;baseEffect=effectGain;baseMono=mono;baseFmMono=fmMono;baseDriver=driverSuspended;historyOrigin=player.replayOrigin();}
};
CeFmAudioOwner::CeFmAudioOwner(Audio::Mixer& mixer,bool coupled,FmCore core):_impl(new Impl(mixer,coupled,core)),_preferredCore(core){}
CeFmAudioOwner::~CeFmAudioOwner(){if(_impl->attached)_impl->mixer.stopHandle(_impl->handle);{Common::StackLock lock(_impl->mutex);_impl->clear();}delete _impl;}
bool CeFmAudioOwner::load(const std::vector<uint8_t>& main,const std::vector<uint8_t>& fm,const std::vector<uint8_t>& index,const std::vector<uint8_t>& drums,std::string& error,const std::vector<uint8_t>* subcode,const std::vector<uint8_t>* fmAllocation){
 // A loaded save binds its historical synthesizer. Loading fresh assets starts
 // the configured backend again, without converting an active checkpoint.
 if(_impl->core!=_preferredCore){CeFmAudioOwner fresh(_impl->mixer,_impl->coupledOutput,_preferredCore);fresh.setPaused(_impl->paused);fresh.setGains(_impl->musicGain,_impl->effectGain);fresh.setMono(_impl->fmMono);fresh.setPercussionMono(_impl->mono);fresh.setDriverSuspended(_impl->driverSuspended);if(!fresh.load(main,fm,index,drums,error,subcode,fmAllocation))return false;std::swap(_impl,fresh._impl);return true;}
 if(fmAllocation&&!fmAllocationCompatible(fm,*fmAllocation,error))return false;const std::vector<uint8_t>& playback=fmAllocation?*fmAllocation:fm;
 Common::StackLock lock(_impl->mutex);_impl->clear();_impl->loaded=false;if(!_impl->synth.init(error)||!_impl->player.load(main.data(),main.size(),playback.data(),playback.size(),error)||!_impl->assets.load(index.data(),index.size(),drums.data(),drums.size(),error)){_impl->failure=error;return false;}std::vector<uint8_t> publication;if(subcode&&!readFmGainPublication(subcode->data(),subcode->size(),publication,error)){_impl->failure=error;return false;}_impl->gainPublication.swap(publication);_impl->sources[4]=subcode?*subcode:std::vector<uint8_t>();_impl->sources[0]=main;_impl->sources[1]=fm;_impl->sources[5]=playback;_impl->sources[2]=index;_impl->sources[3]=drums;_impl->loaded=true;_impl->updatesBase=_impl->player.updates();_impl->player.setGains(_impl->fmGain(_impl->musicGain),_impl->fmGain(_impl->effectGain));if(_impl->fmMono&&!_impl->player.start(243,_impl->synth,error)){_impl->failure=error;return false;}_impl->historyOrigin=_impl->player.replayOrigin();return true;
}
bool CeFmAudioOwner::start(uint16_t command,std::string& error){
 bool attach=false,paused=false;{Common::StackLock lock(_impl->mutex);error.clear();if(!_impl->loaded){error="Native FM assets are not loaded";return false;}if(!_impl->failure.empty()){error=_impl->failure;return false;}if(_impl->publicationVersion>=5){uint8_t id=uint8_t(command),priority=0;if(id>=50&&id<63){const auto& main=_impl->sources[0];size_t record=0x6142+size_t(id)*8+3;if(record>=main.size()){error="FM priority source table unavailable";return false;}priority=main[record];}bool admitted;if(!_impl->publication.post(id,priority,admitted,error))return false;}else if(_impl->driverSuspended||!_impl->commands.empty()){if(_impl->commands.size()>=31){error="Native FM suspended command ring capacity exhausted";return false;}_impl->commands.push_back(uint8_t(command));}else if(!_impl->applyCommand(uint8_t(command),error)){_impl->failure=error;return false;}_impl->remember(1,uint8_t(command));attach=!_impl->attached;paused=_impl->paused;}
 // MAIN mixer can call readBuffer while holding its own mutex: never acquire it
 // with our state mutex held. All main mixer lifecycle calls are main-thread only.
 if(attach){_impl->mixer.playStream(Audio::Mixer::kMusicSoundType,&_impl->handle,_impl,-1,255,0,DisposeAfterUse::NO,true,false);if(!_impl->mixer.isSoundHandleActive(_impl->handle)){error="Cannot attach native FM output stream";return false;}_impl->attached=true;if(paused)_impl->mixer.pauseHandle(_impl->handle,true);}return true;
}
void CeFmAudioOwner::setGains(uint8_t music,uint8_t effect){Common::StackLock lock(_impl->mutex);if(_impl->musicGain!=music||_impl->effectGain!=effect)_impl->remember(2,music,effect);_impl->musicGain=music;_impl->effectGain=effect;_impl->player.setGains(_impl->fmGain(music),_impl->fmGain(effect));}
void CeFmAudioOwner::setPercussionMono(bool value){Common::StackLock lock(_impl->mutex);if(_impl->mono!=value)_impl->remember(3,uint8_t(value));_impl->mono=value;}
void CeFmAudioOwner::setMono(bool value){Common::StackLock lock(_impl->mutex);_impl->remember(4,uint8_t(value));_impl->mono=_impl->fmMono=value;if(_impl->loaded){std::string error;if(!_impl->player.start(value?243:244,_impl->synth,error))_impl->failure=error;}}
void CeFmAudioOwner::setDriverSuspended(bool value){Common::StackLock lock(_impl->mutex);if(_impl->driverSuspended!=value)_impl->remember(5,uint8_t(value));_impl->driverSuspended=value;}
void CeFmAudioOwner::setPaused(bool value){bool changed;{Common::StackLock lock(_impl->mutex);changed=_impl->paused!=value;_impl->paused=value;}if(changed&&_impl->attached)_impl->mixer.pauseHandle(_impl->handle,value);}
void CeFmAudioOwner::reset(){if(_impl->attached){_impl->mixer.stopHandle(_impl->handle);_impl->attached=false;}Common::StackLock lock(_impl->mutex);_impl->clear();}
bool CeFmAudioOwner::silencePcm(std::string& error){
 Common::StackLock lock(_impl->mutex);error.clear();
 if(_impl->publicationVersion<6){if(_impl->percussion.activeCount()||(_impl->pendingPercussion&&_impl->offset<_impl->pending.size())){error="Legacy enhanced FM history has no independent pending PCM lane";return false;}return true;}
 if(_impl->pendingFm.size()!=_impl->pending.size()){error="PCM silence requires the owned FM output lane";return false;}
 _impl->percussion.silence();std::copy(_impl->pendingFm.begin()+_impl->offset,_impl->pendingFm.end(),_impl->pending.begin()+_impl->offset);_impl->pendingPercussion=false;_impl->remember(6);return true;
}
bool CeFmAudioOwner::pcmSamplesPending()const{Common::StackLock lock(_impl->mutex);if(_impl->publicationVersion<6||_impl->pendingFm.size()!=_impl->pending.size())return false;for(size_t i=_impl->offset;i<_impl->pending.size();++i)if(_impl->pending[i]!=_impl->pendingFm[i])return true;return false;}
bool CeFmAudioOwner::pcmOutputBusy()const{Common::StackLock lock(_impl->mutex);return _impl->percussion.activeCount()!=0||(_impl->pendingPercussion&&_impl->offset<_impl->pending.size());}
FmAudioSnapshot CeFmAudioOwner::snapshot()const {Common::StackLock lock(_impl->mutex);FmAudioSnapshot s;s.core=_impl->core;s.frames=_impl->frames;s.updates=_impl->player.updates()-_impl->updatesBase;s.nonzeroFrames=_impl->nonzero;s.percussionEvents=_impl->drumEvents;s.activePercussion=_impl->percussion.activeCount();s.queuedCommands=_impl->publicationVersion>=5?_impl->publication.size():_impl->commands.size();s.driverSuspended=_impl->driverSuspended;s.primaryCompletionSerial=_impl->player.primaryCompletionSerial();s.secondaryCompletionSerial=_impl->player.secondaryCompletionSerial();s.completionPublicationGeneration=_impl->player.completionPublicationGeneration();s.completionPublication=_impl->player.completionPublication();s.semanticDriverDigest=_impl->player.semanticDigest();s.activeVoiceMask=_impl->player.activeVoiceMask();s.keyOnMask=_impl->synth.keyOnMask();s.active=_impl->player.active();s.paused=_impl->paused;s.error=_impl->failure;return s;}
bool CeFmAudioOwner::renderOffline(uint32_t count,std::vector<int16_t>& pcm,std::string& error){error.clear();if(!count||count>1000000){error="FM offline render outside bounded native frame domain";return false;}if(_impl->attached)_impl->mixer.pauseHandle(_impl->handle,true);bool success;{Common::StackLock lock(_impl->mutex);std::vector<int16_t> next(size_t(count)*2);success=_impl->pull(count*2,next.data(),false,error);if(success)pcm.swap(next);}if(_impl->attached)_impl->mixer.pauseHandle(_impl->handle,false);return success;}
bool CeFmAudioOwner::canSnapshotState(std::string& error)const{error.clear();Common::StackLock lock(_impl->mutex);if(!_impl->failure.empty()){error=_impl->failure;return false;}if((_impl->offset&1)||_impl->historyOverflow||!fmHistoryFramesAllowed(4,_impl->frames)){error="Native music save history has exceeded its supported replay boundary";return false;}return true;}
bool CeFmAudioOwner::snapshotState(std::vector<uint8_t>& output,std::string& error)const{
 error.clear();Common::StackLock lock(_impl->mutex);if(!_impl->failure.empty()){error=_impl->failure;return false;}if((_impl->offset&1)||_impl->historyOverflow||!fmHistoryFramesAllowed(4,_impl->frames)){error="Native FM semantic replay exceeds save history boundary";return false;}
 std::vector<uint8_t> next={'F','M','A','H',_impl->publicationVersion,uint8_t(_impl->loaded),uint8_t(_impl->paused),uint8_t(_impl->baseDriver),_impl->baseMusic,_impl->baseEffect,uint8_t(_impl->baseMono),uint8_t(_impl->baseFmMono),uint8_t(_impl->coupledOutput)};putHistory(next,_impl->frames,8);
 for(unsigned i=0;i<6;++i){putHistory(next,_impl->sources[i].size(),4);putHistory(next,assetHash(_impl->sources[i]),8);}putHistory(next,_impl->history.size(),4);
 for(const FmHistoryEvent& event:_impl->history){putHistory(next,event.frame,8);next.push_back(event.kind);next.push_back(event.a);next.push_back(event.b);}
 const FmReplayOrigin& origin=_impl->historyOrigin;if(!origin.valid){error="Native FM history has no cleared semantic origin";return false;}
 putHistory(next,origin.version,4);putHistory(next,origin.updates,8);putHistory(next,origin.primary,8);putHistory(next,origin.secondary,8);putHistory(next,origin.publicationGeneration,8);next.push_back(origin.publication);next.push_back(origin.musicGain);next.push_back(origin.effectGain);next.push_back(origin.warmup);putHistory(next,origin.dirtyLevelMask,2);next.push_back(uint8_t(origin.loaded));next.push_back(uint8_t(origin.muted));next.push_back(uint8_t(origin.startupHold));next.push_back(uint8_t(origin.forceMono));
 std::vector<uint8_t> generator;if(!_impl->synth.snapshotGeneratorState(generator,error))return false;
 if(_impl->pending.size()>1472||_impl->offset>_impl->pending.size()){error="Native FM pending chunk exceeds bounded source update";return false;}
 putHistory(next,generator.size(),4);next.insert(next.end(),generator.begin(),generator.end());
 putHistory(next,_impl->nonzero,8);putHistory(next,_impl->drumEvents,8);putHistory(next,_impl->remainder,8);putHistory(next,_impl->pending.size(),2);putHistory(next,_impl->offset,2);for(int16_t sample:_impl->pending)putHistory(next,uint16_t(sample),2);
 if(_impl->publicationVersion>=6){if(_impl->pendingFm.size()!=_impl->pending.size()){error="FM lane has no complete pending owner";return false;}for(int16_t sample:_impl->pendingFm)putHistory(next,uint16_t(sample),2);}
 putHistory(next,assetHash(next),8);output.swap(next);return true;
}
bool CeFmAudioOwner::snapshotLegacyState(std::vector<uint8_t>& output,std::string& error)const{
 error.clear();Common::StackLock lock(_impl->mutex);if(!_impl->failure.empty()){error=_impl->failure;return false;}if(_impl->publicationVersion>=5){error="Legacy FM history cannot represent queued publication policy";return false;}if((_impl->offset&1)||_impl->historyOverflow||!fmHistoryEventsAllowed(2,_impl->history.size())||!fmHistoryFramesAllowed(2,_impl->frames)){error="Native FM semantic replay exceeds save history boundary";return false;}
 const FmReplayOrigin& origin=_impl->historyOrigin;if(!origin.valid||origin.updates||origin.primary||origin.secondary||origin.publicationGeneration||origin.publication||origin.dirtyLevelMask!=((_impl->fmGain(_impl->baseMusic)!=255||_impl->fmGain(_impl->baseEffect)!=255)?1023:0)||origin.forceMono!=_impl->baseFmMono||origin.musicGain!=_impl->fmGain(_impl->baseMusic)||origin.effectGain!=_impl->fmGain(_impl->baseEffect)){error="Legacy FM history cannot represent nonzero retained driver origin";return false;}
 std::vector<uint8_t> next={'F','M','A','H',2,uint8_t(_impl->loaded),uint8_t(_impl->paused),uint8_t(_impl->baseDriver),_impl->baseMusic,_impl->baseEffect,uint8_t(_impl->baseMono),uint8_t(_impl->baseFmMono),uint8_t(_impl->coupledOutput)};putHistory(next,_impl->frames,8);
 for(unsigned i=0;i<5;++i){putHistory(next,_impl->sources[i].size(),4);putHistory(next,assetHash(_impl->sources[i]),8);}putHistory(next,_impl->history.size(),4);
 for(const FmHistoryEvent& event:_impl->history){putHistory(next,event.frame,8);next.push_back(event.kind);next.push_back(event.a);next.push_back(event.b);}
 output.swap(next);return true;
}
bool CeFmAudioOwner::restoreState(const std::vector<uint8_t>& bytes,std::string& error,uint64_t primaryObserved,uint64_t secondaryObserved,uint64_t publicationObserved){
 error.clear();if(bytes.size()<72||bytes[0]!='F'||bytes[1]!='M'||bytes[2]!='A'||bytes[3]!='H'||(bytes[4]!=1&&bytes[4]!=2&&bytes[4]!=3&&bytes[4]!=4&&bytes[4]!=5&&bytes[4]!=6&&bytes[4]!=7&&bytes[4]!=8)||bytes[5]>1||bytes[6]>1||bytes[7]>1||bytes[10]>1||bytes[11]>1){error="Invalid native FM history header";return false;}
 const unsigned sourceCount=bytes[4]==1?4:(bytes[4]>=4?6:5),header=bytes[4]==1?12:13;if(bytes.size()<header+12+sourceCount*12){error="Truncated native FM source identities";return false;}if(bytes[4]>=2&&(bytes[12]>1||(bytes[12]!=0)!=_impl->coupledOutput)){error="Native FM output-domain owner mismatch";return false;}
 size_t cursor=header;const uint64_t frames=getHistory(bytes,cursor,8);if(!fmHistoryFramesAllowed(bytes[4],frames)){error="Native FM saved replay exceeds bounded frame domain";return false;}
 std::vector<uint8_t> source[6];{Common::StackLock lock(_impl->mutex);for(unsigned i=0;i<6;++i)source[i]=_impl->sources[i];for(unsigned i=0;i<sourceCount;++i){uint64_t length=getHistory(bytes,cursor,4),hash=getHistory(bytes,cursor,8);if(bytes[5]){if(length!=_impl->sources[i].size()||hash!=assetHash(_impl->sources[i])){error="Native FM source asset identity mismatch";return false;}source[i]=_impl->sources[i];}else{source[i].clear();if(length||hash!=assetHash(source[i])){error="Unloaded native FM state contains source assets";return false;}}}}
 const uint64_t count=getHistory(bytes,cursor,4);if(!fmHistoryEventsAllowed(bytes[4],count)||count>(bytes.size()-cursor)/11||(bytes[4]<3&&(count!=(bytes.size()-cursor)/11||(bytes.size()-cursor)%11))){error="Invalid native FM history extent";return false;}
 std::vector<FmHistoryEvent> events;uint64_t previous=0;for(uint64_t i=0;i<count;++i){uint64_t frame=getHistory(bytes,cursor,8);uint8_t kind=bytes[cursor++],a=bytes[cursor++],b=bytes[cursor++];if(frame<previous||frame>frames||kind<1||kind>(bytes[4]>=6?6:5)||(kind>=3&&a>1)||(kind==6&&a)||(kind!=2&&b)){error="Invalid native FM semantic history event";return false;}events.push_back(FmHistoryEvent(frame,kind,a,b));previous=frame;}
 if(!bytes[5]&&(frames||count)){error="Unloaded native FM state owns generated audio";return false;}
 FmReplayOrigin savedOrigin;std::vector<uint8_t> generatorState;std::vector<int16_t> savedPending,savedFm;uint64_t savedNonzero=0,savedDrums=0,savedRemainder=0;size_t savedOffset=0;
 if(bytes[4]>=3){
  if(cursor>bytes.size()||bytes.size()-cursor<50){error="Truncated native FM origin/checkpoint";return false;}
  savedOrigin.version=uint32_t(getHistory(bytes,cursor,4));savedOrigin.updates=getHistory(bytes,cursor,8);savedOrigin.primary=getHistory(bytes,cursor,8);savedOrigin.secondary=getHistory(bytes,cursor,8);savedOrigin.publicationGeneration=getHistory(bytes,cursor,8);savedOrigin.publication=bytes[cursor++];savedOrigin.musicGain=bytes[cursor++];savedOrigin.effectGain=bytes[cursor++];savedOrigin.warmup=bytes[cursor++];savedOrigin.dirtyLevelMask=uint16_t(getHistory(bytes,cursor,2));uint8_t loaded=bytes[cursor++],muted=bytes[cursor++],hold=bytes[cursor++],mono=bytes[cursor++];if(loaded>1||muted>1||hold>1||mono>1||bool(loaded)!=bool(bytes[5])){error="Invalid native FM origin flags";return false;}savedOrigin.loaded=loaded!=0;savedOrigin.muted=muted!=0;savedOrigin.startupHold=hold!=0;savedOrigin.forceMono=mono!=0;
  uint64_t length=getHistory(bytes,cursor,4);
  if(length<1200||length>20000||length>bytes.size()-cursor||bytes.size()-cursor-length<36){error="Invalid native FM checkpoint extent";return false;}generatorState.assign(bytes.begin()+cursor,bytes.begin()+cursor+size_t(length));cursor+=size_t(length);
  savedNonzero=getHistory(bytes,cursor,8);savedDrums=getHistory(bytes,cursor,8);savedRemainder=getHistory(bytes,cursor,8);uint64_t samples=getHistory(bytes,cursor,2);savedOffset=size_t(getHistory(bytes,cursor,2));
  if(samples>1472||(samples&1)||(savedOffset&1)||savedOffset>samples||samples>(bytes.size()-cursor)/2||bytes.size()-cursor!=samples*(bytes[4]>=6?4:2)+8||savedNonzero>frames||savedRemainder>=599227434){error="Invalid native FM pending output state";return false;}
  for(uint64_t i=0;i<samples;++i){uint16_t v=uint16_t(getHistory(bytes,cursor,2));savedPending.push_back(v<32768?int16_t(v):int16_t(int(v)-65536));}if(bytes[4]>=6)for(uint64_t i=0;i<samples;++i){uint16_t v=uint16_t(getHistory(bytes,cursor,2));savedFm.push_back(v<32768?int16_t(v):int16_t(int(v)-65536));}size_t checksumAt=cursor;uint64_t checksum=getHistory(bytes,cursor,8);std::vector<uint8_t> covered(bytes.begin(),bytes.begin()+checksumAt);if(checksum!=assetHash(covered)){error="Native FM checkpoint integrity mismatch";return false;}
 }

 CeFmAudioOwner prepared(_impl->mixer,_impl->coupledOutput,bytes[4]==8?kFmGens:kFmNuked);prepared.setPaused(true);prepared.setGains(bytes[8],bytes[9]);prepared.setMono(bytes[11]!=0);prepared.setPercussionMono(bytes[10]!=0);prepared.setDriverSuspended(bytes[7]!=0);
 if(bytes[5]&&!prepared.load(source[0],source[1],source[2],source[3],error,source[4].empty()?nullptr:&source[4],source[5]==source[1]?nullptr:&source[5]))return false;
 prepared._impl->publicationVersion=bytes[4]>=5?bytes[4]:4;prepared._impl->player.setSourceResetPublication(bytes[4]>=7);
 if(bytes[4]>=3){if(!prepared._impl->player.restoreReplayOrigin(savedOrigin,error))return false;prepared._impl->updatesBase=savedOrigin.updates;prepared._impl->historyOrigin=savedOrigin;prepared._impl->synth.setReplayDiscard(true);}
 auto advance=[&](uint64_t target){while(prepared._impl->frames<target){uint32_t n=uint32_t(std::min(uint64_t(4096),target-prepared._impl->frames));std::vector<int16_t> ignored;if(!prepared.renderOffline(n,ignored,error))return false;}return true;};
 for(const FmHistoryEvent& event:events){if(!advance(event.frame))return false;if(event.kind==1){if(!prepared.start(event.a,error))return false;}else if(event.kind==2)prepared.setGains(event.a,event.b);else if(event.kind==3)prepared.setPercussionMono(event.a!=0);else if(event.kind==4)prepared.setMono(event.a!=0);else if(event.kind==5)prepared.setDriverSuspended(event.a!=0);else if(!prepared.silencePcm(error))return false;}
 if(!advance(frames))return false;
 if(bytes[4]>=3){
  if(prepared._impl->drumEvents!=savedDrums||prepared._impl->remainder!=savedRemainder||prepared._impl->pending.size()!=savedPending.size()||prepared._impl->offset!=savedOffset){error="Native FM checkpoint disagrees with semantic replay";return false;}
  if(!prepared._impl->synth.restoreGeneratorState(generatorState,error))return false;
  prepared._impl->nonzero=savedNonzero;prepared._impl->pending.swap(savedPending);if(bytes[4]>=6)prepared._impl->pendingFm.swap(savedFm);
 }
 {Common::StackLock lock(prepared._impl->mutex);if(primaryObserved>prepared._impl->player.primaryCompletionSerial()||secondaryObserved>prepared._impl->player.secondaryCompletionSerial()||publicationObserved>prepared._impl->player.completionPublicationGeneration()){error="Native FM completion cursor exceeds replayed source history";return false;}prepared._impl->history=events;prepared._impl->historyOverflow=false;}
 std::swap(_impl,prepared._impl);if(prepared._impl->attached){prepared._impl->mixer.stopHandle(prepared._impl->handle);prepared._impl->attached=false;}setPaused(bytes[6]!=0);return true;
}

}
