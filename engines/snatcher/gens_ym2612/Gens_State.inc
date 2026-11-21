/* SPDX-License-Identifier: LGPL-2.1-or-later
 * 2026 Snatcher integration contributors: scalar checkpoint API only.
 * Synthesis algorithm and tables are unchanged. No pointers or padding saved.
 */
#include <stdint.h>
#include <memory>
namespace {
struct GensArchive {
 std::vector<unsigned char>& bytes;size_t at=0;bool read,ok=true;
 GensArchive(std::vector<unsigned char>& b,bool r):bytes(b),read(r){}
 void field(int& value){uint32_t v=uint32_t(value);if(read){if(at+4>bytes.size()){ok=false;return;}v=0;for(unsigned i=0;i<4;++i)v=(v<<8)|bytes[at++];value=v<=INT_MAX?int(v):INT_MIN+int(v-0x80000000u);}else for(int i=3;i>=0;--i)bytes.push_back(static_cast<unsigned char>(v>>(i*8)));}
 template<size_t N>void field(int(&v)[N]){for(auto& n:v)field(n);}
 template<size_t N,size_t M>void field(int(&v)[N][M]){for(auto& row:v)field(row);}
 bool pointer(const int*& value,tables_t& g,unsigned type){
  int id=-1;if(!read){uintptr_t p=reinterpret_cast<uintptr_t>(value);auto find=[&](const void* base,size_t n,unsigned family){uintptr_t b=reinterpret_cast<uintptr_t>(base);if(p>=b&&p<b+n*4&&(p-b)%4==0)id=int((family<<16)|((p-b)/4));};
   if(type==0)find(g.DT_TAB,256,0);if(type==1)find(g.AR_TAB,128,1);if(type>=2)find(g.DR_TAB,96,2);if(type!=0)find(g.NULL_RATE,32,3);
  }field(id);if(!ok)return false;unsigned family=unsigned(id)>>16,index=unsigned(id)&65535;
  if(type==0&&family==0&&index<256&&index%32==0)value=reinterpret_cast<const int*>(g.DT_TAB)+index;
  else if(type==1&&family==1&&index<=62&&index%2==0)value=reinterpret_cast<const int*>(g.AR_TAB)+index;
  else if(type>=2&&family==2&&index<=62&&index%2==0)value=reinterpret_cast<const int*>(g.DR_TAB)+index;
  else if(type!=0&&family==3&&index==0)value=reinterpret_cast<const int*>(g.NULL_RATE);
  else return false;return true;
 }
};
bool gensArchive(Ym2612_GENS_Impl& p,GensArchive& a){
 a.field(p.mute_mask);a.field(p.g.LFOcnt);a.field(p.g.LFOinc);
 a.field(p.YM2612.TimerBase);
 a.field(p.YM2612.Status);
 a.field(p.YM2612.TimerA);
 a.field(p.YM2612.TimerAL);
 a.field(p.YM2612.TimerAcnt);
 a.field(p.YM2612.TimerB);
 a.field(p.YM2612.TimerBL);
 a.field(p.YM2612.TimerBcnt);
 a.field(p.YM2612.Mode);
 a.field(p.YM2612.DAC);
 a.field(p.YM2612.REG);
 for(auto& ch:p.YM2612.CHANNEL){
 a.field(ch.S0_OUT);
 a.field(ch.LEFT);
 a.field(ch.RIGHT);
 a.field(ch.ALGO);
 a.field(ch.FB);
 a.field(ch.FMS);
 a.field(ch.AMS);
 a.field(ch.FNUM);
 a.field(ch.FOCT);
 a.field(ch.KC);
 a.field(ch.FFlag);
  for(auto& sl:ch.SLOT){
   a.field(sl.MUL);
   a.field(sl.TL);
   a.field(sl.TLL);
   a.field(sl.SLL);
   a.field(sl.KSR_S);
   a.field(sl.KSR);
   a.field(sl.SEG);
   a.field(sl.env_xor);
   a.field(sl.env_max);
   a.field(sl.Fcnt);
   a.field(sl.Finc);
   a.field(sl.Ecurp);
   a.field(sl.Ecnt);
   a.field(sl.Einc);
   a.field(sl.Ecmp);
   a.field(sl.EincA);
   a.field(sl.EincD);
   a.field(sl.EincS);
   a.field(sl.EincR);
   a.field(sl.INd);
   a.field(sl.ChgEnM);
   a.field(sl.AMS);
   a.field(sl.AMSon);
   if(!a.pointer(sl.DT,p.g,0)||!a.pointer(sl.AR,p.g,1)||!a.pointer(sl.DR,p.g,2)||!a.pointer(sl.SR,p.g,2)||!a.pointer(sl.RR,p.g,2))return false;
   if(a.read)sl.OUTp=nullptr; // unused upstream member; never an audio routing pointer
   if(a.read&&(sl.Ecurp<0||sl.Ecurp>3||sl.Ecnt<0||sl.Ecnt>ENV_END||sl.TL<0||sl.TL>4064||sl.TLL<0||sl.TLL>4064||sl.KSR_S<0||sl.KSR_S>3||sl.KSR< -1||sl.KSR>31||sl.MUL<0||sl.MUL>30||sl.SEG<0||sl.SEG>15||sl.AMS<0||sl.AMS>31||sl.SLL<0||sl.SLL>ENV_END||sl.Ecmp<0||(sl.Ecmp>ENV_END&&!(sl.Ecmp==ENV_END+1&&sl.Ecnt==ENV_END&&sl.Einc==0&&sl.Ecurp>=SUBSTAIN))||sl.Einc<0||sl.Einc>ENV_END||sl.EincA<0||sl.EincA>ENV_END||sl.EincD<0||sl.EincD>ENV_END||sl.EincS<0||sl.EincS>ENV_END||sl.EincR<0||sl.EincR>ENV_END||(sl.env_xor!=0&&sl.env_xor!=ENV_MASK)||(sl.env_max!=INT_MAX&&sl.env_max!=ENV_MASK)))return false;
  }
  if(a.read&&(ch.ALGO<0||ch.ALGO>7||ch.FB<0||ch.FB>31||ch.FMS<0||ch.FMS>24||ch.AMS<0||ch.AMS>31||(ch.LEFT!=0&&ch.LEFT!=-1)||(ch.RIGHT!=0&&ch.RIGHT!=-1)))return false;
  for(unsigned i=0;i<4;++i)if(a.read&&(ch.FNUM[i]<0||ch.FNUM[i]>2047||ch.FOCT[i]<0||ch.FOCT[i]>7||ch.KC[i]<0||ch.KC[i]>31))return false;
 }
 if(a.read){if(p.mute_mask<0||p.mute_mask>63||p.YM2612.Mode<0||p.YM2612.Mode>255)return false;for(auto& bank:p.YM2612.REG)for(int v:bank)if(v< -1||v>255)return false;}
 return a.ok;
}
}
bool Ym2612_GENS_Emu::save_state(std::vector<unsigned char>& out) const{
 if(!impl)return false;std::vector<unsigned char> bytes;GensArchive a(bytes,false);if(!gensArchive(*impl,a))return false;out.swap(bytes);return true;
}
bool Ym2612_GENS_Emu::load_state(const std::vector<unsigned char>& input){
 if(!impl||input.size()>20000)return false;auto bytes=input;std::unique_ptr<Ym2612_GENS_Impl> p(new Ym2612_GENS_Impl(*impl));GensArchive a(bytes,true);int timerBase=impl->YM2612.TimerBase;if(!gensArchive(*p,a)||a.at!=bytes.size()||p->YM2612.TimerBase!=timerBase)return false;
 // Move scalar state, then rebind every table reference to this instance.
 // Serializing the staged pointers creates the same symbolic table indices.
 std::vector<unsigned char> canonical;GensArchive writer(canonical,false);if(!gensArchive(*p,writer))return false;GensArchive reader(canonical,true);return gensArchive(*impl,reader);
}
