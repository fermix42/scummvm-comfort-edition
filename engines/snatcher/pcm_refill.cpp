/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <mutex>
#include "pcm_refill.h"
#include "native_archive.h"
#include <algorithm>
#include <limits>
namespace Snatcher {
struct ResidentPcmMutex { std::mutex value; };
bool completedPcmStreamPrefix(const uint8_t* bytes,size_t size,std::vector<uint8_t>& prefix,std::string& error){
 error.clear();if(!bytes||!size||size%2048||size>size_t(65535)*2048){error="Completed stream requires a finite original sector extent";return false;}
 // SUB1561E..15658 starts with up to32 sectors at the allocation's base.
 // SUB15898/158A0..158F4 subsequently alternates the two16-sector halves.
 // Partial final transfers replace only their real bytes; the rest of that
 // half retains its preceding write, not zeros or a rotated final64K slice.
 size_t initial=std::min(size,size_t(65536));std::vector<uint8_t> next(bytes,bytes+initial);
 size_t offset=initial;unsigned half=0;
 while(offset<size){size_t count=std::min(size-offset,size_t(32768));size_t destination=size_t(half)*32768;std::copy(bytes+offset,bytes+offset+count,next.begin()+destination);offset+=count;half^=1;}
 prefix.swap(next);return true;
}
ResidentPcmStaging::ResidentPcmStaging(size_t capacity):_mutex(new ResidentPcmMutex),_bytes(capacity),_known(0),_bank(0xffff),_generation(0),_writerKind(kPcmWriterNone),_writerId(0){}
bool ResidentPcmStaging::replacePrefix(uint16_t bank,const uint8_t* bytes,size_t size,std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);if(!bank||bank==0xffff||!bytes||!size||size>_bytes.size()||_generation==std::numeric_limits<uint64_t>::max()){error="Resident source replacement has invalid bank/extent/generation";return false;}
 std::copy(bytes,bytes+size,_bytes.begin());_known=std::max(_known,size);_bank=bank;++_generation;_writerKind=kPcmWriterResident;_writerId=bank;return true;
}
bool ResidentPcmStaging::beginStream(uint16_t id,const uint8_t* bytes,size_t size,bool& pending,std::string& error){
 error.clear();if(!bytes||!size||size%2048||size>size_t(65535)*2048||std::min(size,size_t(65536))>capacity()){error="Stream start requires a finite original sector allocation";return false;}
 // SUB15620..1563C removes the later-transfer owner for <=32 sectors;
 // SUB15674 waits for that complete prefill before publishing playback.
 // Later asynchronous half writes are not inferred from elapsed audio time.
 const bool later=size>65536;
 if(!(later?invalidateStream(id,error):completeStream(id,bytes,size,error)))return false;
 pending=later;return true;
}
bool ResidentPcmStaging::completeStream(uint16_t id,const uint8_t* bytes,size_t size,std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);
 if(!id||id==0xffff||std::min(size,size_t(65536))>_bytes.size()||_generation==std::numeric_limits<uint64_t>::max()){error="Completed stream requires a valid writer, allocation and generation";return false;}
 std::vector<uint8_t> prefix;if(!completedPcmStreamPrefix(bytes,size,prefix,error))return false;
 std::copy(prefix.begin(),prefix.end(),_bytes.begin());_known=std::max(_known,prefix.size());++_generation;_writerKind=kPcmWriterStream;_writerId=id;return true;
}
bool ResidentPcmStaging::publishStreamTransfer(uint16_t id,size_t destination,const uint8_t* bytes,size_t size,std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);
 if(!id||id==0xffff||!bytes||!size||size%2048||(destination!=0&&destination!=32768)||size>65536-destination||(destination&&size>32768)||destination>_known||destination+size>_bytes.size()||_generation==std::numeric_limits<uint64_t>::max()){error="Stream transfer lacks a finite owned source destination";return false;}
 std::copy(bytes,bytes+size,_bytes.begin()+destination);_known=std::max(_known,destination+size);++_generation;_writerKind=kPcmWriterStream;_writerId=id;return true;
}
PcmStreamFeed::PcmStreamFeed():_mutex(new ResidentPcmMutex),_id(0),_rate(0),_encoded(0),_nextSource(0),_nextTrigger(49152),_pendingDestination(0){}
PcmStreamFeed::~PcmStreamFeed()=default;
bool PcmStreamFeed::start(const std::shared_ptr<ResidentPcmStaging>& staging,uint16_t id,const uint8_t* source,size_t size,const std::vector<int16_t>& pcm,size_t encoded,uint32_t rate,std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);
 if(_id||!staging||!id||id==0xffff||!source||!size||size%2048||size>size_t(65535)*2048||!encoded||encoded>size||encoded>std::numeric_limits<size_t>::max()/2||pcm.size()!=(encoded-encoded/32768)*2||rate<1000||rate>192000){error="Stream feed requires validated encoded and played extents";return false;}
 std::vector<uint8_t> bytes(source,source+size);std::vector<int16_t> played(pcm);
 const size_t initial=std::min(size,size_t(65536));if(staging->capacity()<initial){error="Stream prefill exceeds its allocation";return false;}
 _source.swap(bytes);_pcm.swap(played);_staging=staging;_id=id;_encoded=encoded;_rate=rate;_nextSource=initial;return prefetch(error);
}
bool PcmStreamFeed::publishInitial(std::string& error){std::lock_guard<std::mutex> lock(_mutex->value);error.clear();if(!_id||_state.cancelled||_state.publishedSourceBytes){error="Stream initial publication requires a newly accepted owner";return false;}if(!_staging->publishStreamTransfer(_id,0,_source.data(),_nextSource,error))return false;_state.publishedSourceBytes=_nextSource;return true;}
bool PcmStreamFeed::prefetch(std::string& error){
 if(_state.terminalSeen||_state.cancelled)return true;
 const size_t before=_state.copiedCodes,next=std::min(_encoded,before+8192);
 const size_t first=before-before/32768,last=next-next/32768;
 if(last-first>16384-_queue.size()/2){error="Stream prefetch exceeds its finite copy window";return false;}
 _queue.insert(_queue.end(),_pcm.begin()+first*2,_pcm.begin()+last*2);_state.copiedCodes=next;_state.terminalSeen=next==_encoded;
 if(next>=_nextTrigger&&_nextSource<_source.size()){
  _state.pendingSourceOffset=_nextSource;_state.pendingBytes=std::min(size_t(32768),_source.size()-_nextSource);
  _pendingDestination=((_nextSource-65536)/32768&1)*32768;_nextTrigger+=32768;
 }
 return true;
}
bool PcmStreamFeed::serviceRefill(std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);if(!_id||!_state.publishedSourceBytes){error="Stream refill has no published source owner";return false;}if(!_error.empty()){error=_error;return false;}if(_state.cancelled)return true;
 // Only this worker completes resource service; the mixer never publishes
 // allocation bytes. A queued request alone does not establish knowledge.
 if(_state.pendingBytes){const size_t count=_state.pendingBytes;if(!_staging->publishStreamTransfer(_id,_pendingDestination,_source.data()+_state.pendingSourceOffset,count,error)){_error=error;return false;}_nextSource+=count;_state.publishedSourceBytes=_nextSource;_state.pendingBytes=0;_state.pendingSourceOffset=0;}
 const size_t consumed=std::min(_encoded,_state.consumedFrames+_state.consumedFrames/32767);
 if(!_state.terminalSeen&&_state.copiedCodes-consumed<8192&&!prefetch(error)){_error=error;return false;}return true;
}
void PcmStreamFeed::cancel(){std::lock_guard<std::mutex> lock(_mutex->value);_state.cancelled=true;_state.pendingBytes=0;_state.pendingSourceOffset=0;_queue.clear();}
PcmStreamFeedSnapshot PcmStreamFeed::snapshot()const{std::lock_guard<std::mutex> lock(_mutex->value);auto state=_state;state.queuedFrames=_queue.size()/2;return state;}
bool PcmStreamFeed::sourceState(PcmStreamSourceState& state,bool includeBytes,std::string& error)const{std::lock_guard<std::mutex> lock(_mutex->value);error.clear();if(!_id||!_error.empty()){error=_error.empty()?"Stream source has no owner":_error;return false;}state.staging=_staging;state.progress=_state;state.nextSource=_nextSource;state.nextTrigger=_nextTrigger;state.pendingDestination=_pendingDestination;if(includeBytes)state.source=_source;return true;}
uint32_t PcmStreamFeed::sourceRate()const{return _rate;}
size_t PcmStreamFeed::readFrames(int16_t* out,size_t frames){std::lock_guard<std::mutex> lock(_mutex->value);if(!_id||!_error.empty()||_state.cancelled||!frames||!_state.publishedSourceBytes)return 0;if(!out||frames>1000000){_error="Stream pull requires a finite native destination";return 0;}const size_t count=std::min(frames,_queue.size()/2);for(size_t i=0;i<count*2;++i){out[i]=_queue.front();_queue.pop_front();}_state.consumedFrames+=count;return count;}
bool PcmStreamFeed::ended()const{std::lock_guard<std::mutex> lock(_mutex->value);return _error.empty()&&(_state.cancelled||(_state.terminalSeen&&_queue.empty()));}
std::string PcmStreamFeed::error()const{std::lock_guard<std::mutex> lock(_mutex->value);return _error;}
bool ResidentPcmStaging::copyKnownBytes(std::vector<uint8_t>& bytes,std::string& error)const{
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);if(!_known){error="Source allocation has no known bytes";return false;}
 std::vector<uint8_t> next(_bytes.begin(),_bytes.begin()+_known);bytes.swap(next);return true;
}
bool ResidentPcmStaging::invalidateStream(uint16_t id,std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);if(!id||id==0xffff||_bytes.empty()||_generation==std::numeric_limits<uint64_t>::max()){error="Unknown stream allocation requires a valid writer identity";return false;}
 _known=0;++_generation;_writerKind=kPcmWriterStream;_writerId=id;return true;
}
bool ResidentPcmStaging::snapshot(ResidentPcmStagingSnapshot& state,std::string& error)const{
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);if(_bytes.empty()){error="Source allocation has no finite capacity";return false;}
 ResidentPcmStagingSnapshot next;next.capacity=_bytes.size();next.knownBytes.assign(_bytes.begin(),_bytes.begin()+_known);next.bank=_bank;next.generation=_generation;next.writerKind=_writerKind;next.writerId=_writerId;state=next;return true;
}
bool ResidentPcmStaging::restore(const ResidentPcmStagingSnapshot& state,std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);
 bool empty=state.writerKind==kPcmWriterNone;
 if(!state.capacity||state.capacity!=_bytes.size()||state.knownBytes.size()>state.capacity||state.bank==0||state.writerKind<kPcmWriterNone||state.writerKind>kPcmWriterStream||
    (empty&&(!state.knownBytes.empty()||state.bank!=0xffff||state.generation||state.writerId))||
    (!empty&&(!state.generation||!state.writerId||state.writerId==0xffff))||
    (state.writerKind==kPcmWriterResident&&(state.bank==0xffff||state.writerId!=state.bank||state.knownBytes.empty()))||
    (state.writerKind==kPcmWriterStream&&!state.knownBytes.empty()&&state.knownBytes.size()<2048)){
  error="Malformed source allocation snapshot ownership or extent";return false;
 }
 std::vector<uint8_t> next(state.capacity);std::copy(state.knownBytes.begin(),state.knownBytes.end(),next.begin());_bytes.swap(next);_known=state.knownBytes.size();_bank=state.bank;_generation=state.generation;_writerKind=state.writerKind;_writerId=state.writerId;return true;
}
size_t ResidentPcmStaging::capacity()const{std::lock_guard<std::mutex> lock(_mutex->value);return _bytes.size();}
size_t ResidentPcmStaging::knownBytes()const{std::lock_guard<std::mutex> lock(_mutex->value);return _known;}
uint16_t ResidentPcmStaging::bank()const{std::lock_guard<std::mutex> lock(_mutex->value);return _bank;}
uint64_t ResidentPcmStaging::generation()const{std::lock_guard<std::mutex> lock(_mutex->value);return _generation;}
bool ResidentPcmStaging::window(size_t offset,size_t maximum,std::vector<uint8_t>& bytes,std::string& error)const{
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);if(offset>=_known){error="Resident refill reaches unknown source staging bytes";return false;}
 size_t count=std::min(maximum,_known-offset);std::vector<uint8_t> next(_bytes.begin()+offset,_bytes.begin()+offset+count);bytes.swap(next);return true;
}
ResidentPcmStaging::~ResidentPcmStaging() = default;
PcmResidentFeed::PcmResidentFeed():_mutex(new ResidentPcmMutex){}
PcmResidentFeed::~PcmResidentFeed() = default;
bool PcmResidentFeed::start(const std::shared_ptr<ResidentPcmStaging>& staging,const ResidentPcmFeedParameters& parameters,std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);
 if(_state.started||!staging||parameters.rate<1000||parameters.rate>192000||parameters.payloadOffset<16||parameters.payloadOffset>=staging->knownBytes()){error="Resident feed requires an owned source profile and known sample payload";return false;}
 _staging=staging;_parameters=parameters;_state=ResidentPcmFeedSnapshot();_state.started=true;_state.sourceCursor=parameters.payloadOffset;
 if(!prefetch(error)){_state.error=error;return false;}return true;
}
bool PcmResidentFeed::prefetch(std::string& error){
 // Caller holds the voice mutex. Staging returns one consistent prefix/tail
 // generation; it never calls a voice or story owner while holding its lock.
 std::vector<uint8_t> bytes;if(!_staging->window(_state.sourceCursor,8192,bytes,error))return false;
 ResidentPcmFeedSnapshot next=_state;std::deque<Frame> frames;size_t sourceStart=_state.sourceCursor;
 unsigned left=_parameters.pan>>4,right=_parameters.pan&15;if(_parameters.mono)left=right=(left+right)/2;
 unsigned level=_parameters.scale?(unsigned(_parameters.level)*_parameters.scale)>>8:_parameters.level;if(_parameters.quiet)level=(level*0x70)>>8;
 for(size_t i=0;i<8192;++i){
  // Source15C18..15C88 installs its boundary marker instead of the last code
  // of each16K source-copy cycle, then advances the source cursor over it.
  // Model that game-copy omission without any device registers or wave RAM.
  if((next.copiedUnits&16383)==16383){++next.copiedUnits;++next.droppedSourceUnits;continue;}
  if(i>=bytes.size()){error="Resident refill lacks known bytes before a real source end marker";return false;}
  unsigned code=bytes[i];if(code==255){next.terminalSeen=true;next.terminalOffset=sourceStart+i;break;}
  unsigned magnitude=code&127;int l=int(magnitude*level*left/32)*2,r=int(magnitude*level*right/32)*2;if(!(code&128)){l=-l;r=-r;}
  Frame frame;frame.left=int16_t(l);frame.right=int16_t(r);frame.ordinal=next.copiedUnits++;frames.push_back(frame);
 }
 if(!next.terminalSeen){if(sourceStart>std::numeric_limits<size_t>::max()-8192){error="Resident refill cursor overflow";return false;}next.sourceCursor=sourceStart+8192;if(next.sourceCursor==65536)next.sourceCursor=0;}
 if(frames.size()>16384-_queue.size()){error="Resident source prefetch queue exceeds its finite owned window";return false;}
 for(const Frame& frame:frames)_queue.push_back(frame);++next.refills;next.queuedFrames=_queue.size();next.finished=next.terminalSeen&&_queue.empty();_state=next;return true;
}
bool PcmResidentFeed::serviceRefill(std::string& error){
 error.clear();std::lock_guard<std::mutex> lock(_mutex->value);if(!_state.started){error="Resident refill has no started source owner";return false;}
 if(!_state.error.empty()){error=_state.error;return false;}
 if(_state.terminalSeen||_state.copiedUnits-_state.consumedUnits>=8192)return true;
 if(!prefetch(error)){_state.error=error;return false;}return true;
}
bool PcmResidentFeed::owns(const std::shared_ptr<ResidentPcmStaging>& staging)const{std::lock_guard<std::mutex> lock(_mutex->value);return _state.started&&_staging==staging;}
ResidentPcmFeedSnapshot PcmResidentFeed::snapshot()const{std::lock_guard<std::mutex> lock(_mutex->value);return _state;}
uint32_t PcmResidentFeed::sourceRate()const{std::lock_guard<std::mutex> lock(_mutex->value);return _parameters.rate;}
size_t PcmResidentFeed::readFrames(int16_t* stereo,size_t frames){
 std::lock_guard<std::mutex> lock(_mutex->value);if(!_state.started||!_state.error.empty()||!frames)return 0;
 if(!stereo||frames>1000000){_state.error="Resident source pull has invalid destination/finite extent";return 0;}
 // A callback may exhaust the current publication before the worker runs.
 // Return only actual available frames; readiness is neither EOF nor a
 // permanent source error. Unknown allocation bytes still fail in prefetch.
 size_t count=std::min(frames,_queue.size());for(size_t i=0;i<count;++i){const Frame& frame=_queue.front();stereo[i*2]=frame.left;stereo[i*2+1]=frame.right;_state.consumedUnits=frame.ordinal+1;_queue.pop_front();}
 _state.renderedFrames+=count;_state.queuedFrames=_queue.size();_state.finished=_state.terminalSeen&&_queue.empty();return count;
}
bool PcmResidentFeed::ended()const{std::lock_guard<std::mutex> lock(_mutex->value);return _state.started&&_state.error.empty()&&_state.finished;}
std::string PcmResidentFeed::error()const{std::lock_guard<std::mutex> lock(_mutex->value);return _state.error;}
}

