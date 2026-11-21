/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../pcm_refill.h"
#include <cstdio>
#include <stdexcept>
#include <thread>
namespace {
unsigned checks;
void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL:%s\n",message);throw std::runtime_error(message);}}
std::vector<uint8_t> data(size_t terminal,uint8_t code=0x81){std::vector<uint8_t> bytes(terminal+17,code);bytes[terminal+16]=255;return bytes;}
Snatcher::ResidentPcmFeedParameters parameters(){Snatcher::ResidentPcmFeedParameters p;p.payloadOffset=16;p.rate=13224;p.level=238;p.pan=0x77;return p;}
size_t drain(Snatcher::PcmResidentFeed& feed,std::vector<int16_t>& out){size_t frames=0;std::string error;int16_t buffer[512];for(unsigned i=0;i<10000&&!feed.ended();++i){check(feed.serviceRefill(error),"real worker service remains owned");size_t n=feed.readFrames(buffer,256);check(feed.error().empty(),"source pull cannot hide an underrun");out.insert(out.end(),buffer,buffer+n*2);frames+=n;}check(feed.ended(),"actual source terminal and drained queue establish end");return frames;}
}
int main(){using namespace Snatcher;std::string error;auto p=parameters();
 {
  const size_t encoded=220001;std::vector<uint8_t> source(108*2048,0x81);std::vector<int16_t> pcm;
  for(size_t i=0;i<encoded;++i){source[i]=uint8_t(128+(i*19+1)%127);if((i&32767)!=32767){pcm.push_back(int16_t(i%30000));pcm.push_back(int16_t(-int(i%30000)));}}for(size_t i=encoded;i<encoded+4;++i)source[i]=255;
  auto stage=std::make_shared<ResidentPcmStaging>(106496);std::vector<uint8_t> old(100000,0x94),actual;check(stage->replacePrefix(11,old.data(),old.size(),error),"tracked stream starts with genuine allocation tail");
  PcmStreamFeed stream;check(stream.start(stage,485,source.data(),source.size(),pcm,encoded,16276,error),"tracked native stream prepares immutable source and played extents");
  check(stage->copyKnownBytes(actual,error)&&actual==old,"preparation alone cannot publish unaccepted source bytes");
  check(stream.publishInitial(error)&&stage->copyKnownBytes(actual,error)&&std::equal(source.begin(),source.begin()+65536,actual.begin())&&std::equal(old.begin()+65536,old.end(),actual.begin()+65536),"accepted initial service publishes exact64KiB and genuine tail");
  std::vector<int16_t> output;int16_t block[514];size_t requests=0;for(unsigned calls=0;!stream.ended()&&calls<10000;++calls){
   auto before=stream.snapshot();if(before.pendingBytes){++requests;std::vector<uint8_t> unpublished;check(stage->copyKnownBytes(unpublished,error),"pending transfer retains a readable actual allocation");
    check(stream.serviceRefill(error)&&stage->copyKnownBytes(actual,error),"worker completes exact immutable resource request");size_t destination=((before.pendingSourceOffset-65536)/32768&1)*32768;
    check(std::equal(source.begin()+before.pendingSourceOffset,source.begin()+before.pendingSourceOffset+before.pendingBytes,actual.begin()+destination),"only completed source request establishes half-buffer publication");
   }else check(stream.serviceRefill(error),"source-copy progress service remains owned");
   size_t n=stream.readFrames(block,257);output.insert(output.end(),block,block+n*2);
  }
  check(stream.ended()&&output==pcm&&requests==5&&stream.snapshot().publishedSourceBytes==source.size(),"all requested halves and final partial transfer retain exact played samples and real endpoint");
  std::vector<uint8_t> mapped;check(completedPcmStreamPrefix(source.data(),source.size(),mapped,error)&&stage->copyKnownBytes(actual,error)&&std::equal(mapped.begin(),mapped.end(),actual.begin()),"naturally requested final allocation agrees with independent completed transfer map");
  auto cancelled=std::make_shared<ResidentPcmStaging>(106496);PcmStreamFeed stop;check(stop.start(cancelled,486,source.data(),source.size(),pcm,encoded,16276,error)&&stop.publishInitial(error),"cancellation fixture accepts its initial real publication");
  for(unsigned i=0;!stop.snapshot().pendingBytes&&i<1000;++i){check(stop.serviceRefill(error),"advance source copy to first refill request");stop.readFrames(block,257);}
  auto pending=stop.snapshot();check(pending.pendingSourceOffset==65536&&pending.pendingBytes==32768&&pending.copiedCodes==49152,"first request belongs to sourcecopyC000, not audible EOF");
  check(cancelled->copyKnownBytes(actual,error)&&actual.size()==65536&&std::equal(actual.begin(),actual.end(),source.begin()),"pending first half has not changed published bytes");stop.cancel();
  check(stop.serviceRefill(error)&&!stop.snapshot().pendingBytes&&cancelled->copyKnownBytes(actual,error)&&std::equal(actual.begin(),actual.end(),source.begin()),"cancelled pending transfer never becomes retroactively known");
  ResidentPcmStagingSnapshot snapshot;check(cancelled->snapshot(snapshot,error),"freeze actual cancelled allocation for persistence");ResidentPcmStaging restored(106496);check(restored.restore(snapshot,error)&&restored.copyKnownBytes(actual,error)&&actual==snapshot.knownBytes,"cancelled source snapshot reconstructs only published bytes");
 }
 {
  auto staging=std::make_shared<ResidentPcmStaging>(106496);std::vector<uint8_t> old(100000,0x81),copied;old.back()=255;
  check(staging->replacePrefix(11,old.data(),old.size(),error),"short stream prefill begins with genuine resident tail");
  for(size_t sectors:{1u,31u,32u}){
   std::vector<uint8_t> bytes(sectors*2048,0x82);bytes[bytes.size()-8]=255;bool pending=true;
   check(staging->beginStream(485,bytes.data(),bytes.size(),pending,error)&&!pending,"entire <=32-sector allocation publishes before playback completion");
   check(staging->copyKnownBytes(copied,error)&&copied.size()==old.size()&&std::equal(bytes.begin(),bytes.end(),copied.begin())&&std::equal(old.begin()+65536,old.end(),copied.begin()+65536),"preloaded stream owns exact prefix and preserves genuine old tail");
   ResidentPcmStagingSnapshot state;check(staging->snapshot(state,error)&&state.writerKind==kPcmWriterStream&&state.writerId==485&&state.bank==11,"preloaded writer remains distinct from resident bank identity");
   auto restored=std::make_shared<ResidentPcmStaging>(106496);check(restored->restore(state,error)&&restored->copyKnownBytes(copied,error)&&copied==state.knownBytes,"preloaded allocation survives snapshot without a future completion callback");
   PcmResidentFeed feed;check(feed.start(restored,p,error),"effect can consume genuinely preloaded bytes without waiting for speech EOF");std::vector<int16_t> samples;
   const size_t payload=bytes.size()-24;check(drain(feed,samples)==payload-payload/16384,"preloaded raw-source effect terminates on real FF with source copy omissions");
  }
  ResidentPcmStagingSnapshot before,after;check(staging->snapshot(before,error),"capture preloaded source before malformed replacement");bool pending=false;
  check(!staging->beginStream(486,nullptr,2048,pending,error)&&!pending,"invalid start preserves pending result");
  check(!staging->beginStream(486,old.data(),2049,pending,error)&&!pending,"fractional source sector cannot publish");
  check(staging->snapshot(after,error)&&after.knownBytes==before.knownBytes&&after.generation==before.generation,"invalid start cannot alter actual allocation");
  std::vector<uint8_t> longStream(33*2048,0x83);check(staging->beginStream(486,longStream.data(),longStream.size(),pending,error)&&pending&&staging->knownBytes()==0,"long writer explicitly withdraws stale bytes while later transfer history is unresolved");
  std::vector<uint8_t> replacement(2048,0x84);check(staging->beginStream(487,replacement.data(),replacement.size(),pending,error)&&!pending&&staging->copyKnownBytes(copied,error)&&copied==replacement,"short replacement cancels unknown long history and establishes only its actual preloaded prefix");
 }
 {
  ResidentPcmStaging allocation(106496);ResidentPcmStagingSnapshot fresh,saved,after;std::vector<uint8_t> copied(1,42);
  check(allocation.snapshot(fresh,error)&&fresh.knownBytes.empty()&&fresh.bank==0xffff&&!fresh.generation&&fresh.writerKind==kPcmWriterNone,"fresh allocation snapshot is explicit unknown");
  check(!allocation.copyKnownBytes(copied,error)&&copied.size()==1&&copied[0]==42,"unknown source copy rejects transactionally");
  std::vector<uint8_t> resident(100000,0x81);resident.back()=255;check(allocation.replacePrefix(11,resident.data(),resident.size(),error),"establish resident tail provenance");
  for(size_t sectors=31;sectors<=32;++sectors){
   std::vector<uint8_t> stream(sectors*2048,uint8_t(0x80+sectors));check(allocation.completeStream(485,stream.data(),stream.size(),error),"31/32 sector completed stream prefixes accepted");
   check(allocation.copyKnownBytes(copied,error)&&copied.size()==resident.size(),"single transfer retains exact known old tail");
   check(std::equal(stream.begin(),stream.end(),copied.begin())&&std::equal(resident.begin()+stream.size(),resident.end(),copied.begin()+stream.size()),"every source byte matches new prefix or retained tail");
   check(allocation.snapshot(saved,error)&&saved.bank==11&&saved.writerKind==kPcmWriterStream&&saved.writerId==485,"stream writer is independent from last resident bank");
  }
  ResidentPcmStaging restored(106496);check(restored.restore(saved,error)&&restored.snapshot(after,error)&&after.knownBytes==saved.knownBytes&&after.generation==saved.generation&&after.writerId==485&&after.bank==11,"staging snapshots reconstruct actual known bytes and ownership");
  ResidentPcmStagingSnapshot bad=saved;bad.capacity--;check(!restored.restore(bad,error),"foreign allocation capacity rejected");bad=saved;bad.writerKind=ResidentPcmWriterKind(99);check(!restored.restore(bad,error),"unknown writer kind rejected");bad=saved;bad.writerId=0;check(!restored.restore(bad,error),"nonempty source requires writer identity");bad=saved;bad.writerKind=kPcmWriterResident;check(!restored.restore(bad,error),"resident writer cannot disagree with bank identity");bad=saved;bad.generation=0;check(!restored.restore(bad,error),"known bytes require real generation");bad=saved;bad.knownBytes.resize(106497);check(!restored.restore(bad,error),"oversized known prefix rejected");
  check(restored.snapshot(after,error)&&after.knownBytes==saved.knownBytes&&after.generation==saved.generation,"corrupt snapshot attempts preserve complete source allocation");
  for(size_t sectors:{1u,31u,32u,33u,47u,48u,49u,63u,64u,65u,79u,80u,81u,97u,257u}){
   std::vector<uint8_t> stream(sectors*2048);for(size_t i=0;i<stream.size();++i)stream[i]=uint8_t((i*37+i/65536*11+i/2048*7)%255);
   std::vector<uint8_t> mapped;check(completedPcmStreamPrefix(stream.data(),stream.size(),mapped,error)&&mapped.size()==std::min(stream.size(),size_t(65536)),"completed sector plan has exact written extent");
   for(size_t i=0;i<mapped.size();++i){size_t last=i+((stream.size()-1-i)/65536)*65536;check(mapped[i]==stream[last],"each allocation byte retains its actual last source write");}
   check(restored.completeStream(486,stream.data(),stream.size(),error)&&restored.copyKnownBytes(copied,error),"all completed source half-transfer counts publish");
   check(std::equal(mapped.begin(),mapped.end(),copied.begin())&&std::equal(resident.begin()+65536,resident.end(),copied.begin()+65536),"completed stream preserves every known source tail byte above64K");
   check(restored.snapshot(saved,error)&&saved.bank==11&&saved.writerKind==kPcmWriterStream&&saved.writerId==486,"long source map preserves independent last resident identity");
  }
  std::vector<uint8_t> large(33*2048,0x82),untouched(1,42);check(!completedPcmStreamPrefix(nullptr,2048,untouched,error)&&untouched==std::vector<uint8_t>(1,42),"invalid source map leaves output untouched");check(!completedPcmStreamPrefix(large.data(),size_t(65536)*2048,untouched,error),"source sector count beyond original word rejected before access");check(!restored.completeStream(485,large.data(),2049,error),"fractional sectors rejected");check(!restored.completeStream(0,large.data(),2048,error),"missing stream writer rejected");check(restored.snapshot(after,error)&&after.knownBytes==saved.knownBytes&&after.generation==saved.generation,"malformed stream publication is transactional");
  check(restored.invalidateStream(486,error)&&restored.snapshot(after,error)&&after.knownBytes.empty()&&after.bank==11&&after.writerId==486&&after.writerKind==kPcmWriterStream,"aborted partial stream forgets knowledge without fabricating last resident identity");
  check(!restored.copyKnownBytes(copied,error),"invalidated raw source cannot leak old bytes");ResidentPcmStaging unknownRestored(106496);check(unknownRestored.restore(after,error)&&unknownRestored.knownBytes()==0,"explicit unknown stream snapshot is restorable");
  std::vector<uint8_t> shortStream(24576,0x80);shortStream[24476]=255;
  check(unknownRestored.completeStream(485,shortStream.data(),shortStream.size(),error)&&unknownRestored.copyKnownBytes(copied,error)&&copied==shortStream,"real short stream reestablishes only actual prefix after unknown transfer");
  check(unknownRestored.knownBytes()==24576&&unknownRestored.bank()==11,"forgotten old tail stays unknown after new prefix");
  auto rawStage=std::make_shared<ResidentPcmStaging>(106496);check(rawStage->completeStream(485,shortStream.data(),shortStream.size(),error),"raw source prefix can be first known producer");PcmResidentFeed raw;check(raw.start(rawStage,p,error),"source effect accepts actual raw prefix plus16 without a sample header");std::vector<int16_t> samples;check(drain(raw,samples)==24459,"raw stream firstFF and original periodic omission determine actual end");
  check(rawStage->snapshot(after,error)&&after.bank==0xffff&&after.writerKind==kPcmWriterStream,"stream-only allocation never invents resident bank identity");
  check(restored.restore(fresh,error)&&restored.knownBytes()==0&&restored.bank()==0xffff,"canonical unknown initial allocation restores explicitly");
 }
 auto stage=std::make_shared<ResidentPcmStaging>(65536);auto original=data(40000);check(stage->capacity()==65536&&!stage->knownBytes()&&!stage->generation(),"explicit finite allocation begins unknown");
 check(!stage->replacePrefix(0,original.data(),original.size(),error)&&!stage->replacePrefix(1,nullptr,1,error)&&!stage->generation(),"invalid source writes reject transactionally");
 check(stage->replacePrefix(11,original.data(),original.size(),error)&&stage->knownBytes()==original.size()&&stage->bank()==11&&stage->generation()==1,"actual source bank owns only written prefix");
 PcmResidentFeed feed;check(feed.start(stage,p,error)&&feed.owns(stage)&&feed.sourceRate()==p.rate,"source profile and staging identity bind once");
 check(!feed.start(stage,p,error)&&feed.snapshot().copiedUnits==8192&&feed.snapshot().queuedFrames==8192,"initial prefetch is8192, never complete copied waveform");
 int16_t one[2];check(feed.readFrames(one,1)==1&&one[0]==104&&one[1]==104,"native two-copy level/pan conversion retains source arithmetic");
 auto replacement=data(10000,0x82);check(stage->replacePrefix(16,replacement.data(),replacement.size(),error)&&stage->knownBytes()==original.size(),"shorter incoming bank preserves actual old tail");
 check(feed.snapshot().queuedFrames==8191&&feed.serviceRefill(error)&&feed.snapshot().terminalSeen&&feed.snapshot().terminalOffset==10016,"worker encounters genuine new-bankFF, not cached old sample length");
 std::vector<int16_t> output;size_t n=drain(feed,output);check(n==9999&&output.size()==19998,"old prefetched8192 and new source tail drain to actual new terminal");
 for(size_t i=0;i<n;++i)check(output[2*i]==(i<8191?104:208)&&output[2*i+1]==output[2*i],"prefetched samples remain immutable while future samples reflect replacement");
 check(feed.readFrames(one,1)==0&&feed.ended()&&feed.error().empty(),"genuine end stays once-only and is not an error");
 auto longer=std::make_shared<ResidentPcmStaging>(65536);auto first=data(10000),second=data(50000,0x82);check(longer->replacePrefix(1,first.data(),first.size(),error),"short old extent source");PcmResidentFeed extended;check(extended.start(longer,p,error)&&longer->replacePrefix(2,second.data(),second.size(),error),"new prefix can replace old terminal with longer actual content");output.clear();n=drain(extended,output);check(n==50000-3&&extended.snapshot().droppedSourceUnits==3,"source firstFF and periodic copy omissions govern length beyond old header");
 auto pad=std::make_shared<ResidentPcmStaging>(65536);auto padding=data(40000);padding[16+16383]=255;check(pad->replacePrefix(1,padding.data(),padding.size(),error),"source copy-boundary fixture");PcmResidentFeed omitted;check(omitted.start(pad,p,error),"source omission feed");output.clear();n=drain(omitted,output);check(n==39998&&omitted.snapshot().droppedSourceUnits==2&&omitted.snapshot().terminalOffset==40016,"the boundary source byte is skipped without testing it as terminal");
 auto safe=std::make_shared<ResidentPcmStaging>(65536);check(safe->replacePrefix(11,original.data(),original.size(),error),"future-disjoint original source");PcmResidentFeed tail;check(tail.start(safe,p,error),"future-disjoint feed");output.clear();int16_t block[512];size_t consumed=0;while(consumed<20000){check(tail.serviceRefill(error),"tail progression service");size_t count=std::min(size_t(256),20000-consumed);check(tail.readFrames(block,count)==count,"actual source progression");consumed+=count;}
 std::vector<uint8_t> prefix(8192,0x82);check(safe->replacePrefix(16,prefix.data(),prefix.size(),error)&&tail.snapshot().sourceCursor>=prefix.size(),"new prefix is behind actual future source cursor");output.clear();drain(tail,output);for(int16_t value:output)check(value==104,"future-disjoint overwrite cannot change consumed or prefetched old-tail audio");
 auto unknown=std::make_shared<ResidentPcmStaging>(65536);std::vector<uint8_t> partial(8208,0x81);check(unknown->replacePrefix(1,partial.data(),partial.size(),error),"finite known source prefix");PcmResidentFeed missing;check(missing.start(unknown,p,error)&&missing.readFrames(one,1)==1&&!missing.serviceRefill(error)&&!missing.error().empty()&&!missing.ended(),"unknown future staging is sticky failure, never silence or fabricated terminal");
 check(missing.readFrames(one,1)==0&&!missing.ended(),"failed source cannot later masquerade as finished");
 PcmResidentFeed starved;check(starved.start(safe,p,error),"explicit worker-underrun fixture");std::vector<int16_t> large(20000,1234);check(starved.readFrames(large.data(),10000)==8192&&starved.error().empty()&&!starved.ended()&&large[16384]==1234,"callback returns only actual publication without padding, failure or false completion");check(starved.readFrames(one,1)==0&&starved.serviceRefill(error)&&starved.readFrames(one,1)==1,"worker service resumes the same starved owner");
 auto wrapping=std::make_shared<ResidentPcmStaging>(65536);std::vector<uint8_t> wrappedBytes(65536,129);wrappedBytes[32]=255;check(wrapping->replacePrefix(1,wrappedBytes.data(),wrappedBytes.size(),error),"actual full allocation for aligned source wrap");PcmResidentFeed wrapped;p.payloadOffset=8192;check(wrapped.start(wrapping,p,error),"aligned source payload owns native wrap");output.clear();check(drain(wrapped,output)==57373&&wrapped.snapshot().terminalOffset==32,"only exact64KiB source boundary wraps, retains16KiB omissions and actual prefix terminal");
 PcmResidentFeed invalid;p=parameters();p.rate=0;check(!invalid.start(stage,p,error),"unknown source pitch remains guarded");
 // Source snapshot/read locks and prefix publication must be safe across the
 // worker/audio boundary. The audio side consumes only the first prefetched
 //4096 frames; every concurrent replacement affects only future refills.
 auto concurrent=std::make_shared<ResidentPcmStaging>(65536);check(concurrent->replacePrefix(1,original.data(),original.size(),error),"concurrent source allocation");PcmResidentFeed threaded;p=parameters();check(threaded.start(concurrent,p,error),"concurrent prefetched voice");bool audioGood=true;std::thread audio([&]{int16_t bytes[128];for(unsigned i=0;i<64;++i){if(threaded.readFrames(bytes,64)!=64){audioGood=false;return;}for(int16_t value:bytes)if(value!=104)audioGood=false;}});for(unsigned i=0;i<100;++i)check(concurrent->replacePrefix(2,second.data(),second.size(),error),"atomic prefix publication while audio consumes old queue");audio.join();check(audioGood&&threaded.error().empty()&&threaded.snapshot().renderedFrames==4096,"audio callback sees only its already-owned immutable queue");
 std::printf("PCM refill:%u checks,0 failures\n",checks);return 0;
}
