/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "credits.h"
namespace Snatcher {
namespace {
uint16_t word(const uint8_t*p){return uint16_t(unsigned(p[0])*256+p[1]);}
uint32_t longWord(const uint8_t*p){return uint32_t(word(p))<<16|word(p+2);}
bool bcd(uint32_t v){if(v&255)return false;for(unsigned s=8;s<32;s+=4)if(((v>>s)&15)>9)return false;unsigned frames=((v>>12)&15)*10+((v>>8)&15),seconds=((v>>20)&15)*10+((v>>16)&15);return frames<75&&seconds<60;}
bool known(uint32_t h){if(h>=0x291d4&&h<=0x29204&&(h-0x291d4)%6==0)return true;switch(h){case 0x2920a:case 0x2923a:case 0x29250:case 0x2925c:case 0x29266:case 0x29280:case 0x2928c:case 0x29298:case 0x292ae:case 0x292bc:case 0x292e6:case 0x292f4:case 0x29310:return true;default:return false;}}
}
CreditsTimeline::CreditsTimeline():_active(false),_finished(false),_requested(false),_alternate(false),_index(-1),_countdown(0),_sourceStamp(0){}
bool CreditsTimeline::start(const std::vector<uint8_t>& bytes,bool alternate,std::string& error){
 error.clear();const size_t table=0x10fc,rows=28;
 if(bytes.size()<0x131e||bytes.size()>16*1024*1024||longWord(bytes.data())!=0x2800c||longWord(bytes.data()+8)!=0x290d4||longWord(bytes.data()+4)<0x2800c||uint64_t(longWord(bytes.data()+4))-0x28000>=bytes.size()){error="credits resource header/extent is not established";return false;}
 CreditsTimeline next;next._alternate=alternate;next._active=true;
 for(size_t i=0;i<rows;++i){const uint8_t*p=bytes.data()+table+i*6;int32_t h=int32_t(0x290fc)+int16_t(word(p));uint32_t deadline=longWord(p+2);if(h<0x28000||uint64_t(h)-0x28000>=bytes.size()||!known(uint32_t(h))||!bcd(deadline)){error="credits timeline contains an unknown handler or invalid deadline";return false;}Record r={uint32_t(h),deadline};next._records.push_back(r);}
 uint64_t stamp=UINT64_C(14695981039346656037);for(uint8_t b:bytes)stamp=(stamp^b)*UINT64_C(1099511628211);next._sourceStamp=stamp;*this=next;return true;
}
bool CreditsTimeline::waitingStart()const{return _active&&_index>=0&&size_t(_index)<_records.size()&&_records[size_t(_index)].handler==0x29298;}
bool CreditsTimeline::tick(const CreditsInput& in,std::vector<CreditsEvent>& events,std::string& error){error.clear();CreditsTimeline next=*this;std::vector<CreditsEvent> out;if(!next.tickOwned(in,out,error))return false;*this=next;events.swap(out);return true;}
bool CreditsTimeline::tickOwned(const CreditsInput& in,std::vector<CreditsEvent>& out,std::string& error){
 if(!_active||in.paused||!in.ready)return true;
 if(_index<0){if(!_requested){_requested=true;out.push_back(CreditsEvent(kCreditsStartTrack,19));return true;}if(!in.cdPlaying)return true;_index=0;_requested=false;out.push_back(CreditsEvent(kCreditsResetRelativeClock));return true;}
 if(size_t(_index)>=_records.size()){error="credits timeline exhausted without completion owner";return false;}
 if(!in.clockKnown)return true;if(!bcd(in.relativeClock)){error="credits relative CD clock is not source packed BCD";return false;}const Record& r=_records[size_t(_index)];if(r.deadline>in.relativeClock)return true;
 ++_index;const uint32_t h=r.handler;
 if(h>=0x291d4&&h<=0x29204&&(h-0x291d4)%6==0)out.push_back(CreditsEvent(kCreditsInstallScene,(h-0x291d4)/6));
 else if(h==0x29310||h==0x292e6||h==0x292f4)out.push_back(CreditsEvent(kCreditsPalette,h==0x29310?0x131a:h==0x292e6?0x12f0:0x12fe));
 else if(h==0x2920a){if(!in.cdIdleOrPaused)--_index;else{_countdown=300;if(_alternate)_index=24;}}
 else if(h==0x2925c)out.push_back(CreditsEvent(kCreditsStartTrack,2));
 else if(h==0x29266){if(!in.cdPlaying)--_index;else _countdown=24;}
 else if(h==0x2928c){_countdown=300;out.push_back(CreditsEvent(kCreditsInstallScene,9));}
 else if(h==0x29298){if(!in.startPressed)--_index;else{_countdown=64;out.push_back(CreditsEvent(kCreditsPalette,0x12e2));}}
 else if(h==0x292bc){_finished=true;_active=false;_index=0;out.push_back(CreditsEvent(kCreditsApplicationComplete));}
 else {if(!_countdown){error="credits countdown has no preceding lifecycle owner";return false;}if(--_countdown)--_index;else if(h==0x2923a){_countdown=64;out.push_back(CreditsEvent(kCreditsPalette,0x12e2));}}
 return true;
}
}
