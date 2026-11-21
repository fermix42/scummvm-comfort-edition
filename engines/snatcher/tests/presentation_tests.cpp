/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../presentation.h"
#include "../animation.h"
#include <cstdio>
#include <limits>
namespace {
int checks=0,failures=0;
void check(bool v,const char *s){++checks;if(!v){++failures;std::fprintf(stderr,"FAIL presentation: %s\n",s);}}
}
int runPresentationTests(){
 checks=failures=0;
 {
  Snatcher::FontGlyph glyph={};glyph.rows[0]=0xc0;std::vector<uint8_t> canvas(8192,0xcc);std::string error;
  check(Snatcher::rasterOriginalFontGlyph(glyph,'A',0,0,1,false,canvas,error)&&canvas[0]==0x11&&canvas[1]==0xcc,"font replaces both selected nibbles while retaining unset pixels");
  check(Snatcher::rasterOriginalFontGlyph(glyph,'A',1,0,0,false,canvas,error)&&canvas[0]==0x10&&canvas[1]==0x0c,"color0 erases glyph bits without clearing neighboring pixels");
  canvas.assign(8192,0xcc);check(Snatcher::rasterOriginalFontGlyph(glyph,'1',0,0,1,true,canvas,error)&&canvas[0]==0xc1&&canvas[1]==0x1c,"fixed-width narrow glyph adjustment belongs to the shared raster owner");
  canvas.assign(8192,0xcc);check(Snatcher::rasterOriginalFontGlyph(glyph,'A',255,0,7,false,canvas,error)&&canvas[995]==0xc7&&canvas[0]==0x7c,"source byte coordinates wrap across the canvas edge");
  const auto prior=canvas;check(!Snatcher::rasterOriginalFontGlyph(glyph,'A',0,57,1,false,canvas,error)&&canvas==prior,"invalid font height rejects before any pixel mutation");
  check(!Snatcher::rasterOriginalFontGlyph(glyph,'A',0,0,16,false,canvas,error)&&canvas==prior,"font color is bounded to one nibble");
 }
 using namespace Snatcher;std::string e;FramePiece piece={-8,4,16,8,0x400,0x2345};std::vector<FramePiece> frame(1,piece);std::vector<PlacedPiece> out;FramePlacement p;p.x=100;p.y=50;
 check(placeFrame(frame,p,64,out,e)&&out[0].x==92&&out[0].y==54&&out[0].attributes==0x2345,"ordinary placement");
 p.flipX=p.flipY=true;p.attributeOrMask=0x8000;p.overridePalette=true;p.palette=2;
 check(placeFrame(frame,p,64,out,e)&&out[0].x==92&&out[0].y==38&&out[0].attributes==0xdb45,"flip dimensions and palette override");
 check(!placeFrame(frame,p,0,out,e)&&out.empty(),"piece bound");p.palette=4;check(!placeFrame(frame,p,64,out,e),"invalid palette override");p.palette=0;p.flipX=p.flipY=false;p.x=std::numeric_limits<int>::max();frame[0].x=1;check(!placeFrame(frame,p,64,out,e)&&out.empty(),"coordinate overflow");
 frame[0].width=7;check(!placeFrame(frame,p,64,out,e),"invalid piece size");
 TextReader r;std::vector<TextToken> tokens;const uint8_t text[]={0x41,0xf9,0x42,0xf4,0xf2,0x43,0xf6,0xfe,0xff};
 check(r.start(text,sizeof(text),0,e),"text start");check(r.read(1,tokens,e)&&tokens.size()==1&&tokens[0].kind==kTextGlyph&&tokens[0].value==0x41&&r.cursor()==1,"glyph budget");
 check(r.read(10,tokens,e)&&tokens.size()==4&&tokens[0].kind==kTextStyle&&tokens[0].value==1&&tokens[2].kind==kTextMode&&tokens[2].value==2&&tokens[3].kind==kTextLineBreak&&tokens[3].value==1&&r.cursor()==5,"style mode and linebreak");
 check(r.read(10,tokens,e)&&tokens.size()==2&&tokens.back().kind==kTextPause&&tokens.back().value==3,"second pause identity");
 check(r.read(10,tokens,e)&&tokens.size()==2&&tokens[0].kind==kTextStyle&&tokens[0].value==6&&tokens[1].kind==kTextEnd&&r.ended(),"end and final style");check(r.read(10,tokens,e)&&tokens.empty(),"end idempotent");
 for(unsigned c=0xec;c<=0xf0;c+=2){uint8_t dynamic[]={0x41,uint8_t(c),0xff};check(r.start(dynamic,sizeof(dynamic),0,e)&&!r.read(10,tokens,e)&&r.cursor()==0&&tokens.empty()&&!e.empty(),"substitution explicit and transactional");}
 check(!r.start(text,sizeof(text),sizeof(text),e),"invalid text start");check(!r.start(text,sizeof(text)-1,0,e),"unterminated text rejected");check(r.start(text,sizeof(text),0,e)&&!r.read(0,tokens,e)&&!r.read(256,tokens,e),"invalid budgets");
 OriginalFont font;FontGlyph glyph;std::vector<uint8_t> fontData(0x7aac+102,1);for(unsigned i=0;i<102;++i)fontData[0x7aac+i]=uint8_t(3+i%7);for(unsigned row=0;row<8;++row)fontData[0x777c+row]=uint8_t(0x80u>>row);
 check(font.load(&fontData[0],fontData.size(),e)&&font.loaded()&&font.glyph(0x20,glyph,e)&&glyph.advance==3,"bounded font tables");
 {std::vector<uint8_t> patterns,names;initializeOriginalFontCanvas(patterns,names);FontProgramState state;state.active=true;uint8_t dirty=0;const std::vector<uint8_t> program={0xfc,1,0xf7,1,0xfe,12,0xfb,0,7,0x20,0x20,0xff};
 check(font.stepProgram(program,state,patterns,dirty,e)&&state.active&&!state.glyphs&&!state.remaining&&!dirty,"font delay prefix consumes its own worker");
 check(font.stepProgram(program,state,patterns,dirty,e)&&state.glyphs==1&&state.x==9&&dirty==3&&(patterns[28]>>4)==12,"shared font program supports numeric-interface color12 and crossing tile rows");
 check(font.stepProgram(program,state,patterns,dirty,e)&&state.glyphs==2&&state.x==18,"fixed-width body advances once per eligible consumer");
 check(font.stepProgram(program,state,patterns,dirty,e)&&!state.active&&state.remaining==1,"font terminator preserves the source preloaded delay field");
 const auto before=patterns;check(font.stepProgram(program,state,patterns,dirty,e)&&patterns==before,"inactive font program does not invent a later update");
 state=FontProgramState();state.active=true;check(!font.stepProgram({0xfc,0,0xfe,16,0x20,0xff},state,patterns,dirty,e)&&patterns==before,"invalid program color cannot touch its glyph canvas");
 }
 std::vector<uint8_t> canvas(64,9);check(drawGlyph(font,0x20,0,0,2,canvas,8,8,e),"native original glyph raster");
 for(unsigned row=0;row<8;++row)for(unsigned col=0;col<8;++col)check(canvas[row*8+col]==(row==col?2:9),"MSB-left rows and transparent zero");
 check(!font.glyph(0x1f,glyph,e)&&!font.glyph(0x86,glyph,e),"glyph bounds");check(drawGlyph(font,0x20,-7,-7,3,canvas,8,8,e)&&canvas[0]==3,"clipped glyph");
 check(!drawGlyph(font,0x20,0,0,16,canvas,8,8,e),"color nibble bound");check(!font.load(&fontData[0],fontData.size()-1,e)&&!font.loaded(),"short width table rejected");
 fontData[0x7aac]=0;check(!font.load(&fontData[0],fontData.size(),e),"invalid advance rejected");
 const uint8_t actors[]={1,2,255,255,0,3,0,2,0x80,0,0,0,1,0,0,1};std::vector<ActorInitializer> actorList;
 check(readActorInitializers(actors,sizeof(actors),0,12,actorList,e)&&actorList.size()==1&&actorList[0].x==-1&&actorList[0].y==3&&actorList[0].sequenceOffset==12&&actorList[0].frameTableOffset==0,"actor resource-relative parser");
 check(!readActorInitializers(actors,sizeof(actors),0,11,actorList,e)&&actorList.empty(),"partial actor rejected");check(!readActorInitializers(actors,15,0,12,actorList,e),"actor sequence bounded");
 PatternBank bank;std::vector<uint8_t> patterns(128);for(unsigned tile=0;tile<4;++tile)for(unsigned i=0;i<32;++i)patterns[tile*32+i]=uint8_t((tile+1)*17);
 check(bank.assign(0,patterns,e),"pattern allocation");std::vector<uint8_t> pixels(256,0);PlacedPiece square={0,0,16,16,0x2000};
 check(bank.draw(square,pixels,16,16,e)&&pixels[0]==17&&pixels[8*16]==18&&pixels[8]==19&&pixels[8*16+8]==20,"column-major cells and palette groups");
 square.attributes|=0x1800;check(bank.draw(square,pixels,16,16,e)&&pixels[0]==20&&pixels[8*16+8]==17,"whole-piece flips");
 square.attributes=0;square.x=-8;square.y=-8;check(bank.draw(square,pixels,16,16,e)&&pixels[0]==4,"piece clipping");
 check(!bank.assign(2047,patterns,e)&&!bank.assign(0,std::vector<uint8_t>(31,0),e),"invalid pattern allocations");
 std::vector<uint16_t> names;names.push_back(0);names.push_back(0x4001);pixels.assign(256,9);check(bank.drawPlane(names,2,1,0,0,pixels,16,16,e)&&pixels[0]==1&&pixels[8]==34&&pixels[8*16]==1,"row-major tile plane and palette");
 check(bank.drawPlane(names,2,1,8,-8,pixels,16,16,e)&&pixels[0]==34&&pixels[8]==1,"asset window wraps in both directions");
 check(!bank.drawPlane(names,3,1,0,0,pixels,16,16,e),"tile-plane extent rejected");
 bank.clear();pixels.assign(256,9);check(!bank.draw(square,pixels,16,16,e)&&pixels[0]==9,"unavailable patterns reject transactionally");check(!bank.drawPlane(names,2,1,0,0,pixels,16,16,e)&&pixels[0]==9,"plane missing allocation transactional");
 ScrollState scroll;check(scroll.queuePosition(0,kScrollY,-64,e)&&scroll.queuePosition(1,kScrollY,-40,e),"two semantic plane requests");scroll.nextUpdate(kScrollAbsolute);check(scroll.tick(e)&&scroll.state().positions[0].y16==-64*65536&&scroll.state().positions[1].y16==-40*65536,"absolute initial scroll");
 check(scroll.setVelocity(0,kScrollX,128,e)&&scroll.tick(e)&&scroll.state().positions[0].x16==32768&&scroll.state().positions[0].y16==-64*65536,"signed8.8 velocity update");check(scroll.state().pending[1].y16==0&&scroll.state().nextMode==kScrollRelative,"absolute and pending are one-shot");
 check(scroll.queuePosition(0,kScrollX,10,e)&&scroll.tick(e)&&scroll.state().positions[0].x16==11*65536,"relative pending offset then velocity");check(scroll.tick(e)&&scroll.state().positions[0].x16==11*65536+32768,"relative offset consumed once");
 check(scroll.queuePosition(0,kScrollX,-5,e),"queue absolute");scroll.nextUpdate(kScrollAbsolute);check(scroll.tick(e)&&scroll.state().positions[0].x16==-5*65536+32768&&scroll.state().positions[1].y16==0,"absolute starts from pending vectors then velocity");
 scroll.pause(true);check(scroll.queuePosition(0,kScrollX,9,e),"paused pending");scroll.nextUpdate(kScrollAbsolute);check(scroll.tick(e)&&scroll.state().pending[0].x16==9*65536&&scroll.state().positions[0].x16==-5*65536+32768,"pause retains update ownership");scroll.pause(false);check(scroll.tick(e)&&scroll.state().positions[0].x16==9*65536+32768,"resume consumes pending once");
 check(!scroll.queuePosition(2,kScrollX,0,e)&&!scroll.setVelocity(0,ScrollAxis(9),0,e),"invalid plane and axis");scroll.nextUpdate(ScrollUpdateMode(99));check(!scroll.tick(e)&&!e.empty(),"unknown scroll mode explicit");scroll.clear();check(scroll.state().positions[0].x16==0&&scroll.state().velocities[0].x16==0,"scroll reset");
 check(scroll.queuePosition(0,kScrollX,32767,e)&&scroll.setVelocity(0,kScrollX,256,e),"overflow fixture input");scroll.nextUpdate(kScrollAbsolute);check(scroll.tick(e)&&scroll.state().positions[0].x16==(-2147483647-1),"defined fixed position wrap");
 // Native actor actions preserve original flags and once-only completion consumption.
 ActorController controller;check(controller.initialize(1,0xf5,e)&&controller.initialize(2,0xf5,e)&&controller.initialize(3,0xf5,e),"actor ownership initialization");
 check(controller.initialize(1,0,e)&&!controller.initialize(64,0,e),"active low-word actor reinitializes and out-of-range rejects");check(controller.reserve(1,true,e)&&!controller.initialize(1,0,e)&&controller.reserve(1,false,e),"high-byte reserved actor rejects initialization");
 check(controller.link(1,2,e)&&controller.link(2,3,e),"nested actor links");
 check(!controller.link(3,1,e)&&!controller.link(1,2,e)&&!controller.link(1,0,e),"cycle duplicate sentinel link reject");
 check(controller.applyAction(1,3,e)==kActorActionWaiting,"active actor completion waits");
 check(controller.markCompletion(1,e)&&controller.applyAction(1,3,e)==kActorActionApplied&&controller.applyAction(1,3,e)==kActorActionWaiting,"completion latch consumed once");
 check(controller.applyAction(1,0,e)==kActorActionApplied&&controller.state(1)->flags==0&&controller.state(2)->flags==0&&controller.state(3)->flags==0,"action zero propagated recursively");
 check(controller.applyAction(1,1,e)==kActorActionApplied&&controller.state(3)->flags==1,"pause flag propagated");
 check(controller.applyAction(1,2,e)==kActorActionApplied&&controller.state(3)->flags==3&&controller.state(1)->completionFlags==0,"stop flags and completion clear");
 check(controller.applyAction(1,4,e)==kActorActionApplied&&controller.state(1)->completionFlags==255&&controller.state(2)->completionFlags==0,"protection action owner only");
 check(controller.applyAction(1,5,e)==kActorActionApplied&&controller.state(3)->recordIndex==65535&&controller.state(3)->remaining==65535&&controller.state(1)->flags==0,"restart sentinels and flags propagate");
 check(controller.applyAction(0,3,e)==kActorActionApplied&&controller.applyAction(64,0,e)==kActorActionError&&controller.applyAction(1,6,e)==kActorActionError,"inactive completion and unknown action");
 controller.clear();check(!controller.state(1)->active,"actor ownership reset");
 // Native palette commands are tested at original update boundaries, not host FPS.
 PaletteController palette;PaletteEffect effect;effect.type=1;effect.byteOffset=2;effect.countMinusOne=0;effect.delay=1;effect.frames.push_back(std::vector<uint16_t>(1,0x0eee));effect.frames.push_back(std::vector<uint16_t>(1,0x0246));
 check(palette.enqueue(effect,e)&&palette.tick(e)&&palette.colors()[1]==0x0eee&&palette.activeEffects()==1,"palette sequence first frame immediate");
 palette.pause(true);check(palette.tick(e)&&palette.colors()[1]==0x0eee,"palette pause retains delay");palette.pause(false);
 check(palette.tick(e)&&palette.colors()[1]==0x0eee&&palette.tick(e)&&palette.colors()[1]==0x0246&&palette.activeEffects()==0,"palette delay and once-only sequence end");
 effect.type=6;effect.delay=0;check(palette.enqueue(effect,e)&&palette.tick(e)&&palette.tick(e)&&palette.tick(e)&&palette.colors()[1]==0x0eee&&palette.activeEffects()==1,"palette cycle source wraps");
 PaletteEffect fade;fade.type=3;fade.byteOffset=2;fade.countMinusOne=0;fade.replaceCycling=true;check(palette.enqueue(fade,e)&&palette.activeEffects()==1,"replace cycling cancels existing cycling effects");
 for(unsigned i=0;i<8;++i)palette.tick(e);check(palette.colors()[1]==0&&palette.activeEffects()==0,"eight component-decrement updates");
 fade.type=5;check(palette.enqueue(fade,e),"white fade enqueue");for(unsigned i=0;i<8;++i)palette.tick(e);check(palette.colors()[1]==0x0eee,"white fade per-channel clamp");
 effect.type=2;effect.frames.resize(1);effect.frames[0][0]=0x0246;check(palette.enqueue(effect,e)&&palette.tick(e)&&palette.colors()[1]==0x0222,"black fill then approach target in same update");for(unsigned i=1;i<8;++i)palette.tick(e);check(palette.colors()[1]==0x0246&&palette.activeEffects()==0,"approach exact target over eight updates");
 effect.type=4;check(palette.enqueue(effect,e)&&palette.tick(e)&&palette.colors()[1]==0x0ccc,"white fill then approach target");palette.clear();
 effect.type=7;effect.countMinusOne=1;effect.frames[0].push_back(0x0eee);check(palette.enqueue(effect,e)&&palette.tick(e)&&palette.colors()[1]==0x0222&&palette.colors()[2]==0x0222,"uniform target uses first source color");palette.clear();effect.type=8;effect.countMinusOne=0;effect.frames[0].resize(1);effect.frames[0][0]=0x026a;check(palette.enqueue(effect,e),"grayscale source enqueue");for(unsigned i=0;i<8;++i)palette.tick(e);check(palette.colors()[1]==0x0666,"grayscale arithmetic mean rounded even");
 effect.type=10;effect.frames.clear();check(palette.enqueue(effect,e)&&palette.tick(e)&&palette.colors()[1]==0&&palette.activeEffects()==0,"palette clear terminates immediately");
 for(unsigned i=0;i<12;++i)check(palette.enqueue(effect,e),"bounded twelve palette slots");check(!palette.enqueue(effect,e),"full palette queue does not overwrite");palette.clear();
 effect.byteOffset=255;check(!palette.enqueue(effect,e),"palette odd offset rejects");effect.byteOffset=254;effect.countMinusOne=1;check(!palette.enqueue(effect,e),"palette extent rejects");
 std::vector<FramePiece> largeFrame(65,FramePiece{0,0,8,8,0,0});FramePlacement plain;check(placeFrame(largeFrame,plain,65,out,e)&&out.size()==65,"safe dynamic frame exceeds old arbitrary cap");check(!placeFrame(largeFrame,plain,64,out,e)&&out.empty(),"explicit caller frame bound preserved");
 std::vector<uint8_t> paletteSource(0xf63c-0xd400,0);std::vector<uint16_t> textColors;for(unsigned i=0;i<8;++i){size_t offset=0xf602-0xd400+i*8;paletteSource[offset]=0;paletteSource[offset+1]=uint8_t(i*2);}check(readGameTextPalette(paletteSource.data(),paletteSource.size(),textColors,e)&&textColors.size()==8&&textColors[7]==14,"game text palette data locators");paletteSource[0xf602-0xd400]=1;check(!readGameTextPalette(paletteSource.data(),paletteSource.size(),textColors,e)&&textColors.empty(),"text palette validation transactional");check(!readGameTextPalette(paletteSource.data(),8,textColors,e),"text palette extent checked before reads");
 // E528 replacement tasks define colors; only relative tasks read inheritance.
 {PaletteController owner;PaletteEffect replace;replace.type=10;replace.byteOffset=128;replace.countMinusOne=0;PaletteEffect relative=replace;relative.type=3;std::vector<bool> known(128,false);uint8_t posted=0;
 check(owner.enqueue(relative,e)&&!owner.tick(e,&posted,&known)&&posted==0&&!known[64]&&owner.activeEffects()==1,"unknown relative palette read rejects transactionally");
 owner.clear();check(owner.enqueue(replace,e)&&owner.enqueue(relative,e)&&owner.tick(e,&posted,&known)&&known[64]&&!known[63]&&!known[65]&&owner.colors()[64]==0,"source ordered clear then fade establishes exactly written range");
 owner.clear();known.assign(128,false);replace.type=1;replace.frames={{0x0eee}};check(owner.enqueue(replace,e),"source explicit replacement enqueued without invented inheritance");owner.pause(true);check(owner.tick(e,&posted,&known)&&!known[64],"paused replacement does not establish unpublished ownership");owner.pause(false);check(owner.tick(e,&posted,&known)&&known[64]&&owner.colors()[64]==0x0eee,"replacement frame owns its destination on actual execution");
 owner.clear();known.assign(128,false);check(owner.enqueue(relative,e)&&owner.enqueue(replace,e)&&!owner.tick(e,&posted,&known)&&!known[64]&&owner.activeEffects()==2,"later replacement cannot rescue an earlier unknown read");}
 std::printf("presentation: %d checks, %d failures\n",checks,failures);return failures;
}
#ifdef SNATCHER_PRESENTATION_TEST_MAIN
#include <fstream>
#include <iterator>
#include <sstream>
int runAnimationTests();
namespace {
std::vector<std::string> columns(const std::string &line){std::vector<std::string> values;std::istringstream stream(line);std::string part;while(std::getline(stream,part,'\t'))values.push_back(part);return values;}
bool readResource(const std::string &path,std::vector<uint8_t>& data){std::ifstream in(path.c_str(),std::ios::binary);if(!in)return false;in.seekg(0,std::ios::end);std::streamoff length=in.tellg();if(length<0||length>16*1024*1024)return false;in.seekg(0);data.resize(size_t(length));return data.empty()||bool(in.read(reinterpret_cast<char*>(&data[0]),length));}
}
int main(int argc,char **argv){
 if(argc==1)return runPresentationTests()+runAnimationTests()?1:0;
 if(argc==4&&std::string(argv[1])=="--font"){
  std::ifstream in(argv[2],std::ios::binary);std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());Snatcher::OriginalFont font;std::string error;
  if(!in||!font.load(data.empty()?0:&data[0],data.size(),error))return 2;
  std::ofstream out(argv[3],std::ios::binary);for(unsigned code=0x20;code<=0x85;++code){std::vector<uint8_t> pixels(64,0);Snatcher::FontGlyph glyph;if(!font.glyph(uint8_t(code),glyph,error)||!Snatcher::drawGlyph(font,uint8_t(code),0,0,1,pixels,8,8,error))return 2;out.write(reinterpret_cast<const char*>(&pixels[0]),64);out.put(char(glyph.advance));}return out?0:2;
 }
 if(argc==4&&std::string(argv[1])=="--geometry"){
  std::ifstream manifest(argv[2]);std::ofstream out(argv[3]);if(!manifest||!out)return 2;std::string line;
  while(std::getline(manifest,line)){std::vector<std::string> c=columns(line);if(c.size()!=4)return 2;std::vector<uint8_t> data;std::vector<Snatcher::FramePiece> pieces;std::string error;
   if(!readResource(c[1],data)||!Snatcher::readFrameGeometry(data.empty()?0:&data[0],data.size(),std::stoul(c[2]),unsigned(std::stoul(c[3])),65536,pieces,error))return 2;
   out<<c[0]<<'\t'<<pieces.size();for(size_t i=0;i<pieces.size();++i){const Snatcher::FramePiece&p=pieces[i];out<<'\t'<<p.x<<','<<p.y<<','<<p.width<<','<<p.height<<','<<p.sizeWord<<','<<p.attributeWord;}out<<'\n';
  }return out&&!manifest.bad()?0:2;
 }
 if(argc==4&&std::string(argv[1])=="--actors"){
  std::ifstream manifest(argv[2]);std::ofstream out(argv[3]);if(!manifest||!out)return 2;std::string line;
  while(std::getline(manifest,line)){std::vector<std::string> c=columns(line);if(c.size()!=4)return 2;std::vector<uint8_t> data;std::vector<Snatcher::ActorInitializer> actors;std::string error;
   if(!readResource(c[1],data)||!Snatcher::readActorInitializers(data.empty()?0:&data[0],data.size(),std::stoul(c[2]),std::stoul(c[3]),actors,error))return 2;
   for(size_t i=0;i<actors.size();++i){const Snatcher::ActorInitializer&a=actors[i];out<<c[0]<<'\t'<<i<<'\t'<<a.actorIndex<<'\t'<<unsigned(a.flags)<<'\t'<<a.x<<'\t'<<a.y<<'\t'<<a.frameTableOffset<<'\t'<<a.sequenceOffset<<'\n';}
  }return out&&!manifest.bad()?0:2;
 }
 if(argc==4&&std::string(argv[1])=="--animations"){
  std::ifstream manifest(argv[2]);std::ofstream out(argv[3]);if(!manifest||!out)return 2;std::string line;
  while(std::getline(manifest,line)){std::vector<std::string> c=columns(line);if(c.size()!=5)return 2;std::vector<uint8_t> data;std::string error;Snatcher::AnimationPlayer player;
   if(!readResource(c[1],data)||!player.start(data.empty()?0:&data[0],data.size(),std::stoul(c[2]),int16_t(std::stoi(c[3])),int16_t(std::stoi(c[4])),error))return 2;
   for(unsigned tick=0;tick<256;++tick){Snatcher::AnimationResult result=player.tick(error);std::vector<Snatcher::ScrollRequest> requests=player.takeScrollRequests();const Snatcher::AnimationState&s=player.state();out<<c[0]<<'\t'<<tick<<'\t'<<int(result)<<'\t'<<s.recordIndex<<'\t'<<s.frameIndex<<'\t'<<s.remaining<<'\t'<<s.hidden<<'\t'<<s.styleWord<<'\t'<<unsigned(s.paletteOverride)<<'\t'<<s.x16<<'\t'<<s.y16<<'\t'<<s.velocityX16<<'\t'<<s.velocityY16<<'\t';for(size_t k=0;k<requests.size();++k)out<<int(requests[k].kind)<<':'<<requests[k].plane<<':'<<int(requests[k].axis)<<':'<<requests[k].value<<',';out<<'\n';if(result!=Snatcher::kAnimationFrame)break;}
  }return out&&!manifest.bad()?0:2;
 }
 std::fprintf(stderr,"Usage: presentation-tests [--font SUBCODE PRIVATE_OUTPUT]\n");return 2;
}
#endif
