/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fm_gens.h"
#include "gens_ym2612/Ym2612_GENS.h"
#include <algorithm>
#include <limits>
namespace Snatcher {
namespace {
void put(std::vector<uint8_t>& b,uint64_t v,unsigned n){for(unsigned i=n;i;--i)b.push_back(uint8_t(v>>((i-1)*8)));}
uint64_t get(const std::vector<uint8_t>& b,size_t& at,unsigned n){uint64_t v=0;while(n--)v=(v<<8)|b[at++];return v;}
uint64_t hash(const std::vector<uint8_t>& b,size_t n){uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;++i)h=(h^b[i])*UINT64_C(1099511628211);return h;}
const char identity[]="8238afd9503a59ffcec0e2685584d5ce819ad7c6:gens-state1";
}
struct GensFmSynthRenderer::Impl {
 Ym2612_GENS_Emu core;uint32_t rate;bool coupled,ready=false,discard=false;uint64_t frames=0;uint8_t keys=0;FmOutputCoupling coupling[2];
 Impl(uint32_t r,bool c):rate(r),coupled(c){}
};
GensFmSynthRenderer::GensFmSynthRenderer(uint32_t r,bool c):_impl(new Impl(r,c)){}
GensFmSynthRenderer::~GensFmSynthRenderer(){delete _impl;}
bool GensFmSynthRenderer::init(std::string& e){e.clear();if(_impl->rate<8000||_impl->rate>192000){e="Gens output rate outside8000..192000Hz";return false;}if(!_impl->ready){const char* why=_impl->core.set_rate(_impl->rate,53693175.0/7.0);if(why){e=why;return false;}_impl->ready=true;}return true;}
bool GensFmSynthRenderer::reset(std::string& e){if(!init(e))return false;_impl->core.reset();_impl->core.mute_voices(0);_impl->frames=0;_impl->keys=0;for(auto& c:_impl->coupling)c.reset();return true;}
bool GensFmSynthRenderer::apply(const std::vector<FmSynthWrite>& writes,std::string& e){if(!init(e))return false;if(_impl->discard)return true;for(const auto& w:writes){if(w.part>1){e="Gens register bank outside two banks";return false;}if(w.part)_impl->core.write1(w.address,w.value);else _impl->core.write0(w.address,w.value);if(!w.part&&w.address==0x28){unsigned channel=(w.value&3)+((w.value&4)?3:0);if(channel<6){if(w.value&0xf0)_impl->keys|=uint8_t(1u<<channel);else _impl->keys&=uint8_t(~(1u<<channel));}}}return true;}
bool GensFmSynthRenderer::instrument(unsigned c,const FmInstrument& v,std::string& e){std::vector<FmSynthWrite>w;return encodeFmInstrument(c,v,w,e)&&apply(w,e);}
bool GensFmSynthRenderer::frequency(unsigned c,uint16_t v,std::string& e){std::vector<FmSynthWrite>w;return encodeFmFrequency(c,v,w,e)&&apply(w,e);}
bool GensFmSynthRenderer::levels(unsigned c,const uint8_t v[4],std::string& e){std::vector<FmSynthWrite>w;return encodeFmLevels(c,v,w,e)&&apply(w,e);}
bool GensFmSynthRenderer::carrierLevels(unsigned c,const uint8_t v[4],uint8_t m,std::string& e){std::vector<FmSynthWrite>w;return encodeFmCarrierLevels(c,v,m,w,e)&&apply(w,e);}
bool GensFmSynthRenderer::key(unsigned c,bool on,std::string& e){std::vector<FmSynthWrite>w;return encodeFmKey(c,on,w,e)&&apply(w,e);}
bool GensFmSynthRenderer::fastRelease(unsigned c,std::string& e){std::vector<FmSynthWrite>w;return encodeFmFastRelease(c,w,e)&&apply(w,e);}
bool GensFmSynthRenderer::pan(unsigned c,uint8_t v,std::string& e){std::vector<FmSynthWrite>w;return encodeFmPan(c,v,w,e)&&apply(w,e);}
bool GensFmSynthRenderer::lfo(uint8_t v,std::string& e){return apply(std::vector<FmSynthWrite>(1,FmSynthWrite(0,0x22,v)),e);}
bool GensFmSynthRenderer::renderPcm(uint32_t frames,std::vector<int16_t>& out,std::string& e){
 e.clear();if(!frames||frames>1000000||UINT64_MAX-_impl->frames<frames){e="Gens render frame extent invalid";return false;}if(!init(e))return false;std::vector<int16_t> next(size_t(frames)*2,0);
 if(!_impl->discard){for(uint32_t at=0;at<frames;){uint32_t n=std::min(256u,frames-at);_impl->core.run(int(n),next.data()+at*2);at+=n;}if(_impl->coupled)for(size_t i=0;i<next.size();++i){int64_t v=_impl->coupling[i&1].process(int64_t(next[i])*65536,_impl->rate)/65536;next[i]=int16_t(std::max(INT64_C(-32768),std::min(INT64_C(32767),v)));}}
 _impl->frames+=frames;out.swap(next);return true;
}
uint32_t GensFmSynthRenderer::sampleRate()const{return _impl->rate;}
uint64_t GensFmSynthRenderer::renderedFrames()const{return _impl->frames;}
uint8_t GensFmSynthRenderer::keyOnMask()const{return _impl->keys;}
void GensFmSynthRenderer::setReplayDiscard(bool value){_impl->discard=value;}
bool GensFmSynthRenderer::snapshotState(std::vector<uint8_t>& out,std::string& e)const{
 e.clear();if(!_impl->ready){GensFmSynthRenderer fresh(_impl->rate,_impl->coupled);return fresh.init(e)&&fresh.snapshotState(out,e);}if(_impl->discard){e="Gens checkpoint requires an initialized real generator";return false;}std::vector<uint8_t> state;if(!_impl->core.save_state(state)){e="Gens table references are not checkpointable";return false;}
 std::vector<uint8_t>b={'N','F','G','1',1};b.insert(b.end(),identity,identity+sizeof(identity)-1);put(b,_impl->rate,4);put(b,_impl->frames,8);put(b,_impl->keys,1);put(b,_impl->coupled,1);for(const auto& c:_impl->coupling){put(b,uint64_t(c._input),8);put(b,uint64_t(c._output),8);}put(b,state.size(),4);b.insert(b.end(),state.begin(),state.end());put(b,hash(b,b.size()),8);out.swap(b);return true;
}
bool GensFmSynthRenderer::restoreState(const std::vector<uint8_t>& b,std::string& e,uint64_t expected){
 e.clear();const size_t fixed=5+sizeof(identity)-1+4+8+1+1+32+4+8;if(b.size()<fixed||b.size()>20000||!std::equal(b.begin(),b.begin()+5,std::vector<uint8_t>({'N','F','G','1',1}).begin())||!std::equal(identity,identity+sizeof(identity)-1,b.begin()+5)){e="Gens checkpoint identity mismatch";return false;}
 size_t end=b.size()-8,checksumAt=end;if(get(b,checksumAt,8)!=hash(b,end)){e="Gens checkpoint integrity mismatch";return false;}size_t at=5+sizeof(identity)-1;uint32_t rate=uint32_t(get(b,at,4));uint64_t frames=get(b,at,8);uint8_t keys=uint8_t(get(b,at,1)),coupled=uint8_t(get(b,at,1));if(rate!=_impl->rate||coupled!=unsigned(_impl->coupled)||keys>63||(expected!=UINT64_MAX&&frames!=expected)){e="Gens checkpoint owner mismatch";return false;}
 GensFmSynthRenderer staged(rate,coupled!=0);if(!staged.init(e))return false;auto signed64=[](uint64_t n){return n<=uint64_t(INT64_MAX)?int64_t(n):INT64_MIN+int64_t(n-(UINT64_C(1)<<63));};for(auto& c:staged._impl->coupling){c._input=signed64(get(b,at,8));c._output=signed64(get(b,at,8));if(c._input<-(INT64_C(1)<<41)||c._input>(INT64_C(1)<<41)||c._output<-(INT64_C(1)<<43)||c._output>(INT64_C(1)<<43)){e="Gens output history outside finite domain";return false;}}
 uint64_t size=get(b,at,4);if(size!=end-at){e="Gens checkpoint extent mismatch";return false;}std::vector<uint8_t> state(b.begin()+at,b.begin()+end);if(!staged._impl->core.load_state(state)){e="Invalid Gens scalar state or table reference";return false;}staged._impl->frames=frames;staged._impl->keys=keys;std::swap(_impl,staged._impl);return true;
}
}
