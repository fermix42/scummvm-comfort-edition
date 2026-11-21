/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dialogue_window.h"
#include <algorithm>
namespace Snatcher {
namespace {
uint16_t word(const uint8_t *p){return uint16_t(uint16_t(p[0])<<8|p[1]);}
GraphicsTransfer packet(uint16_t destination,const std::vector<uint8_t>& bytes){GraphicsTransfer t;t.recordOffset=t.sourceOffset=t.encodedEnd=0;t.destination=destination;t.delay=0;t.decoded=bytes;return t;}
}
DialogueLayout freshModuleDialogueLayout(){DialogueLayout layout;layout.established=true;return layout;}
DialogueWindow::DialogueWindow():_loaded(false){cancel();}
void DialogueWindow::cancel(){_fontModePending=_fontModeBound=false;_menuTask=_menuOpening=_menuCursorPending=false;_menuPhase=0;_menuPrograms.clear();_menuProgramCursor=_menuSelectionProgram=0;_taskOwned=_ackPending=_captionResetPending=_policyPending=_atlasPending=false;_taskPhase=kTaskReset;_dirtyRows=0;_namesDirty=_clearCanvasPending=_promptNamesDirty=false;_transition=0;_views.clear();_menu=false;_menuSelection=0;_menuCount=0;_source.clear();_patterns.clear();_names.clear();_stream.clear();_sourceBase=_sourceCursor=_streamCursor=_revealed=0;_speakerWord=0;_speaker=_bodyY=_textColor=_color=_x=_y=_delay=_remaining=_promptOffset=_promptRemaining=_mode=0;_active=_waiting=_finished=_textEnd=_streamActive=_nameStage=_lineBreak=_pause=_promptVisible=false;}
bool DialogueWindow::loadAssets(const uint8_t *data,size_t size,std::string& error){
 error.clear();if(!data||size<0xa4f8){error="truncated original dialogue assets";return false;}OriginalFont font;if(!font.load(data,size,error))return false;
 // Source ECB6 compressed atlas record: source177A4, allocationBCE0,800bytes.
 const size_t record=0xecb6-0xd400;if(data[record]!=0x41||word(data+record+6)!=0xbce0||word(data+record+10)!=800){error="unestablished original prompt atlas allocation";return false;}
 uint32_t source=(uint32_t(data[record+2])<<24)|(uint32_t(data[record+3])<<16)|(uint32_t(data[record+4])<<8)|data[record+5];
 if(source<0xd400||source-0xd400+2>size){error="prompt asset source outside SUBCODE";return false;}size_t offset=source-0xd400,consumed=0;std::vector<uint8_t> patterns;
 if(word(data+offset)!=800||!decodeGraphics(data+offset+2,size-offset-2,800,800,patterns,consumed,error))return false;
 const size_t cursorRecord=0xecc4-0xd400;uint32_t cursorSource=(uint32_t(data[cursorRecord+2])<<24)|(uint32_t(data[cursorRecord+3])<<16)|(uint32_t(data[cursorRecord+4])<<8)|data[cursorRecord+5];
 if(cursorSource<0xd400||cursorSource-0xd400+2>size||word(data+cursorRecord+6)!=0xde00||word(data+cursorRecord+10)!=512){error="unestablished original menu cursor allocation";return false;}size_t cursorOffset=cursorSource-0xd400;std::vector<uint8_t> cursorPatterns;std::vector<FramePiece> cursorGeometry;
 if(word(data+cursorOffset)!=512||!decodeGraphics(data+cursorOffset+2,size-cursorOffset-2,512,512,cursorPatterns,consumed,error)||!readFrameGeometry(data,size,0xecd2-0xd400,0,64,cursorGeometry,error))return false;
 std::vector<uint8_t> copy(data,data+size);_cursorPatterns.swap(cursorPatterns);_cursorGeometry.swap(cursorGeometry);_font=font;_subcode.swap(copy);_promptPatterns.swap(patterns);_loaded=true;cancel();return true;
}
void DialogueWindow::resetCanvas(){_canvasFresh=true;_clearCanvasPending=_namesDirty=true;_promptNamesDirty=false;_dirtyRows=0;initializeOriginalFontCanvas(_patterns,_names);_promptVisible=false;}
bool DialogueWindow::begin(const uint8_t *data,size_t size,size_t offset,uint16_t speakerWord,const DialogueLayout& layout,std::string& error){DialogueWindow staged=*this;if(!staged.beginOwned(data,size,offset,speakerWord,layout,error))return false;*this=staged;return true;}
bool DialogueWindow::beginTask(const uint8_t* data,size_t size,size_t offset,uint16_t speakerWord,const DialogueLayout& layout,std::string& error){
 DialogueWindow next=*this;if(!next.begin(data,size,offset,speakerWord,layout,error))return false;
 next._fontModeBound=true;next._taskOwned=true;next._taskPhase=kTaskReset;next._sourceCursor=next._streamCursor=next._revealed=0;next._stream.clear();next._streamActive=next._nameStage=next._waiting=next._finished=next._textEnd=next._lineBreak=next._pause=false;next._mode=uint8_t(speakerWord>>8);next._bodyY=layout.top;next._textColor=next._color=1;next._x=next._y=next._delay=next._remaining=0;next._views.clear();next._transition=0;next._dirtyRows=0;next._namesDirty=next._clearCanvasPending=next._promptNamesDirty=false;next._canvasFresh=true;*this=next;return true;
}
bool DialogueWindow::coordinateTask(std::string& error){
 // The queued caption coordinator runs before the font consumer. Each phase
 // is one game task visit, not a promised video callback: Main copy ownership
 // can prevent a Sub worker visit. Keep that scheduling owner outside text.
 error.clear();if(!_taskOwned||!_active||_finished||_streamActive)return true;DialogueWindow next=*this;
 switch(next._taskPhase){
 case kTaskReset:next._captionResetPending=true;next._taskPhase=kTaskPolicy;break;
 case kTaskPolicy:next._policyPending=true;next._taskPhase=kTaskAtlas;break;
 case kTaskAtlas:next._atlasPending=true;next._taskPhase=kTaskCanvas;break;
 case kTaskCanvas:next.resetCanvas();next._sourceCursor=0;next._textEnd=next._lineBreak=next._pause=false;next._textColor=1;next._views.push_back(kDialogueDefaultView);next._taskPhase=kTaskSpeaker;break;
 case kTaskSpeaker:if(next._speaker&&next._speaker!=255){if(!next.queueSpeaker(error))return false;}else next._bodyY=next._layout.top;next._taskPhase=kTaskBody;if(!next._speaker)next._views.push_back(kDialogueSpecialView);break;
 case kTaskBody:next._nameStage=false;if(!next.queueBody(error))return false;if(next._textEnd||next._pause)next._taskPhase=kTaskReady;else{next._bodyY=uint8_t(next._bodyY+9);if(next._bodyY>=0x39)next._taskPhase=kTaskReady;}break;
 case kTaskReady:next._waiting=true;next._promptOffset=0;next._promptRemaining=1;next._taskPhase=kTaskInput;break;
 case kTaskInput:{bool automatic=false;if(!next._ackPending&&!next.advancePrompt(automatic,error))return false;if(next._ackPending||automatic){next._ackPending=false;next._waiting=false;next._views.push_back(kDialogueCloseView);next._taskPhase=next._textEnd?kTaskFinish:kTaskNextCanvas;}break;}
 case kTaskFinish:next._finished=true;next._active=false;break;
 case kTaskNextCanvas:next.resetCanvas();next._taskPhase=kTaskReopen;break;
 case kTaskReopen:next._views.push_back(kDialogueDefaultView);next._taskPhase=kTaskSpeaker;break;
 default:error="invalid native dialogue task phase";return false;
 }
 *this=next;return true;
}
bool DialogueWindow::tickGlyphs(bool paused,std::string& error,bool fastReveal){error.clear();if(paused||!_active||_finished||!_streamActive)return true;DialogueWindow next=*this;if(fastReveal)next._delay=0;if(!next.consume(error))return false;*this=next;return true;}
bool DialogueWindow::beginOwned(const uint8_t *data,size_t size,size_t offset,uint16_t speakerWord,const DialogueLayout& layout,std::string& error){
 error.clear();if(!_loaded||!layout.established||!data||size>65536||offset>=size||layout.top>9||layout.variant>2||(layout.substitutionKnown&&layout.substitution>5)){error="invalid or unowned dialogue configuration";return false;}
 size_t end=offset;while(end<size&&data[end]!=0xff)++end;if(end==size){error="unterminated dialogue source";return false;}
 cancel();_layout=layout;_speakerWord=speakerWord;_speaker=uint8_t(uint8_t(speakerWord)-1);_sourceBase=offset;_source.assign(data+offset,data+end+1);_active=true;_mode=uint8_t(speakerWord>>8);_textColor=_color=1;resetCanvas();_bodyY=layout.top;
 return queueSpeaker(error);
}
bool DialogueWindow::queueSpeaker(std::string& error){
 _bodyY=_layout.top;_streamCursor=0;_delay=_remaining=0;
 if(_speaker&&_speaker!=255){
  const size_t table=0x13552-0xd400;if(_speaker>=48){error="speaker identity outside original directory";return false;}int index=int(table)+int(int16_t(word(&_subcode[table+_speaker*2])));if(index<0||size_t(index)>=_subcode.size()){error="speaker name outside SUBCODE";return false;}
  uint8_t color=3;if(!(_subcode[size_t(index)]&0x80)){color=_subcode[size_t(index)]&7;++index;}
  _stream={0xfc,0,0xf9,1,0xfb,16,_layout.top,0xfe,color};unsigned count=0;
  while(size_t(index)<_subcode.size()&&_subcode[size_t(index)]!=0xff&&count<25){_stream.push_back(_subcode[size_t(index++)]);++count;}
  if(size_t(index)>=_subcode.size()||_subcode[size_t(index)]!=0xff){error="speaker name exceeds original25glyph bound";return false;}_stream.push_back(0xff);_streamActive=true;_nameStage=true;_bodyY=uint8_t(_layout.top+9);
 }else if(!queueBody(error))return false;
 return true;
}
bool DialogueWindow::queueBody(std::string& error){
 _stream={0xfc,1,0xf9,1,0xfe,_textColor,0xfb,uint8_t(_speaker?24:40),_bodyY};_streamCursor=0;_delay=_remaining=0;_lineBreak=_pause=false;unsigned count=0,limit=50;
 while(_sourceCursor<_source.size()&&count<limit){uint8_t c=_source[_sourceCursor++];if(c==0xff){_textEnd=true;break;}if(c==0xf2){_lineBreak=true;break;}if(c==0xf6){_pause=true;break;}if(c==0xf4){_mode=2;continue;}if(c==0xec||c==0xee||c==0xf0){if(!_layout.substitutionKnown){error="dialogue controller substitution owner unresolved";return false;}static const uint8_t maps[3][6]={{'A','A','B','C','C','B'},{'B','C','A','A','B','C'},{'C','B','C','B','A','A'}};c=maps[c==0xec?0:c==0xee?1:2][_layout.substitution];}else if(c>=0xf8){_textColor=c&7;_stream.push_back(0xfe);_stream.push_back(_textColor);continue;}_stream.push_back(c);++count;}
 _stream.push_back(0xff);_streamActive=true;return true;
}
bool DialogueWindow::raster(uint8_t code,std::string& error){
 FontGlyph g;if(!_font.glyph(code,g,error)||!rasterOriginalFontGlyph(g,code,_x,_y,_color,_layout.fixedWidth,_patterns,error))return false;
 _dirtyRows|=uint8_t((1u<<(_y/8))|(1u<<((_y+7)/8)));
 _x=uint8_t(_x+(_layout.fixedWidth?9:g.advance));++_revealed;return true;
}
bool DialogueWindow::consume(std::string& error){
 FontProgramState state;state.cursor=_streamCursor;state.glyphs=_revealed;state.delay=_delay;state.remaining=_remaining;state.color=_color;state.x=_x;state.y=_y;state.fixedWidth=_layout.fixedWidth;state.active=_streamActive;state.modeChanged=_fontModePending;
 if(!_font.stepProgram(_stream,state,_patterns,_dirtyRows,error))return false;
 _streamCursor=state.cursor;_revealed=state.glyphs;_delay=state.delay;_remaining=state.remaining;_color=state.color;_x=state.x;_y=state.y;_layout.fixedWidth=state.fixedWidth;_streamActive=state.active;_fontModePending=state.modeChanged;return true;
}
void DialogueWindow::promptNames(unsigned offset){_promptNamesDirty=true;const size_t table=0x138c4-0xd400;const size_t records=0x1387c-0xd400+size_t(_layout.variant)*24;for(unsigned row=0;row<2;++row){unsigned destination=word(&_subcode[records+6+row*10]);size_t at=destination-0xec80;if(at+4<=_names.size())for(unsigned i=0;i<4;++i)_names[at+i]=_subcode[table+offset*4+row*4+i];}_promptVisible=true;}
bool DialogueWindow::advancePrompt(bool& automatic,std::string& error){
 automatic=false;const size_t timeline=0x1391c-0xd400;
 if(--_promptRemaining||!_speaker)return true;
 if(_promptOffset>16){error="prompt sequence offset outside original timeline";return false;}
 unsigned prior=_promptOffset;promptNames(prior);_promptRemaining=_subcode[timeline+prior+1];_promptOffset=uint8_t(prior+2);
 if(!_subcode[timeline+_promptOffset+1]){if(_mode){automatic=prior!=0;}else _promptOffset=0;}return true;
}
void DialogueWindow::continuePage(){_waiting=false;_views.push_back(kDialogueCloseView);_transition=_textEnd?1:2;}
bool DialogueWindow::tickPrompt(std::string& error){error.clear();if(!_active||_finished||_menu||!_waiting)return true;DialogueWindow next=*this;bool automatic=false;if(!next.advancePrompt(automatic,error))return false;if(automatic)next.continuePage();*this=next;return true;}
bool DialogueWindow::tick(bool paused,std::string& error,bool fastReveal){
 error.clear();if(paused||!_active||_finished)return true;DialogueWindow staged=*this;
 // Confirm during reveal removes the future glyph delay. The current delay
 // countdown, page boundaries and actual acknowledgement remain independent.
 if(fastReveal&&!staged._menu)staged._delay=0;
 if(staged._transition==1){staged._finished=true;staged._active=false;staged._transition=0;}
 else if(staged._transition==2){staged.resetCanvas();staged._bodyY=staged._layout.top;staged._transition=3;}
 else if(staged._transition==3){staged._views.push_back(kDialogueReopenView);staged._transition=4;}
 else if(staged._transition==4){if(!staged.queueSpeaker(error))return false;staged._transition=0;}
 else if(staged._waiting){if(!staged.tickPrompt(error))return false;}
 else if(staged._streamActive){if(!staged.consume(error))return false;}
 else if(staged._nameStage){staged._nameStage=false;if(!staged.queueBody(error))return false;}
 else if(staged._textEnd||staged._pause||staged._bodyY+9>=0x39){staged._waiting=true;staged._promptOffset=0;staged._promptRemaining=1;}
 else{staged._bodyY=uint8_t(staged._bodyY+9);if(!staged.queueBody(error))return false;}
 *this=staged;return true;
}
bool DialogueWindow::acknowledge(std::string& error){error.clear();if(_menu||!_active||!_waiting||(_taskOwned&&_taskPhase!=kTaskInput)){error="dialogue is not awaiting input";return false;}if(_taskOwned)_ackPending=true;else continuePage();return true;}
bool DialogueWindow::transfers(std::vector<GraphicsTransfer>& out,std::string& error)const{out.clear();error.clear();if(!_loaded||_patterns.size()!=8192||_names.size()!=512){error="dialogue allocations not established";return false;}out.push_back(packet(0,_patterns));out.push_back(packet(0xec80,_names));out.push_back(packet(0xbce0,_promptPatterns));if(_menu)out.push_back(packet(0xde00,_cursorPatterns));return true;}
bool DialogueWindow::beginMenu(const uint8_t *data,size_t size,const std::vector<uint16_t>& identities,unsigned selected,const DialogueLayout& layout,std::string& error){
 error.clear();unsigned capacity=layout.variant?6:7;if(!_loaded||!layout.established||!data||size>65536||identities.empty()||identities.size()>8||selected>=identities.size()){error="menu paging/context owner unresolved or invalid menu operands";return false;}
 DialogueWindow next=*this;next.cancel();next._layout=layout;next._active=true;next._menu=true;next._menuSelection=selected;next._menuCount=uint8_t(identities.size());next.resetCanvas();next._delay=next._remaining=0;next._color=4;
 // Source10D42/10EAE copies original identities into 33byte bounded labels;
 // source10F90 recolors old selection4 and selected1 without invented strings.
 bool paged=identities.size()>capacity;unsigned firstPage=capacity-1;bool second=paged&&selected>=firstPage;
 if(paged){next._x=124;next._y=second?0:uint8_t(firstPage*9);for(unsigned i=0;i<3;++i){next._x=124;const uint8_t colors[3]={1,4,5};next._color=colors[i];if(!next.raster(uint8_t((second?0x80:0x83)+i),error))return false;}}
 for(size_t row=0;row<identities.size();++row){if(paged&&(second?row<firstPage:row>=firstPage))continue;size_t at=0x3800+size_t(identities[row]);if(at>=size){error="menu text identity outside scenario";return false;}next._x=16;next._y=uint8_t((second?row-firstPage+1:row)*9);next._color=row==selected?1:4;bool end=false;
  for(unsigned count=0;count<33&&at<size;++count){uint8_t c=data[at++];if(c==0xff){end=true;break;}if(c<0x20||c>0x85){error="menu label requires unsupported internal text control";return false;}if(!next.raster(c,error))return false;}if(!end){error="menu label exceeds established33byte copy owner";return false;}
 }
 next._waiting=true;*this=next;return true;
}
bool DialogueWindow::menuPieces(std::vector<PlacedPiece>& pieces,std::string& error)const{pieces.clear();error.clear();if(!_menu||!_active){error="menu cursor owner not active";return false;}FramePlacement placement;unsigned capacity=_layout.variant?6:7;unsigned row=_menuSelection;if(_menuCount>capacity&&row>=capacity-1)row=row-(capacity-1)+1;placement.x=16;placement.y=143+int(row)*9+(_layout.variant?16:0);return placeFrame(_cursorGeometry,placement,64,pieces,error);}

bool DialogueWindow::beginMenuTask(const uint8_t *data,size_t size,const std::vector<uint16_t>& identities,unsigned selected,const DialogueLayout& layout,bool opening,std::string& error){
 DialogueWindow next=*this;if(!next.beginMenu(data,size,identities,selected,layout,error))return false;
 const unsigned capacity=layout.variant?6:7,first=capacity-1;const bool paged=identities.size()>capacity,second=paged&&selected>=first;
 const bool reuse=!opening&&_menu&&_active&&_waiting&&_menuCount==identities.size()&&_layout.variant==layout.variant&&(!paged||(_menuSelection>=first)==second);
 next._fontModeBound=true;next._menuTask=true;next._menuOpening=opening;next._menuPhase=opening?0:reuse?3:2;next._waiting=false;next._revealed=0;next._stream.clear();next._streamActive=false;
 if(reuse){next._patterns=_patterns;next._names=_names;next._canvasFresh=_canvasFresh;next._dirtyRows=0;next._namesDirty=next._clearCanvasPending=false;}
 else{next._canvasFresh=true;next._dirtyRows=0;next._namesDirty=next._clearCanvasPending=false;}
 auto label=[&](unsigned identity,uint8_t color){unsigned row=second?identity-first+1:identity;std::vector<uint8_t> program={0xfc,0,0xf9,1,0xfe,color,0xfb,16,uint8_t(row*9)};size_t at=0x3800+identities[identity];while(data[at]!=0xff)program.push_back(data[at++]);program.push_back(0xff);next._menuPrograms.insert(next._menuPrograms.end(),program.begin(),program.end());};
 auto arrow=[&](){std::vector<uint8_t> program={0xfc,0,0xf9,1};for(unsigned i=0;i<3;++i){const uint8_t colors[]={1,4,5};program.insert(program.end(),{0xfe,colors[i],0xfb,124,uint8_t(second?0:first*9),uint8_t((second?0x80:0x83)+i)});}program.push_back(0xff);next._menuPrograms.insert(next._menuPrograms.end(),program.begin(),program.end());};
 if(!reuse){for(unsigned row=0;row<capacity;++row){if(paged&&row==(second?0:first)){arrow();continue;}unsigned identity=second?row+first-1:row;if(identity<identities.size())label(identity,4);}}
 label(reuse?_menuSelection:selected,4);next._menuSelectionProgram=next._menuPrograms.size();label(selected,1);
 *this=next;return true;
}
bool DialogueWindow::coordinateMenu(std::string& error){
 error.clear();if(!_menuTask||!_active||_waiting||_streamActive)return true;DialogueWindow next=*this;
 switch(next._menuPhase){
 case 0:next._captionResetPending=true;next._menuPhase=1;break;
 case 1:next._policyPending=true;next._menuPhase=2;break;
 case 2:next.resetCanvas();next._menuPhase=3;break;
 case 3:if(next._menuProgramCursor<next._menuPrograms.size()){
   if(!next._menuOpening&&next._menuProgramCursor==next._menuSelectionProgram)next._menuCursorPending=true;
   size_t end=next._menuProgramCursor;while(end<next._menuPrograms.size()&&next._menuPrograms[end]!=0xff)++end;if(end==next._menuPrograms.size()){error="unterminated menu font task";return false;}
   next._stream.assign(next._menuPrograms.begin()+next._menuProgramCursor,next._menuPrograms.begin()+end+1);next._menuProgramCursor=end+1;next._streamCursor=0;next._delay=next._remaining=0;next._streamActive=true;
  }else next._menuPhase=4;break;
 case 4:if(next._menuOpening){next._views.push_back(kDialogueMenuView);next._menuCursorPending=true;}next._menuPhase=5;break;
 case 5:next._waiting=true;break;
 default:error="invalid menu font task phase";return false;
 }
 *this=next;return true;
}

}
