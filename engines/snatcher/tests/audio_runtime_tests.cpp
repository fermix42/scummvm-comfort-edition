/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../audio_runtime.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
namespace {
int checks=0,failures=0;
void check(bool ok,const char *s){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL audio runtime: %s\n",s);}}
void word(std::vector<uint8_t>& d,size_t p,uint16_t v){d[p]=uint8_t(v>>8);d[p+1]=uint8_t(v);}
void longword(std::vector<uint8_t>& d,size_t p,uint32_t v){word(d,p,uint16_t(v>>16));word(d,p+2,uint16_t(v));}
Snatcher::AudioProfile profile(){Snatcher::AudioProfile p;p.rateVerified=p.polarityVerified=p.gainVerified=p.loopVerified=true;p.rate=12000;p.channels=1;p.volume=200;return p;}
struct Fake:Snatcher::AudioBackend {
 uint64_t next;unsigned plays,stops,pauses;bool failPlay;std::map<uint64_t,bool> alive;
 Fake():next(1),plays(0),stops(0),pauses(0),failPlay(false){}
 bool play(const std::vector<int16_t>&,const Snatcher::AudioProfile&,Snatcher::AudioRole,uint64_t& h,std::string& e)override{if(failPlay){e="fixture failure";return false;}h=next++;alive[h]=true;++plays;return true;}
 bool playFeed(const std::shared_ptr<Snatcher::NativeAudioFeed>&,const Snatcher::AudioProfile& p,Snatcher::AudioRole role,uint64_t& h,std::string& e)override{return play({},p,role,h,e);}
 void stop(uint64_t h)override{++stops;alive.erase(h);}
 void pause(uint64_t,bool)override{++pauses;}
 bool active(uint64_t h)const override{std::map<uint64_t,bool>::const_iterator i=alive.find(h);return i!=alive.end()&&i->second;}
};
}
namespace {struct Feed:Snatcher::NativeAudioFeed {bool finished=false;std::string failure;uint32_t sourceRate()const override{return 12000;}size_t readFrames(int16_t*,size_t)override{return 0;}bool ended()const override{return finished;}std::string error()const override{return failure;}};}
int runAudioRuntimeTests(){
 using namespace Snatcher;checks=failures=0;std::string e;std::vector<uint8_t> index(0x4030,0);index[0x4000]=3;
 {
  // Deterministically schedule the mixer's final source read between the
  // controller's separate observations, without a sleep or a racing test.
  struct FinishingBackend:Fake {
   std::shared_ptr<Feed> source;mutable bool finishOnQuery=false,drained=false;
   bool active(uint64_t h)const override{if(finishOnQuery){source->finished=true;drained=true;finishOnQuery=false;}return !drained&&Fake::active(h);}
  } backend;
  AudioController controller(backend);backend.source=std::make_shared<Feed>();
  AudioProfile format=profile();format.channels=2;format.volume=255;AudioToken token;
  check(controller.startFeed(backend.source,format,kAudioSpeech,AudioOwner(63,3482),7,token,e),"completion interleaving owns an actual controller feed");
  check(controller.feedError().empty(),"live source and backend remain healthy");
  backend.finishOnQuery=true;
  check(controller.feedError().empty(),"mixer marker consumption between observations is normal completion, not premature backend termination");
  AudioCompletion completion;check(!controller.take(completion),"health query does not publish completion");
  controller.poll();check(controller.take(completion)&&completion.token.id==token.id&&completion.result==kAudioFinished&&!controller.take(completion),"interleaved marker and drain publish exactly one normal completion");
 }
 index[0x4010]=4;word(index,0x4014,1024);longword(index,0x4016,0x55000);
 index[0x4020]=8;index[0x4021]=1;word(index,0x4024,1312);longword(index,0x4026,0x65020);
 std::vector<PcmEffect> effects;check(parsePcmEffects(index.data(),index.size(),effects,e)&&effects.size()==2,"effect directory bounded parse");
 const PcmEffect validResidentEffect=effects.at(1);
 check(effects[0].source==kDrumSource&&effects[0].headerOffset==0&&effects[1].source==kResidentSource&&effects[1].headerOffset==32,"effect source normalized");
 std::vector<PcmSample> drums(1),resident(1);drums[0]=PcmSample();resident[0]=PcmSample();drums[0].headerOffset=0;resident[0].headerOffset=32;PcmSample selected;
 check(resolvePcmEffect(effects[0],drums,resident,selected,e)&&selected.headerOffset==0,"fixed drum source");
 check(resolvePcmEffect(effects[1],drums,resident,selected,e)&&selected.headerOffset==32,"resident bank source");
 resident[0].headerOffset=33;check(!resolvePcmEffect(effects[1],drums,resident,selected,e)&&!e.empty(),"resident bank mismatch rejects");
 index[0x402a]=1;check(!parsePcmEffects(index.data(),index.size(),effects,e)&&effects.empty(),"unknown extension rejects");index[0x402a]=0;
 check(!parsePcmEffects(index.data(),index.size()-1,effects,e),"truncated effect directory");index[0x4021]=2;check(!parsePcmEffects(index.data(),index.size(),effects,e),"unsupported channel");index[0x4021]=1;
 word(index,0x4024,0);check(!parsePcmEffects(index.data(),index.size(),effects,e),"zero pitch-step unsupported");word(index,0x4024,1024);
 longword(index,0x4026,0x1000);check(!parsePcmEffects(index.data(),index.size(),effects,e),"out-of-domain reference");
 std::vector<PcmCatalogEntry> catalog(3);for(unsigned i=0;i<3;++i){catalog[i]=PcmCatalogEntry();catalog[i].id=i+1;}catalog[0].mode=1;catalog[1].mode=2;catalog[2].mode=0x82;PcmRequest request;
 check(planPcmRequest(catalog,1,request,e)&&request.kind==kLoadResidentBank,"resident load distinct from playback");check(planPcmRequest(catalog,2,request,e)&&request.kind==kStartStream,"stream2 request");check(planPcmRequest(catalog,3,request,e)&&request.kind==kStartStream,"stream82 request");check(!planPcmRequest(catalog,0,request,e),"invalid catalog request");
 check(planFmControl(0,true).kind==kNoFmRequest,"zero FM command is verified no request");check(planFmControl(0xffff,true).kind==kRequestFmFade,"FF FM fade request");check(planFmControl(0x1234,true).command==0x34&&planFmControl(0x1234,true).resetFade,"FM command uses low byte and reset flag");check(!planFmControl(0xf1,true).resetFade&&!planFmControl(1,false).resetFade,"FM control-range preserves fade state");
 {std::vector<uint8_t> sub(0x15fba-0xd400+256),expected(256),mapped;for(unsigned i=0;i<256;++i)sub[0x15fba-0xd400+i]=expected[i]=uint8_t(i*i/255);check(readFmGainPublication(sub.data(),sub.size(),mapped,e)&&mapped==expected&&mapped[112]!=112&&mapped[255]==255,"FM gain publication is source-owned nonlinear mapping, independent from PCM logical gain");auto saved=mapped;check(!readFmGainPublication(sub.data(),sub.size()-1,mapped,e)&&mapped==saved,"truncated publication table rejects transactionally");}
 AudioActivity activity;uint8_t activityBits=255;
 check(!activity.snapshot(activityBits,e)&&activityBits==0,"unknown audio owners do not imply silence");
 activity.beginResidentTransfer();activity.completeResidentTransfer();check(activity.knownMask()==4&&!activity.snapshot(activityBits,e),"resident completion clears only its own flag");
 activity.clearForSoundReset();check(activity.snapshot(activityBits,e)&&activityBits==0,"verified sound reset clears all four activity bits");
 activity.forgetFlag(3);check(activity.snapshot(activityBits,e,7)&&activityBits==0&&!activity.snapshot(activityBits,e),"independent low-three-bit owner context survives unknown effect completion");activity.clearForSoundReset();
 activity.setKnownFlag(0,true);activity.beginResidentTransfer();activity.completeResidentTransfer();check(activity.snapshot(activityBits,e)&&activityBits==1,"resident load preserves unrelated streaming activity");
 AudioActivityWait wait;check(wait.tick(activity,e)==kAudioWaitPending&&wait.remaining()==3599,"audio wait arms and decrements on first tick");
 activity.setKnownFlag(0,false);check(wait.tick(activity,e)==kAudioWaitPending&&wait.tick(activity,e)==kAudioWaitFinished,"quiet audio advances on following coordinator tick");
 check(wait.tick(activity,e)==kAudioWaitFinished,"finished audio wait is stable");
 AudioActivityWait timeout;activity.setKnownFlag(3,true);bool allPending=true;for(unsigned i=0;i<3600;++i)allPending&=timeout.tick(activity,e)==kAudioWaitPending;
 check(allPending&&timeout.remaining()==0&&timeout.tick(activity,e)==kAudioWaitFinished,"busy audio timeout expires exactly3600ticks then advances");
 AudioActivity unknownActivity;AudioActivityWait unresolved;check(unresolved.tick(unknownActivity,e)==kAudioWaitUnknown&&!e.empty(),"unknown audio wait reports exact unresolved ownership");
 MusicControl music;MusicControlPacket packet;
 check(music.gain()==255&&!music.fading()&&!music.pending(),"music initialization from sound reset");
 check(music.tick(false,packet)&&packet.command==0&&packet.musicGain==255&&packet.effectGain==255,"initial logical gain notification");
 check(!music.tick(false,packet),"unchanged music control sends nothing");
 check(music.submit(0,true,e)&&!music.pending(),"music zero command ignored");
 check(music.submit(0x1234,true,e)&&music.submit(0xf1,true,e)&&music.pending()==2,"music commands normalize and queue");
 check(music.tick(true,packet)&&packet.command==0x34&&packet.effectGain==112&&music.pending()==1,"one command per tick and effects attenuation");
 check(music.tick(true,packet)&&packet.command==0xf1&&music.pending()==0,"FIFO music order");
 music.beginSpeechDuck();for(unsigned i=0;i<102;++i)music.tick(false,packet);
 check(music.gain()==153&&music.envelope()==kMusicDuck,"speech duck subtracts two every second tick through threshold");
 music.tick(false,packet);music.tick(false,packet);check(music.gain()==153&&music.envelope()==kMusicSteady,"duck stops below154");
 music.endSpeechDuck();for(unsigned i=0;i<153;++i)music.tick(false,packet);
 check(music.gain()==255&&music.envelope()==kMusicRestore,"restore adds two every third tick");
 for(unsigned i=0;i<3;++i)music.tick(false,packet);check(music.envelope()==kMusicSteady,"restore completes on following cadence");
 check(music.submit(255,true,e)&&music.fading()&&!music.pending(),"fade request is not queued command");
 music.beginSpeechDuck();music.tick(false,packet);check(music.gain()==252&&music.envelope()==kMusicSteady,"fade overrides duck");
 for(unsigned i=0;i<90;++i)music.tick(false,packet);check(music.gain()==0&&music.fading(),"fade saturates by subtraction guard without false completion");
 check(music.submit(0xf1,true,e)&&music.fading()&&music.gain()==0,"control command preserves fade");music.tick(false,packet);
 check(music.submit(3,true,e)&&!music.fading()&&music.gain()==255,"ordinary command explicitly resets fade");music.tick(false,packet);
 for(unsigned i=0;i<127;++i)check(music.submit(1,false,e),"bounded original music ring submission");
 check(!music.submit(2,true,e)&&music.pending()==127&&music.gain()==255,"ambiguous full ring fails without state mutation");
 for(unsigned i=0;i<127;++i)check(music.tick(false,packet)&&packet.command==1,"bounded music ring drain");
 check(!music.pending()&&!music.tick(false,packet),"music ring drains once");
 std::vector<uint8_t> cd(2352,0);cd[2]=0xff;cd[3]=0x7f;cd[5]=0x80;cd[6]=0xff;cd[7]=0xff;std::vector<int16_t> cdPcm;
 check(decodeCdAudioFrames(cd.data(),cd.size(),cdPcm,e)&&cdPcm.size()==1176&&cdPcm[0]==0&&cdPcm[1]==32767&&cdPcm[2]==-32768&&cdPcm[3]==-1,"CD-DA exact signed little-endian domain");
 check(!decodeCdAudioFrames(cd.data(),2351,cdPcm,e)&&cdPcm.empty(),"CD-DA incomplete sector rejects");check(!decodeCdAudioFrames(cd.data(),0,cdPcm,e),"CD-DA empty rejects");AudioProfile cdProfile=cdAudioProfile(219);check(cdProfile.rate==44100&&cdProfile.channels==2&&cdProfile.volume==219&&!cdProfile.loop,"CD-DA standardized profile with explicit volume");
 {Fake resetBackend;AudioController resetAudio(resetBackend);AudioToken old,newer;AudioCompletion completed;AudioProfile verified=profile();std::vector<int16_t> samples(8,0);
 check(resetAudio.start(samples,verified,kAudioSpeech,AudioOwner(63,1),1,old,e)&&resetAudio.start(samples,verified,kAudioEffect,AudioOwner(63,2),9,newer,e),"session reset covers different narrative generations");resetAudio.reset();check(!resetAudio.activeCount()&&!resetAudio.take(completed)&&!resetAudio.cancel(old,e)&&!resetAudio.cancel(newer,e),"reset stops all owned jobs and rejects stale callbacks");check(resetAudio.start(samples,verified,kAudioSpeech,AudioOwner(63,3),10,newer,e)&&newer.id>old.id,"session reset never reuses audio identity");}
 Fake backend;AudioController c(backend,2,3);AudioToken a,b,t;AudioCompletion completion;std::vector<int16_t> pcm(4,0);AudioOwner owner(0x3a,88);AudioProfile p=profile(),unknown;
 check(!c.start(pcm,unknown,kAudioSpeech,owner,7,t,e)&&backend.plays==0,"unknown calibration never reaches mixer");
 p.loop=true;check(!c.start(pcm,p,kAudioSpeech,owner,7,t,e),"unverified looping unsupported");p.loop=false;
 check(!c.start(std::vector<int16_t>(),p,kAudioSpeech,owner,7,t,e),"empty stream rejected");
 backend.failPlay=true;check(!c.start(pcm,p,kAudioSpeech,owner,7,t,e)&&!t.id,"backend failure no token");backend.failPlay=false;
 check(c.start(pcm,p,kAudioSpeech,owner,7,a,e)&&a.id==1&&a.generation==7,"start owns stable token");check(c.start(pcm,p,kAudioEffect,AudioOwner(0x3a,95),8,b,e),"second audio token");
 check(!c.start(pcm,p,kAudioMusic,owner,7,t,e),"active capacity");check(!c.cancel(AudioToken(a.id,8),e),"stale generation cannot cancel");
 backend.alive[2]=false;backend.alive[1]=false;c.setPaused(true);c.poll();check(c.completionCount()==0&&backend.pauses==2,"pause freezes delivery");c.setPaused(true);check(backend.pauses==2,"repeated pause idempotent");c.setPaused(false);c.poll();
 check(c.take(completion)&&completion.token.id==a.id&&completion.owner.resource==owner.resource&&completion.owner.node==88&&completion.result==kAudioFinished,"completion owner and stable poll order");
 check(c.take(completion)&&completion.token.id==b.id&&!c.take(completion),"completion delivered once");c.poll();check(!c.take(completion),"no duplicate completion on repeated poll");
 check(!c.cancel(a,e),"completed token cannot cancel");
 check(c.start(pcm,p,kAudioSpeech,owner,9,a,e)&&c.start(pcm,p,kAudioSpeech,owner,10,b,e),"new generations start");c.cancelGeneration(9);check(c.take(completion)&&completion.token.id==a.id&&completion.result==kAudioCancelled&&c.activeCount()==1,"scene generation cancellation");
 c.cancelGeneration(9);check(!c.take(completion),"generation cancellation idempotent");check(c.cancel(b,e)&&c.take(completion)&&completion.result==kAudioCancelled,"explicit cancellation");
 Fake bounded;AudioController queue(bounded,2,2);check(queue.start(pcm,p,kAudioEffect,owner,1,a,e)&&queue.start(pcm,p,kAudioEffect,owner,1,b,e),"queue reservation");bounded.alive[1]=false;bounded.alive[2]=false;queue.poll();check(!queue.start(pcm,p,kAudioEffect,owner,1,t,e)&&queue.completionCount()==2,"pending completions reserve capacity");check(queue.take(completion)&&queue.start(pcm,p,kAudioEffect,owner,1,t,e),"draining permits new start");
 {
  AudioProfile source=sourcePcmStreamProfile();check(source.rateVerified&&source.polarityVerified&&source.loopVerified&&source.gainVerified&&source.rate==16276&&source.channels==2&&source.volume==255,"source stream factory qualifies source amplitude without claiming host waveform identity");
  PcmCatalogEntry entry={};entry.id=36;entry.mode=0x82;entry.level=255;entry.pan=0xf0;entry.offset=2;entry.length=12;
  std::vector<uint8_t> bytes={9,9,0,128,1,129,127,254,255,255,255,255,128,128};PreparedPcmStream stream;
  source.gainVerified=false;check(!preparePcmStream(entry,bytes.data(),bytes.size(),source,false,stream,e)&&stream.pcm.empty(),"unqualified stream never decodes for playback");source.gainVerified=true;
  check(preparePcmStream(entry,bytes.data(),bytes.size(),source,false,stream,e)&&stream.sample.sampleCount==6&&stream.pcm.size()==12,"qualified stream bounded marker decode");
  check(stream.pcm[0]==0&&stream.pcm[2]==0&&stream.pcm[4]==-476&&stream.pcm[6]==476&&stream.pcm[8]==-32768&&stream.pcm[10]==32767,"stream source magnitude/sign truncation and saturation");
  bool rightSilent=true;for(size_t i=1;i<stream.pcm.size();i+=2)rightSilent&=stream.pcm[i]==0;check(rightSilent,"source pan high nibble owns left output");
  PreparedPcmStream mono;check(preparePcmStream(entry,bytes.data(),bytes.size(),source,true,mono,e)&&mono.pcm[4]==-220&&mono.pcm[5]==-220,"original mono averages nibble controls before gain conversion");
  PreparedPcmStream quietStream;check(preparePcmStream(entry,bytes.data(),bytes.size(),source,false,quietStream,e,true)&&quietStream.pcm[4]==-208&&quietStream.pcm[6]==208&&quietStream.pcm[8]==-26428,"quiet source stream level floors255*112/256 before magnitude and four-copy mix");
  PreparedPcmStream saved=stream;source.volume=254;check(!preparePcmStream(entry,bytes.data(),bytes.size(),source,false,stream,e)&&stream.pcm==saved.pcm,"double-applied CE gain rejected transactionally");source.volume=255;source.rate=16000;check(!preparePcmStream(entry,bytes.data(),bytes.size(),source,false,stream,e),"nominal stored rate never overrides calibrated source clock");source.rate=16276;
  entry.length=13;check(!preparePcmStream(entry,bytes.data(),bytes.size(),source,false,stream,e),"out-of-bank stream extent rejects");entry.length=12;entry.mode=1;check(!preparePcmStream(entry,bytes.data(),bytes.size(),source,false,stream,e),"resident allocation is not a streamed voice");entry.mode=2;
  check(preparePcmStream(entry,bytes.data(),bytes.size(),source,false,stream,e),"mode2 shares stream conversion");
  Fake streamBackend;AudioController streamController(streamBackend,8,32);AudioActivity streamActivity;streamActivity.clearForSoundReset();MusicControl streamMusic;PcmStreamOwner voice(streamController,streamActivity,&streamMusic);AudioToken voiceToken,fxToken,cdToken;AudioCompletion end;
  check(streamController.start(pcm,p,kAudioEffect,AudioOwner(9,3),7,fxToken,e)&&streamController.start(pcm,p,kAudioMusic,AudioOwner(9,4),7,cdToken,e),"independent effect/music ownership before speech");streamActivity.setKnownFlag(3,true);
  check(voice.start(stream,AudioOwner(63,58),7,voiceToken,e)&&voice.active()&&voice.catalogId()==36&&streamMusic.envelope()==kMusicDuck,"mode2 actual backend submission starts owned speech and duck");
  check(streamActivity.snapshot(activityBits,e)&&activityBits==2&&streamController.activeCount()==2,"stream start clears transfer/effect ownership while independent CD survives");
  check(streamController.take(end)&&end.token.id==fxToken.id&&end.result==kAudioCancelled&&!voice.consume(end),"stream interrupts effect without consuming unrelated completion");
  AudioCompletion stale={AudioToken(voiceToken.id,8),AudioOwner(63,58),kAudioFinished};check(!voice.consume(stale)&&voice.active(),"stale generation cannot clear voice activity");
  streamController.setPaused(true);streamBackend.alive[3]=false;streamController.poll();check(!streamController.take(end)&&voice.active(),"pause retains speech activity until real completion delivery");streamController.setPaused(false);streamController.poll();
  check(streamController.take(end)&&end.token.id==voiceToken.id&&voice.consume(end)&&!voice.active()&&streamMusic.envelope()==kMusicRestore,"actual mixer end clears owned bit1 and restores mode2 duck");
  check(streamActivity.snapshot(activityBits,e)&&activityBits==0&&!voice.consume(end),"stream terminal consumption is once-only");
  stream.source.mode=0x82;stream.source.id=37;check(voice.start(stream,AudioOwner(63,60),8,voiceToken,e)&&streamMusic.envelope()==kMusicRestore,"mode82 does not begin a mode2 duck");
  AudioToken replaced=voiceToken;streamBackend.failPlay=true;check(!voice.start(stream,AudioOwner(63,61),9,voiceToken,e)&&voice.token().id==replaced.id&&voice.active(),"failed replacement preserves prior playback/activity");streamBackend.failPlay=false;
  check(voice.start(stream,AudioOwner(63,62),9,voiceToken,e)&&voiceToken.id>replaced.id,"replacement acquires unique voice identity");
  check(streamController.take(end)&&end.token.id==replaced.id&&!voice.consume(end)&&voice.active(),"late old cancellation cannot clear replacement speech bit");
  streamController.cancelGeneration(9);check(streamController.take(end)&&voice.consume(end)&&!voice.active(),"generation cancellation clears only its matching active voice");
  check(voice.start(stream,AudioOwner(63,64),10,voiceToken,e),"sequential stream after generation barrier");voice.reset();check(!voice.active()&&streamController.take(end)&&!voice.consume(end)&&end.result==kAudioCancelled,"new-game reset stops stream and invalidates delayed completion");
  check(voice.cancel(e),"repeated stream cancellation is idempotent");
  stream.source.mode=2;check(voice.start(stream,AudioOwner(63,65),11,voiceToken,e)&&streamMusic.envelope()==kMusicDuck,"source mode2 explicitly resets duck on new request");stream.source.mode=0x82;check(voice.start(stream,AudioOwner(63,66),12,voiceToken,e)&&streamMusic.envelope()==kMusicDuck,"mode82 replacement preserves inherited duck instead of inventing restore");
  check(streamController.take(end)&&!voice.consume(end)&&streamMusic.envelope()==kMusicDuck,"interrupted old mode2 completion cannot restore music");voice.reset();check(streamController.take(end)&&!voice.consume(end)&&streamMusic.envelope()==kMusicDuck,"cancellation is not the original mode2 terminal restore event");

 }
 {
  // The original stream-copy ring reserves its last code for a redirecting
  // marker; this differs from the16KiB resident-effect copy family.
  for(size_t encoded:{size_t(32766),size_t(32767),size_t(32768),size_t(32769),size_t(65535),size_t(65536),size_t(65537)})for(unsigned mode:{2u,0x82u}){
   std::vector<uint8_t> bytes(encoded+6,255);bytes[0]=bytes[1]=9;for(size_t i=0;i<encoded;++i)bytes[i+2]=uint8_t(((i*37+7)%127)|(i&1?0:128));
   PcmCatalogEntry entry;entry.id=36;entry.mode=uint8_t(mode);entry.offset=2;entry.length=encoded+4;entry.level=64;entry.pan=0xf7;PreparedPcmStream prepared;
   check(preparePcmStream(entry,bytes.data(),bytes.size(),sourcePcmStreamProfile(),false,prepared,e),"source-valid stream ring fixture prepares");
   const size_t played=encoded-encoded/32768;
   check(prepared.sample.sampleCount==encoded&&prepared.sample.markerLength==4&&prepared.playbackFrames==played&&prepared.droppedSourceFrames==encoded/32768&&prepared.pcm.size()==played*2,"encoded extent, actual played frames and omitted source codes are distinct");
   for(size_t frame=0;frame<played;++frame){size_t source=frame+frame/32767;unsigned code=bytes[source+2],magnitude=code&127;int left=int(magnitude*64*15/32)*4,right=int(magnitude*64*7/32)*4;if(!(code&128)){left=-left;right=-right;}
    check(prepared.pcm[frame*2]==left&&prepared.pcm[frame*2+1]==right,"absolute source ordinal excludes only ring-boundary replacement codes");}
   Fake backend;AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner owner(controller,activity);AudioToken token;PreparedPcmStream bad=prepared;++bad.droppedSourceFrames;
   check(!owner.start(bad,AudioOwner(63,1),8,token,e)&&!token.id&&!backend.plays,"incorrect dropped-code metadata cannot acquire a playback owner");bad=prepared;++bad.playbackFrames;
   check(!owner.start(bad,AudioOwner(63,1),8,token,e)&&!token.id&&!backend.plays,"incorrect played-frame metadata cannot acquire a playback owner");
   check(owner.start(prepared,AudioOwner(63,1),8,token,e)&&token.id&&owner.active(),"stream owner admits exact shortened playback while retaining encoded source extent");
  }
 }
 {
  PcmEffect effect={};effect.id=1;effect.step=1024;effect.priority=4;effect.channel=0;effect.level=255;effect.pan=0xf0;effect.source=kDrumSource;effect.headerOffset=0;
  std::vector<uint8_t> source(24,0);longword(source,0,8);longword(source,4,0xffffffff);word(source,8,16000);source[16]=1;source[17]=129;source[18]=127;source[19]=254;for(unsigned i=20;i<24;++i)source[i]=255;
  PreparedPcmEffect prepared;AudioProfile effectProfile=sourcePcmEffectProfile(effect);check(effectProfile.rate==16276&&effectProfile.channels==2&&effectProfile.gainVerified&&uint64_t(effectProfile.exactRate.numerator)*768==uint64_t(12500000)*effectProfile.exactRate.denominator,"effect pitch retains exact common source clock and step, not header nominal rate");
  check(preparePcmEffect(effect,source.data(),source.size(),effectProfile,false,0,prepared,e)&&prepared.sample.sampleCount==4&&prepared.pcm[0]==-238&&prepared.pcm[2]==238&&prepared.pcm[4]==-30360&&prepared.pcm[6]==30120,"effect two-copy magnitude/envelope conversion");
  PreparedPcmEffect scaled;check(preparePcmEffect(effect,source.data(),source.size(),effectProfile,true,128,scaled,e)&&scaled.pcm[0]==-54&&scaled.pcm[1]==-54,"effect optional scale and mono nibble average precede mix");
  PreparedPcmEffect quietEffect;check(preparePcmEffect(effect,source.data(),source.size(),effectProfile,false,128,quietEffect,e,true)&&quietEffect.pcm[0]==-50&&quietEffect.pcm[2]==50,"effect request scale precedes shared quietlevel truncation");
  {Fake backend;AudioController controller(backend,8,32);AudioActivity activity;activity.clearForSoundReset();PcmEffectOwner owner(controller,activity);
   check(!owner.residentReplacementSafe(0,2048)&&!owner.residentReplacementSafe(0xffff,2048)&&!owner.residentReplacementSafe(36,2048)&&!owner.residentReplacementSafe(31,0)&&!owner.residentReplacementSafe(31,2049)&&!owner.residentReplacementSafe(31,53*2048)&&owner.residentReplacementSafe(31,52*2048),"resident replacement validates exact source bank/sector domain even without voices");
   struct Case{size_t offset,count;uint16_t bank;PcmSourceKind kind;bool safe;};const Case cases[]={{16,8191,30,kResidentSource,true},{16,8192,30,kResidentSource,false},{63488,20684,30,kResidentSource,true},{63487,20684,30,kResidentSource,false},{16,72232,31,kResidentSource,true},{16,72232,30,kDrumSource,true}};
   for(const Case& c:cases){PreparedPcmEffect voice=prepared;voice.source.source=c.kind;voice.source.headerOffset=c.offset-16;voice.sample.headerOffset=c.offset-16;voice.sample.payloadOffset=c.offset;voice.sample.sampleCount=c.count;voice.sample.markerLength=4;voice.sample.encodedLength=c.count+4;voice.pcm.assign(c.count*2,123);bool accepted;AudioToken token;check(owner.start(voice,c.bank,AudioOwner(71,2810),9,accepted,token,e)&&accepted,"admit qualified source voice for overwrite policy");auto before=owner.voices();unsigned plays=backend.plays,stops=backend.stops;check(owner.residentReplacementSafe(31,31*2048)==c.safe,"initial prefetch boundary, exact tail overlap, samebank and drum domains");auto after=owner.voices();check(before.size()==1&&after.size()==1&&after[0].metadataKnown&&after[0].token.id==token.id&&after[0].token.generation==9&&after[0].effect==voice.source.id&&after[0].source==c.kind&&after[0].bank==c.bank&&after[0].payloadOffset==c.offset&&after[0].sampleCount==c.count&&backend.plays==plays&&backend.stops==stops,"policy and readonly telemetry preserve exact live token, voice identity and backend continuation");
    PreparedPcmEffect lower=voice;lower.source.priority=1;lower.sample.payloadOffset=0;AudioToken rejected;check(owner.start(lower,99,AudioOwner(),9,accepted,rejected,e)&&!accepted&&owner.voices()[0].token.id==token.id&&owner.residentReplacementSafe(31,31*2048)==c.safe,"rejected priority request cannot contaminate accepted source metadata");owner.reset();AudioCompletion done;while(controller.take(done))owner.consume(done);
   }
   PreparedPcmEffect unknown=prepared;unknown.sample.payloadOffset=0;bool accepted;AudioToken token;check(owner.start(unknown,30,AudioOwner(),9,accepted,token,e)&&accepted&&!owner.voices()[0].metadataKnown&&!owner.residentReplacementSafe(30,2048),"unqualified metadata fails replacement closed even for nominal samebank");owner.reset();check(owner.voices().empty()&&owner.residentReplacementSafe(31,2048),"reset removes stale overwrite source identity");
  }
  effectProfile.gainVerified=false;check(!preparePcmEffect(effect,source.data(),source.size(),effectProfile,false,0,scaled,e),"unqualified effect profile rejected");effectProfile.gainVerified=true;
  effect.headerOffset=1;check(!preparePcmEffect(effect,source.data(),source.size(),effectProfile,false,0,scaled,e),"effect requires exact selected-source sample header");effect.headerOffset=0;effect.step=0;check(!sourcePcmEffectProfile(effect).rateVerified,"invalid effect pitch remains unqualified");effect.step=1024;
  Fake effectBackend;AudioController effectController(effectBackend,8,32);AudioActivity effectActivity;effectActivity.clearForSoundReset();PcmStreamOwner stream(effectController,effectActivity);PcmEffectOwner effectOwner(effectController,effectActivity,&stream);bool accepted=false;AudioToken first,second,low;AudioCompletion done;
  check(effectOwner.start(prepared,34,AudioOwner(63,1),1,accepted,first,e)&&accepted&&effectOwner.activeCount()==1,"first source effect channel starts");
  PreparedPcmEffect lower=prepared;lower.source.priority=3;check(effectOwner.start(lower,34,AudioOwner(63,2),1,accepted,low,e)&&!accepted&&!low.id&&effectController.activeCount()==1,"lower-priority effect request does not replace or fabricate completion");
  PreparedPcmEffect other=prepared;other.source.channel=1;other.source.source=kResidentSource;check(effectOwner.start(other,35,AudioOwner(63,3),2,accepted,second,e)&&accepted&&effectOwner.activeCount()==2,"two source effect slots can own different bank/generations");
  effectBackend.failPlay=true;check(!effectOwner.start(prepared,34,AudioOwner(63,4),1,accepted,low,e)&&effectOwner.activeCount()==2,"failed effect replacement leaves prior owners intact");effectBackend.failPlay=false;
  AudioToken old=first;check(effectOwner.start(prepared,34,AudioOwner(63,5),3,accepted,first,e)&&accepted&&first.id>old.id,"equal source priority replaces channel owner");check(effectController.take(done)&&done.token.id==old.id&&!effectOwner.consume(done),"late replaced effect completion cannot clear new slot");
  effectController.setPaused(true);effectBackend.alive[2]=false;effectController.poll();check(!effectController.take(done)&&effectOwner.activeCount()==2,"effect pause defers actual endpoint consumption");effectController.setPaused(false);effectController.poll();check(effectController.take(done)&&effectOwner.consume(done)&&effectOwner.activeCount()==1,"effect actual endpoint consumes matching channel");
  check(effectActivity.snapshot(activityBits,e)&&activityBits==0,"source global effect bit clears at either endpoint even while other slot active");
  done.token=AudioToken(first.id,1);check(!effectOwner.consume(done)&&effectOwner.activeCount()==1,"effect stale generation rejects");effectController.cancelGeneration(3);check(effectController.take(done)&&effectOwner.consume(done)&&!effectOwner.activeCount(),"effect generation cancellation resolves exact slot");
  check(effectOwner.start(prepared,34,AudioOwner(63,6),4,accepted,first,e)&&accepted,"effect can restart after terminal barrier");effectOwner.reset();check(!effectOwner.activeCount()&&effectController.take(done)&&!effectOwner.consume(done),"effect reset invalidates late cancellation record");
 }
 for(unsigned value=0;value<256;++value){PcmEffectControl control=planPcmEffectControl(uint16_t(0x100|value),73);PcmEffectControlKind expected=value==0?kNoPcmEffectRequest:value<=73?kPcmEffectRecord:value<248?kNoPcmEffectRequest:value==252?kPcmEffectMonoOn:value==253?kPcmEffectMonoOff:value==255?kPcmEffectStop:kPcmEffectVerifiedNoOp;check(control.id==value&&control.kind==expected,"all low-byte effect command/record/control classes");}
 {Fake backend;AudioController owner(backend,2,4);std::shared_ptr<Feed> source(new Feed());AudioProfile format=profile();format.channels=2;format.volume=255;AudioToken token;AudioCompletion completion;check(owner.startFeed(source,format,kAudioEffect,AudioOwner(78,12),11,token,e)&&token.id&&owner.activeCount()==1,"native feed acquires an actual token and backend");uint64_t id=token.id;backend.alive[1]=false;owner.poll();check(owner.activeCount()==1&&!owner.take(completion)&&!owner.feedError().empty(),"backend inactivity before actual source marker is error, never completion");backend.alive[1]=true;source->failure="unknown resident byte";owner.poll();check(owner.activeCount()==1&&owner.feedError()==source->failure&&!owner.take(completion),"unknown prefetched source retains token and explicit failure");check(owner.cancel(token,e)&&owner.take(completion)&&completion.token.id==id&&completion.result==kAudioCancelled,"explicit source cancellation owns failed-feed teardown");source.reset(new Feed());backend.failPlay=true;check(!owner.startFeed(source,format,kAudioEffect,AudioOwner(78,14),12,token,e)&&!token.id&&!owner.activeCount(),"failed backend feed submission leaves no fake owner");backend.failPlay=false;format.rate=13000;check(!owner.startFeed(source,format,kAudioEffect,AudioOwner(),12,token,e),"source rate cannot be silently resampled at feed ownership boundary");format.rate=12000;check(owner.startFeed(source,format,kAudioEffect,AudioOwner(78,15),12,token,e),"new qualified feed after explicit cancellation");source->finished=true;backend.alive[2]=false;owner.poll();check(owner.take(completion)&&completion.token.id==token.id&&completion.result==kAudioFinished&&!owner.take(completion),"actual source marker plus drained backend releases exactly one completion");}
 {Fake backend;AudioController controller(backend,4,16);AudioActivity activity;activity.clearForSoundReset();PcmEffectOwner effects(controller,activity);std::shared_ptr<ResidentPcmStaging> staging(new ResidentPcmStaging(32768));std::vector<uint8_t> oldBank(32768,0x81);for(unsigned i=0;i<16;++i)oldBank[i]=0;for(unsigned i=0;i<4;++i)oldBank[20016+i]=255;check(staging->replacePrefix(11,oldBank.data(),oldBank.size(),e),"actual old resident source extent");PreparedPcmEffect effect;effect.source=PcmEffect();effect.source.id=27;effect.source.step=1024;effect.source.priority=4;effect.source.level=100;effect.source.pan=255;effect.source.source=kResidentSource;effect.sample=PcmSample();effect.sample.payloadOffset=16;effect.sample.sampleCount=20000;effect.sample.encodedLength=20004;effect.sample.markerLength=4;effect.profile=sourcePcmEffectProfile(effect.source);effect.pcm.assign(40000,0);ResidentPcmFeedParameters parameters;parameters.payloadOffset=16;parameters.rate=effect.profile.rate;parameters.level=100;parameters.pan=255;std::shared_ptr<PcmResidentFeed> feed(new PcmResidentFeed());check(feed->start(staging,parameters,e),"actual resident effect prefetch before backend submission");bool accepted=false;AudioToken token;check(effects.startFeed(effect,11,feed,staging,AudioOwner(80,14),7,accepted,token,e)&&accepted&&effects.voices()[0].prefetchOwned,"long resident feed binds real source and token");auto originalToken=token;std::shared_ptr<ResidentPcmStaging> foreign(new ResidentPcmStaging(32768));check(effects.residentReplacementSafe(16,26624,staging)&&!effects.residentReplacementSafe(16,26624,foreign)&&!effects.residentReplacementSafe(16,26624),"overlap admitted only for same owned mutable staging, never unrelated/copied source");PreparedPcmEffect lower=effect;lower.source.priority=3;lower.pcm.clear();AudioToken rejected;check(effects.startFeed(lower,11,{}, {},AudioOwner(),7,accepted,rejected,e)&&!accepted&&!rejected.id&&effects.voices()[0].token.id==originalToken.id,"source priority rejection precedes new source/header/prefetch validation");std::vector<uint8_t> nextBank(26624,0x82);nextBank[12000]=255;unsigned plays=backend.plays,stops=backend.stops;check(staging->replacePrefix(16,nextBank.data(),nextBank.size(),e)&&effects.voices()[0].token.id==originalToken.id&&backend.plays==plays&&backend.stops==stops,"actual prefix replacement preserves existing token, priority and backend");std::vector<int16_t> output(16384);check(feed->readFrames(output.data(),8192)==8192&&effects.serviceRefills(e)&&feed->snapshot().terminalOffset==12000,"worker observes real new end marker after old prefetched samples");check(feed->readFrames(output.data(),8192)==3792&&feed->ended(),"actual source marker replaces old sample length as completion boundary");AudioCompletion completion;controller.poll();check(!controller.take(completion)&&effects.activeCount()==1,"source end alone does not fabricate mixer drain");backend.alive[1]=false;controller.poll();check(controller.take(completion)&&completion.result==kAudioFinished&&effects.consume(completion)&&!effects.activeCount(),"real backend endpoint resolves actual voice once");uint64_t refills=feed->snapshot().refills;check(effects.serviceRefills(e)&&feed->snapshot().refills==refills,"inactive old source cannot receive later worker refills");}
 {PcmEffect rawEffect=validResidentEffect;std::vector<uint8_t> bytes(128,128);bytes[51]=255;PreparedPcmEffect raw,unchanged;AudioProfile actual=sourcePcmEffectProfile(rawEffect);
 check(preparePcmStagingEffect(rawEffect,bytes.data(),bytes.size(),actual,false,0,raw,e)&&raw.sample.payloadOffset==48&&raw.sample.sampleCount==3&&raw.sample.markerLength==1,"raw streamed staging needs no fabricated sample header");
 check(raw.pcm==std::vector<int16_t>(6,0),"source silence remains actual converted silence");unchanged=raw;bytes[51]=128;
 check(!preparePcmStagingEffect(rawEffect,bytes.data(),bytes.size(),actual,false,0,raw,e)&&raw.pcm==unchanged.pcm,"unknown short staging fails transactionally instead of fake endpoint");
 bytes.resize(48+8192,129);check(preparePcmStagingEffect(rawEffect,bytes.data(),bytes.size(),actual,false,0,raw,e)&&raw.sample.sampleCount==8192&&raw.sample.markerLength==0,"complete known prefetch window does not invent future terminator");
 bytes[48]=255;check(!preparePcmStagingEffect(rawEffect,bytes.data(),bytes.size(),actual,false,0,raw,e),"immediate marker endpoint remains explicit unsupported");}
 std::printf("audio runtime: %d checks, %d failures\n",checks,failures);return failures;
}
#ifdef SNATCHER_AUDIO_RUNTIME_TEST_MAIN
static bool readFile(const char *path,std::vector<uint8_t>& out){std::ifstream f(path,std::ios::binary);out.assign(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());return bool(f)&&!out.empty();}
int streamPriorityContracts(){using namespace Snatcher;checks=failures=0;std::string error;Fake backend;AudioController controller(backend,8,32);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner stream(controller,activity);PcmEffectOwner effects(controller,activity,&stream);PreparedPcmStream prepared;prepared.source.id=36;prepared.source.mode=2;prepared.source.priority=4;prepared.sample.sampleCount=16;prepared.playbackFrames=16;prepared.droppedSourceFrames=0;prepared.pcm.assign(32,100);prepared.profile=sourcePcmStreamProfile();AudioToken token;
 check(stream.start(prepared,AudioOwner(1,2),23,token,error)&&stream.priority()==4,"real stream owner retains catalog priority");
 PcmEffect effect;effect.id=1;effect.priority=3;effect.channel=0;effect.source=kDrumSource;bool admitted=true;unsigned plays=backend.plays,stops=backend.stops;
 check(effects.preflight(effect,admitted,error)&&!admitted&&error.empty(),"lower slot0 request rejects before source fetch without hard failure");PreparedPcmEffect unneeded;unneeded.source=effect;AudioToken rejected;bool accepted=true;
 check(effects.start(unneeded,0,AudioOwner(2,3),23,accepted,rejected,error)&&!accepted&&!rejected.id&&stream.token().id==token.id&&stream.token().generation==token.generation&&stream.priority()==4&&backend.plays==plays&&backend.stops==stops,"rejected sample never replaces backend or stream token");
 for(uint8_t priority:{uint8_t(4),uint8_t(5),uint8_t(6)}){effect.priority=priority;check(!effects.preflight(effect,admitted,error)&&!admitted&&stream.token().id==token.id&&stream.token().generation==token.generation,"source-eligible slot0 replacement retains unresolved owner guard");}
 for(uint8_t priority:{uint8_t(4),uint8_t(6),uint8_t(7),uint8_t(8)}){effect.channel=1;effect.priority=priority;check(!effects.preflight(effect,admitted,error)&&!admitted&&stream.token().id==token.id&&stream.token().generation==token.generation,"slot1 is not blanket-rejected as a lower-priority slot0 request");}
 backend.failPlay=true;prepared.source.priority=6;AudioToken failed;check(!stream.start(prepared,AudioOwner(3,4),23,failed,error)&&stream.priority()==4&&stream.token().id==token.id&&stream.token().generation==token.generation,"failed replacement retains real old priority");backend.failPlay=false;check(stream.start(prepared,AudioOwner(3,4),23,token,error)&&stream.priority()==6,"accepted replacement publishes its own priority");check(stream.cancel(error)&&!stream.active()&&!stream.priority(),"cancel retires stream priority without completing another owner");
 effect.channel=0;effect.priority=3;check(effects.preflight(effect,admitted,error)&&admitted,"same request admissible after real stream cancellation");
 std::printf("stream priority: %d checks,%d failures\n",checks,failures);return failures?1:0;}
int publicationGateContracts(){using namespace Snatcher;checks=failures=0;std::string error;
 struct Eager:Fake {size_t pulled=99;bool endedEarly=true;bool playFeed(const std::shared_ptr<NativeAudioFeed>& feed,const AudioProfile& p,AudioRole role,uint64_t& h,std::string& e)override{int16_t samples[4]={};pulled=feed->readFrames(samples,2);endedEarly=feed->ended();return Fake::play({},p,role,h,e);}} backend;
 AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner owner(controller,activity);PreparedPcmStream prepared;prepared.source.id=36;prepared.source.mode=2;prepared.source.priority=4;prepared.sample.sampleCount=4096;prepared.playbackFrames=4096;prepared.droppedSourceFrames=0;prepared.profile=sourcePcmStreamProfile();prepared.pcm.assign(8192,1234);AudioToken old;
 check(owner.start(prepared,AudioOwner(1,1),24,old,error),"prior real stream owner");auto stage=std::make_shared<ResidentPcmStaging>(106496);auto feed=std::make_shared<PcmStreamFeed>();std::vector<uint8_t> bytes(8192,128);prepared.source.id=37;check(feed->start(stage,37,bytes.data(),bytes.size(),prepared.pcm,4096,prepared.profile.rate,error),"new immutable source prepared before initial publication");
 backend.failPlay=true;AudioToken next;check(!owner.start(prepared,AudioOwner(1,2),24,next,error,feed)&&!next.id&&owner.token().id==old.id&&owner.priority()==4,"failed acquisition cannot replace prior owner");check(backend.pulled==0&&!backend.endedEarly&&feed->snapshot().consumedFrames==0&&feed->snapshot().publishedSourceBytes==0&&stage->knownBytes()==0,"eager failed backend cannot play or publish future initial data");
 backend.failPlay=false;check(owner.start(prepared,AudioOwner(1,2),24,next,error,feed)&&next.id!=old.id&&backend.pulled==0&&!backend.endedEarly,"successful eager acquisition still waits for source publication");check(feed->snapshot().publishedSourceBytes==8192&&feed->snapshot().consumedFrames==0&&stage->knownBytes()==8192,"only accepted source transfer establishes initial allocation");int16_t samples[2]={};check(feed->readFrames(samples,1)==1&&samples[0]==1234&&samples[1]==1234&&feed->snapshot().consumedFrames==1,"first allowed frame retains exact initial sample and cursor");AudioCompletion completion;check(controller.take(completion)&&completion.result==kAudioCancelled&&!owner.consume(completion)&&owner.token().id==next.id,"retired prior completion cannot end current published stream");check(owner.cancel(error)&&feed->snapshot().cancelled&&feed->snapshot().publishedSourceBytes==8192&&!owner.active(),"cancellation preserves actual published bytes and retires new owner");check(controller.take(completion)&&completion.result==kAudioCancelled&&!owner.consume(completion),"cancel does not fabricate natural stream completion");
 std::printf("PCM publication gate: %d checks,%d failures\n",checks,failures);return failures?1:0;
}
int pcmSilenceContracts(){using namespace Snatcher;checks=failures=0;std::string error;
 struct Backend:Fake{std::map<uint64_t,bool> muted;void pause(uint64_t h,bool v)override{muted[h]=v;Fake::pause(h,v);}} backend;AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner stream(controller,activity);PcmEffectOwner effects(controller,activity,&stream);PreparedPcmStream speech;speech.source.id=36;speech.source.mode=2;speech.source.priority=4;speech.sample.sampleCount=16;speech.playbackFrames=16;speech.droppedSourceFrames=0;speech.pcm.assign(32,100);speech.profile=sourcePcmStreamProfile();AudioToken first,next;check(stream.start(speech,AudioOwner(1,1),24,first,error),"source speech owner before FF");uint64_t h=backend.next-1;effects.silence();uint8_t bits=0;check(stream.active()&&stream.priority()==4&&controller.activeCount()==1&&controller.completionCount()==0&&backend.muted[h]&&activity.snapshot(bits,error)&&(bits&2)&&!(bits&8),"FF silences output without ending speech, priority or source active bit");controller.setPaused(true);controller.setPaused(false);controller.poll();check(backend.muted[h]&&controller.completionCount()==0,"application resume does not undo source silence");check(stream.start(speech,AudioOwner(1,2),24,next,error)&&next.id!=first.id&&!backend.muted[backend.next-1]&&backend.muted[h],"new accepted stream enables its own channels only");AudioCompletion done;check(controller.take(done)&&done.result==kAudioCancelled&&!stream.consume(done),"replaced silenced owner cannot complete new speech");check(stream.cancel(error)&&!stream.active(),"explicit cancellation remains distinct");while(controller.take(done))stream.consume(done);
 PreparedPcmEffect effect;effect.source.id=1;effect.source.priority=4;effect.source.channel=0;effect.source.source=kDrumSource;effect.source.step=1024;effect.sample.sampleCount=8;effect.pcm.assign(16,200);effect.profile=sourcePcmEffectProfile(effect.source);bool accepted;AudioToken a,b,c;check(effects.start(effect,0,AudioOwner(2,1),24,accepted,a,error)&&accepted,"first logical effect slot");uint64_t ha=backend.next-1;effect.source.channel=1;check(effects.start(effect,0,AudioOwner(2,2),24,accepted,b,error)&&accepted,"second logical effect slot");uint64_t hb=backend.next-1;effects.silence();check(effects.activeCount()==2&&backend.muted[ha]&&backend.muted[hb]&&activity.snapshot(bits,error)&&!(bits&8),"FF retains both slot owners while clearing only effect activity");effect.source.channel=0;effect.source.priority=3;check(effects.start(effect,0,AudioOwner(2,3),24,accepted,c,error)&&!accepted&&effects.activeCount()==2&&backend.muted[ha]&&backend.muted[hb],"silenced slot retains source priority admission");effect.source.priority=5;check(effects.start(effect,0,AudioOwner(2,4),24,accepted,c,error)&&accepted&&effects.activeCount()==2&&!backend.muted[backend.next-1]&&backend.muted[hb],"new admitted pair resumes while other pair stays silenced");check(controller.take(done)&&done.result==kAudioCancelled&&!effects.consume(done),"old silenced pair cancellation cannot retire its replacement");controller.setPaused(true);controller.setPaused(false);check(!backend.muted[backend.next-1]&&backend.muted[hb],"global pause compose with independent source channel silence");effects.reset();check(!controller.activeCount()&&!effects.activeCount(),"explicit reset retires all real owners");std::printf("PCM silence ownership: %d checks,%d failures\n",checks,failures);return failures?1:0;
}
int main(int argc,char **argv){
 if(argc==2&&std::string(argv[1])=="--pcm-silence")return pcmSilenceContracts();
 if(argc==2&&std::string(argv[1])=="--publication-gate")return publicationGateContracts();
 if(argc==2&&std::string(argv[1])=="--stream-priority")return streamPriorityContracts();
 if(argc==1)return runAudioRuntimeTests()?1:0;
 if((argc==8&&std::string(argv[1])=="--effect-export")||(argc==5&&std::string(argv[1])=="--effects")){
  using namespace Snatcher;std::vector<uint8_t> index,bank,drum;std::string error;if(!readFile(argv[2],index)||!readFile(argv[3],bank)||!readFile(argv[4],drum))return 2;
  std::vector<PcmCatalogEntry> catalog;std::vector<PcmEffect> effects;if(!parsePcmCatalog(index.data(),index.size(),bank.size(),catalog,error)||!parsePcmEffects(index.data(),index.size(),effects,error))return 1;
  if(argc==8){unsigned fx=0,bankId=0;std::istringstream a(argv[5]),b(argv[6]);a>>fx;b>>bankId;if(!a||!b||!fx||fx>effects.size()||bankId>65535)return 2;const PcmEffect& effect=effects[fx-1];const PcmCatalogEntry *entry=findPcmEntry(catalog,uint16_t(bankId));if(!entry||entry->mode!=1)return 2;PreparedPcmEffect result;const uint8_t *data=effect.source==kDrumSource?drum.data():bank.data()+entry->offset;size_t size=effect.source==kDrumSource?drum.size():entry->length;
   if(!preparePcmEffect(effect,data,size,sourcePcmEffectProfile(effect),false,0,result,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
   std::ofstream out(argv[7],std::ios::binary);if(!out)return 2;for(int16_t value:result.pcm){char bytes[2]={char(uint16_t(value)),char(uint16_t(value)>>8)};out.write(bytes,2);}if(!out)return 2;
   std::printf("EFFECT\t%u\t%u\t%zu\t%u\n",fx,bankId,result.sample.sampleCount,result.profile.rate);return 0;
  }
  size_t count=0;for(const PcmCatalogEntry& entry:catalog)if(entry.mode==1)for(const PcmEffect& effect:effects){const uint8_t *data=effect.source==kDrumSource?drum.data():bank.data()+entry.offset;size_t size=effect.source==kDrumSource?drum.size():entry.length;PreparedPcmEffect result;if(preparePcmEffect(effect,data,size,sourcePcmEffectProfile(effect),false,0,result,error)){++count;std::printf("E\t%u\t%u\t%zu\t%u\n",entry.id,effect.id,result.sample.sampleCount,result.profile.rate);}}
  std::printf("EFFECT_CONTEXTS\t%zu\n",count);return 0;
 }
 if(argc==6&&std::string(argv[1])=="--stream-export"){
  using namespace Snatcher;std::vector<uint8_t> index,bank;std::string error;if(!readFile(argv[2],index)||!readFile(argv[3],bank))return 2;
  std::vector<PcmCatalogEntry> catalog;if(!parsePcmCatalog(index.data(),index.size(),bank.size(),catalog,error))return 1;unsigned id=0;std::istringstream number(argv[4]);number>>id;const PcmCatalogEntry *entry=findPcmEntry(catalog,uint16_t(id));if(!number||id>65535||!entry)return 2;
  PreparedPcmStream stream;if(!preparePcmStream(*entry,bank.data(),bank.size(),sourcePcmStreamProfile(),false,stream,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
  std::ofstream out(argv[5],std::ios::binary);if(!out)return 2;for(int16_t value:stream.pcm){char bytes[2]={char(uint16_t(value)),char(uint16_t(value)>>8)};out.write(bytes,2);}if(!out)return 2;
  std::printf("STREAM\t%u\t%zu\t%u\t%u\n",entry->id,stream.sample.sampleCount,stream.profile.rate,unsigned(stream.profile.channels));return 0;
 }
 if(argc==4&&std::string(argv[1])=="--streams"){
  using namespace Snatcher;std::vector<uint8_t> index,bank;std::string error;if(!readFile(argv[2],index)||!readFile(argv[3],bank))return 2;
  std::vector<PcmCatalogEntry> catalog;if(!parsePcmCatalog(index.data(),index.size(),bank.size(),catalog,error))return 1;
  AudioProfile calibrated=sourcePcmStreamProfile();calibrated.gainVerified=true;size_t streams=0,total=0,banks=0;
  // Explicit standalone conversion fixture qualification; this does not alter
  // the runtime factory's separate source-amplitude qualification evidence.
  for(const PcmCatalogEntry& entry:catalog){if(entry.mode==1){++banks;continue;}PreparedPcmStream stream;if(!preparePcmStream(entry,bank.data(),bank.size(),calibrated,false,stream,error)){std::fprintf(stderr,"stream %u: %s\n",entry.id,error.c_str());return 1;}++streams;total+=stream.sample.sampleCount;std::printf("S\t%u\t%u\t%u\t%u\t%zu\n",entry.id,unsigned(entry.mode),unsigned(entry.level),unsigned(entry.pan),stream.sample.sampleCount);}
  std::printf("STREAMS\t%zu\tSAMPLES\t%zu\tBANKS\t%zu\n",streams,total,banks);return 0;
 }
 if(argc==3&&std::string(argv[1])=="--music-trace"){
  std::ifstream script(argv[2]);if(!script)return 2;Snatcher::MusicControl music;Snatcher::MusicControlPacket packet;std::string line,error;unsigned step=0;
  while(std::getline(script,line)){std::istringstream in(line);char op;unsigned a=0,b=0;in>>op>>a>>b;bool emitted=false,ok=true;
   if(op=='S')ok=music.submit(uint16_t(a),b!=0,error);else if(op=='D')music.beginSpeechDuck();else if(op=='U')music.endSpeechDuck();else if(op=='T')emitted=music.tick(a!=0,packet);else return 2;
   std::printf("%u\t%u\t%u\t%u\t%u\t%u\t%zu\t%u\t%u\t%u\n",step++,unsigned(ok),unsigned(emitted),emitted?unsigned(packet.command):0,unsigned(music.gain()),unsigned(music.envelope()),music.pending(),unsigned(music.fading()),emitted?unsigned(packet.musicGain):0,emitted?unsigned(packet.effectGain):0);
  }return 0;
 }
 if(argc!=5||std::string(argv[1])!="--original"){std::fprintf(stderr,"Usage: audio-runtime-tests --original INDEX BANK DRUM\n");return 2;}
 using namespace Snatcher;std::vector<uint8_t> index,bank,drum;std::string error;if(!readFile(argv[2],index)||!readFile(argv[3],bank)||!readFile(argv[4],drum))return 2;
 std::vector<PcmCatalogEntry> catalog;std::vector<PcmEffect> effects;std::vector<PcmSample> drums;size_t tail;
 if(!parsePcmCatalog(index.data(),index.size(),bank.size(),catalog,error)||!parsePcmEffects(index.data(),index.size(),effects,error)||!parsePcmSamples(drum.data(),drum.size(),false,drums,tail,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
 std::printf("EFFECTS\t%zu\n",effects.size());for(size_t i=0;i<effects.size();++i){const PcmEffect& f=effects[i];std::printf("E\t%u\t%u\t%zu\t%u\t%u\t%u\t%u\t%u\n",f.id,unsigned(f.source),f.headerOffset,f.priority,f.channel,f.level,f.pan,f.step);}
 size_t resolved=0;for(size_t b=0;b<catalog.size();++b){PcmRequest request;if(!planPcmRequest(catalog,catalog[b].id,request,error))return 1;if(request.kind!=kLoadResidentBank)continue;std::vector<PcmSample> resident;if(!parsePcmSamples(bank.data()+request.entry.offset,request.entry.length,true,resident,tail,error))return 1;
  for(size_t f=0;f<effects.size();++f){PcmSample s;if(resolvePcmEffect(effects[f],drums,resident,s,error)){++resolved;std::printf("R\t%u\t%u\t%zu\n",request.entry.id,effects[f].id,s.headerOffset);}}
 }std::printf("RESOLVED\t%zu\n",resolved);return 0;
}
#endif