namespace Snatcher {
bool PcmStreamFeed::archive(NativeArchive& a,std::string& error,const std::shared_ptr<ResidentPcmStaging>& staging){
 std::lock_guard<std::mutex> lock(_mutex->value);if(a.reading)_staging=staging;
 a.vector(_source,16*1024*1024);a.vector(_pcm,16*1024*1024);a.queue(_queue,32768);a.field(_id);a.field(_rate);a.number(_encoded);a.number(_nextSource);a.number(_nextTrigger);a.number(_pendingDestination);
 auto& s=_state;a.number(s.copiedCodes);a.number(s.consumedFrames);a.number(s.queuedFrames);a.number(s.publishedSourceBytes);a.number(s.pendingSourceOffset);a.number(s.pendingBytes);a.flag(s.terminalSeen);a.flag(s.cancelled);a.text(_error);
 if(!_staging||!_id||_rate<1000||_rate>192000||_encoded>_source.size()||_nextSource>_source.size()||s.copiedCodes>_encoded||s.pendingSourceOffset>_source.size()||s.pendingBytes>_source.size()-s.pendingSourceOffset||s.consumedFrames>_pcm.size()/2||_queue.size()%2||_pendingDestination>65536)a.ok=false;
 if(!a.ok)error="Invalid active PCM stream continuation";return a.ok;
}
bool PcmResidentFeed::archive(NativeArchive& a,std::string& error,const std::shared_ptr<ResidentPcmStaging>& staging){
 std::lock_guard<std::mutex> lock(_mutex->value);if(a.reading)_staging=staging;
 auto& p=_parameters;a.number(p.payloadOffset);a.field(p.rate);a.field(p.level);a.field(p.pan);a.field(p.scale);a.flag(p.mono);a.flag(p.quiet);
 auto& s=_state;a.flag(s.started);a.flag(s.terminalSeen);a.flag(s.finished);a.number(s.sourceCursor);a.number(s.terminalOffset);a.number(s.queuedFrames);a.field(s.copiedUnits);a.field(s.consumedUnits);a.field(s.renderedFrames);a.field(s.droppedSourceUnits);a.field(s.refills);a.text(s.error);
 a.objects(_queue,16384,[&](Frame& f){a.field(f.left);a.field(f.right);a.field(f.ordinal);});
 if(!_staging||p.rate<1000||p.rate>192000||s.sourceCursor>_staging->capacity()||s.consumedUnits>s.copiedUnits)a.ok=false;
 if(!a.ok)error="Invalid active resident PCM continuation";return a.ok;
}
}
