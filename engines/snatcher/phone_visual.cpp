/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "phone_visual.h"
#include <algorithm>
namespace Snatcher {
namespace {
void clearView(PhoneVisualState& s,bool full){int16_t published=s.publishedPlaneBY;bool temporary=s.temporaryDialogue;s=PhoneVisualState();if(!full){s.publishedPlaneBY=published;s.temporaryDialogue=temporary;}}
bool valid(const PhoneVisualState& s){std::string error;return validatePhoneVisualState(s,error);}
}
bool validatePhoneVisualState(const PhoneVisualState& s,std::string& error){error.clear();bool ok=(!s.mode||(s.mode>=0x17&&s.mode<=0x1d))&&(!s.tag||(s.tag>=0x11&&s.tag<=0x17))&&s.phase<=2&&s.basePhase<=2&&s.publishedPlaneBY>=-64&&s.publishedPlaneBY<=96&&s.publishedPlaneBY%8==0;
 if(!s.mode)ok=ok&&!s.tag&&!s.phase&&!s.basePhase&&!s.offset&&!s.baseOffset;
 else if(s.mode==0x17){ok=ok&&!s.phase&&!s.offset;if(!s.basePhase)ok=ok&&!s.baseOffset;else if(s.basePhase==1){ok=ok&&s.baseOffset>=48&&s.baseOffset<=88&&s.baseOffset%8==0;if(s.baseOffset!=48)ok=ok&&s.tag==0x11;}else ok=ok&&s.baseOffset==96&&s.tag==0x11;}
 else{ok=ok&&!s.basePhase&&!s.baseOffset;if(!s.phase)ok=ok&&!s.offset;else{ok=ok&&s.tag==s.mode-6;if(s.mode==0x18)ok=ok&&(s.phase==1?(s.offset>=-64&&s.offset<0&&s.offset%8==0):s.offset==0);else if(s.mode==0x19||s.mode==0x1c)ok=ok&&s.phase==1&&s.offset==96;else if(s.mode==0x1b)ok=ok&&s.phase==1&&s.offset==-64;else ok=ok&&(s.phase==1?(s.offset>=48&&s.offset<=96&&s.offset%8==0):s.offset==48);}}
 if(!ok)error="phone view state is not reachable through recovered source phases";return ok;}
PhoneVisualRequestResult requestPhoneVisual(PhoneVisualState& s,uint16_t command,std::string& error){error.clear();if(command>255||!valid(s)){error="invalid native phone view state/command";return kPhoneVisualInvalid;}if(command==0xfe){if(!s.mode)return kPhoneVisualDelegate;bool reopen=s.mode==0x17&&s.temporaryDialogue;if(s.mode==0x17)s.publishedPlaneBY=0;clearView(s,reopen);return reopen?kPhoneVisualStartOrdinaryDialogue:kPhoneVisualAccepted;}if(command==0xf&&s.mode==0x17){s.temporaryDialogue=true;return kPhoneVisualAccepted;}if(command<0x17||command>0x1d)return kPhoneVisualDelegate;if(command==s.mode)return kPhoneVisualAccepted;s.mode=uint8_t(command);s.phase=s.basePhase=0;s.offset=s.baseOffset=0;return kPhoneVisualAccepted;}
bool tickPhoneVisual(PhoneVisualState& s,std::string& error){error.clear();if(!valid(s)){error="native phone phase outside source tables";return false;}if(!s.mode)return true;
 if(s.mode==0x17){if(!s.basePhase){s.baseOffset=48;s.basePhase=1;}else{if(s.basePhase==1){s.baseOffset=int16_t(s.baseOffset+8);if(s.baseOffset>=96){s.baseOffset=96;s.basePhase=2;}}s.tag=0x11;}}
 else if(s.mode==0x18){if(!s.phase){s.tag=0x12;s.offset=-64;s.phase=1;}else if(s.phase==1){s.offset=int16_t(s.offset+8);if(s.offset>=0){s.offset=0;s.phase=2;}}else clearView(s,false);}
 else if(s.mode==0x19){if(!s.phase){s.tag=0x13;s.offset=96;s.phase=1;}}
 else if(s.mode==0x1a||s.mode==0x1d){if(!s.phase){s.tag=s.mode==0x1a?0x14:0x17;s.offset=96;s.phase=1;}else if(s.phase==1){s.offset=int16_t(s.offset-8);if(s.offset<48){s.offset=48;s.phase=2;}}else clearView(s,true);}
 else if(s.mode==0x1b){if(!s.phase){s.tag=0x15;s.offset=-64;s.phase=1;}}
 else if(s.mode==0x1c){if(!s.phase){s.tag=0x16;s.offset=96;s.phase=1;}}
 return true;}
bool phoneVisualBusy(const PhoneVisualState& s){if(!s.mode)return false;if(s.mode==0x17)return s.basePhase<2;if(s.mode==0x19||s.mode==0x1b||s.mode==0x1c)return s.phase==0;return true;}
bool validatePhoneFramePublication(const PhoneFramePublication& f,std::string& error){
 error.clear();unsigned expected=f.tag==0x11?PhoneFramePublication::kSplit80Restore144:f.tag>=0x12&&f.tag<=0x15?PhoneFramePublication::kSplit80:f.tag>=0x16&&f.tag<=0x17?PhoneFramePublication::kSplit73:PhoneFramePublication::kDelegate;
 if((!f.known&&(f.tag||f.program||f.upperBY))||(f.tag&&!(f.tag>=0x11&&f.tag<=0x17))||f.program!=expected||f.upperBY<-64||f.upperBY>96||f.upperBY%8||(!f.tag&&f.upperBY)){error="invalid phone physical publication owner";return false;}return true;
}
bool beginPhonePublication(PhoneVisualState& s,PhoneFramePublication& f,std::string& error){
 if(!validatePhoneVisualState(s,error))return false;PhoneFramePublication next;next.known=true;next.tag=s.tag;
 if(s.tag){next.program=s.tag==0x11?PhoneFramePublication::kSplit80Restore144:s.tag>=0x16?PhoneFramePublication::kSplit73:PhoneFramePublication::kSplit80;next.upperBY=s.tag==0x11?s.baseOffset:s.offset;s.publishedPlaneBY=next.upperBY;}
 f=next;return validatePhoneFramePublication(f,error);
}
bool phoneVisualBands(const PhoneFramePublication& f,const PhoneVisualState& s,unsigned height,std::vector<PhoneVisualBand>& bands,std::string& error){
 bands.clear();if(!validatePhoneVisualState(s,error)||!validatePhoneFramePublication(f,error)||!height||height>512){if(error.empty())error="invalid phone scene band owner/extent";return false;}
 if(!f.known||f.program==PhoneFramePublication::kDelegate)return true;unsigned first=f.program==PhoneFramePublication::kSplit73?73:80;
 auto add=[&](unsigned begin,unsigned end,int16_t y){if(begin<height&&begin<end){PhoneVisualBand band={begin,std::min(end,height),0,y};bands.push_back(band);}};
 add(0,first,f.upperBY);int16_t middle=int16_t(-(f.program==PhoneFramePublication::kSplit80Restore144?s.baseOffset:s.offset));
 if(f.program==PhoneFramePublication::kSplit80Restore144){add(first,144,middle);add(144,height,s.publishedPlaneBY);}else add(first,height,middle);return true;
}
bool readLowerViewPalette(const std::vector<uint16_t>& source,const std::vector<bool>& known,std::vector<uint16_t>& colors,std::string& error){
 error.clear();if(source.size()!=128||known.size()!=128){error="lower-view palette requires its128-color source owner";return false;}std::vector<uint16_t> next;for(unsigned i=80;i<128;++i){if(!known[i]){error="lower-view palette references unresolved secondary color";return false;}if(source[i]&0xf111){error="lower-view source palette is not normal intensity";return false;}next.push_back(source[i]);}colors.swap(next);return true;
}
bool lowerViewPaletteSpans(const std::vector<uint16_t>& source,const std::vector<bool>& known,std::vector<ScenePaletteSpan>& spans,std::string& error){
 std::vector<uint16_t> colors;if(!readLowerViewPalette(source,known,colors,error))return false;
 std::vector<ScenePaletteSpan> next(12);for(unsigned group=0;group<12;++group){ScenePaletteSpan& span=next[group];span.row=136+group;span.column=0;span.firstColor=16+4*group;span.colors.assign(colors.begin()+4*group,colors.begin()+4*group+4);}spans.swap(next);return true;
}
GameLayoutRequest decodeGameLayoutRequest(uint16_t word){return !word?kGameLayoutUnchanged:word==0xff?kGameLayoutTall:kGameLayoutWide;}

}
