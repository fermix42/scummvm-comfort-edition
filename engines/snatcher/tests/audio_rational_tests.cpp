/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../audio_rate.h"
#include "../native_archive.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace Snatcher;
namespace {
unsigned checks=0;
void check(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL rational PCM: %s\n",message);std::exit(1);}}
struct Feed:NativeAudioFeed {
 std::vector<int16_t> pcm;size_t at;uint32_t rate;unsigned calls,stallEvery;bool fail;size_t released;
 Feed(size_t frames,uint32_t r):pcm(frames*2),at(0),rate(r),calls(0),stallEvery(0),fail(false),released(frames){for(size_t i=0;i<frames;++i){pcm[i*2]=int16_t((i*193+16381)%65536-32768);pcm[i*2+1]=int16_t((i*757+5)%65536-32768);}}
 uint32_t sourceRate()const override{return rate;}
 size_t readFrames(int16_t* out,size_t frames)override{++calls;if(fail||(stallEvery&&calls%stallEvery==0))return 0;size_t count=std::min(frames,std::min(released,pcm.size()/2)-at);std::copy(pcm.begin()+at*2,pcm.begin()+(at+count)*2,out);at+=count;return count;}
 bool ended()const override{return at==pcm.size()/2;}
 std::string error()const override{return fail?"explicit fixture source failure":std::string();}
};
int16_t oracle(const Feed& feed,uint64_t frame,unsigned channel,const RationalAudioRate& rate,uint32_t target){
 // Independent absolute sample-age calculation, not an incremental renderer.
 const uint64_t denominator=uint64_t(rate.denominator)*target,position=frame*rate.numerator;
 size_t current=size_t(position/denominator);int last=current?feed.pcm[(current-1)*2+channel]:0,now=feed.pcm[current*2+channel];
 int64_t weighted=int64_t(now-last)*(position%denominator)+int64_t(denominator/2),part=weighted/int64_t(denominator);
 if(weighted<0&&weighted%int64_t(denominator))--part;
 return int16_t(last+part);
}
void run(size_t length,RationalAudioRate rate,uint32_t target,unsigned chunk,unsigned stalls){
 uint32_t nominal=0;std::string error;check(nearestMixerRate(rate,nominal,error),"oracle nominal input rate");
 std::shared_ptr<Feed> source(new Feed(length,nominal));source->stallEvery=stalls;RationalPcmTransport renderer;
 if(!length){check(!renderer.start(source,rate,target,error),"empty source cannot invent playback");return;}
 check(renderer.start(source,rate,target,error),"exact source transport starts");uint64_t expectedFrames=0;
 check(resampledFrameCount(length,rate,target,expectedFrames,error),"independent exact finite count");
 uint64_t output=0;std::vector<int16_t> buffer(chunk*2);unsigned attempts=0;
 while(!renderer.ended()){
  check(++attempts<expectedFrames*3+100,"underflow cannot stall finite test forever");
  size_t count=renderer.readFrames(buffer.data(),chunk);check(count<=chunk&&output+count<=expectedFrames,"finite request bounds");
  for(size_t i=0;i<count;++i)for(unsigned channel=0;channel<2;++channel)check(buffer[i*2+channel]==oracle(*source,output+i,channel,rate,target),"absolute rational sample-age and interpolation oracle");
  output+=count;
 }
 check(output==expectedFrames&&renderer.renderedFrames()==expectedFrames&&renderer.consumedFrames()==length,"exact endpoint and source ownership");
 check(!renderer.readFrames(buffer.data(),chunk)&&renderer.ended()&&renderer.error().empty(),"stable genuine EOF");
}
}
int main(){
 // EOF of the source precedes EOF of the interpolator. Save both the final
 // interpolation tail and the drained-but-not-yet-polled completion state.
 for(unsigned prefix:{1u,3u}){
  auto feed=std::make_shared<Feed>(1,16276);RationalPcmTransport live;std::string e;
  check(live.start(feed,RationalAudioRate(390625,24),44100,e),"tail fixture starts");
  int16_t pcm[16];check(live.readFrames(pcm,prefix)==prefix&&feed->ended(),"source exhausted before controller completion");
  NativeArchive saved;check(live.archive(saved,e),"exhausted source transport archives");
  RationalPcmTransport fresh;check(!fresh.start(feed,RationalAudioRate(390625,24),44100,e),"new playback still rejects EOF source");
  RationalPcmTransport loaded;NativeArchive read(saved.bytes);
  check(loaded.restore(feed,RationalAudioRate(390625,24),44100,read,e)&&read.finished(),"continuation accepts owned exhausted source");
  int16_t a[16]={},b[16]={};size_t count=live.readFrames(a,8),restored=loaded.readFrames(b,8);
  check(count==restored&&std::equal(a,a+2*count,b)&&live.ended()&&loaded.ended(),"exact remaining tail and EOF survive restore");
  auto corrupt=saved.bytes;corrupt[3]^=1;NativeArchive bad(corrupt);RationalPcmTransport rejected;
  check(!rejected.restore(feed,RationalAudioRate(390625,24),44100,bad,e)&&rejected.sourceRate()==0,"wrong archived output rate rejects transactionally");
 }
 for(unsigned step:{256u,512u,768u,1024u,1280u,1536u,2048u}){
  RationalAudioRate rate(390625*step,24576);
  for(uint32_t target:{8000u,22050u,44100u,48000u,96000u})for(size_t length:{size_t(0),size_t(1),size_t(2),size_t(19),size_t(1000)})for(unsigned chunk:{1u,7u,257u})run(length,rate,target,chunk,0);
 }
 for(unsigned chunk:{1u,31u,1024u})run(65537,RationalAudioRate(390625,24),44100,chunk,5);
 std::string error;std::shared_ptr<Feed> source(new Feed(8,16276));RationalPcmTransport owner;
 check(!owner.start(source,RationalAudioRate(0,1),44100,error)&&!owner.start(source,RationalAudioRate(1,0),44100,error)&&!owner.start(source,RationalAudioRate(390625,24),0,error),"invalid start is transactional");
 source->rate=16000;check(!owner.start(source,RationalAudioRate(390625,24),44100,error),"nominal identity mismatch rejects");source->rate=16276;
 check(owner.start(source,RationalAudioRate(390625,24),44100,error)&&!owner.start(source,RationalAudioRate(390625,24),44100,error),"owner cannot restart while active");
 int16_t sample[32]={};check(!owner.readFrames(sample,0)&&!owner.readFrames(nullptr,8)&&!owner.renderedFrames(),"empty output requests consume nothing");
 source->released=0;check(!owner.readFrames(sample,8)&&!owner.ended()&&owner.error().empty()&&!owner.renderedFrames(),"temporary source starvation is not EOF or fabricated silence");
 source->released=8;check(owner.readFrames(sample,1)==1&&sample[0]==0&&sample[1]==0,"preserved zero interpolation history at actual source start");
 source->fail=true;check(owner.error()==source->error()&&!owner.ended(),"source failure remains explicit, not completion");
 std::printf("rational PCM transport: %u checks,0 failures\n",checks);return 0;
}
