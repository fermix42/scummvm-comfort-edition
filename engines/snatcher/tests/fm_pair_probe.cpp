/* SPDX-License-Identifier: GPL-3.0-or-later */
// Private original-data diagnostic. No fitted phase/gain/delay or production
// voice changes: suppress only key-ons outside the requested diagnostic mask.
#include "../fm_player.h"
#include "../fm_gens.h"
#include <fstream>
#include <iterator>
#include <memory>
#include <cstdlib>
#include <cstdio>
using namespace Snatcher;
static std::vector<uint8_t> read(const char* path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
struct PairSink:public FmSynthSink {
 std::unique_ptr<FmSynthSink> synth;unsigned mask,tick=0;std::ofstream writes;
 void record(const std::vector<FmSynthWrite>& w){if(writes)for(const auto& v:w)writes<<tick<<','<<unsigned(v.part)<<','<<unsigned(v.address)<<','<<unsigned(v.value)<<'\n';}
 PairSink(bool gens,unsigned m):synth(gens?static_cast<FmSynthSink*>(new GensFmSynthRenderer()):static_cast<FmSynthSink*>(new NativeFmSynthRenderer())),mask(m){}
 bool reset(std::string& e)override{return synth->reset(e);}
 bool instrument(unsigned c,const FmInstrument& i,std::string& e)override{std::vector<FmSynthWrite>w;if(!encodeFmInstrument(c,i,w,e))return false;record(w);return synth->instrument(c,i,e);}
 bool frequency(unsigned c,uint16_t v,std::string& e)override{std::vector<FmSynthWrite>w;if(!encodeFmFrequency(c,v,w,e))return false;record(w);return synth->frequency(c,v,e);}
 bool levels(unsigned c,const uint8_t v[4],std::string& e)override{std::vector<FmSynthWrite>w;if(!encodeFmLevels(c,v,w,e))return false;record(w);return synth->levels(c,v,e);}
 bool carrierLevels(unsigned c,const uint8_t v[4],uint8_t m,std::string& e)override{std::vector<FmSynthWrite>w;if(!encodeFmCarrierLevels(c,v,m,w,e))return false;record(w);return synth->carrierLevels(c,v,m,e);}
 bool key(unsigned c,bool on,std::string& e)override{on=on&&(mask&(1u<<c));std::vector<FmSynthWrite>w;if(!encodeFmKey(c,on,w,e))return false;record(w);return synth->key(c,on,e);}
 bool fastRelease(unsigned c,std::string& e)override{std::vector<FmSynthWrite>w;if(!encodeFmFastRelease(c,w,e))return false;record(w);return synth->fastRelease(c,e);}
 bool pan(unsigned c,uint8_t v,std::string& e)override{std::vector<FmSynthWrite>w;if(!encodeFmPan(c,v,w,e))return false;record(w);return synth->pan(c,v,e);}
 bool lfo(uint8_t v,std::string& e)override{record({FmSynthWrite(0,0x22,v)});return synth->lfo(v,e);}
 bool renderPcm(uint32_t n,std::vector<int16_t>& p,std::string& e)override{return synth->renderPcm(n,p,e);}
 uint32_t sampleRate()const override{return 44100;}
};
int main(int argc,char** argv){
 if(argc!=6&&argc!=7)return 2;std::string core=argv[3];unsigned mask=unsigned(std::strtoul(argv[4],nullptr,0));
 if((core!="gens"&&core!="nuked")||(mask!=16&&mask!=32&&mask!=48))return 2;
 auto main=read(argv[1]),fm=read(argv[2]);PairSink sink(core=="gens",mask);FmPlayer player;std::string error;
 if(argc==7){sink.writes.open(argv[6]);if(!sink.writes)return 5;sink.writes<<"tick,part,address,value\n";}
 player.setSourceResetPublication(true);
 if(!player.load(main.data(),main.size(),fm.data(),fm.size(),error)||!player.start(56,sink,error)){std::fprintf(stderr,"%s\n",error.c_str());return 3;}
 std::ofstream output(argv[5],std::ios::binary);uint64_t fraction=0;
 for(unsigned tick=0;tick<120;++tick){std::vector<FmDrumEvent> drums;std::vector<int16_t> pcm;
  sink.tick=tick+1;
  fraction+=UINT64_C(44100)*10000000;uint32_t frames=uint32_t(fraction/599227434);fraction%=599227434;
  if(!player.tick(sink,drums,error)||!sink.renderPcm(frames,pcm,error)){std::fprintf(stderr,"%s\n",error.c_str());return 4;}
  for(int16_t sample:pcm){char bytes[2]={char(uint16_t(sample)),char(uint16_t(sample)>>8)};output.write(bytes,2);}
 }
 return output?0:5;
}
