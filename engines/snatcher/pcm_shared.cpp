/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pcm_shared.h"
#include "native_archive.h"
#include <algorithm>
#include <array>
#include <limits>
#include <mutex>
namespace Snatcher {
namespace {
int16_t clip(int n){return int16_t(std::max(-32768,std::min(32767,n)));}
int interpolate(int last,int current,uint64_t position,uint64_t boundary){
 int64_t n=int64_t(current-last)*int64_t(position)+int64_t(boundary/2);
 return last+int(n>=0?n/int64_t(boundary):-((-n+int64_t(boundary)-1)/int64_t(boundary)));
}
void decode(uint8_t code,uint8_t level,uint8_t pan,int* out){
 int magnitude=code&127;out[0]=magnitude*level*(pan>>4)/32*2;out[1]=magnitude*level*(pan&15)/32*2;
 if(!(code&128)){out[0]=-out[0];out[1]=-out[1];}
}
}
struct NativePcmBus::Impl {
 struct Pair {
  bool active=false,muted=false,terminal=false,resident=false,pendingLoop=false;uint8_t priority=0,level=0,pan=0;
  size_t cursor=0,copy=0,source=0,loop=0,marker=0;uint64_t serial=0,position=0,boundary=0,step=0;
  uint64_t copied=0,consumed=0;int last[2]={0,0},current[2]={0,0};AudioRole role=kAudioSpeech;
  std::shared_ptr<const std::vector<uint8_t>> immutable;
 } pair[2];
 AudioBackend& backend;PreparedPcmStream stream;std::shared_ptr<PcmStreamFeed> feed;
 RationalPcmTransport ordinary;PcmStreamSourceState source;
 mutable std::mutex mutex;std::array<uint8_t,32768> ring{};std::array<bool,32768> known{};
 bool ready=false,cancelled=false,promoted=false,streamDone=false,effectBit=false,streamMode=true,resident=false,loopStream=true;
 uint64_t nextSerial=1;std::string failure;
 Impl(AudioBackend& b,const PreparedPcmStream& s,const std::shared_ptr<PcmStreamFeed>& f):backend(b),stream(s),feed(f){}
 bool audible()const{return (pair[0].active&&!pair[0].muted)||(pair[1].active&&!pair[1].muted);}
 bool promote(std::string& error){
  if(promoted)return true;
  if(!feed->sourceState(source,true,error))return false;
  const auto& p=source.progress;const uint64_t consumed=ordinary.consumedFrames();
  if(!p.publishedSourceBytes||p.cancelled){error="Shared PCM promotion requires actual published stream bytes";return false;}
  // Reconstruct only the source-copy bytes already published by the old
  // worker. The immutable input is not permission to read future disc data.
  size_t start=p.copiedCodes>32768?p.copiedCodes-32768:0;
  for(size_t i=start;i<p.copiedCodes&&i<source.source.size();++i){ring[i&32767]=(i&32767)==32767?255:source.source[i];known[i&32767]=true;}
  for(unsigned i=0;i<2;++i){Pair& v=pair[i];v.active=true;v.resident=true;v.priority=i?0:stream.source.priority;v.level=stream.effectiveLevel;v.pan=stream.effectivePan;
   v.cursor=size_t(consumed+consumed/32767)&32767;v.copy=p.copiedCodes&32767;v.source=p.copiedCodes&65535;
   v.loop=0;v.copied=p.copiedCodes;v.consumed=consumed+consumed/32767;v.position=ordinary.sourcePosition();v.boundary=ordinary.sourceBoundary();v.step=390625;
   v.terminal=p.terminalSeen;v.marker=stream.sample.sampleCount&32767;
   if(v.terminal){ring[v.marker]=255;known[v.marker]=true;v.pendingLoop=v.marker<8192&&v.cursor>=8192;if(!v.pendingLoop)v.loop=(v.marker+32767)&32767;}
   for(unsigned age=0;age<2;++age){int* value=age?v.last:v.current;if(consumed>age){size_t n=size_t(consumed-1-age);n+=n/32767;if(n<stream.sample.sampleCount)decode(source.source[n],v.level,v.pan,value);}}
  }
  promoted=true;return true;
 }
 bool copy(unsigned slot,std::string& error){
  Pair& v=pair[slot];if(!v.active||v.terminal)return true;
  const size_t mask=streamMode?32767:16383,base=streamMode?0:(v.copy&16384);
  std::vector<uint8_t> bytes;
  if(v.resident){if(!source.staging){error="Shared PCM copier has no owned resident staging";return false;}if(!source.staging->window(v.source,8192,bytes,error))return false;}
  else {if(!v.immutable||v.source>=v.immutable->size()){error="Shared PCM copier reached an unpublished source extent";return false;}size_t count=std::min(size_t(8192),v.immutable->size()-v.source);bytes.assign(v.immutable->begin()+v.source,v.immutable->begin()+v.source+count);}
  size_t used=0;for(;used<8192;++used){size_t destination=base+((v.copy-base)&mask);
   if(((destination-base)&mask)==mask){ring[destination]=255;known[destination]=true;
    if(streamMode&&slot==0&&!v.serial&&used<bytes.size()&&bytes[used]==255){v.terminal=true;v.marker=destination;v.loop=(destination+32767)&32767;++used;break;}
    ++v.copy;++v.copied;continue;}
   if(used>=bytes.size()){error="Shared PCM copy lacks known data before a real FF endpoint";return false;}
   uint8_t code=bytes[used];ring[destination]=code;known[destination]=true;
   if(code==255){v.terminal=true;v.marker=destination;v.pendingLoop=((destination-base)&mask)<8192&&((v.cursor-base)&mask)>=8192;if(!v.pendingLoop){v.loop=base+((destination-base+mask)&mask);if(slot==0&&loopStream)pair[1].loop=v.loop;}++used;break;}
   ++v.copy;++v.copied;
  }
  v.source+=used;v.copy=base+((v.copy-base)&mask);
  if(!v.terminal&&v.source==65536&&v.resident)v.source=0;
  if(!v.terminal&&v.copy==base){v.loop=base;if(slot==0&&loopStream)pair[1].loop=base;}
  // Resource requests belong to logical slot0's source-copy producer. Only a
  // later worker visit publishes them; playback never performs file I/O.
  if(slot==0&&streamMode&&!v.terminal&&source.nextSource<source.source.size()&&!source.progress.pendingBytes){
   size_t pointer=v.source;
   bool lower=(source.nextSource/32768)%2==0;
   if((lower&&(pointer==49152||pointer==57344))||(!lower&&(pointer==16384||pointer==24576))){
    source.progress.pendingSourceOffset=source.nextSource;source.progress.pendingBytes=std::min(size_t(32768),source.source.size()-source.nextSource);source.pendingDestination=lower?0:32768;
   }
  }
  return true;
 }
 void retire(unsigned slot){
  pair[slot].active=false;pair[slot].priority=0;
  if(streamMode&&slot==0){if(pair[1].serial)pair[1].muted=true;else pair[1].active=false;streamDone=true;streamMode=false;resident=false;source.progress.pendingBytes=0;}
  else if(resident)effectBit=false;
 }
 bool next(unsigned slot){
  Pair& v=pair[slot];size_t mask=streamMode?32767:16383,base=streamMode?0:(v.copy&16384);
  if(!v.terminal&&(slot==0||v.serial)&&v.cursor==v.copy)return false;
  size_t cursor=v.cursor&32767;
  if(v.pendingLoop&&(cursor&mask)<8192){v.pendingLoop=false;v.loop=(v.marker+32767)&32767;if(slot==0&&loopStream)pair[1].loop=v.loop;}
  // The source monitor compares its published endpoint with the playhead,
  // independently of a later writer's byte at that position.
  if(v.terminal&&cursor==v.marker&&((streamMode&&slot==0)||(resident&&(slot==0||v.serial)))){retire(slot);return true;}
  if(!known[cursor])return false;
  if(ring[cursor]==255){
   if(v.terminal&&cursor==v.marker&&((streamMode&&slot==0)||(resident&&(slot==0||v.serial)))){retire(slot);return true;}
   if((cursor-base)==mask)++v.consumed;
   cursor=v.loop&32767;
   if(v.terminal&&cursor==v.marker&&((streamMode&&slot==0)||(resident&&(slot==0||v.serial)))){retire(slot);return true;}
   if(!known[cursor]||ring[cursor]==255){failure="Shared PCM source loop has no published sample";return false;}
  }
  v.last[0]=v.current[0];v.last[1]=v.current[1];decode(ring[cursor],v.level,v.pan,v.current);
  v.cursor=(cursor+1)&32767;++v.consumed;return true;
 }
};
class NativePcmBus::Job:public NativeAudioFeed {
 friend class NativePcmBus;
 std::weak_ptr<NativePcmBus> bus;unsigned slot;uint64_t serial;uint32_t rate;
public:
 Job(const std::shared_ptr<NativePcmBus>& b,unsigned s,uint64_t id):bus(b),slot(s),serial(id),rate(b->sourceRate()){}
 bool jobIdentity(const void* owner,unsigned& s,uint64_t& id)const override{if(bus.lock().get()!=owner)return false;s=slot;id=serial;return true;}
 uint32_t sourceRate()const override{return rate;}
 size_t readFrames(int16_t*,size_t)override{return 0;}
 bool ended()const override{auto b=bus.lock();return !b||b->jobEnded(slot,serial);}
 std::string error()const override{auto b=bus.lock();return b?b->error():std::string();}
};
NativePcmBus::NativePcmBus(AudioBackend& b,const PreparedPcmStream& s,const std::shared_ptr<PcmStreamFeed>& f):_impl(new Impl(b,s,f)){}
NativePcmBus::NativePcmBus(AudioBackend& backend,const std::shared_ptr<ResidentPcmStaging>& staging,const PcmIdleContext& context):_impl(new Impl(backend,PreparedPcmStream(),{})){_impl->source.staging=staging;_impl->promoted=true;_impl->streamDone=true;_impl->streamMode=false;_impl->resident=context.mode==1;_impl->loopStream=context.copyMode==2;}
NativePcmBus::~NativePcmBus()=default;
bool NativePcmBus::initialize(std::string& error){if(!_impl->stream.sourceValidated||!_impl->feed){error="Shared PCM requires a source-validated stream feed";return false;}return _impl->ordinary.start(_impl->feed,_impl->stream.profile.exactRate,sourceRate(),error);}
void NativePcmBus::activate(){std::lock_guard<std::mutex> lock(_impl->mutex);_impl->ready=true;}
void NativePcmBus::deactivate(){std::lock_guard<std::mutex> lock(_impl->mutex);_impl->ready=false;}
void NativePcmBus::cancel(){std::lock_guard<std::mutex> lock(_impl->mutex);_impl->cancelled=true;_impl->source.progress.pendingBytes=0;if(_impl->feed)_impl->feed->cancel();}
void NativePcmBus::silence(){std::lock_guard<std::mutex> lock(_impl->mutex);std::string error;if(!_impl->promote(error)){_impl->failure=error;return;}for(auto& v:_impl->pair)v.muted=true;_impl->effectBit=false;}
bool NativePcmBus::residentMode(std::string& error){std::lock_guard<std::mutex> lock(_impl->mutex);if(!_impl->promote(error))return false;_impl->streamMode=false;_impl->resident=true;_impl->loopStream=false;_impl->source.progress.pendingBytes=0;return true;}
bool NativePcmBus::finishStream(std::string& error){std::lock_guard<std::mutex> lock(_impl->mutex);if(_impl->streamDone)return true;if(!_impl->promote(error)){_impl->failure=error;return false;}_impl->retire(0);return true;}
bool NativePcmBus::idleContext(PcmIdleContext& out,std::string& error)const{std::lock_guard<std::mutex> lock(_impl->mutex);error.clear();if(_impl->pair[0].active||_impl->pair[1].active||_impl->source.progress.pendingBytes){error="PCM context still owns active pairs or a pending source publication";return false;}PcmIdleContext next;next.known=true;next.mode=_impl->resident?1:0;next.copyMode=_impl->loopStream?2:(_impl->resident?1:0);if(!validatePcmIdleContext(next,error))return false;out=next;return true;}
bool NativePcmBus::service(std::string& error){
 std::lock_guard<std::mutex> lock(_impl->mutex);error.clear();if(!_impl->failure.empty()){error=_impl->failure;return false;}if(_impl->cancelled||(!_impl->ready&&!_impl->promoted))return true;
 if(!_impl->promoted)return _impl->feed->serviceRefill(error);
 auto& source=_impl->source;if(source.progress.pendingBytes){size_t count=source.progress.pendingBytes;if(!source.staging->publishStreamTransfer(_impl->stream.source.id,source.pendingDestination,source.source.data()+source.progress.pendingSourceOffset,count,error))return false;source.nextSource+=count;source.progress.pendingBytes=0;}
 for(int slot=1;slot>=0;--slot){auto& v=_impl->pair[slot];if(_impl->resident&&v.active&&v.muted&&v.terminal&&v.cursor==v.marker&&(slot==0||v.serial))_impl->retire(slot);size_t mask=_impl->streamMode?32767:16383;size_t ahead=(v.copy-v.cursor)&mask;if((slot==0||v.serial)&&v.active&&!v.terminal&&ahead<8192&&!_impl->copy(slot,error)){_impl->failure=error;return false;}}
 return true;
}
uint8_t NativePcmBus::priority(unsigned slot)const{std::lock_guard<std::mutex> lock(_impl->mutex);if(slot>1)return 0;return _impl->promoted?_impl->pair[slot].priority:slot?0:_impl->stream.source.priority;}
bool NativePcmBus::effectActivity()const{std::lock_guard<std::mutex> lock(_impl->mutex);return _impl->effectBit;}
void NativePcmBus::noteEffectRequest(){std::lock_guard<std::mutex> lock(_impl->mutex);_impl->effectBit=true;}
size_t NativePcmBus::pendingSourceBytes()const{std::lock_guard<std::mutex> lock(_impl->mutex);return _impl->cancelled?0:(_impl->promoted?_impl->source.progress.pendingBytes:_impl->feed->snapshot().pendingBytes);}
size_t NativePcmBus::playbackCursor(unsigned slot)const{std::lock_guard<std::mutex> lock(_impl->mutex);return slot<2?_impl->pair[slot].cursor:0;}
bool NativePcmBus::owns(const std::shared_ptr<ResidentPcmStaging>& staging)const{std::lock_guard<std::mutex> lock(_impl->mutex);if(_impl->promoted)return _impl->source.staging==staging;PcmStreamSourceState state;std::string error;return _impl->feed&&_impl->feed->sourceState(state,false,error)&&state.staging==staging;}
bool NativePcmBus::replace(const PreparedPcmEffect& effect,const std::function<bool(const std::shared_ptr<NativeAudioFeed>&,std::string&)>& admit,std::string& error){
 std::lock_guard<std::mutex> lock(_impl->mutex);error.clear();bool wasPromoted=_impl->promoted;if(!_impl->promote(error))return false;
 auto failPromotion=[&](){if(!wasPromoted)_impl->promoted=false;};
 unsigned slot=effect.source.channel;if(slot>1||effect.source.priority<_impl->pair[slot].priority){error="Shared PCM replacement does not own selected priority";failPromotion();return false;}
 // Prepare against a private copy. Failed source validation or admission
 // changes neither published bytes nor the old audible owner.
 auto old=_impl->pair[slot];auto oldOther=_impl->pair[1-slot];auto ring=_impl->ring;auto known=_impl->known;auto source=_impl->source;
 Impl::Pair next;next.active=true;next.serial=_impl->nextSerial;next.priority=effect.source.priority;next.level=effect.effectiveLevel;next.pan=effect.effectivePan;
 next.cursor=next.copy=slot*16384;next.loop=next.cursor;next.role=kAudioEffect;next.resident=effect.source.source==kResidentSource;next.source=next.resident?effect.sample.payloadOffset:0;next.immutable=std::make_shared<const std::vector<uint8_t>>(effect.sourceCodes);
 next.boundary=uint64_t(effect.profile.exactRate.denominator)*sourceRate();next.position=next.boundary;next.step=effect.profile.exactRate.numerator;
 if(!next.step||!next.boundary){error="Shared PCM effect lacks exact pitch ownership";failPromotion();return false;}
 _impl->pair[slot]=next;bool copied=_impl->copy(slot,error);auto prepared=_impl->pair[slot];auto preparedOther=_impl->pair[1-slot];auto preparedRing=_impl->ring;auto preparedKnown=_impl->known;auto preparedSource=_impl->source;
 _impl->pair[slot]=old;_impl->pair[1-slot]=oldOther;_impl->ring=ring;_impl->known=known;_impl->source=source;
 if(!copied){failPromotion();return false;}
 // Job construction must not reenter this mutex through sourceRate().
 auto job=std::shared_ptr<NativeAudioFeed>(new Job(shared_from_this(),slot,next.serial));
 if(!admit(job,error)){failPromotion();return false;}
 _impl->pair[slot]=prepared;_impl->pair[1-slot]=preparedOther;_impl->ring=preparedRing;_impl->known=preparedKnown;_impl->source=preparedSource;++_impl->nextSerial;_impl->effectBit=true;return true;
}
std::shared_ptr<NativeAudioFeed> NativePcmBus::streamJob(){return std::shared_ptr<NativeAudioFeed>(new Job(shared_from_this(),2,0));}
bool NativePcmBus::jobEnded(unsigned slot,uint64_t serial)const{std::lock_guard<std::mutex> lock(_impl->mutex);if(_impl->cancelled)return true;if(slot==2)return _impl->streamDone||(!_impl->promoted&&_impl->ordinary.ended());return !_impl->pair[slot].active||_impl->pair[slot].serial!=serial;}
uint32_t NativePcmBus::sourceRate()const{return _impl->backend.outputRate();}
size_t NativePcmBus::readFrames(int16_t* out,size_t count){
 std::lock_guard<std::mutex> lock(_impl->mutex);if(!_impl->ready||_impl->cancelled||!out||!_impl->failure.empty())return 0;
 if(!_impl->promoted){size_t n=_impl->ordinary.readFrames(out,count);unsigned gain=_impl->backend.roleVolume(kAudioSpeech);if(gain!=256)for(size_t i=0;i<n*2;++i)out[i]=int16_t(int(out[i])*int(gain)/256);return n;}
 size_t made=0;for(;made<count;++made){if(_impl->streamDone&&!_impl->audible())break;if(!_impl->pair[0].active&&!_impl->pair[1].active){out[made*2]=out[made*2+1]=0;continue;}int mix[2]={0,0};
  const auto before0=_impl->pair[0],before1=_impl->pair[1];
  for(unsigned slot=0;slot<2;++slot){auto& v=_impl->pair[slot];if(!v.active||v.muted)continue;
   while(v.position>=v.boundary){if(!_impl->next(slot)){if(_impl->pair[0].active)_impl->pair[0]=before0;if(_impl->pair[1].active)_impl->pair[1]=before1;return made;}if(!v.active)break;v.position-=v.boundary;}
   if(!v.active)continue;unsigned gain=_impl->backend.roleVolume(v.role);for(unsigned c=0;c<2;++c)mix[c]+=interpolate(v.last[c],v.current[c],v.position,v.boundary)*int(gain)/256;v.position+=v.step;
  }
  if(_impl->streamDone&&!_impl->audible())break;
  out[made*2]=clip(mix[0]);out[made*2+1]=clip(mix[1]);
 }return made;
}
bool NativePcmBus::ended()const{std::lock_guard<std::mutex> lock(_impl->mutex);return _impl->cancelled||(_impl->ready&&(_impl->promoted?(_impl->streamDone&&!_impl->audible()):_impl->ordinary.ended()));}
std::string NativePcmBus::error()const{std::lock_guard<std::mutex> lock(_impl->mutex);return _impl->failure.empty()?(_impl->feed?_impl->feed->error():std::string()):_impl->failure;}
}

