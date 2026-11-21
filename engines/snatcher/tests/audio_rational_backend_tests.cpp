/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../audio_runtime.h"
#include "audio/audiostream.h"
#include "audio/mixer.h"
#include "audio/decoders/raw.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL exact CE PCM adapter: %s\n",why);std::exit(1);}}
class Mixer:public Audio::Mixer {
public:
 Audio::AudioStream* stream=nullptr;bool paused=false;SoundType type=kPlainSoundType;byte volume=0;unsigned stops=0;mutable bool drainOnActive=false;
 ~Mixer(){stopAll();}
 bool isReady()const override{return true;}
 Common::Mutex& mutex()override{std::abort();}
 void playStream(SoundType kind,Audio::SoundHandle*,Audio::AudioStream* data,int,byte level,int8,DisposeAfterUse::Flag owned,bool,bool)override{check(!stream&&owned==DisposeAfterUse::YES,"CE owns one disposable source");stream=data;type=kind;volume=level;}
 void stopAll()override{delete stream;stream=nullptr;}
 void stopID(int)override{stopAll();}
 void stopHandle(Audio::SoundHandle)override{++stops;stopAll();}
 void pauseAll(bool value)override{paused=value;}
 void pauseID(int,bool value)override{paused=value;}
 void pauseHandle(Audio::SoundHandle,bool value)override{paused=value;}
 bool isSoundIDActive(int)const override{return stream&&!stream->endOfStream();}
 int getSoundID(Audio::SoundHandle)const override{return -1;}
 bool isSoundHandleActive(Audio::SoundHandle)const override{
  if(drainOnActive){drainOnActive=false;int16_t samples[64];unsigned calls=0;
   while(stream&&!stream->endOfStream()){check(++calls<4096,"bounded completion interleaving drain");stream->readBuffer(samples,64);}
  }
  return stream&&!stream->endOfStream();
 }
 void muteSoundType(SoundType,bool)override{}
 bool isSoundTypeMuted(SoundType)const override{return false;}
 void setChannelVolume(Audio::SoundHandle,byte)override{}
 byte getChannelVolume(Audio::SoundHandle)const override{return volume;}
 void setChannelBalance(Audio::SoundHandle,int8)override{}
 int8 getChannelBalance(Audio::SoundHandle)const override{return 0;}
 void setChannelFaderL(Audio::SoundHandle,uint8)override{}
 uint8 getChannelFaderL(Audio::SoundHandle)const override{return 255;}
 void setChannelFaderR(Audio::SoundHandle,uint8)override{}
 uint8 getChannelFaderR(Audio::SoundHandle)const override{return 255;}
 void setChannelRate(Audio::SoundHandle,uint32)override{}
 uint32 getChannelRate(Audio::SoundHandle)const override{return getOutputRate();}
 void resetChannelRate(Audio::SoundHandle)override{}
 uint32 getSoundElapsedTime(Audio::SoundHandle)const override{return 0;}
 Audio::Timestamp getElapsedTime(Audio::SoundHandle)const override{std::abort();}
 void loopChannel(Audio::SoundHandle)override{std::abort();}
 bool hasActiveChannelOfType(SoundType kind)const override{return stream&&type==kind&&!stream->endOfStream();}
 void setVolumeForSoundType(SoundType,int)override{}
 int getVolumeForSoundType(SoundType)const override{return 256;}
 uint getOutputRate()const override{return 44100;}
 bool getOutputStereo()const override{return true;}
 uint getOutputBufSize()const override{return 256;}
 uint getOutputBytesPerSample()const override{return 2;}
 bool getClamping()const override{return true;}
};
struct Feed:Snatcher::NativeAudioFeed {
 size_t at=0,released=0;std::vector<int16_t> values;explicit Feed(const std::vector<int16_t>& pcm):released(pcm.size()/2),values(pcm){}
 uint32_t sourceRate()const override{return 16276;}
 size_t readFrames(int16_t* out,size_t frames)override{size_t n=std::min(frames,released-at);for(size_t i=0;i<n*2;++i)out[i]=values[at*2+i];at+=n;return n;}
 bool ended()const override{return at==values.size()/2;}
 std::string error()const override{return std::string();}
};
std::vector<int16_t> drain(Audio::AudioStream& stream,unsigned chunk){std::vector<int16_t> pcm;std::vector<int16_t> buffer(chunk*2);unsigned calls=0;while(!stream.endOfStream()){check(++calls<2000000,"bounded actual CE source drain");int n=stream.readBuffer(buffer.data(),int(buffer.size()));check(n>=0&&!(n&1),"actual CE source emits complete frames");pcm.insert(pcm.end(),buffer.begin(),buffer.begin()+n);}return pcm;}
}
// The unchanged nominal raw-stream branch is deliberately outside this test.
// Exact profiles must not invoke it; compile the actual production adapter.
namespace Audio {SeekableAudioStream* makeRawStream(const byte*,uint32,int,byte,DisposeAfterUse::Flag){std::abort();}}
int streamAdmissionAdapter(){using namespace Snatcher;Mixer mixer;CeAudioBackend backend(mixer);std::string error;AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner stream(controller,activity);PcmEffectOwner effects(controller,activity,&stream);PreparedPcmStream prepared;prepared.source.id=36;prepared.source.mode=2;prepared.source.priority=4;prepared.sample.sampleCount=4096;prepared.playbackFrames=4096;prepared.droppedSourceFrames=0;prepared.profile=sourcePcmStreamProfile();prepared.pcm.resize(8192);for(size_t i=0;i<prepared.pcm.size();++i)prepared.pcm[i]=int16_t(i*7-28000);
 uint64_t handle;check(backend.play(prepared.pcm,prepared.profile,kAudioSpeech,handle,error),"reference native exact transport");auto expected=drain(*mixer.stream,257);backend.stop(handle);AudioToken token;check(stream.start(prepared,AudioOwner(3,1),23,token,error),"actual CE stream with catalog priority");int16_t prefix[86];int n=mixer.stream->readBuffer(prefix,86);check(n>0&&!(n&1),"real stream has delivered a prefix");std::vector<int16_t> actual(prefix,prefix+n);auto* original=mixer.stream;unsigned stops=mixer.stops;
 PreparedPcmEffect rejected;rejected.source.id=1;rejected.source.priority=3;rejected.source.channel=0;rejected.source.source=kDrumSource;AudioToken effectToken;bool accepted=true;check(effects.start(rejected,0,AudioOwner(3,2),23,accepted,effectToken,error)&&!accepted&&!effectToken.id&&mixer.stream==original&&mixer.stops==stops&&stream.active(),"priority rejection preserves actual CE sample source and token");auto suffix=drain(*mixer.stream,31);actual.insert(actual.end(),suffix.begin(),suffix.end());check(actual==expected,"every output sample survives rejected overlap unchanged");AudioCompletion completion;controller.poll();check(controller.take(completion)&&completion.result==kAudioFinished&&stream.consume(completion)&&!stream.active()&&!stream.priority(),"natural matching endpoint retires priority");bool admitted;check(effects.preflight(rejected.source,admitted,error)&&admitted,"endpoint changes admission through actual owner");std::printf("CE stream admission: %u checks,0 failures; full future PCM unchanged\n",checks);return 0;}
