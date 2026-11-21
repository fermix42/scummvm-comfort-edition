/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "text_entry.h"
#include <algorithm>
namespace Snatcher {
bool textEntryMenuContext(const uint8_t* data,size_t size,uint16_t x,uint16_t y,uint16_t& context,std::string& error){error.clear();const size_t table=0x109e8-0xd400;unsigned column=uint8_t(uint16_t(x+0x48)>>2),row=uint8_t(uint16_t(y+0x28)>>2);if(!data||size<table+720||column>=36||row>=20){error="pointer context outside original grid owner";return false;}context=data[table+row*36+column];return true;}
TextEntry::TextEntry():_loaded(false),_active(false),_completed(false),_selected(0),_heldRepeat(0),_pendingKey(0),_cursor(0),_gridIndex(0),_mode(0),_columns(0),_originX(0),_originY(0),_textX(0),_phase(0),_anchor(0),_blinkDeadline(0),_underline(false),_blinkStarted(false){_buffer.fill(0);_repeat.fill(0);}
bool TextEntry::loadAssets(const uint8_t* p,size_t n,std::string& error){error.clear();if(!p||n<0x11942-0xd400){error="truncated original entry tables";return false;}OriginalFont font;if(!font.load(p,n,error))return false;_font=font;_graphicsSource.assign(p,p+n);_graphicsBusy=_graphicsReady=false;_numeric.assign(p+0x114dc-0xd400,p+0x11512-0xd400);_alpha.assign(p+0x11512-0xd400,p+0x115ca-0xd400);_frames.assign(p+0x115ca-0xd400,p+0x11682-0xd400);_phoneLengths.assign(p+0x11938-0xd400,p+0x11942-0xd400);_numericText.assign(p+0x11454-0xd400,p+0x1148e-0xd400);_loaded=true;return true;}
void TextEntry::coordinates(TextEntryEffects& e)const{e.cursorX=uint8_t(_originX+(_gridIndex%_columns)*8);e.cursorY=uint8_t(_originY+(_gridIndex/_columns+1)*8);}
bool TextEntry::begin(const std::vector<TextEntryChoice>& choices,const TextEntrySettings& settings,TextEntryEffects& e,std::string& error){error.clear();e=TextEntryEffects();if(!_loaded||choices.empty()||choices.size()>255||settings.session>2||settings.limit>18){error="invalid text-entry ownership";return false;}for(size_t i=0;i<choices.size();++i)if(choices[i].label.empty()||std::find(choices[i].label.begin(),choices[i].label.end(),uint8_t(255))!=choices[i].label.end()){error="invalid original choice label";return false;}_graphicsBusy=_graphicsReady=false;_choices=choices;_settings=settings;_pendingKey=0;_selected=0;_completed=false;if(settings.session==2){finish(e);return true;}if(choices.size()<9){error="entry requires original large-choice dispatch";return false;}_mode=choices[0].label[0]>=0x41?3:2;_settings.mode=_mode;_columns=_mode==2?6:26;_gridIndex=_mode==2?45:27;_originX=_mode==2?192:16;_originY=_mode==2?135:144;_textX=settings.limit?64:(_mode==2?24:48);if(!_settings.limit)_settings.limit=_mode==2?10:17;_buffer.fill(0);_cursor=0;_phase=0;_underline=false;_blinkStarted=false;_blinkDeadline=0;_active=true;e.view=_mode==2?15:16;e.changed=true;e.cursorMoved=true;coordinates(e);return true;}
void TextEntry::finish(TextEntryEffects& e){std::vector<uint8_t> text;for(size_t i=0;i<_buffer.size();++i)if(_buffer[i])text.push_back(_buffer[i]);if(text.empty()||std::find_if(text.begin(),text.end(),[](uint8_t c){return c!=32;})==text.end())text.assign(1,32);_selected=0;for(size_t i=0;i<_choices.size();++i){bool match=_settings.session==1?std::find(_choices[i].label.begin(),_choices[i].label.end(),text[0])!=_choices[i].label.end():_choices[i].label==text;if(match){_selected=i;break;}}bool autoMatch=_settings.session==2;_settings.session=uint16_t(_settings.session+1);if(_settings.session!=2){_settings.session=0;_settings.mode=0;}_settings.limit=_settings.phonePolicy=0;_active=false;_completed=true;e.completed=true;e.changed=true;if(!autoMatch){e.view=_mode==2?254:17;e.cursorHidden=true;}}
void TextEntry::erase(){if(!_cursor)return;if(!_buffer[_cursor])--_cursor;_buffer[_cursor]=0;}
void TextEntry::startFontRequest(const FontRequest& request){_graphicsProgram=request.bytes;_graphicsBlink=request.blink;_graphicsFont.cursor=0;_graphicsFont.delay=_graphicsFont.remaining=0;_graphicsFont.active=true;}
void TextEntry::queueUnderline(){_graphicsUnderline=!_graphicsUnderline;_underline=_graphicsUnderline;_graphicsQueue.push_back(FontRequest({0xfc,0,0xf9,1,0xfb,uint8_t(_textX+_cursor*9),33,0xfe,uint8_t(_graphicsUnderline?1:0),'_',0xff},true));}
bool TextEntry::beginGraphics(bool fixedWidth,std::string& error){
 error.clear();if(!_loaded||!_active||_graphicsSource.empty()){error="text-entry font setup requires an active original owner";return false;}
 _graphicsBusy=_graphicsInitial=true;_graphicsReady=false;_graphicsPhase=0;_graphicsQueue.clear();_graphicsProgram.clear();_graphicsFont=FontProgramState();_graphicsFont.fixedWidth=fixedWidth;_graphicsUnderline=true;_graphicsBlink=false;_graphicsFresh=true;_graphicsResetCaption=_graphicsPolicy=_graphicsPalette=_graphicsNamesPending=_graphicsClearCanvas=_graphicsClearBuffer=false;_graphicsDirtyRows=0;_graphicsView=-1;initializeOriginalFontCanvas(_graphicsCanvas,_graphicsNames);
 for(unsigned i=0;i<_settings.limit;++i)_graphicsQueue.push_back(FontRequest({0xfc,0,0xf9,1,0xfb,uint8_t(_textX+i*9),33,0xfe,1,'_',0xff}));
 return true;
}
bool TextEntry::queueGraphics(const TextEntryEffects& effects,std::string& error){
 error.clear();if(!effects.bodyRedraw&&!effects.underlineRedraw)return true;if(!_graphicsReady||_graphicsBusy){error="text-entry font producer is not ready for another request";return false;}
 _graphicsQueue.clear();_graphicsBusy=true;_graphicsInitial=false;_graphicsPhase=effects.bodyRedraw?12:13;
 if(effects.bodyRedraw){_graphicsQueue.push_back(FontRequest({0xfc,0,0xf7,1,0xff}));std::vector<uint8_t> body={0xfc,0,0xf9,1,0xfb,_textX,24,0xfe,1};for(uint8_t code:_buffer)if(code)body.push_back(code);if(body.size()==9)body.push_back(' ');body.push_back(0xff);_graphicsQueue.push_back(FontRequest(body));_graphicsQueue.push_back(FontRequest({0xfc,0,0xf7,0,0xff}));}
 queueUnderline();return true;
}
// SUB11DCE/11DF6/13D1C: modal close, yield, queued16 canvas/name reset.
// Queued16 dispatches through SUB1037C to13D4E; it is not a second view close.
bool TextEntry::beginCloseGraphics(int view,std::string& error){
 error.clear();if(!_completed||!_graphicsReady||_graphicsBusy||(view!=17&&view!=254)){error="entry close requires its completed graphics owner";return false;}
 _graphicsBusy=true;_graphicsPhase=14;_graphicsView=view;return true;
}
bool TextEntry::coordinateGraphics(bool visualBusy,TextEntryEffects& effects,std::string& error){
 error.clear();effects=TextEntryEffects();if(!_graphicsBusy||_graphicsFont.active)return true;
 switch(_graphicsPhase){
 case 0:_graphicsResetCaption=true;_graphicsPhase=1;break;
 case 1:_graphicsPolicy=true;_graphicsPhase=2;break;
 case 2:_graphicsPalette=true;_graphicsPhase=3;break;
 case 3:initializeOriginalFontCanvas(_graphicsCanvas,_graphicsNames);_graphicsFresh=_graphicsClearCanvas=_graphicsNamesPending=true;_graphicsPhase=4;break;
 case 4:if(_mode==2){startFontRequest(FontRequest(_numericText));_graphicsPhase=5;}else{_graphicsView=16;_graphicsPhase=6;}break;
 case 5:_graphicsView=15;_graphicsPhase=8;break;
 case 6:_graphicsPhase=7;break;
 case 7:if(visualBusy)return true;_graphicsPhase=8; // Source view wait then underline loop.
  // fall through
 case 8:if(!_graphicsQueue.empty()){startFontRequest(_graphicsQueue.front());_graphicsQueue.pop_front();}else{queueUnderline();startFontRequest(_graphicsQueue.front());_graphicsQueue.pop_front();_graphicsPhase=9;}break;
 case 9:_graphicsBusy=false;_graphicsReady=true;_graphicsInitial=false;effects.cursorMoved=true;coordinates(effects);break;
 case 14:_graphicsPhase=15;break; // Modal request is staged before the next visit.
 case 15:if(visualBusy)return true;_graphicsPhase=16;break;
 case 16:initializeOriginalFontCanvas(_graphicsCanvas,_graphicsNames);_graphicsFresh=_graphicsClearCanvas=_graphicsNamesPending=true;_graphicsPhase=17;break;
 case 17:_graphicsBusy=false;break; // Caller also waits for Main publication.
 case 12:std::fill(_graphicsCanvas.begin()+3072,_graphicsCanvas.begin()+3872,uint8_t(0));_graphicsClearBuffer=true;_graphicsPhase=13;break;
 case 13:if(!_graphicsQueue.empty()){startFontRequest(_graphicsQueue.front());_graphicsQueue.pop_front();}else _graphicsBusy=false;break;
 default:error="invalid text-entry font producer phase";return false;
 }
 return true;
}
bool TextEntry::consumeGraphics(uint8_t counter,std::string& error){
 error.clear();if(!_graphicsBusy||!_graphicsFont.active)return true;
 if(!_font.stepProgram(_graphicsProgram,_graphicsFont,_graphicsCanvas,_graphicsDirtyRows,error))return false;
 if(!_graphicsFont.active&&_graphicsBlink){_blinkStarted=true;_blinkDeadline=uint8_t(counter+32);_graphicsBlink=false;}
 return true;
}
bool TextEntry::move(int delta,std::string& error){int next=int(_gridIndex);const std::vector<uint8_t>& g=grid();for(unsigned guard=0;guard<512;++guard){next+=delta;if(next<0||size_t(next)>=g.size()){error="entry grid redirect escaped original table";return false;}uint8_t c=g[size_t(next)];if(!c)return true;if(c==1)continue;if(c==2){delta=1;continue;}if(c==3){delta=-1;continue;}if(c==4){delta=_columns;continue;}if(c==5){delta=-int(_columns);continue;}if(c==6){next+=_columns;delta=-1;continue;}if(c==7){next-=_columns;delta=1;continue;}_gridIndex=uint8_t(next);return true;}error="entry grid redirect cycle";return false;}
void TextEntry::advanceHeldRepeat(uint8_t pressed,uint8_t held){_heldRepeat=0;for(unsigned i=0;i<8;++i){uint8_t bit=uint8_t(1u<<i);if(pressed&bit){_repeat[i]=32;_heldRepeat|=bit;}else if(held&bit){if(_repeat[i]&&!--_repeat[i]){_repeat[i]=4;_heldRepeat|=bit;}}else _repeat[i]=0;}}
bool TextEntry::step(uint8_t pressed,uint8_t held,uint8_t tick,TextEntryEffects& e,std::string& error,bool repeatAlreadyAdvanced){error.clear();e=TextEntryEffects();if(!_active||_pendingKey){error="entry has no ready input owner";return false;}if(!repeatAlreadyAdvanced)advanceHeldRepeat(pressed,held);uint8_t repeated=_heldRepeat;
 if(!_blinkStarted){_blinkStarted=true;_blinkDeadline=uint8_t(tick+32);}else if(tick==_blinkDeadline){_underline=!_underline;_blinkDeadline=uint8_t(tick+32);e.underlineRedraw=true;e.changed=true;e.inputDeferred=true;coordinates(e);return true;}
 uint8_t input=pressed;if(pressed)_phase=0;else if(!_phase){_anchor=tick;if(held){_phase=1;--_anchor;}}else if(_phase==1){if(!((tick^_anchor)&31)){_phase=2;input=held;}}else if(_mode==3)input=repeated;else if(!((tick^_anchor)&7))input=held;
 uint8_t previousGrid=_gridIndex;if(input&1){if(!move(-int(_columns),error))return false;e.changed=true;}else if(input&2){if(!move(_columns,error))return false;e.changed=true;}else if(input&8){if(!move(1,error))return false;e.changed=true;}else if(input&4){if(!move(-1,error))return false;e.changed=true;}else if(input&0x10){e.bodyRedraw=_cursor!=0;erase();e.click=e.changed=true;if(_mode==3){e.keyActor=62;e.keyRecord=104;e.keyX=88;e.keyY=192;e.cursorHidden=_gridIndex!=139;e.waitKeyCompletion=true;_pendingKey=1;}}else if((pressed&0x80)&&!_settings.phonePolicy){e.click=true;if(_mode==3){e.keyActor=62;e.keyRecord=108;e.keyX=200;e.keyY=192;e.cursorHidden=true;}e.waitKeyCompletion=true;_pendingKey=2;}else if(input&0x20){uint8_t c=grid()[_gridIndex];e.changed=true;e.click=!_settings.phonePolicy;if(_settings.phonePolicy)e.keyActor=c=='*'?2:c=='#'?3:c&31;else if(_mode==3){e.keyActor=62;e.keyRecord=_frames[_gridIndex];}if(c==8){e.click=true;e.waitKeyCompletion=true;_pendingKey=2;}else if(c==9){e.click=true;e.bodyRedraw=_cursor!=0;erase();}else if(c>=8){if(_cursor<_settings.limit){e.bodyRedraw=true;_buffer[_cursor]=c;if(_cursor+1!=_settings.limit)++_cursor;}if(_settings.phonePolicy){uint8_t first=_buffer[0];unsigned length=(first=='*'||first=='#')?2:((first&15)<_phoneLengths.size()?_phoneLengths[first&15]:0);if(length&&length<=18&&_buffer[length-1])finish(e);}}}e.cursorMoved=previousGrid!=_gridIndex;coordinates(e);return true;}
std::vector<MouseTarget> TextEntry::mouseTargets()const{
 std::vector<MouseTarget> out;if(!_active||_pendingKey||!_columns)return out;
 const std::vector<uint8_t>& g=grid();
 for(unsigned i=0;i<g.size();++i)if(g[i]>=8){
  MouseRect rect(_originX+int(i%_columns)*8,_originY+int(i/_columns+1)*8,8,8);
  if(_mode==3){
   // The source grid uses 8-pixel cursor anchors, but the keyboard artwork
   // has 16-pixel keys. Wide keys use anchors inside their visible faces.
   rect.w=rect.h=16;
   if(i==139){rect.x=72;rect.w=48;} // Backspace
   else if(i==146){rect.x=120;rect.w=64;} // Space
   else if(i==153){rect.x=184;rect.w=48;} // OK
  }
  out.push_back(MouseTarget(rect,i));
 }
 if(_mode==2&&!_settings.phonePolicy)out.push_back(MouseTarget(MouseRect(24,192,64,16),kTextEntryMouseSubmit));return out;
}
uint8_t TextEntry::mouseActionMask(unsigned target)const{for(const MouseTarget& t:mouseTargets())if(t.id==target)return target==kTextEntryMouseSubmit?0x80:0x20;return 0;}
bool TextEntry::mouseSelect(int x,int y,TextEntryEffects& e,std::string& error){
 error.clear();e=TextEntryEffects();for(const MouseTarget& t:mouseTargets())if(t.id<grid().size()&&t.rect.contains(x,y)){e.cursorMoved=_gridIndex!=t.id;_gridIndex=uint8_t(t.id);e.changed=e.cursorMoved;coordinates(e);return true;}error="mouse entry selection outside enabled grid";return false;
}
bool TextEntry::resumeKey(bool actorComplete,TextEntryEffects& e,std::string& error){error.clear();e=TextEntryEffects();if(!_active||!_pendingKey){error="entry has no pending source key barrier";return false;}if(!actorComplete){e.waitKeyCompletion=true;return true;}uint8_t pending=_pendingKey;_pendingKey=0;e.restoreKeyPosition=_mode==3;e.changed=true;if(pending==2)finish(e);coordinates(e);return true;}
bool TextEntry::transfers(std::vector<GraphicsTransfer>& out,std::string& error)const{out.clear();error.clear();if(!_loaded||(!_active&&!_completed)){error="entry text allocation unavailable";return false;}std::vector<uint8_t> patterns,names;initializeOriginalFontCanvas(patterns,names);auto draw=[&](uint8_t code,unsigned x,unsigned y,uint8_t color)->bool{FontGlyph g;return _font.glyph(code,g,error)&&rasterOriginalFontGlyph(g,code,uint8_t(x),uint8_t(y),color,true,patterns,error);};if(_mode==2){unsigned x=0,y=0;uint8_t color=1;for(size_t i=0;i<_numericText.size();){uint8_t code=_numericText[i++];if(code==255)break;if(code==0xf7||code==0xfe){if(i==_numericText.size()){error="truncated original numeric text control";return false;}uint8_t value=_numericText[i++];if(code==0xfe)color=value;continue;}if(code==0xfb){if(_numericText.size()-i<2){error="truncated original numeric position";return false;}x=_numericText[i++];y=_numericText[i++];continue;}if(color>15||y>56||!draw(code,x,y,color)){if(error.empty())error="numeric text outside owned allocation";return false;}x=uint8_t(x+9);}}
 unsigned x=_textX;for(unsigned i=0;i<18;++i)if(_buffer[i]){if(!draw(_buffer[i],x,24,1))return false;x=uint8_t(x+9);}if(_active)for(unsigned i=0;i<_settings.limit;++i)if((i!=_cursor||_underline)&&!draw('_',uint8_t(_textX+i*9),33,1))return false;GraphicsTransfer t;t.recordOffset=t.sourceOffset=t.encodedEnd=0;t.delay=0;t.destination=0;t.decoded=patterns;out.push_back(t);t.destination=0xec80;t.decoded=names;out.push_back(t);return true;}
}