namespace Snatcher {
std::shared_ptr<NativeAudioFeed> NativePcmBus::restoreJob(unsigned slot,uint64_t serial){return std::shared_ptr<NativeAudioFeed>(new Job(shared_from_this(),slot,serial));}
bool NativePcmBus::identifyJob(const std::shared_ptr<NativeAudioFeed>& f,unsigned& slot,uint64_t& serial)const{return f&&f->jobIdentity(this,slot,serial);}
bool NativePcmBus::archive(NativeArchive& a,std::string& error){
 std::lock_guard<std::mutex> lock(_impl->mutex);auto& b=*_impl;
 a.flag(b.ready);a.flag(b.cancelled);a.flag(b.promoted);a.flag(b.streamDone);a.flag(b.effectBit);a.flag(b.streamMode);a.flag(b.resident);a.flag(b.loopStream);a.field(b.nextSerial);a.text(b.failure);
 for(auto& v:b.ring)a.field(v);for(auto& v:b.known)a.flag(v);
 for(auto& v:b.pair){a.flag(v.active);a.flag(v.muted);a.flag(v.terminal);a.flag(v.resident);a.flag(v.pendingLoop);a.field(v.priority);a.field(v.level);a.field(v.pan);a.number(v.cursor);a.number(v.copy);a.number(v.source);a.number(v.loop);a.number(v.marker);a.field(v.serial);a.field(v.position);a.field(v.boundary);a.field(v.step);a.field(v.copied);a.field(v.consumed);for(auto& n:v.last)a.field(n);for(auto& n:v.current)a.field(n);a.field(v.role);
  bool immutable=bool(v.immutable);a.flag(immutable);if(immutable){std::vector<uint8_t> bytes=a.reading?std::vector<uint8_t>():*v.immutable;a.vector(bytes,16*1024*1024);if(a.reading)v.immutable=std::make_shared<const std::vector<uint8_t>>(bytes);}
  if(v.cursor>=32768||v.copy>=32768||v.loop>=32768||v.marker>=32768||(v.active&&(!v.step||!v.boundary))||v.role>kAudioSharedPcm)a.ok=false;
 }
 auto& s=b.stream;a.field(s.source.id);a.field(s.source.priority);a.number(s.sample.sampleCount);a.field(s.effectiveLevel);a.field(s.effectivePan);
 auto& src=b.source;a.vector(src.source,16*1024*1024);auto& q=src.progress;a.number(q.copiedCodes);a.number(q.consumedFrames);a.number(q.queuedFrames);a.number(q.publishedSourceBytes);a.number(q.pendingSourceOffset);a.number(q.pendingBytes);a.flag(q.terminalSeen);a.flag(q.cancelled);a.number(src.nextSource);a.number(src.nextTrigger);a.number(src.pendingDestination);
 bool feed=bool(b.feed);a.flag(feed);if(feed){if(a.reading)b.feed=std::make_shared<PcmStreamFeed>();if(!b.feed->archive(a,error,b.source.staging))return false;
  // Rebind the transport to the restored feed before restoring its exact age.
  if(a.reading){if(!b.ordinary.restore(b.feed,sourcePcmStreamProfile().exactRate,sourceRate(),a,error))return false;}
  else if(!b.ordinary.archive(a,error))return false;
 }
 if(src.nextSource>src.source.size()||q.pendingSourceOffset>src.source.size()||q.pendingBytes>src.source.size()-q.pendingSourceOffset||src.pendingDestination>65536||!b.nextSerial)a.ok=false;
 if(!a.ok)error="Invalid shared PCM continuation";return a.ok;
}
}
