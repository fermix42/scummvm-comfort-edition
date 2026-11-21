/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../audio_percussion.h"
#include <cstdio>
#include <cstdlib>
#include <map>
using namespace Snatcher;
namespace {
unsigned checks;void check(bool value){++checks;if(!value){std::fprintf(stderr,"percussion failure %u\n",checks);std::exit(1);}}
void word(std::vector<uint8_t>& b,size_t at,unsigned value){b[at]=uint8_t(value>>8);b[at+1]=uint8_t(value);}
void longword(std::vector<uint8_t>& b,size_t at,uint32_t value){word(b,at,value>>16);word(b,at+2,value);}
struct Backend:AudioBackend {uint64_t next=1;unsigned plays=0,stops=0;bool fail=false,paused=false;std::map<uint64_t,bool> handles;
 bool play(const std::vector<int16_t>&,const AudioProfile&,AudioRole role,uint64_t& h,std::string& error)override{check(role==kAudioMusic);if(fail){error="test backend failure";return false;}h=next++;handles[h]=true;++plays;return true;}
 void stop(uint64_t h)override{handles.erase(h);++stops;}void pause(uint64_t,bool value)override{paused=value;}bool active(uint64_t h)const override{auto i=handles.find(h);return i!=handles.end()&&i->second;}
};
}
int main(){
 std::vector<uint8_t> index(0x4010+12*16,0),drums(12*24,0);index[0x4000]=13;
 for(unsigned i=0;i<12;++i){size_t p=0x4010+i*16;index[p]=uint8_t(i%4+1);index[p+1]=uint8_t(i%2);index[p+2]=uint8_t(200+i);index[p+3]=0xab;word(index,p+4,1024);longword(index,p+6,0x55000+i*24);size_t d=i*24;longword(drums,d,8);longword(drums,d+4,0xffffffff);word(drums,d+8,16000);drums[d+16]=0x81;drums[d+17]=0x82;drums[d+18]=1;drums[d+19]=127;for(unsigned n=20;n<24;++n)drums[d+n]=255;}
 PcmPercussionAssets assets;std::string error;check(assets.load(index.data(),index.size(),drums.data(),drums.size(),error));check(!assets.load(nullptr,0,drums.data(),drums.size(),error)&&assets.loaded());
 const uint8_t gains[]={0,1,112,127,254,255};
 for(unsigned track=6;track<8;++track)for(unsigned id=0;id<32;++id)for(unsigned level5=0;level5<32;++level5)for(uint8_t gain:gains)for(unsigned mono=0;mono<2;++mono)for(unsigned attenuate=0;attenuate<2;++attenuate){
  PcmPercussionRequest r;r.track=uint8_t(track);r.id=uint8_t(id);r.level5=uint8_t(level5);r.gain=gain;r.mono=mono!=0;r.attenuate=attenuate!=0;PreparedPcmEffect effect;bool sound;
  bool expected=id&&id<13&&id!=6&&id!=8&&id!=9&&id!=10;check(assets.prepare(r,effect,sound,error)&&sound==expected);
  if(sound){unsigned level=199+id;if(level5)level=level*(level5*8)/256;if(attenuate)level=level*112/256;if(gain!=255)level=level*gain/256;unsigned left=10,right=mono?10:11;
   check(effect.source.channel==track-6&&effect.pcm.size()==8&&effect.pcm[0]==int(level*left/32)*2&&effect.pcm[1]==int(level*right/32)*2&&effect.pcm[4]==-effect.pcm[0]&&effect.pcm[5]==-effect.pcm[1]);}
 }
 PcmPercussionRequest r;r.track=5;PreparedPcmEffect effect;bool sound;check(!assets.prepare(r,effect,sound,error));r.track=6;r.level5=32;check(!assets.prepare(r,effect,sound,error));r.level5=31;r.id=1;check(assets.prepare(r,effect,sound,error)&&sound);
 Backend backend;AudioController audio(backend);PcmPercussionOwner owner(audio);bool accepted;check(owner.start(r,effect,AudioOwner(63,87),2,accepted,error)&&accepted&&owner.activeCount()==1);
 r.track=7;check(assets.prepare(r,effect,sound,error)&&owner.start(r,effect,AudioOwner(63,87),2,accepted,error)&&accepted&&owner.activeCount()==2);
 unsigned plays=backend.plays;effect.source.priority=0;check(owner.start(r,effect,AudioOwner(63,87),2,accepted,error)&&!accepted&&backend.plays==plays&&owner.activeCount()==2);
 effect.source.priority=5;backend.fail=true;check(!owner.start(r,effect,AudioOwner(63,87),2,accepted,error)&&!accepted&&owner.activeCount()==2);backend.fail=false;
 check(owner.start(r,effect,AudioOwner(63,87),2,accepted,error)&&accepted&&owner.activeCount()==2);AudioCompletion completion;check(audio.take(completion)&&completion.result==kAudioCancelled&&!owner.consume(completion));
 audio.setPaused(true);for(auto& h:backend.handles)h.second=false;audio.poll();check(owner.activeCount()==2&&audio.completionCount()==0&&backend.paused);audio.setPaused(false);audio.poll();unsigned done=0;while(audio.take(completion)){check(owner.consume(completion)&&completion.result==kAudioFinished);++done;}check(done==2&&!owner.activeCount());check(!owner.consume(completion));
 r.track=6;check(assets.prepare(r,effect,sound,error)&&owner.start(r,effect,AudioOwner(63,87),3,accepted,error));owner.reset();check(!owner.activeCount()&&!audio.activeCount());while(audio.take(completion))check(!owner.consume(completion));
 std::printf("native percussion: %u checks, 0 failures\n",checks);return 0;
}
