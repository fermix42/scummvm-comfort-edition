/* SPDX-License-Identifier: GPL-3.0-or-later */
// Included after the renderer implementation definition. Portable native audio
// continuation only: fixed-width fields, no struct padding, pointers or machine.
namespace {
struct SynthArchive {
 std::vector<uint8_t>& bytes;size_t at;bool reading,ok;
 SynthArchive(std::vector<uint8_t>& b,bool read):bytes(b),at(0),reading(read),ok(true){}
 template<class T> void field(T& v){
  static_assert(std::is_integral<T>::value,"scalar audio field");
  typedef typename std::make_unsigned<T>::type U;U raw=U(v);
  if(reading){if(at>bytes.size()||sizeof(T)>bytes.size()-at){ok=false;return;}raw=0;for(unsigned i=0;i<sizeof(T);++i)raw=U((raw<<8)|bytes[at++]);
   if(std::is_signed<T>::value&&raw>U(std::numeric_limits<T>::max()))v=T(std::numeric_limits<T>::min()+T(raw-U(std::numeric_limits<T>::max())-1));else v=T(raw);
  }else for(unsigned i=sizeof(T);i;--i)bytes.push_back(uint8_t(raw>>((i-1)*8)));
 }
 template<class T,size_t N> void field(T(&values)[N]){for(size_t i=0;i<N;++i)field(values[i]);}
};
uint64_t synthChecksum(const std::vector<uint8_t>& b,size_t size){uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<size;++i)h=(h^b[i])*UINT64_C(1099511628211);return h;}
bool validSynthFields(const ym3438_t& g){
 if(g.mol < -768||g.mol>768||g.mor < -768||g.mor>768||g.ch_lock < -256||g.ch_lock>255||g.ch_read < -256||g.ch_read>255)return false;
 for(unsigned i=0;i<6;++i)if(g.ch_acc[i] < -256||g.ch_acc[i]>255||g.ch_out[i] < -256||g.ch_out[i]>255)return false;
 if(g.pg_block>7||g.pg_kcode>31||g.pg_fnum>2047||g.eg_cycle>12||g.eg_inc>4||g.eg_timer>4095||g.eg_quotient>2)return false;
 if(g.cycles>=24||g.channel!=g.cycles%6||g.lfo_freq>7||g.eg_timer_low_lock>3||g.mode_ch3>3||g.mode_kon_channel>7||g.address>511||g.write_data>511||g.lfo_pm>31||g.lfo_am>126)return false;
 for(unsigned i=0;i<8;++i)if(g.mode_test_21[i]>1||g.mode_test_2c[i]>1)return false;
 for(unsigned i=0;i<6;++i)if(g.connect[i]>7||g.fb[i]>7||g.ams[i]>3||g.pms[i]>7||g.fnum[i]>2047||g.fnum_3ch[i]>2047||g.block[i]>7||g.block_3ch[i]>7||g.kcode[i]>31||g.kcode_3ch[i]>31||g.pan_l[i]>1||g.pan_r[i]>1)return false;
 for(unsigned i=0;i<24;++i)if(g.pg_phase[i]>0xfffff||g.pg_inc[i]>0xfffff||g.eg_state[i]>3||g.eg_level[i]>1023||g.eg_out[i]>1023||g.ks[i]>3||g.ar[i]>31||g.sr[i]>31||g.dr[i]>31||g.dt[i]>7||g.multi[i]>30||g.sl[i]>31||g.rr[i]>62||g.am[i]>1||g.tl[i]>127||g.ssg_eg[i]>15)return false;
 return true;
}
}
void NativeFmSynthRenderer::setReplayDiscard(bool discard){_impl->discard=discard;}
bool NativeFmSynthRenderer::snapshotState(std::vector<uint8_t>& output,std::string& error)const{
 error.clear();if(_impl->discard){error="Cannot checkpoint synthesis-discard reconstruction";return false;}
 std::vector<uint8_t> bytes;SynthArchive a(bytes,false);uint32_t magic=0x4e465331,schema=1,rate=_impl->rate;uint8_t coupled=_impl->coupledOutput;uint64_t frames=_impl->frames;
 a.field(magic);a.field(schema);
 // Exact approved dependency source identity, independent of compiler layout.
 const char identity[]="335747d78cb0abbc3b55b004e62dad9763140115:8fa385546f0f2d1c975d097002af00cd729ae2ae097c068e9c883ce08ddf3a76";
 for(unsigned i=0;i<sizeof(identity)-1;++i){uint8_t c=uint8_t(identity[i]);a.field(c);}
 a.field(rate);a.field(coupled);a.field(frames);uint8_t phase=uint8_t(_impl->phase),delay=uint8_t(_impl->delay);a.field(phase);a.field(delay);a.field(_impl->cycleRemaining);a.field(_impl->pin);
 for(unsigned i=0;i<2;++i){a.field(_impl->coupling[i]._input);a.field(_impl->coupling[i]._output);}
 FmSynthWrite current=_impl->current;a.field(current.part);a.field(current.address);a.field(current.value);uint16_t count=uint16_t(_impl->pending.size());a.field(count);for(FmSynthWrite w:_impl->pending){a.field(w.part);a.field(w.address);a.field(w.value);}
 ym3438_t generator=_impl->generator;
#define FM_NATIVE_FIELD(name) a.field(generator.name);
#include "fm_synth_fields.inc"
#undef FM_NATIVE_FIELD
 uint64_t checksum=synthChecksum(bytes,bytes.size());a.field(checksum);output.swap(bytes);return true;
}
bool NativeFmSynthRenderer::restoreState(const std::vector<uint8_t>& input,std::string& error,uint64_t expectedFrames){
 error.clear();if(input.size()<1200||input.size()>20000){error="Invalid native synthesizer checkpoint extent";return false;}
 std::vector<uint8_t> bytes=input;SynthArchive a(bytes,true);uint32_t magic=0,schema=0,rate=0;uint8_t coupled=0,phase=0,delay=0;uint64_t frames=0;
 a.field(magic);a.field(schema);bool identityValid=true;const char identity[]="335747d78cb0abbc3b55b004e62dad9763140115:8fa385546f0f2d1c975d097002af00cd729ae2ae097c068e9c883ce08ddf3a76";for(unsigned i=0;i<sizeof(identity)-1;++i){uint8_t c=0;a.field(c);if(c!=uint8_t(identity[i]))identityValid=false;}
 a.field(rate);a.field(coupled);a.field(frames);Impl staged(_impl->rate,_impl->coupledOutput);a.field(phase);a.field(delay);a.field(staged.cycleRemaining);a.field(staged.pin);
 for(unsigned i=0;i<2;++i){a.field(staged.coupling[i]._input);a.field(staged.coupling[i]._output);}
 a.field(staged.current.part);a.field(staged.current.address);a.field(staged.current.value);uint16_t count=0;a.field(count);if(count>4096){error="Native synthesizer pending-write extent";return false;}for(unsigned i=0;i<count;++i){FmSynthWrite w;a.field(w.part);a.field(w.address);a.field(w.value);if(w.part>1)a.ok=false;staged.pending.push_back(w);}
 ym3438_t& generator=staged.generator;
#define FM_NATIVE_FIELD(name) a.field(generator.name);
#include "fm_synth_fields.inc"
#undef FM_NATIVE_FIELD
 const size_t checksumAt=a.at;uint64_t checksum=0;a.field(checksum);
 bool couplingValid=staged.pin[0]>=-771&&staged.pin[0]<=765&&staged.pin[1]>=-771&&staged.pin[1]<=765;for(unsigned i=0;i<2;++i)if(staged.coupling[i]._input<-(INT64_C(1)<<41)||staged.coupling[i]._input>(INT64_C(1)<<41)||staged.coupling[i]._output<-(INT64_C(1)<<43)||staged.coupling[i]._output>(INT64_C(1)<<43))couplingValid=false;
 if(!a.ok||a.at!=bytes.size()||magic!=0x4e465331||schema!=1||!identityValid||(expectedFrames!=UINT64_MAX&&frames!=expectedFrames)||rate!=_impl->rate||coupled>1||bool(coupled)!=_impl->coupledOutput||phase>2||delay>2||(!phase&&delay)||(phase&&!delay)||staged.current.part>1||staged.cycleRemaining>rate*42||!couplingValid||!validSynthFields(generator)||checksum!=synthChecksum(bytes,checksumAt)){error="Invalid native synthesizer continuation state";return false;}
 staged.phase=phase;staged.delay=delay;staged.frames=frames;*_impl=staged;return true;
}
