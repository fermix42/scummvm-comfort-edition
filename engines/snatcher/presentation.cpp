/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "presentation.h"
#include <algorithm>
#include <limits>
#include <sstream>
namespace Snatcher {
namespace {
uint16_t word(const uint8_t *p){return uint16_t(uint16_t(p[0])<<8|p[1]);}
int16_t signedWord(const uint8_t *p){unsigned w=word(p);return int16_t(w<0x8000?int(w):int(w)-65536);}
uint32_t longWord(const uint8_t *p){return uint32_t(word(p))<<16|word(p+2);}
int32_t addFixed(int32_t a,int32_t b){uint32_t v=uint32_t(a)+uint32_t(b);return v<=0x7fffffff?int32_t(v):int32_t(int64_t(v)-0x100000000LL);}
}
bool ScrollState::queuePosition(unsigned plane,ScrollAxis axis,int16_t integer,std::string& error){
 error.clear();if(plane>=2||(axis!=kScrollX&&axis!=kScrollY)){error="invalid scroll position identity";return false;}
 int32_t value=int32_t(integer)*65536;if(axis==kScrollX)_state.pending[plane].x16=value;else _state.pending[plane].y16=value;return true;
}
bool ScrollState::setVelocity(unsigned plane,ScrollAxis axis,int16_t rate,std::string& error){
 error.clear();if(plane>=2||(axis!=kScrollX&&axis!=kScrollY)){error="invalid scroll velocity identity";return false;}
 int32_t value=int32_t(rate)*256;if(axis==kScrollX)_state.velocities[plane].x16=value;else _state.velocities[plane].y16=value;return true;
}
bool ScrollState::tick(std::string& error){
 error.clear();if(_state.nextMode!=kScrollRelative&&_state.nextMode!=kScrollAbsolute){error="unsupported scroll update mode";return false;}if(_state.paused)return true;
 for(unsigned i=0;i<2;++i){int32_t x=_state.pending[i].x16,y=_state.pending[i].y16;
  if(_state.nextMode==kScrollRelative){x=addFixed(x,_state.positions[i].x16);y=addFixed(y,_state.positions[i].y16);}
  _state.positions[i].x16=addFixed(x,_state.velocities[i].x16);_state.positions[i].y16=addFixed(y,_state.velocities[i].y16);_state.pending[i]=ScrollPoint();
 }_state.nextMode=kScrollRelative;return true;
}
bool ActorController::initialize(unsigned actor,uint8_t flags,std::string& error){
 error.clear();if(actor>=64){error="actor identity outside 64 slots";return false;}
 if(_actors[actor].reserved){error="actor slot is reserved by its original high-byte owner";return false;}
 _actors[actor]=ActorControlState();_actors[actor].active=true;_actors[actor].flags=flags;return true;
}
bool ActorController::link(unsigned parent,unsigned child,std::string& error){
 error.clear();if(parent>=64||child>=64||child==0||parent==child){error="invalid actor link identity/sentinel";return false;}
 // Original uses zero as the linked-list sentinel. Reject malformed cycles,
 // retaining all previous links rather than partially installing them.
 unsigned at=_actors[parent].head;bool seen[64]={false};
 while(at){if(at>=64||seen[at]||at==child){error="actor link cycle/duplicate";return false;}seen[at]=true;at=_actors[at].next;}
 at=parent;for(unsigned steps=0;at&&steps<64;++steps){if(at==child){error="actor parent cycle";return false;}at=_actors[at].parent;}
 _actors[child].next=_actors[parent].head;_actors[parent].head=child;_actors[child].parent=parent;return true;
}
bool ActorController::setFlags(unsigned actor,uint8_t flags,std::string& error){error.clear();if(actor>=64){error="invalid actor flags identity";return false;}_actors[actor].flags=flags;return true;}
bool ActorController::reserve(unsigned actor,bool value,std::string& error){error.clear();if(actor>=64){error="invalid reserved actor identity";return false;}_actors[actor].reserved=value;return true;}
bool ActorController::retire(unsigned actor,std::string& error){error.clear();if(actor>=64){error="invalid actor retirement identity";return false;}_actors[actor]=ActorControlState();return true;}
bool ActorController::markFlags(unsigned actor,uint8_t mask,std::string& error){error.clear();if(actor>=64){error="invalid actor state-flag identity";return false;}_actors[actor].completionFlags|=mask;return true;}
bool ActorController::markCompletion(unsigned actor,std::string& error){error.clear();if(actor>=64){error="invalid actor completion identity";return false;}_actors[actor].completionFlags|=2;return true;}
ActorActionResult ActorController::applyAction(unsigned actor,unsigned subtype,std::string& error){
 error.clear();if(actor>=64||subtype>5){error="unsupported actor identity/action subtype";return kActorActionError;}
 ActorControlState& owner=_actors[actor];
 if(subtype==3){if(!owner.active||(owner.completionFlags&2)){owner.completionFlags&=uint8_t(~2);return kActorActionApplied;}return kActorActionWaiting;}
 std::vector<unsigned> targets(1,actor);
 if(subtype!=4){bool seen[64]={false};seen[actor]=true;for(size_t i=0;i<targets.size();++i){unsigned at=_actors[targets[i]].head;while(at){if(at>=64||seen[at]){error="actor action linked-list cycle/bounds";return kActorActionError;}seen[at]=true;targets.push_back(at);at=_actors[at].next;}}}
 for(size_t i=0;i<targets.size();++i){ActorControlState& a=_actors[targets[i]];
  if(subtype==0)a.flags&=2;
  else if(subtype==1)a.flags|=1;
  else if(subtype==2){a.flags=3;a.completionFlags=0;}
  else if(subtype==4)a.completionFlags=0xff;
  else {a.completionFlags=0;a.flags=0;a.recordIndex=a.remaining=0xffff;}
 }return kActorActionApplied;
}
bool readGameTextPalette(const uint8_t *subcode,size_t size,std::vector<uint16_t>& colors,std::string& error){colors.clear();error.clear();const size_t first=0xf602-0xd400,last=first+7*8;if(!subcode||size<last+2){error="original text palette words exceed SUBCODE extent";return false;}std::vector<uint16_t> out;for(unsigned i=0;i<8;++i){uint16_t color=word(subcode+first+i*8);if(color&0xf111){error="original text palette has unestablished normal-color bits";return false;}out.push_back(color);}colors.swap(out);return true;}
PaletteController::PaletteController(){clear();}
void PaletteController::clear(){_colors.assign(128,0);_slots.assign(12,Slot());_paused=false;}
void PaletteController::cancelEffects(){_slots.assign(12,Slot());}
size_t PaletteController::activeEffects()const{size_t count=0;for(size_t i=0;i<_slots.size();++i)if(_slots[i].effect.type)++count;return count;}
bool readPaletteEffect(const uint8_t* data,size_t size,size_t begin,size_t end,PaletteEffect& out,std::string& error,uint32_t resourceBase){
 out=PaletteEffect();error.clear();if(!data||begin>end||end>size||end-begin<4){error="truncated palette-effect header";return false;}
 PaletteEffect e;e.type=data[begin]&0x7f;e.replaceCycling=(data[begin]&0x80)!=0;e.delay=data[begin+1];e.byteOffset=data[begin+2];e.countMinusOne=data[begin+3];
 if(!e.type||e.type>10||(e.byteOffset&1)||size_t(e.byteOffset)/2+e.countMinusOne+1>128){error="unsupported palette effect/range";return false;}
 bool frames=e.type==1||e.type==2||e.type==4||e.type==6||e.type==7||e.type==8||e.type==9;
 if(frames){size_t at=begin+4;bool sequence=e.type==1||e.type==6;
  for(unsigned budget=0;budget<64;++budget){if(at>end||end-at<4){error="missing palette source pointer";return false;}uint32_t address=longWord(data+at);if(address<resourceBase||uint64_t(address)-resourceBase>size){error="palette source outside resource";return false;}size_t offset=address-resourceBase,n=size_t(e.countMinusOne)+1;if(n>(size-offset)/2){error="palette source colors truncated";return false;}std::vector<uint16_t> colors;for(size_t i=0;i<n;++i)colors.push_back(word(data+offset+i*2));e.frames.push_back(colors);at+=4;
   if(!sequence)break;if(at<=end&&end-at>=2&&word(data+at)==0xffff){out=e;return true;}
  }
  if(sequence){error="unterminated palette source sequence";return false;}
 }out=e;return true;
}
bool PaletteController::setColors(const std::vector<uint16_t>& colors,std::string& error){error.clear();if(colors.size()!=128){error="inherited palette extent must contain128colors";return false;}for(uint16_t color:colors)if(color&0xf111){error="inherited palette contains unestablished normal-color bits";return false;}_colors=colors;return true;}
bool PaletteController::enqueue(const PaletteEffect& e,std::string& error){
 error.clear();if(!e.type||e.type>10||(e.byteOffset&1)||size_t(e.byteOffset)/2+e.countMinusOne+1>128){error="invalid palette effect/range";return false;}
 bool needs=e.type==1||e.type==2||e.type==4||e.type==6||e.type==7||e.type==8||e.type==9;
 if(needs&&e.frames.empty()){error="palette effect missing source frames";return false;}
 for(size_t i=0;i<e.frames.size();++i)if(e.frames[i].size()!=size_t(e.countMinusOne)+1){error="palette source frame length mismatch";return false;}
 size_t slot=0;while(slot<12&&_slots[slot].effect.type)++slot;if(slot==12){error="palette effect queue full";return false;}
 if(e.replaceCycling)for(size_t i=0;i<12;++i)if(_slots[i].effect.type==6)_slots[i]=Slot();
 _slots[slot]=Slot();_slots[slot].effect=e;return true;
}
bool PaletteController::tick(std::string& error,uint8_t* publicationMask,std::vector<bool>* knownColors){
 if(knownColors&&knownColors->size()!=128){error="palette known-color owner extent";return false;}
 PaletteController before=*this;std::vector<bool> known=knownColors?*knownColors:std::vector<bool>(128,true);
 if(publicationMask)*publicationMask=0;error.clear();if(_paused)return true;
 for(size_t k=0;k<12;++k){Slot& s=_slots[k];unsigned type=s.effect.type;if(!type)continue;
  if(type==2||type==4){for(size_t i=0;i<=s.effect.countMinusOne;++i){_colors[s.effect.byteOffset/2+i]=type==2?0:0x0eee;known[s.effect.byteOffset/2+i]=true;}s.effect.type=9;type=9;}
  if(s.wait){--s.wait;continue;}s.wait=s.effect.delay;if(publicationMask)*publicationMask|=uint8_t(s.effect.byteOffset&128?2:1);
  size_t start=s.effect.byteOffset/2,n=size_t(s.effect.countMinusOne)+1;
  if(type==1||type==6){for(size_t i=0;i<n;++i)_colors[start+i]=s.effect.frames[s.frame][i];++s.frame;if(s.frame==s.effect.frames.size()){if(type==1)s.effect.type=0;else s.frame=0;}}
  else if(type==10){for(size_t i=0;i<n;++i)_colors[start+i]=0;s.effect.type=0;}
  else {for(size_t i=0;i<n;++i)if(!known[start+i]){*this=before;if(publicationMask)*publicationMask=0;error="palette effect touches unresolved inherited color";return false;}for(size_t i=0;i<n;++i){uint16_t current=_colors[start+i],result=0,target=0;
    if(type==7)target=s.effect.frames[0][0];else if(type==8){unsigned c=s.effect.frames[0][i];unsigned v=((c&15)+((c>>4)&15)+((c>>8)&15))/3;v&=14;target=uint16_t(v|v<<4|v<<8);}else if(type==9)target=s.effect.frames[0][i];
    for(unsigned shift=0;shift<=8;shift+=4){unsigned c=(current>>shift)&15,t=(target>>shift)&15;
     if(type==3){if(c)c=(c-2)&15;}else if(type==5){if(c!=14)c=(c+2)&15;}else {if(c<t)c=(c+2)&15;else if(c>t)c=(c-2)&15;}
     result|=uint16_t(c<<shift);
    }_colors[start+i]=result;
   }if(++s.updates==8)s.effect.type=0;
  }
  for(size_t i=0;i<n;++i)known[start+i]=true;
 }if(knownColors)knownColors->swap(known);return true;
}
bool readActorInitializers(const uint8_t *data,size_t size,size_t begin,size_t end,std::vector<ActorInitializer>& out,std::string& error,uint32_t resourceBase){
 out.clear();error.clear();if(!data||begin>=end||end>size||(end-begin)%12){error="invalid actor initializer extent";return false;}
 std::vector<ActorInitializer> parsed;
 for(size_t off=begin;off<end;off+=12){uint32_t frame=longWord(data+off+6);int64_t sequence=int64_t(off)+12+signedWord(data+off+10);
  if(data[off]>=64||frame<resourceBase||frame-resourceBase>=size||sequence<0||uint64_t(sequence)>size||size-size_t(sequence)<4){error="actor identity/pointer outside resource";return false;}
  ActorInitializer a={data[off],data[off+1],signedWord(data+off+2),signedWord(data+off+4),size_t(frame-resourceBase),size_t(sequence)};parsed.push_back(a);
 }out.swap(parsed);return true;
}
void PatternBank::clear(){std::fill(_patterns.begin(),_patterns.end(),0);std::fill(_present.begin(),_present.end(),false);}
void PatternBank::invalidate(unsigned first,unsigned count){if(first>2048||count>2048-first)return;for(unsigned i=first;i<first+count;++i)_present[i]=false;}
bool PatternBank::assign(unsigned first,const std::vector<uint8_t>& packed,std::string& error){
 error.clear();if(first>=2048||packed.empty()||packed.size()%32||packed.size()/32>2048-first){error="invalid pattern allocation";return false;}
 std::copy(packed.begin(),packed.end(),_patterns.begin()+size_t(first)*32);for(size_t i=0;i<packed.size()/32;++i)_present[first+i]=true;return true;
}
bool PatternBank::sample(uint16_t attributes,unsigned x,unsigned y,uint8_t& color,std::string& error)const{error.clear();color=0;unsigned pattern=attributes&0x7ff;if(x>=8||y>=8||!_present[pattern]){error="scene tile references unavailable pattern/pixel";return false;}if(attributes&0x0800)x=7-x;if(attributes&0x1000)y=7-y;uint8_t packed=_patterns[size_t(pattern)*32+y*4+x/2],nibble=x%2?packed&15:packed>>4;if(nibble)color=uint8_t(((attributes>>13)&3)*16+nibble);return true;}
bool PatternBank::draw(const PlacedPiece& p,std::vector<uint8_t>& canvas,size_t width,size_t height,std::string& error,const std::vector<uint8_t>* admittedRowWidths)const{
 error.clear();if(!width||!height||width>4096||height>4096||canvas.size()!=width*height||!p.width||!p.height||p.width>32||p.height>32||p.width%8||p.height%8){error="invalid pattern draw extent";return false;}
 unsigned first=p.attributes&0x7ff,count=(p.width/8)*(p.height/8);if(count>2048-first){error="piece patterns exceed cache";return false;}
 if(admittedRowWidths){if(admittedRowWidths->size()!=height){error="sprite row admission extent mismatch";return false;}for(uint8_t n:*admittedRowWidths)if(n>p.width){error="sprite row admission exceeds piece";return false;}}
 else for(unsigned i=0;i<count;++i)if(!_present[first+i]){error="piece references unavailable pattern";return false;}
 for(unsigned y=0;y<p.height;++y)for(unsigned x=0;x<p.width;++x){
  int64_t px=int64_t(p.x)+x,py=int64_t(p.y)+y;
  if(admittedRowWidths&&(px<0||py<0||uint64_t(px)>=width||uint64_t(py)>=height||x>=(*admittedRowWidths)[size_t(py)]))continue;
  unsigned sx=p.attributes&0x0800?p.width-1-x:x,sy=p.attributes&0x1000?p.height-1-y:y;
  unsigned pattern=first+(sx/8)*(p.height/8)+sy/8;if(!_present[pattern]){error="piece references unavailable pattern";return false;}uint8_t packed=_patterns[size_t(pattern)*32+(sy%8)*4+(sx%8)/2];uint8_t color=sx%2?packed&15:packed>>4;
  if(color&&px>=0&&py>=0&&uint64_t(px)<width&&uint64_t(py)<height)canvas[size_t(py)*width+size_t(px)]=uint8_t(((p.attributes>>13)&3)*16+color);
 }return true;
}
bool PatternBank::drawPlane(const std::vector<uint16_t>& names,size_t columns,size_t rows,int sourceX,int sourceY,std::vector<uint8_t>& canvas,size_t width,size_t height,std::string& error)const{
 error.clear();if(!columns||!rows||columns>128||rows>128||names.size()!=columns*rows||!width||!height||width>4096||height>4096||canvas.size()!=width*height){error="invalid native tile-plane extent";return false;}
 std::vector<uint8_t> result=canvas;int64_t planeWidth=int64_t(columns)*8,planeHeight=int64_t(rows)*8;
 for(size_t y=0;y<height;++y)for(size_t x=0;x<width;++x){
  int64_t sx=(int64_t(x)+sourceX)%planeWidth,sy=(int64_t(y)+sourceY)%planeHeight;if(sx<0)sx+=planeWidth;if(sy<0)sy+=planeHeight;
  uint16_t attr=names[size_t(sy/8)*columns+size_t(sx/8)];unsigned pattern=attr&0x7ff;
  if(!_present[pattern]){error="tile plane references unavailable pattern";return false;}
  unsigned px=unsigned(sx)%8,py=unsigned(sy)%8;if(attr&0x0800)px=7-px;if(attr&0x1000)py=7-py;
  uint8_t packed=_patterns[size_t(pattern)*32+py*4+px/2],color=px%2?packed&15:packed>>4;
  if(color)result[y*width+x]=uint8_t(((attr>>13)&3)*16+color);
 }canvas.swap(result);return true;
}
void initializeOriginalFontCanvas(std::vector<uint8_t>& patterns,std::vector<uint8_t>& names){
 patterns.assign(8192,0);std::fill(patterns.end()-32,patterns.end(),uint8_t(0x77));names.clear();names.reserve(512);for(unsigned i=0;i<256;++i){uint16_t name=uint16_t(0x8000|(i==255?0:i));names.push_back(uint8_t(name>>8));names.push_back(uint8_t(name));}
}
bool rasterOriginalFontGlyph(const FontGlyph& glyph,uint8_t code,uint8_t x,uint8_t y,uint8_t color,bool fixedWidth,std::vector<uint8_t>& patterns,std::string& error){
 error.clear();if(patterns.size()!=8192||y>56||color>15){error="font glyph exceeds its owned canvas or color range";return false;}
 if(fixedWidth&&(code=='1'||code=='I'))x=uint8_t(x+1);
 for(unsigned row=0;row<8;++row)for(unsigned column=0;column<8;++column)if(glyph.rows[row]&(0x80u>>column)){unsigned px=uint8_t(x+column),py=y+row;size_t at=((py/8)*32+px/8)*32+(py&7)*4+(px&7)/2;uint8_t& value=patterns[at];if(px&1)value=uint8_t((value&0xf0)|color);else value=uint8_t((value&15)|(color<<4));}
 return true;
}
bool OriginalFont::load(const uint8_t *subcode,size_t size,std::string& error){
 _rows.clear();_advances.clear();_loaded=false;error.clear();
 const size_t bitmap=0x777c,widths=0x7aac,count=102;
 if(!subcode||size<widths+count){error="truncated original font tables";return false;}
 for(size_t i=0;i<count;++i)if(!subcode[widths+i]||subcode[widths+i]>9){error="invalid original glyph advance";return false;}
 _rows.assign(subcode+bitmap,subcode+bitmap+count*8);_advances.assign(subcode+widths,subcode+widths+count);_loaded=true;return true;
}
bool OriginalFont::glyph(uint8_t code,FontGlyph& out,std::string& error)const{
 error.clear();if(!_loaded||code<0x20||unsigned(code-0x20)>=_advances.size()){error="original glyph identity outside font";return false;}
 size_t index=code-0x20;for(size_t row=0;row<8;++row)out.rows[row]=_rows[index*8+row];out.advance=_advances[index];return true;
}
bool OriginalFont::stepProgram(const std::vector<uint8_t>& program,FontProgramState& s,std::vector<uint8_t>& patterns,uint8_t& dirtyRows,std::string& error)const{
 error.clear();if(!s.active)return true;if(s.remaining){--s.remaining;return true;}
 for(unsigned guard=0;guard<1024;++guard){
  if(s.cursor>=program.size()){error="font program exhausted before its terminator";return false;}uint8_t code=program[s.cursor++];
  if(code==0xff){s.remaining=s.delay;s.active=false;return true;}
  if(code==0xfc){if(s.cursor>=program.size())break;s.delay=s.remaining=program[s.cursor++];if(s.remaining){--s.remaining;return true;}continue;}
  if(code==0xf9||code==0xfe||code==0xf7){if(s.cursor>=program.size())break;uint8_t value=program[s.cursor++];if(code==0xfe){if(value>15){error="font program color exceeds one nibble";return false;}s.color=value;}else if(code==0xf7){s.fixedWidth=value!=0;s.modeChanged=true;}continue;}
  if(code==0xfb){if(s.cursor+2>program.size())break;s.x=program[s.cursor++];s.y=program[s.cursor++];continue;}
  if(code==0xfd){s.y=uint8_t(s.y+9);s.x=24;continue;}
  FontGlyph g;if(!glyph(code,g,error)||!rasterOriginalFontGlyph(g,code,s.x,s.y,s.color,s.fixedWidth,patterns,error))return false;
  dirtyRows|=uint8_t((1u<<(s.y/8))|(1u<<((s.y+7)/8)));s.x=uint8_t(s.x+(s.fixedWidth?9:g.advance));++s.glyphs;s.remaining=s.delay;if(s.remaining){--s.remaining;return true;}
 }
 error="invalid or oversized original font program";return false;
}
bool drawGlyph(const OriginalFont& font,uint8_t code,int x,int y,uint8_t color,std::vector<uint8_t>& canvas,size_t width,size_t height,std::string& error){
 error.clear();if(!width||!height||width>4096||height>4096||canvas.size()!=width*height||color>15){error="invalid indexed glyph canvas";return false;}
 FontGlyph g;if(!font.glyph(code,g,error))return false;
 for(unsigned row=0;row<8;++row)for(unsigned column=0;column<8;++column){
  int64_t px=int64_t(x)+column,py=int64_t(y)+row;
  if(px>=0&&py>=0&&uint64_t(px)<width&&uint64_t(py)<height&&(g.rows[row]&(0x80u>>column)))canvas[size_t(py)*width+size_t(px)]=color;
 }return true;
}
bool placeFrame(const std::vector<FramePiece>& frame,const FramePlacement& p,size_t cap,std::vector<PlacedPiece>& out,std::string& error){
 out.clear();error.clear();if(frame.size()>cap||p.palette>3){error="invalid frame placement bounds";return false;}
 std::vector<PlacedPiece> placed;
 for(size_t i=0;i<frame.size();++i){const FramePiece& f=frame[i];if(!f.width||!f.height||f.width>32||f.height>32||f.width%8||f.height%8){error="invalid piece dimensions";return false;}
  int64_t x=int64_t(p.x)+(p.flipX?-int64_t(f.x)-f.width:f.x),y=int64_t(p.y)+(p.flipY?-int64_t(f.y)-f.height:f.y);
  if(x<std::numeric_limits<int>::min()||x>std::numeric_limits<int>::max()||y<std::numeric_limits<int>::min()||y>std::numeric_limits<int>::max()){error="placed frame coordinate overflow";return false;}
  uint16_t attr=uint16_t(f.attributeWord|p.attributeOrMask);if(p.flipX)attr^=0x0800;if(p.flipY)attr^=0x1000;
  if(p.overridePalette)attr=uint16_t((attr&0x9fff)|(p.palette<<13));
  PlacedPiece q={int(x),int(y),f.width,f.height,attr};placed.push_back(q);
 }out.swap(placed);return true;
}
bool TextReader::start(const uint8_t *data,size_t size,size_t start,std::string& error){
 _text.clear();_base=start;_cursor=0;_ended=false;error.clear();
 if(!data||size>65536||start>=size){error="text record outside scenario";return false;}
 size_t end=start;while(end<size&&data[end]!=255)++end;
 if(end==size){error="unterminated text record";return false;}_text.assign(data+start,data+end+1);return true;
}
bool TextReader::read(size_t budget,std::vector<TextToken>& out,std::string& error){
 out.clear();error.clear();if(_text.empty()||budget==0||budget>255){error="invalid text reader or glyph budget";return false;}if(_ended)return true;
 std::vector<TextToken> staged;size_t cursor=_cursor,glyphs=0;
 while(cursor<_text.size()){
  size_t offset=_base+cursor;uint8_t c=_text[cursor++];TextToken t={kTextGlyph,c,offset};
  if(c==0xec||c==0xee||c==0xf0){std::ostringstream s;s<<"unsupported context-dependent text substitution at resource offset "<<offset;error=s.str();return false;}
  if(c==0xff){t.kind=kTextEnd;staged.push_back(t);_cursor=cursor;_ended=true;out.swap(staged);return true;}
  if(c==0xf2||c==0xf6){t.kind=c==0xf2?kTextLineBreak:kTextPause;t.value=c==0xf2?1:3;staged.push_back(t);_cursor=cursor;out.swap(staged);return true;}
  if(c==0xf4){t.kind=kTextMode;t.value=2;}
  else if(c>=0xf8){t.kind=kTextStyle;t.value=c&7;}
  else ++glyphs;
  staged.push_back(t);if(glyphs==budget){_cursor=cursor;out.swap(staged);return true;}
 }
 error="text record lost terminator";return false;
}
}
