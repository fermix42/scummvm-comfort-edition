/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio_runtime.h"
#include "native_archive.h"
#include "pcm_shared.h"
#include <limits>
#include <algorithm>
namespace Snatcher {
namespace {
bool fail(std::string& e,const char *s){e=s;return false;}
uint16_t be16(const uint8_t *p){return uint16_t(uint16_t(p[0])<<8|p[1]);}
uint32_t be32(const uint8_t *p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
bool validProfile(const AudioProfile& p,std::string& e){
 if(!p.rateVerified||!p.polarityVerified||!p.gainVerified||!p.loopVerified)return fail(e,"Audio playback profile is not calibrated");
 if(p.rate<1000||p.rate>192000||(p.channels!=1&&p.channels!=2))return fail(e,"Invalid calibrated audio format");
 if(p.exactRate.numerator){uint32_t rounded=0;if(p.channels!=2||!nearestMixerRate(p.exactRate,rounded,e)||rounded!=p.rate)return fail(e,"Exact PCM transport rate disagrees with its nominal stereo profile");}
 else if(p.exactRate.denominator!=1)return fail(e,"Unowned exact PCM transport denominator");
 if(p.loop)return fail(e,"Looping playback contract is unsupported");return true;
}
}
bool parsePcmEffects(const uint8_t *index,size_t size,std::vector<PcmEffect>& out,std::string& e){
 out.clear();e.clear();if(!index||size<0x4020||(size-0x4000)%16||(size-0x4000)/16>255||index[0x4000]!=(size-0x4000)/16)return fail(e,"Invalid PCM effect directory extent");
 for(size_t i=0x4001;i<0x4010;++i)if(index[i])return fail(e,"Unsupported PCM effect directory header");
 std::vector<PcmEffect> parsed;
 for(size_t offset=0x4010;offset<size;offset+=16){const uint8_t *p=index+offset;PcmEffect effect;
  effect.id=uint16_t((offset-0x4000)/16);effect.priority=p[0];effect.channel=p[1];effect.level=p[2];effect.pan=p[3];effect.step=be16(p+4);
  if(effect.channel>1||!effect.step)return fail(e,"Unsupported PCM effect channel or step");
  for(unsigned i=10;i<16;++i)if(p[i])return fail(e,"Unsupported PCM effect extension");
  // Original-format relocation namespaces, normalized immediately to semantic
  // source and bounded header offset. No public API exposes guest addresses.
  uint32_t ref=be32(p+6);
  if(ref>=0x55000&&ref<0x65000){effect.source=kDrumSource;effect.headerOffset=ref-0x55000;}
  else if(ref>=0x65000&&ref<0x80000){effect.source=kResidentSource;effect.headerOffset=ref-0x65000;}
  else return fail(e,"PCM effect sample reference is outside specified sources");
  parsed.push_back(effect);
 }out.swap(parsed);return true;
}
bool resolvePcmEffect(const PcmEffect& effect,const std::vector<PcmSample>& drums,const std::vector<PcmSample>& resident,PcmSample& sample,std::string& e){
 e.clear();sample=PcmSample();const std::vector<PcmSample>& list=effect.source==kDrumSource?drums:resident;
 if(effect.source!=kDrumSource&&effect.source!=kResidentSource)return fail(e,"Unknown PCM effect source");
 for(size_t i=0;i<list.size();++i)if(list[i].headerOffset==effect.headerOffset){sample=list[i];return true;}
 return fail(e,"PCM effect does not resolve to a sample header in the selected source");
}
PcmEffectControl planPcmEffectControl(uint16_t command,size_t records){PcmEffectControl plan;plan.id=uint8_t(command);plan.kind=kNoPcmEffectRequest;
 if(!plan.id)return plan;
 if(plan.id>=248){plan.kind=plan.id==252?kPcmEffectMonoOn:plan.id==253?kPcmEffectMonoOff:plan.id==255?kPcmEffectStop:kPcmEffectVerifiedNoOp;return plan;}
 if(plan.id<=records)plan.kind=kPcmEffectRecord;return plan;
}
bool planPcmRequest(const std::vector<PcmCatalogEntry>& catalog,uint16_t id,PcmRequest& request,std::string& e){
 e.clear();request=PcmRequest();const PcmCatalogEntry *entry=findPcmEntry(catalog,id);if(!entry)return fail(e,"PCM catalog identity is unavailable");
 request.entry=*entry;if(entry->mode==1)request.kind=kLoadResidentBank;else if(entry->mode==2||entry->mode==0x82)request.kind=kStartStream;else return fail(e,"Unsupported PCM request mode");return true;
}
bool decodeCdAudioFrames(const uint8_t *data,size_t size,std::vector<int16_t>& pcm,std::string& e){
 pcm.clear();e.clear();if(!data||!size||size%2352||size>size_t(4096)*2352)return fail(e,"Invalid CD-DA source frame extent");
 pcm.resize(size/2);for(size_t i=0;i<pcm.size();++i){unsigned v=unsigned(data[i*2])|unsigned(data[i*2+1])<<8;pcm[i]=int16_t(v>=32768?int(v)-65536:int(v));}return true;
}
// Source default stream step and four-copy envelope/pan amplitude are qualified
// independently; host resampling/filter waveform equivalence is not implied.
AudioProfile sourcePcmStreamProfile(){AudioProfile p;p.rateVerified=p.polarityVerified=p.loopVerified=true;p.gainVerified=true;p.rate=16276;p.channels=2;p.volume=255;p.loop=false;p.exactRate=RationalAudioRate(390625,24);return p;}
bool preparePcmStream(const PcmCatalogEntry& entry,const uint8_t *bank,size_t size,const AudioProfile& p,bool mono,PreparedPcmStream& out,std::string& e,bool quiet){
 e.clear();if(!validProfile(p,e))return false;
 if(p.rate!=16276||p.channels!=2||p.volume!=255)return fail(e,"PCM stream requires qualified common source rate, stereo and full converted volume");
 if((entry.mode!=2&&entry.mode!=0x82)||!entry.id||!bank||entry.offset>size||entry.length>size-entry.offset||!entry.length)return fail(e,"Invalid PCM stream catalog extent or mode");
 PreparedPcmStream result;result.source=entry;result.profile=p;result.profile.exactRate=sourcePcmStreamProfile().exactRate;size_t padding;
 if(!parsePcmStream(bank+entry.offset,entry.length,result.sample,padding,e))return false;
 if(!result.sample.sampleCount||result.sample.sampleCount>std::numeric_limits<uint32_t>::max()/4)return fail(e,"Invalid PCM stream sample count");
 unsigned left=entry.pan>>4,right=entry.pan&15;if(mono)left=right=(left+right)/2;
 const unsigned level=quiet?(unsigned(entry.level)*0x70)>>8:entry.level;
 result.sourceValidated=true;result.effectiveLevel=uint8_t(level);result.effectivePan=uint8_t(left*16+right);
 result.droppedSourceFrames=result.sample.sampleCount/32768;result.playbackFrames=result.sample.sampleCount-result.droppedSourceFrames;
 result.pcm.reserve(result.playbackFrames*2);
 // Main sound path156E6 installs four copies of this game stream with the
 // same source envelope/pan. Convert those game-level controls once into S16.
 // Truncation is on magnitude before sign, then stereo sum is saturated.
 for(size_t i=0;i<result.sample.sampleCount;++i){
  // Source15CE0..15D60 reserves the final code of each32KiB stream-copy
  // ring for FF, then advances over that encoded source byte. The marker
  // redirects playback; it is neither an audible sample nor inserted silence.
  if((i&32767)==32767)continue;
  unsigned code=bank[entry.offset+i],magnitude=code&127;
  int l=int((magnitude*level*left)/32)*4,r=int((magnitude*level*right)/32)*4;
  if(!(code&128)){l=-l;r=-r;}if(l>32767)l=32767;if(l<-32768)l=-32768;if(r>32767)r=32767;if(r<-32768)r=-32768;
  result.pcm.push_back(int16_t(l));result.pcm.push_back(int16_t(r));
 }
 out=result;return true;
}
AudioProfile sourcePcmEffectProfile(const PcmEffect& effect){
 AudioProfile p;if(!effect.step||effect.channel>1)return p;
 uint32_t rate=uint32_t((uint64_t(12500000)*effect.step+393216)/786432);
 if(rate<1000||rate>192000)return p;
 uint64_t numerator=uint64_t(390625)*effect.step,denominator=24576,a=numerator,b=denominator;while(b){uint64_t remainder=a%b;a=b;b=remainder;}numerator/=a;denominator/=a;if(numerator>std::numeric_limits<uint32_t>::max())return p;
 p.rateVerified=p.polarityVerified=p.gainVerified=p.loopVerified=true;p.rate=rate;p.channels=2;p.volume=255;p.loop=false;p.exactRate=RationalAudioRate(uint32_t(numerator),uint32_t(denominator));return p;
}
bool preparePcmEffect(const PcmEffect& effect,const uint8_t *source,size_t size,const AudioProfile& p,bool mono,uint8_t scale,PreparedPcmEffect& out,std::string& e,bool quiet){
 e.clear();if(!validProfile(p,e))return false;AudioProfile expected=sourcePcmEffectProfile(effect);
 if(!expected.rate||p.rate!=expected.rate||p.channels!=2||p.volume!=255||!effect.id||effect.channel>1)return fail(e,"PCM effect requires qualified source pitch/stereo/full converted volume");
 if(effect.source!=kDrumSource&&effect.source!=kResidentSource)return fail(e,"Unknown PCM effect source kind");
 std::vector<PcmSample> samples;size_t tail;if(!parsePcmSamples(source,size,effect.source==kResidentSource,samples,tail,e))return false;
 const PcmSample *found=nullptr;for(const PcmSample& sample:samples)if(sample.headerOffset==effect.headerOffset){found=&sample;break;}
 if(!found)return fail(e,"PCM effect sample is unavailable in the owned source bank");
 PreparedPcmEffect result;result.source=effect;result.sample=*found;result.profile=p;result.profile.exactRate=expected.exactRate;
 unsigned left=effect.pan>>4,right=effect.pan&15;if(mono)left=right=(left+right)/2;
 unsigned level=scale?(unsigned(effect.level)*scale)>>8:effect.level;
 if(quiet)level=(level*0x70)>>8;
 result.effectiveLevel=uint8_t(level);result.effectivePan=uint8_t(left*16+right);result.sourceCodes.assign(source+found->payloadOffset,source+found->payloadOffset+found->encodedLength);
 result.pcm.reserve(found->sampleCount*2);
 // Each game effect installs two copies (15420), distinct from stream four.
 for(size_t i=0;i<found->sampleCount;++i){unsigned code=source[found->payloadOffset+i],magnitude=code&127;int l=int(magnitude*level*left/32)*2,r=int(magnitude*level*right/32)*2;if(!(code&128)){l=-l;r=-r;}result.pcm.push_back(int16_t(l));result.pcm.push_back(int16_t(r));}
 out=result;return true;
}
bool preparePcmStagingEffect(const PcmEffect& effect,const uint8_t* source,size_t size,const AudioProfile& p,bool mono,uint8_t scale,PreparedPcmEffect& out,std::string& e,bool quiet){
 e.clear();if(!validProfile(p,e))return false;AudioProfile expected=sourcePcmEffectProfile(effect);
 if(effect.source!=kResidentSource||!effect.id||effect.channel>1||!effect.priority||!expected.rate||p.rate!=expected.rate||p.channels!=2||p.volume!=255||!source||effect.headerOffset>size||size-effect.headerOffset<=16)return fail(e,"Effect requires known source allocation, original directory and payload offset");
 const size_t payload=effect.headerOffset+16,available=size-payload;size_t count=0;while(count<available&&source[payload+count]!=255)++count;bool terminal=count<available;
 if(!terminal){if(count<8192)return fail(e,"Effect source reaches unknown staging before a complete prefetch window");count=8192;}
 if(!count)return fail(e,"Immediate effect source marker has no qualified backend endpoint");
 PreparedPcmEffect result;result.source=effect;result.profile=p;result.profile.exactRate=expected.exactRate;result.sample=PcmSample();result.sample.headerOffset=effect.headerOffset;result.sample.payloadOffset=payload;result.sample.sampleCount=count;result.sample.markerLength=terminal?1:0;result.sample.encodedLength=count+result.sample.markerLength;
 unsigned left=effect.pan>>4,right=effect.pan&15;if(mono)left=right=(left+right)/2;unsigned level=scale?(unsigned(effect.level)*scale)>>8:effect.level;if(quiet)level=(level*0x70)>>8;
 result.effectiveLevel=uint8_t(level);result.effectivePan=uint8_t(left*16+right);result.sourceCodes.assign(source+payload,source+payload+count+(terminal?1:0));
 result.pcm.reserve(count*2);for(size_t i=0;i<count;++i){unsigned code=source[payload+i],magnitude=code&127;int l=int(magnitude*level*left/32)*2,r=int(magnitude*level*right/32)*2;if(!(code&128)){l=-l;r=-r;}result.pcm.push_back(int16_t(l));result.pcm.push_back(int16_t(r));}
 out=result;return true;
}
AudioProfile cdAudioProfile(uint8_t volume){AudioProfile p;p.rateVerified=p.polarityVerified=p.gainVerified=p.loopVerified=true;p.rate=44100;p.channels=2;p.volume=volume;p.loop=false;return p;}
bool readFmGainPublication(const uint8_t* sub,size_t size,std::vector<uint8_t>& out,std::string& e){e.clear();const size_t at=0x15fba-0xd400;if(!sub||size<at+256)return fail(e,"Original FM gain publication table unavailable");std::vector<uint8_t> staged(sub+at,sub+at+256);out.swap(staged);return true;}
FmControl planFmControl(uint16_t command,bool resetOnOrdinaryCommand){FmControl control;control.command=uint8_t(command);control.resetFade=false;control.kind=!control.command?kNoFmRequest:control.command==255?kRequestFmFade:kQueueFmCommand;if(control.kind==kQueueFmCommand&&control.command<0xf1)control.resetFade=resetOnOrdinaryCommand;return control;}
void AudioActivity::setKnownFlag(unsigned bit,bool active){if(bit>=4)return;uint8_t mask=uint8_t(1u<<bit);_known|=mask;if(active)_active|=mask;else _active&=uint8_t(~mask);}
bool AudioActivity::snapshot(uint8_t& bits,std::string& e,uint8_t required)const{bits=0;e.clear();if(!required||(required&~15)||(_known&required)!=required)return fail(e,"Audio activity contains unresolved required owners");bits=uint8_t(_active&required);return true;}
AudioWaitResult AudioActivityWait::tick(const AudioActivity& activity,std::string& e){
 e.clear();if(_done)return kAudioWaitFinished;
 if(_advance){_done=true;return kAudioWaitFinished;}
 if(--_remaining==0){_advance=true;return kAudioWaitPending;}
 uint8_t bits;if(!activity.snapshot(bits,e))return kAudioWaitUnknown;
 if(!bits)_advance=true;return kAudioWaitPending;
}
MusicControl::MusicControl():_gain(255),_effectGain(255),_sentGain(0),_sentEffect(0),_counter(0),_fade(false),_envelope(kMusicSteady){}
bool MusicControl::submit(uint16_t value,bool resetFade,std::string& e){
 e.clear();FmControl plan=planFmControl(value,resetFade);
 if(plan.kind==kNoFmRequest)return true;
 if(plan.kind==kRequestFmFade){_fade=true;return true;}
 // The original 128-slot ring has no full guard. Refuse its ambiguous full
 // state rather than dropping requests or presenting false audio completion.
 if(_commands.size()>=127)return fail(e,"Music command queue capacity exceeded");
 if(plan.resetFade){_fade=false;_gain=255;}
 _commands.push_back(plan.command);return true;
}
void MusicControl::beginSpeechDuck(){_envelope=kMusicDuck;_counter=0;}
void MusicControl::endSpeechDuck(){_envelope=kMusicRestore;_counter=0;}
bool MusicControl::tick(bool attenuateEffects,MusicControlPacket& packet){
 _effectGain=attenuateEffects?112:255;
 if(_fade){_envelope=kMusicSteady;if(_gain>=3)_gain=uint8_t(_gain-3);}
 else if(_envelope==kMusicDuck){if(++_counter==2){_counter=0;if(_gain>=154)_gain=uint8_t(_gain-2);else _envelope=kMusicSteady;}}
 else if(_envelope==kMusicRestore){if(++_counter==3){_counter=0;if(_gain!=255)_gain=uint8_t(_gain+2);else _envelope=kMusicSteady;}}
 if(_commands.empty()&&_gain==_sentGain&&_effectGain==_sentEffect)return false;
 packet.command=0;if(!_commands.empty()){packet.command=_commands.front();_commands.pop_front();}
 packet.musicGain=_gain;packet.effectGain=_effectGain;_sentGain=_gain;_sentEffect=_effectGain;return true;
}
AudioController::AudioController(AudioBackend& backend,size_t maxActive,size_t capacity):_backend(backend),_maxActive(maxActive),_capacity(capacity),_next(1),_paused(false){}
AudioController::~AudioController(){for(std::map<uint64_t,Playing>::const_iterator i=_active.begin();i!=_active.end();++i)if(!i->second.logical)_backend.stop(i->second.handle);}
bool AudioController::start(const std::vector<int16_t>& pcm,const AudioProfile& p,AudioRole role,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& e){
 token=AudioToken();e.clear();if(!validProfile(p,e))return false;
 if(pcm.empty()||pcm.size()%p.channels||pcm.size()>std::numeric_limits<uint32_t>::max()/2)return fail(e,"Invalid PCM sample extent");
 if(role!=kAudioSpeech&&role!=kAudioEffect&&role!=kAudioMusic&&role!=kAudioSharedPcm)return fail(e,"Unknown audio role");
 if(_active.size()>=_maxActive||_active.size()+_complete.size()>=_capacity)return fail(e,"Audio controller capacity exceeded");
 if(!_next||_next==std::numeric_limits<uint64_t>::max())return fail(e,"Audio token space exhausted");
 uint64_t handle=0;if(!_backend.play(pcm,p,role,handle,e))return false;
 Playing value;value.token=AudioToken(_next++,generation);value.owner=owner;value.handle=handle;value.role=role;
 _active[value.token.id]=value;token=value.token;if(_paused)_backend.pause(handle,true);return true;
}
bool AudioController::startFeed(const std::shared_ptr<NativeAudioFeed>& feed,const AudioProfile& p,AudioRole role,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& e){
 token=AudioToken();e.clear();if(!validProfile(p,e))return false;if(!feed||feed->sourceRate()!=p.rate||p.channels!=2||p.volume!=255||feed->ended()||!feed->error().empty())return fail(e,"Invalid native prefetched audio source");
 if(role!=kAudioSpeech&&role!=kAudioEffect&&role!=kAudioMusic&&role!=kAudioSharedPcm)return fail(e,"Unknown audio role");if(_active.size()>=_maxActive||_active.size()+_complete.size()>=_capacity)return fail(e,"Audio controller capacity exceeded");if(!_next||_next==std::numeric_limits<uint64_t>::max())return fail(e,"Audio token space exhausted");
 uint64_t handle=0;if(!_backend.playFeed(feed,p,role,handle,e))return false;Playing value;value.token=AudioToken(_next++,generation);value.owner=owner;value.handle=handle;value.role=role;value.feed=feed;_active[value.token.id]=value;token=value.token;if(_paused)_backend.pause(handle,true);return true;
}
bool validatePcmIdleContext(const PcmIdleContext& c,std::string& e){e.clear();if(c.version!=1||(!c.known&&(c.mode||c.copyMode))||(c.known&&!((c.mode==1&&c.copyMode==1)||(c.mode==0&&(c.copyMode==0||c.copyMode==2)))))return fail(e,"Invalid native quiescent PCM copier context");return true;}
bool encodePcmIdleContext(const PcmIdleContext& c,std::vector<uint8_t>& out,std::string& e){if(!validatePcmIdleContext(c,e))return false;out={0,0,0,1,uint8_t(c.known),c.mode,c.copyMode};return true;}
bool decodePcmIdleContext(const std::vector<uint8_t>& bytes,PcmIdleContext& out,std::string& e){e.clear();if(bytes.size()!=7||be32(bytes.data())!=1||bytes[4]>1)return fail(e,"Invalid quiescent PCM context extent/version");PcmIdleContext c;c.known=bytes[4]!=0;c.mode=bytes[5];c.copyMode=bytes[6];if(!validatePcmIdleContext(c,e))return false;out=c;return true;}
bool AudioController::physicalActive(AudioToken token)const{auto i=_active.find(token.id);return i!=_active.end()&&i->second.token.generation==token.generation&&!i->second.logical&&_backend.active(i->second.handle);}
bool AudioController::canAdmit(size_t count,std::string& e)const{e.clear();if(count>_maxActive-_active.size()||count>_capacity-_active.size()-_complete.size())return fail(e,"Audio controller capacity exceeded");if(!_next||count>=std::numeric_limits<uint64_t>::max()-_next)return fail(e,"Audio token space exhausted");return true;}
bool AudioController::observe(const std::shared_ptr<NativeAudioFeed>& job,AudioRole role,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& e){token=AudioToken();if(!job||!canAdmit(1,e))return false;Playing v;v.token=AudioToken(_next++,generation);v.owner=owner;v.handle=0;v.role=role;v.feed=job;v.logical=true;_active[v.token.id]=v;token=v.token;return true;}
std::string AudioController::feedError()const{
 for(const auto& value:_active)if(value.second.feed){
  std::string e=value.second.feed->error();if(!e.empty())return e;
  // Observe mixer drain before checking the terminal source marker. The mixer
  // may consume that marker between these calls; reading the source first
  // can combine an old "not ended" with a new "inactive" and falsely fail.
  if(!value.second.logical&&!_backend.active(value.second.handle)&&!value.second.feed->ended())return "Native audio backend ended before its source marker";
 }
 return std::string();
}
void AudioController::finish(std::map<uint64_t,Playing>::iterator i,AudioResult result,bool stop){
 if(stop&&!i->second.logical)_backend.stop(i->second.handle);if(i->second.role!=kAudioSharedPcm){AudioCompletion c;c.token=i->second.token;c.owner=i->second.owner;c.result=result;_complete.push_back(c);}_active.erase(i);
}
bool AudioController::cancel(AudioToken t,std::string& e){
 e.clear();std::map<uint64_t,Playing>::iterator i=_active.find(t.id);if(i==_active.end()||i->second.token.generation!=t.generation)return fail(e,"Audio token is stale or completed");finish(i,kAudioCancelled,true);return true;
}
void AudioController::reset(){for(const auto& playing:_active)if(!playing.second.logical)_backend.stop(playing.second.handle);_active.clear();_complete.clear();}
void AudioController::cancelGeneration(uint32_t generation){for(std::map<uint64_t,Playing>::iterator i=_active.begin();i!=_active.end();){std::map<uint64_t,Playing>::iterator old=i++;if(old->second.token.generation==generation)finish(old,kAudioCancelled,true);}}
void AudioController::cancelRole(AudioRole role){for(std::map<uint64_t,Playing>::iterator i=_active.begin();i!=_active.end();){std::map<uint64_t,Playing>::iterator old=i++;if(old->second.role==role)finish(old,kAudioCancelled,true);}}
PcmStreamOwner::PcmStreamOwner(AudioController& c,AudioActivity& a,MusicControl *m):_controller(c),_activity(a),_music(m),_catalog(0),_mode(0),_priority(0){}
void PcmStreamOwner::stopped(bool restoreMusic,bool retain){
 if(_bus){if(retain){std::string ignored;_bus->finishStream(ignored);if(_busToken.id)_controller.cancel(_busToken,ignored);_busToken=AudioToken();_bus->deactivate();}
 else{_bus->cancel();_bus.reset();if(_busToken.id){std::string ignored;_controller.cancel(_busToken,ignored);}_busToken=AudioToken();}}
 if(_feed)_feed->cancel();_feed.reset();if(restoreMusic&&_mode==2&&_music)_music->endSpeechDuck();_token=AudioToken();_catalog=0;_mode=_priority=0;_activity.setKnownFlag(0,false);_activity.setKnownFlag(1,false);
}
bool PcmStreamOwner::serviceRefills(std::string& error){error.clear();return _bus?_bus->service(error):(!_feed||_feed->serviceRefill(error));}
bool PcmStreamOwner::start(const PreparedPcmStream& stream,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& e,const std::shared_ptr<PcmStreamFeed>& feed){
 token=AudioToken();e.clear();
 if((stream.source.mode!=2&&stream.source.mode!=0x82)||!stream.source.id||stream.sample.sampleCount==0||stream.droppedSourceFrames!=stream.sample.sampleCount/32768||stream.playbackFrames!=stream.sample.sampleCount-stream.droppedSourceFrames||stream.playbackFrames>std::numeric_limits<size_t>::max()/2||stream.pcm.size()!=stream.playbackFrames*2||stream.profile.channels!=2)return fail(e,"Invalid prepared PCM stream");
 AudioToken next,master;std::shared_ptr<NativePcmBus> bus;
 if(feed&&stream.sourceValidated){
  if(!_controller.canAdmit(2,e))return false;
  bus.reset(new NativePcmBus(_controller.backend(),stream,feed));if(!bus->initialize(e))return false;
  AudioProfile p=stream.profile;p.rate=bus->sourceRate();p.exactRate=RationalAudioRate();
  if(!_controller.startFeed(bus,p,kAudioSharedPcm,owner,generation,master,e))return false;
  if(!feed->publishInitial(e)||!_controller.observe(bus->streamJob(),kAudioSpeech,owner,generation,next,e)){std::string ignored;_controller.cancel(master,ignored);return false;}
 }else{
  if(feed?!_controller.startFeed(feed,stream.profile,kAudioSpeech,owner,generation,next,e):!_controller.start(stream.pcm,stream.profile,kAudioSpeech,owner,generation,next,e))return false;
  if(feed&&!feed->publishInitial(e)){std::string ignored;_controller.cancel(next,ignored);return false;}
 }
 // Validate and acquire the new mixer owner before replacing an old one. A
 // failed backend submission leaves all prior activity and playback intact.
 if(_token.id||_bus){std::string ignored;if(_token.id)_controller.cancel(_token,ignored);stopped(false);}
 _controller.cancelRole(kAudioEffect);_activity.setKnownFlag(3,false);
 _activity.setKnownFlag(0,false);_activity.setKnownFlag(1,true);
 _token=next;_catalog=stream.source.id;_mode=stream.source.mode;_priority=stream.source.priority;_feed=feed;_bus=bus;_busToken=master;token=next;if(bus)bus->activate();
 if(_mode==2&&_music)_music->beginSpeechDuck();return true;
}
uint8_t PcmStreamOwner::priority()const{return _bus?_bus->priority(0):_priority;}
bool PcmStreamOwner::enterResidentMode(std::string& error,const std::shared_ptr<ResidentPcmStaging>& staging){error.clear();if(_bus){if(staging&&!_bus->owns(staging))return fail(error,"Resident mode requires the actual shared source allocation");if(!_bus->residentMode(error))return false;}else if(!staging)return fail(error,"Resident mode requires an owned source allocation");_idleContext=PcmIdleContext();_idleContext.known=true;_idleContext.mode=_idleContext.copyMode=1;if(!_bus)_bus.reset(new NativePcmBus(_controller.backend(),staging,_idleContext));return true;}
bool PcmStreamOwner::ensureBusOutput(AudioOwner owner,uint32_t generation,bool& created,std::string& error){created=false;if(!_bus)return true;if(_controller.physicalActive(_busToken))return true;if(!_controller.canAdmit(2,error))return false;std::string ignored;if(_busToken.id)_controller.cancel(_busToken,ignored);_busToken=AudioToken();_bus->deactivate();AudioProfile p=sourcePcmStreamProfile();p.rate=_bus->sourceRate();p.exactRate=RationalAudioRate();if(!_controller.startFeed(_bus,p,kAudioSharedPcm,owner,generation,_busToken,error))return false;created=true;return true;}
void PcmStreamOwner::finishBusAdmission(bool created,bool accepted){if(!_bus)return;if(accepted){_bus->activate();return;}if(created){std::string ignored;_controller.cancel(_busToken,ignored);_busToken=AudioToken();_bus->deactivate();}}
bool PcmStreamOwner::captureIdleContext(PcmIdleContext& out,std::string& error)const{if(_token.id){error="Active speech has no quiescent copier snapshot";return false;}if(_bus)return _bus->idleContext(out,error);if(!validatePcmIdleContext(_idleContext,error))return false;out=_idleContext;return true;}
bool PcmStreamOwner::prepareIdleContext(const PcmIdleContext& context,const std::shared_ptr<ResidentPcmStaging>& staging,std::shared_ptr<NativePcmBus>& out,std::string& error)const{if(!validatePcmIdleContext(context,error))return false;if(context.known&&context.copyMode&&!staging){error="Known PCM copier context lacks its actual source allocation";return false;}std::shared_ptr<NativePcmBus> next;if(context.known)next.reset(new NativePcmBus(_controller.backend(),staging,context));out=next;return true;}
void PcmStreamOwner::commitIdleContext(const PcmIdleContext& context,const std::shared_ptr<NativePcmBus>& bus){_idleContext=context;_bus=bus;_busToken=AudioToken();}

bool PcmStreamOwner::consume(const AudioCompletion& c){if(!_token.id||c.token.id!=_token.id||c.token.generation!=_token.generation)return false;stopped(c.result==kAudioFinished,c.result==kAudioFinished&&bool(_bus));return true;}
bool PcmStreamOwner::cancel(std::string& e){e.clear();AudioToken old=_token;stopped(false);_idleContext=PcmIdleContext();return !old.id||_controller.cancel(old,e);}
void PcmStreamOwner::reset(){std::string ignored;cancel(ignored);}
PcmEffectOwner::PcmEffectOwner(AudioController& c,AudioActivity& a,PcmStreamOwner *s):_controller(c),_activity(a),_stream(s){}
size_t PcmEffectOwner::activeCount()const{return size_t(_slots[0].token.id!=0)+size_t(_slots[1].token.id!=0);}
std::vector<PcmEffectVoiceSnapshot> PcmEffectOwner::voices()const{std::vector<PcmEffectVoiceSnapshot> result;for(const Slot& slot:_slots)if(slot.token.id){PcmEffectVoiceSnapshot voice=slot.voice;if(slot.feed){auto state=slot.feed->snapshot();voice.prefetchOwned=true;voice.terminalSeen=state.terminalSeen;voice.sourceCursor=state.sourceCursor;voice.queuedFrames=state.queuedFrames;voice.renderedFrames=state.renderedFrames;voice.droppedUnits=state.droppedSourceUnits;}result.push_back(voice);}return result;}
bool PcmEffectOwner::residentReplacementSafe(uint16_t bank,size_t length,const std::shared_ptr<ResidentPcmStaging>& staging)const{
 // Verified source catalog has resident IDs1..35. Source1554C rejects53
 // or more sectors. Caller also qualifies the selected catalog entry as mode1.
 if(!bank||bank>35||!length||length%2048||length>52*2048)return false;
 for(const Slot& slot:_slots){if(!slot.token.id)continue;const PcmEffectVoiceSnapshot& v=slot.voice;if(!v.metadataKnown)return false;
  if(v.source==kDrumSource)continue;if(v.source!=kResidentSource)return false;
  if(slot.bus){if(staging&&slot.bus->owns(staging))continue;return false;}
  if(slot.feed&&staging&&slot.feed->owns(staging)&&slot.feed->error().empty())continue;
  if(!v.bank||v.bank>35)return false;
  if(v.bank==bank||v.sampleCount<8192||v.payloadOffset>=length)continue;return false;
 }return true;
}
bool PcmEffectOwner::preflight(const PcmEffect& record,bool& admitted,std::string& error){admitted=false;error.clear();if(record.channel>1||!record.priority||!record.id||(record.source!=kResidentSource&&record.source!=kDrumSource)){error="Invalid PCM effect directory owner";return false;}if(_stream&&(_stream->active()||_stream->sharedBus())){
 // Source156D4 installs stream priority in logical slot0.153D0 rejects a
 // lower effect before15E24 can replace that slot or fetch its sample.
 if(_stream->sharedBus()){_stream->sharedBus()->noteEffectRequest();_activity.setKnownFlag(3,true);}
 if(record.channel==0&&record.priority<_stream->priority()){_activity.setKnownFlag(3,true);return true;}
 if(!_stream->sharedBus()){error="PCM effect requires a source-owned shared stream allocation";return false;}
 const uint8_t priority=_stream->sharedBus()->priority(record.channel);if(record.priority<priority){_activity.setKnownFlag(3,true);return true;}admitted=true;return true;
}const Slot& slot=_slots[record.channel];if(slot.token.id&&record.priority<slot.priority){_activity.setKnownFlag(3,true);return true;}admitted=true;return true;}
bool PcmEffectOwner::start(const PreparedPcmEffect& effect,uint16_t bank,AudioOwner owner,uint32_t generation,bool& accepted,AudioToken& token,std::string& e){return startOwned(effect,bank,owner,generation,accepted,token,e,{});}
bool PcmEffectOwner::startFeed(const PreparedPcmEffect& effect,uint16_t bank,const std::shared_ptr<PcmResidentFeed>& feed,const std::shared_ptr<ResidentPcmStaging>& staging,AudioOwner owner,uint32_t generation,bool& accepted,AudioToken& token,std::string& error){accepted=false;token=AudioToken();bool admitted;if(!preflight(effect.source,admitted,error))return false;if(!admitted)return true;if(!feed||!staging||!feed->owns(staging)||staging->bank()!=bank||effect.source.source!=kResidentSource||effect.sample.sampleCount<8192||feed->sourceRate()!=effect.profile.rate){error="Resident audio feed is not bound to its actual source allocation";return false;}return startOwned(effect,bank,owner,generation,accepted,token,error,feed);}
bool PcmEffectOwner::serviceRefills(std::string& error){error.clear();for(const Slot& slot:_slots)if(slot.token.id&&slot.feed&&!slot.feed->serviceRefill(error))return false;return true;}
bool PcmEffectOwner::startOwned(const PreparedPcmEffect& effect,uint16_t bank,AudioOwner owner,uint32_t generation,bool& accepted,AudioToken& token,std::string& e,const std::shared_ptr<PcmResidentFeed>& feed){
 accepted=false;token=AudioToken();e.clear();bool admitted;if(!preflight(effect.source,admitted,e))return false;if(!admitted)return true;
 if(effect.source.channel>1||!effect.source.priority||!effect.source.id||!effect.sample.sampleCount||effect.pcm.size()!=effect.sample.sampleCount*2)return fail(e,"Invalid prepared PCM effect");
 if(!validProfile(effect.profile,e))return false;

 Slot& slot=_slots[effect.source.channel];std::shared_ptr<NativePcmBus> bus=_stream?_stream->sharedBus():std::shared_ptr<NativePcmBus>();if(!bus&&slot.token.id&&effect.source.priority<slot.priority){_activity.setKnownFlag(3,true);return true;}
 AudioToken next;
 if(bus){const auto expected=sourcePcmEffectProfile(effect.source);if(!expected.rate||effect.profile.channels!=2||effect.profile.volume!=255||effect.profile.rate!=expected.rate||effect.profile.exactRate.numerator!=expected.exactRate.numerator||effect.profile.exactRate.denominator!=expected.exactRate.denominator)return fail(e,"Shared PCM effect requires its calibrated source pitch and converted stereo profile");}
 if(bus){bool created=false;if(!_stream->ensureBusOutput(owner,generation,created,e))return false;bool ok=_controller.canAdmit(1,e)&&bus->replace(effect,[&](const std::shared_ptr<NativeAudioFeed>& job,std::string& error){return _controller.observe(job,kAudioEffect,owner,generation,next,error);},e);_stream->finishBusAdmission(created,ok);if(!ok)return false;}
 else if(!(feed?_controller.startFeed(feed,effect.profile,kAudioEffect,owner,generation,next,e):_controller.start(effect.pcm,effect.profile,kAudioEffect,owner,generation,next,e)))return false;
 if(slot.token.id){std::string ignored;_controller.cancel(slot.token,ignored);}
 slot.token=next;slot.priority=effect.source.priority;slot.bank=bank;slot.feed=bus?std::shared_ptr<PcmResidentFeed>():feed;slot.bus=bus;
 slot.voice=PcmEffectVoiceSnapshot();slot.voice.channel=effect.source.channel;slot.voice.effect=effect.source.id;slot.voice.bank=bank;slot.voice.source=effect.source.source;slot.voice.payloadOffset=effect.sample.payloadOffset;slot.voice.sampleCount=effect.sample.sampleCount;slot.voice.token=next;
 const PcmSample& sample=effect.sample;slot.voice.metadataKnown=(effect.source.source==kDrumSource||effect.source.source==kResidentSource)&&sample.headerOffset==effect.source.headerOffset&&sample.headerOffset<=std::numeric_limits<size_t>::max()-16&&sample.payloadOffset==sample.headerOffset+16&&(sample.markerLength==4||sample.markerLength==5||(effect.source.source==kResidentSource&&(sample.markerLength==1||((feed||bus)&&sample.markerLength==0))))&&sample.sampleCount<=std::numeric_limits<size_t>::max()-sample.markerLength&&sample.encodedLength==sample.sampleCount+sample.markerLength&&sample.payloadOffset<=std::numeric_limits<size_t>::max()-sample.encodedLength;
 token=next;accepted=true;_activity.setKnownFlag(3,true);return true;
}
bool PcmEffectOwner::consume(const AudioCompletion& c){for(Slot& slot:_slots)if(slot.token.id&&slot.token.id==c.token.id&&slot.token.generation==c.token.generation){auto bus=slot.bus;slot=Slot();
 // Source15998 clears this global activity bit at either effect endpoint;
 // it is not an invented aggregate count of every simultaneously active job.
 _activity.setKnownFlag(3,bus?bus->effectActivity():false);return true;}return false;}
void PcmEffectOwner::reset(){for(Slot& slot:_slots){if(slot.token.id){std::string ignored;_controller.cancel(slot.token,ignored);}slot=Slot();}_activity.setKnownFlag(3,false);}
void PcmEffectOwner::silence(){if(_stream&&_stream->sharedBus())_stream->sharedBus()->silence();_controller.silenceCurrent();_activity.setKnownFlag(3,false);}
void AudioController::silenceCurrent(){for(auto& item:_active){if(item.second.logical||item.second.role==kAudioSharedPcm)continue;item.second.silenced=true;_backend.pause(item.second.handle,true);}}
void AudioController::setPaused(bool paused){if(_paused==paused)return;_paused=paused;for(std::map<uint64_t,Playing>::const_iterator i=_active.begin();i!=_active.end();++i)if(!i->second.logical)_backend.pause(i->second.handle,paused||i->second.silenced);}
void AudioController::poll(){if(_paused)return;for(std::map<uint64_t,Playing>::iterator i=_active.begin();i!=_active.end();){std::map<uint64_t,Playing>::iterator old=i++;if(old->second.silenced)continue;if(old->second.feed&&(!old->second.feed->error().empty()||!old->second.feed->ended()))continue;if(old->second.logical||!_backend.active(old->second.handle))finish(old,kAudioFinished,true);}}
bool AudioController::take(AudioCompletion& c){if(_complete.empty())return false;c=_complete.front();_complete.pop_front();return true;}
}
#ifndef SNATCHER_AUDIO_CORE_ONLY
#include "audio/mixer.h"
#include "audio/decoders/raw.h"
#include "audio/audiostream.h"
#include <cstdlib>
namespace Snatcher {
struct SavedPlayback {std::shared_ptr<NativeAudioFeed> memory,source;std::shared_ptr<RationalPcmTransport> transport;AudioProfile profile;};
struct CeAudioBackend::Impl {std::map<uint64_t,SavedPlayback> playback;std::map<uint64_t,bool> paused;Audio::Mixer& mixer;std::map<uint64_t,Audio::SoundHandle> handles;uint64_t next;Impl(Audio::Mixer& m):mixer(m),next(1){}};
namespace {
class CePcmMemoryFeed:public NativeAudioFeed {
 std::vector<int16_t> _pcm;size_t _at;uint32_t _rate;
public:
 bool archive(NativeArchive& a){a.vector(_pcm,16*1024*1024);a.number(_at);a.field(_rate);if(_at>_pcm.size()||(_at&1)||(_pcm.size()&1)||_rate<1000||_rate>384000)a.ok=false;return a.ok;}
 CePcmMemoryFeed(const std::vector<int16_t>& pcm,uint32_t rate):_pcm(pcm),_at(0),_rate(rate){}
 uint32_t sourceRate()const override{return _rate;}
 size_t readFrames(int16_t* stereo,size_t frames)override{size_t count=std::min(frames,(_pcm.size()-_at)/2);std::copy(_pcm.begin()+_at,_pcm.begin()+_at+count*2,stereo);_at+=count*2;return count;}
 bool ended()const override{return _at==_pcm.size();}
 std::string error()const override{return std::string();}
};
class CeNativeFeedStream:public Audio::AudioStream {
 std::shared_ptr<NativeAudioFeed> _source;
public:
 explicit CeNativeFeedStream(const std::shared_ptr<NativeAudioFeed>& source,bool =false):_source(source){}
 int readBuffer(int16* out,const int samples)override{return samples>0?int(_source->readFrames(out,size_t(1))*2):0;}
 bool isStereo()const override{return true;}
 int getRate()const override{return int(_source->sourceRate());}
 bool endOfData()const override{return _source->ended();}
};
}
CeAudioBackend::CeAudioBackend(Audio::Mixer& mixer):_impl(new Impl(mixer)){}
CeAudioBackend::~CeAudioBackend(){for(std::map<uint64_t,Audio::SoundHandle>::const_iterator i=_impl->handles.begin();i!=_impl->handles.end();++i)_impl->mixer.stopHandle(i->second);delete _impl;}
bool CeAudioBackend::play(const std::vector<int16_t>& pcm,const AudioProfile& p,AudioRole role,uint64_t& handle,std::string& e){
 handle=0;e.clear();if(!validProfile(p,e))return false;
 if(pcm.empty()||pcm.size()%p.channels||pcm.size()>std::numeric_limits<uint32_t>::max()/2)return fail(e,"Invalid CE PCM extent");
 if(p.exactRate.numerator){std::shared_ptr<NativeAudioFeed> source(new CePcmMemoryFeed(pcm,p.rate));if(!playFeed(source,p,role,handle,e))return false;_impl->playback[handle].memory=source;return true;}
 uint32 bytes=uint32(pcm.size()*2);byte *copy=static_cast<byte *>(std::malloc(bytes));if(!copy)return fail(e,"Cannot allocate CE audio stream");
 for(size_t i=0;i<pcm.size();++i){copy[i*2]=byte(uint16_t(pcm[i]));copy[i*2+1]=byte(uint16_t(pcm[i])>>8);}
 byte flags=Audio::FLAG_16BITS|Audio::FLAG_LITTLE_ENDIAN;if(p.channels==2)flags|=Audio::FLAG_STEREO;
 Audio::SeekableAudioStream *stream=Audio::makeRawStream(copy,bytes,int(p.rate),flags,DisposeAfterUse::YES);
 if(!stream){std::free(copy);return fail(e,"Cannot create CE audio stream");}
 Audio::SoundHandle sound;Audio::Mixer::SoundType type=role==kAudioMusic?Audio::Mixer::kMusicSoundType:role==kAudioSpeech?Audio::Mixer::kSpeechSoundType:role==kAudioSharedPcm?Audio::Mixer::kPlainSoundType:Audio::Mixer::kSFXSoundType;
 _impl->mixer.playStream(type,&sound,stream,-1,p.volume,0,DisposeAfterUse::YES);handle=_impl->next++;_impl->handles[handle]=sound;return true;
}
bool CeAudioBackend::playFeed(const std::shared_ptr<NativeAudioFeed>& source,const AudioProfile& p,AudioRole role,uint64_t& handle,std::string& e){
 handle=0;e.clear();if(!validProfile(p,e))return false;if(!source||p.channels!=2||p.volume!=255||source->sourceRate()!=p.rate||source->ended()||!source->error().empty())return fail(e,"Invalid native prefetched CE stream");
 SavedPlayback saved;saved.profile=p;saved.source=source;std::shared_ptr<NativeAudioFeed> delivered=source;
 if(p.exactRate.numerator){std::shared_ptr<RationalPcmTransport> transport(new RationalPcmTransport());if(!transport->start(source,p.exactRate,_impl->mixer.getOutputRate(),e))return false;delivered=transport;saved.transport=transport;}
 Audio::AudioStream* stream=new CeNativeFeedStream(delivered,role==kAudioSharedPcm);Audio::SoundHandle sound;Audio::Mixer::SoundType type=role==kAudioMusic?Audio::Mixer::kMusicSoundType:role==kAudioSpeech?Audio::Mixer::kSpeechSoundType:role==kAudioSharedPcm?Audio::Mixer::kPlainSoundType:Audio::Mixer::kSFXSoundType;_impl->mixer.playStream(type,&sound,stream,-1,p.volume,0,DisposeAfterUse::YES);handle=_impl->next++;_impl->handles[handle]=sound;_impl->playback[handle]=saved;return true;
}
uint32_t CeAudioBackend::outputRate()const{return _impl->mixer.getOutputRate();}
unsigned CeAudioBackend::roleVolume(AudioRole role)const{auto type=role==kAudioSpeech?Audio::Mixer::kSpeechSoundType:Audio::Mixer::kSFXSoundType;return _impl->mixer.isSoundTypeMuted(type)?0:_impl->mixer.getVolumeForSoundType(type);}
void CeAudioBackend::stop(uint64_t h){std::map<uint64_t,Audio::SoundHandle>::iterator i=_impl->handles.find(h);if(i==_impl->handles.end())return;_impl->mixer.stopHandle(i->second);_impl->handles.erase(i);_impl->playback.erase(h);_impl->paused.erase(h);}
void CeAudioBackend::pause(uint64_t h,bool paused){std::map<uint64_t,Audio::SoundHandle>::iterator i=_impl->handles.find(h);if(i!=_impl->handles.end()&&_impl->paused[h]!=paused){_impl->paused[h]=paused;_impl->mixer.pauseHandle(i->second,paused);}}
bool CeAudioBackend::active(uint64_t h)const{std::map<uint64_t,Audio::SoundHandle>::const_iterator i=_impl->handles.find(h);return i!=_impl->handles.end()&&_impl->mixer.isSoundHandleActive(i->second);}
}
#endif

#ifndef SNATCHER_AUDIO_CORE_ONLY
namespace Snatcher {
namespace {
void playbackProfile(NativeArchive& a,AudioProfile& p){a.flag(p.rateVerified);a.flag(p.polarityVerified);a.flag(p.gainVerified);a.flag(p.loopVerified);a.field(p.rate);a.field(p.channels);a.field(p.volume);a.flag(p.loop);a.field(p.exactRate.numerator);a.field(p.exactRate.denominator);}
}
bool CeAudioBackend::snapshotPlayback(uint64_t handle,std::vector<uint8_t>& bytes,std::string& error)const{
 auto it=_impl->playback.find(handle);if(it==_impl->playback.end()){error="PCM playback has no native continuation owner";return false;}auto& s=it->second;NativeArchive a(64*1024*1024);playbackProfile(a,s.profile);bool memory=bool(s.memory),transport=bool(s.transport);a.flag(memory);if(memory&&!static_cast<CePcmMemoryFeed*>(s.memory.get())->archive(a))return false;a.flag(transport);if(transport&&!s.transport->archive(a,error))return false;if(!a.finished())return false;bytes.swap(a.bytes);return true;
}
bool CeAudioBackend::restorePlayback(const std::shared_ptr<NativeAudioFeed>& source,const std::vector<uint8_t>& bytes,AudioRole role,uint64_t& handle,std::string& error,bool validateOnly){
 NativeArchive a(bytes,64*1024*1024);SavedPlayback saved;playbackProfile(a,saved.profile);if(!validProfile(saved.profile,error))return false;bool memory=false,transport=false;a.flag(memory);saved.source=source;if(memory){auto feed=std::make_shared<CePcmMemoryFeed>(std::vector<int16_t>(),0);if(!feed->archive(a))return false;saved.memory=saved.source=feed;}if(!saved.source){error="Playback continuation lost its native source";return false;}a.flag(transport);std::shared_ptr<NativeAudioFeed> delivered=saved.source;if(transport){saved.transport=std::make_shared<RationalPcmTransport>();if(!saved.transport->restore(saved.source,saved.profile.exactRate,_impl->mixer.getOutputRate(),a,error))return false;delivered=saved.transport;}if(!a.finished()){error="Invalid PCM playback archive";return false;}if(validateOnly)return true;
 auto type=role==kAudioMusic?Audio::Mixer::kMusicSoundType:role==kAudioSpeech?Audio::Mixer::kSpeechSoundType:role==kAudioSharedPcm?Audio::Mixer::kPlainSoundType:Audio::Mixer::kSFXSoundType;Audio::SoundHandle sound;_impl->mixer.playStream(type,&sound,new CeNativeFeedStream(delivered,role==kAudioSharedPcm),-1,saved.profile.volume,0,DisposeAfterUse::YES);handle=_impl->next++;_impl->handles[handle]=sound;_impl->playback[handle]=saved;return true;
}
}
#endif