int publicationGateAdapter(){using namespace Snatcher;Mixer mixer;CeAudioBackend backend(mixer);std::string error;PreparedPcmStream prepared;prepared.source.id=36;prepared.source.mode=2;prepared.source.priority=4;prepared.sample.sampleCount=4096;prepared.playbackFrames=4096;prepared.droppedSourceFrames=0;prepared.profile=sourcePcmStreamProfile();prepared.pcm.resize(8192);for(size_t i=0;i<prepared.pcm.size();++i)prepared.pcm[i]=int16_t(int(i%4000)*11-22000);
 uint64_t handle;check(backend.play(prepared.pcm,prepared.profile,kAudioSpeech,handle,error),"independent actual copied transport");auto expected=drain(*mixer.stream,73);backend.stop(handle);auto stage=std::make_shared<ResidentPcmStaging>(106496);auto feed=std::make_shared<PcmStreamFeed>();std::vector<uint8_t> bytes(8192,128);check(feed->start(stage,36,bytes.data(),bytes.size(),prepared.pcm,4096,prepared.profile.rate,error)&&backend.playFeed(feed,prepared.profile,kAudioSpeech,handle,error),"actual exact feed acquired before transfer publication");int16_t out[64];for(unsigned n=0;n<3;++n)check(mixer.stream->readBuffer(out,64)==0&&!mixer.stream->endOfStream()&&backend.active(handle)&&feed->snapshot().consumedFrames==0&&stage->knownBytes()==0,"unpublished CE feed is a stall without output, completion or source progress");check(feed->publishInitial(error),"actual initial transfer publishes");auto actual=drain(*mixer.stream,31);check(actual==expected&&feed->snapshot().consumedFrames==4096&&feed->ended()&&!backend.active(handle),"all future rational PCM and natural endpoint equal without inserted silence");backend.stop(handle);std::printf("CE PCM publication gate: %u checks,0 failures; complete future PCM identical\n",checks);return 0;}
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="--publication-gate")return publicationGateAdapter();
 if(argc==2&&std::string(argv[1])=="--stream-admission")return streamAdmissionAdapter();
 if(argc!=1)return 2;
 {
  using namespace Snatcher;Mixer completionMixer;CeAudioBackend completionBackend(completionMixer);AudioController controller(completionBackend);
  auto source=std::make_shared<Feed>(std::vector<int16_t>(32,1000));AudioToken token;std::string error;
  check(controller.startFeed(source,sourcePcmStreamProfile(),kAudioSpeech,AudioOwner(63,3482),7,token,error),"actual CE rational source owns completion interleaving");
  check(!source->ended(),"source remains live before active query");completionMixer.drainOnActive=true;
  check(controller.feedError().empty()&&source->ended(),"actual CE adapter drains final source and rational tail during active query without a false early-end error");
  AudioCompletion completion;check(!controller.take(completion),"actual adapter health query does not publish a callback");
  controller.poll();check(controller.take(completion)&&completion.token.id==token.id&&completion.result==kAudioFinished&&!controller.take(completion),"actual adapter drain publishes exactly one normal completion");
 }
 using namespace Snatcher;Mixer mixer;CeAudioBackend backend(mixer);std::string error;uint64_t handle=0;
 std::vector<int16_t> pcm(2000);for(unsigned i=0;i<pcm.size();++i)pcm[i]=int16_t(i*31-30000);
 AudioProfile profile=sourcePcmStreamProfile();check(backend.play(pcm,profile,kAudioSpeech,handle,error)&&handle&&backend.active(handle),"actual adapter accepts exact copied speech");
 check(mixer.stream&&mixer.stream->getRate()==44100&&mixer.stream->isStereo()&&mixer.type==Audio::Mixer::kSpeechSoundType&&mixer.volume==255,"exact transport bypasses integer source-rate conversion with unchanged role and gain");
 backend.pause(handle,true);check(mixer.paused,"actual pause reaches mixer");backend.pause(handle,false);check(!mixer.paused,"actual resume reaches mixer");
 auto copied=drain(*mixer.stream,257);check(!backend.active(handle),"copied endpoint follows exact source drain");
 std::vector<uint8_t> endedSave;check(backend.snapshotPlayback(handle,endedSave,error),"drained unpolled playback snapshots");
 uint64_t ignored=0;check(backend.restorePlayback(nullptr,endedSave,kAudioSpeech,ignored,error,true),"drained memory source preflights without restarting");
 backend.stop(handle);check(!mixer.stream&&mixer.stops==1,"copied stop clears actual handle");
 check(backend.restorePlayback(nullptr,endedSave,kAudioSpeech,handle,error)&&!backend.active(handle)&&mixer.stream->endOfStream(),"drained playback restores natural completion");backend.stop(handle);
 uint64_t expected=0;check(resampledFrameCount(1000,profile.exactRate,44100,expected,error)&&copied.size()==expected*2,"actual CE adapter exact finite duration");
 std::shared_ptr<Feed> feed(new Feed(pcm));feed->released=0;check(backend.playFeed(feed,profile,kAudioEffect,handle,error),"actual adapter accepts owned prefetched source");
 int16_t buffer[8]={};check(!mixer.stream->readBuffer(buffer,8)&&!mixer.stream->endOfStream()&&backend.active(handle),"temporary feed starvation retains actual CE handle");feed->released=pcm.size()/2;
 auto streamed=drain(*mixer.stream,7);check(streamed==copied,"copied and prefetched production paths have identical exact transport");check(mixer.type==Audio::Mixer::kSFXSoundType&&!backend.active(handle),"effect role and genuine endpoint retained");backend.stop(handle);
 {
  const size_t encoded=65537;std::vector<uint8_t> bytes(encoded+4,255);for(size_t i=0;i<encoded;++i)bytes[i]=uint8_t(128+(i*13+1)%127);
  PcmCatalogEntry entry;entry.id=36;entry.mode=0x82;entry.offset=0;entry.length=bytes.size();entry.level=64;entry.pan=0xf7;PreparedPcmStream prepared;
  check(preparePcmStream(entry,bytes.data(),bytes.size(),sourcePcmStreamProfile(),false,prepared,error)&&prepared.playbackFrames==65535&&prepared.droppedSourceFrames==2&&prepared.sample.sampleCount==encoded,"real prepared speech retains source extent and skips both32KiB replacement codes");
  AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner owner(controller,activity);AudioToken token;AudioCompletion completed;
  check(owner.start(prepared,AudioOwner(63,2),19,token,error)&&owner.active(),"actual stream lifecycle acquires shortened exact PCM");
  controller.poll();uint8_t bits=0;check(!controller.take(completed)&&owner.active()&&activity.snapshot(bits,error)&&(bits&2),"source stream cannot complete before actual backend drain");
  auto actual=drain(*mixer.stream,257);uint64_t sourceCount=0,playedCount=0;
  check(resampledFrameCount(encoded,profile.exactRate,44100,sourceCount,error)&&resampledFrameCount(prepared.playbackFrames,profile.exactRate,44100,playedCount,error)&&actual.size()==playedCount*2&&playedCount<sourceCount,"exact duration derives from played codes rather than encoded payload extent");
  check(owner.active()&&!controller.take(completed),"backend source end alone does not fabricate narrative completion");controller.poll();
  check(controller.take(completed)&&completed.token.id==token.id&&completed.token.generation==token.generation&&completed.result==kAudioFinished&&owner.consume(completed)&&!owner.active()&&!controller.take(completed)&&activity.snapshot(bits,error)&&!bits,"one real matching completion clears stream activity after shortened playback drains");
 }
 {
  const size_t encoded=220001;std::vector<uint8_t> bytes(108*2048,128);for(size_t i=0;i<encoded;++i)bytes[i]=uint8_t(128+(i*17+1)%127);for(size_t i=encoded;i<encoded+4;++i)bytes[i]=255;
  PcmCatalogEntry entry;entry.id=486;entry.mode=2;entry.offset=0;entry.length=bytes.size();entry.level=97;entry.pan=0xf7;PreparedPcmStream prepared;
  check(preparePcmStream(entry,bytes.data(),bytes.size(),sourcePcmStreamProfile(),false,prepared,error),"long source-valid tracked adapter fixture prepares");
  check(backend.play(prepared.pcm,prepared.profile,kAudioSpeech,handle,error),"independent copied transport provides changed-feed sample contract");auto expectedPcm=drain(*mixer.stream,257);backend.stop(handle);
  auto stage=std::make_shared<ResidentPcmStaging>(106496);auto source=std::make_shared<PcmStreamFeed>();check(source->start(stage,entry.id,bytes.data(),bytes.size(),prepared.pcm,encoded,prepared.profile.rate,error),"actual tracked feed prepares original source");
  AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();MusicControl music;PcmStreamOwner owner(controller,activity,&music);AudioToken token;AudioCompletion completion;
  check(owner.start(prepared,AudioOwner(63,3),21,token,error,source),"actual CE speech owner commits initial transfer after mixer acquisition");
  std::vector<int16_t> actual;int16_t block[514];for(unsigned calls=0;mixer.stream&&!mixer.stream->endOfStream()&&calls<expectedPcm.size()/2+1;++calls){check(owner.serviceRefills(error),"real worker publishes tracked refill before actual adapter pull");int count=mixer.stream->readBuffer(block,514);check(count>=0&&!(count&1),"actual adapter retains complete rational frames");actual.insert(actual.end(),block,block+count);}
  check(actual==expectedPcm&&source->snapshot().publishedSourceBytes==bytes.size(),"tracked request-service delivery preserves every exact rational output sample");
  controller.poll();check(controller.take(completion)&&completion.result==kAudioFinished&&completion.token.id==token.id&&owner.consume(completion)&&!owner.active()&&!controller.take(completion),"actual tracked stream produces one matching natural endpoint");
  source=std::make_shared<PcmStreamFeed>();check(source->start(stage,entry.id,bytes.data(),bytes.size(),prepared.pcm,encoded,prepared.profile.rate,error)&&owner.start(prepared,AudioOwner(63,4),21,token,error,source),"actual cancellation fixture acquires new stream generation");
  for(unsigned calls=0;!source->snapshot().pendingBytes&&calls<expectedPcm.size()/2+1;++calls){check(owner.serviceRefills(error),"advance actual source to pending refill");mixer.stream->readBuffer(block,514);}
  check(source->snapshot().pendingBytes&&owner.cancel(error)&&!mixer.stream&&!owner.active(),"explicit cancellation stops actual CE adapter with pending source request");
  auto frozen=source->snapshot();check(!frozen.pendingBytes&&frozen.cancelled&&frozen.publishedSourceBytes==65536,"actual cancellation drops uncompleted transfer without invented allocation publication");
  controller.poll();check(controller.take(completion)&&completion.result==kAudioCancelled&&!owner.consume(completion)&&!controller.take(completion),"cancelled generation completion cannot finish a future owner");
 }
 profile.exactRate=RationalAudioRate(16000,1);check(!backend.play(pcm,profile,kAudioSpeech,handle,error)&&!handle&&!mixer.stream,"conflicting exact metadata rejects before source publication");
 std::printf("exact CE PCM adapter: %u checks,0 failures\n",checks);return 0;
}
