/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "line_scroll.h"
namespace Snatcher {
void NativeLineScroll::reset(){_state=NativeLineScrollState();}
void NativeLineScroll::start(){std::string error;start(3,error);}
bool NativeLineScroll::start(unsigned mode,std::string& error){error.clear();if(mode<3||mode>6){error="unsupported native line-scroll mode";return false;}reset();_state.active=true;_state.mode=uint8_t(mode);_state.lineEnabled=true;_state.publishedPrefix=128;return true;}
bool NativeLineScroll::tick(uint16_t globalCounter,std::string& error){
 error.clear();if(!_state.active)return true;
 if(_state.mode==5){
  // Main FF36FE..37BE: one pattern install, then8/13/7 source updates.
  if(_state.phase==0){
   const unsigned first[]={48,51,53,72,96},length[]={3,2,3,24,16},sample[]={8,12,8,1022,1020};
   for(unsigned j=0;j<5;++j)for(unsigned i=first[j];i<first[j]+length[j];++i)_state.computed[i]=uint16_t(sample[j]);
   _state.publishedPrefix=112;_state.phase=1;
  }else if(_state.phase==1){if(++_state.counters[0]==8){_state.lineEnabled=false;_state.phase=2;}}
  else if(_state.phase==2){if(++_state.counters[1]==13){_state.lineEnabled=true;_state.phase=3;}}
  else if(++_state.counters[2]==7){_state.active=false;_state.lineEnabled=false;_state.completed=true;}
 }else if(_state.mode==6){
  for(unsigned i=112;i<121;++i)_state.computed[i]=uint16_t((_state.computed[i]+2)&1023);
  for(unsigned i=121;i<128;++i)_state.computed[i]=uint16_t((_state.computed[i]+1022)&1023);
  _state.publishedPrefix=128;
 }else{
  const unsigned periods3[5]={6,10,12,14,18},periods4[5]={7,6,5,4,3};
  const unsigned* periods=_state.mode==3?periods3:periods4;
  bool local[5];for(unsigned i=0;i<5;++i){local[i]=++_state.counters[i]==periods[i];if(local[i])_state.counters[i]=0;}
  if(_state.mode==3){
   const bool update[9]={(globalCounter&1)==0,(globalCounter&3)==0,local[0],(globalCounter&7)==0,local[1],local[2],local[3],(globalCounter&15)==0,local[4]};
   static const unsigned lengths[9]={8,9,12,10,8,6,5,4,3};
   unsigned first=0;for(unsigned band=0;band<9;++band){unsigned end=first+lengths[band];if(update[band]){for(unsigned i=first;i<end;++i)_state.computed[i]=uint16_t((_state.computed[i]+1)&1023);_state.publishedPrefix=uint16_t(end);}first=end;}
  }else{
   const unsigned first[]={0,112,116,120,124},length[]={112,4,4,4,4};
   for(unsigned j=0;j<5;++j)if(local[j]){unsigned end=first[j]+length[j];for(unsigned i=first[j];i<end;++i)_state.computed[i]=uint16_t((_state.computed[i]+1023)&1023);_state.publishedPrefix=uint16_t(end);}
  }
 }
 // The producer writes a prefix ending at the last band updated this call.
 // Unpublished rows retain their prior visible offsets.
 for(unsigned i=0;i<_state.publishedPrefix;++i)_state.published[i]=_state.computed[i];
 _state.counterKnown=true;_state.lastGlobalCounter=globalCounter;return true;
}
bool NativeLineScroll::restore(const NativeLineScrollState& s,std::string& error){
 error.clear();
 if(s.version!=2||s.mode>6||(s.mode&&s.mode<3)||s.publishedPrefix>128||s.phase>3){error="invalid native line-scroll state";return false;}
 NativeLineScroll initial;if(!s.mode){const NativeLineScrollState& z=initial.state();if(s.active||s.counterKnown||s.lastGlobalCounter||s.publishedPrefix||s.phase||s.lineEnabled||s.completed||s.counters!=z.counters||s.computed!=z.computed||s.published!=z.published){error="inactive native line-scroll history";return false;}}
 else{
  if(s.completed!=(s.mode==5&&!s.active)||(!s.active&&!s.completed)||s.lineEnabled!=(s.active&&!(s.mode==5&&s.phase==2))){error="invalid native line-scroll lifecycle";return false;}
  const unsigned p3[]={6,10,12,14,18},p4[]={7,6,5,4,3};
  if(s.mode==3||s.mode==4){const unsigned* periods=s.mode==3?p3:p4;for(unsigned i=0;i<5;++i)if(s.counters[i]>=periods[i]){error="invalid native line-scroll cadence";return false;}}
  else if(s.mode==5){
   bool valid=!s.counters[3]&&!s.counters[4];
   if(s.phase==0)valid=valid&&!s.counterKnown&&s.publishedPrefix==128&&!s.counters[0]&&!s.counters[1]&&!s.counters[2];
   else if(s.phase==1)valid=valid&&s.counters[0]<8&&!s.counters[1]&&!s.counters[2];
   else if(s.phase==2)valid=valid&&s.counters[0]==8&&s.counters[1]<13&&!s.counters[2];
   else valid=valid&&s.counters[0]==8&&s.counters[1]==13&&(s.completed?s.counters[2]==7:s.counters[2]<7);
   if(!valid){error="invalid distortion cadence";return false;}
  }
  else for(unsigned i=0;i<5;++i)if(s.counters[i]){error="invalid mode6 cadence";return false;}
  if(s.mode!=5&&s.phase){error="phase outside finite distortion";return false;}
  if(!s.counterKnown){if(s.lastGlobalCounter||s.publishedPrefix!=128){error="line-scroll initial clock history";return false;}for(unsigned i=0;i<5;++i)if(s.counters[i]){error="line-scroll initial cadence";return false;}for(unsigned i=0;i<128;++i)if(s.computed[i]){error="line-scroll initial offsets";return false;}}
  bool prefix=s.publishedPrefix==128;
  if(s.mode==3)for(unsigned n:{8u,17u,29u,39u,47u,53u,58u,62u,65u})if(s.publishedPrefix==n)prefix=true;
  if(s.mode==4)for(unsigned n:{112u,116u,120u,124u})if(s.publishedPrefix==n)prefix=true;
  if(s.mode==5&&s.publishedPrefix==112)prefix=true;
  if(!prefix){error="invalid native line-scroll publication prefix";return false;}
 }
 for(unsigned i=0;i<128;++i)if(s.computed[i]>1023||s.published[i]>1023||s.computed[i]!=s.published[i]||(s.mode==3&&i>=65&&s.computed[i])){error="invalid native line-scroll offsets";return false;}
 _state=s;return true;
}
int NativeLineScroll::sourceX(unsigned row)const{return _state.active&&_state.lineEnabled&&row>=8&&row<136?_state.published[row-8]:0;}
}
