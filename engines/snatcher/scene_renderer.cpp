/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scene_renderer.h"
#include <algorithm>
namespace Snatcher {
namespace {
// The game's EDA6/EDB4 placement adds the 128-dot display origin before
// emitting position words. Its noninterlace sprite format consumes nine bits;
// higher word bits are not additional offscreen distance. Keep this policy at
// the game compositor boundary, not in generic pattern or plane drawing.
int spriteCoordinate(int coordinate){return int((uint32_t(coordinate)+128u)&511u)-128;}
}
bool readSceneLayoutPreset(const uint8_t *mainIp,size_t size,SceneLayoutPreset preset,SceneLayout& layout,std::string& error){
 layout=SceneLayout();error.clear();if(!mainIp||size<0x15b0||preset<kSceneSquareLayout||preset>kSceneWideLayout){error="invalid original main-IP layout preset";return false;}size_t shape=preset==kSceneSquareLayout?0x1588:preset==kSceneTallLayout?0x15a2:0x15a4,planes=preset==kSceneSquareLayout?0x158e:0x15a6;uint16_t dimensions=uint16_t(uint16_t(mainIp[shape])<<8|mainIp[shape+1]),a=uint16_t(uint16_t(mainIp[planes+2])<<8|mainIp[planes+3]),b=uint16_t(uint16_t(mainIp[planes+4])<<8|mainIp[planes+5]);
 if((dimensions&0xff00)!=0x9000||((dimensions&0xff)!=0x11&&(dimensions&0xff)!=0x10&&(dimensions&0xff)!=1)||(a&0xff00)!=0x8200||(b&0xff00)!=0x8400||mainIp[0x158c]!=0x8c||mainIp[0x158d]!=0){error="unestablished main-IP game layout encoding";return false;}
 SceneLayout out;out.width=256;out.height=224;out.columns=(dimensions&1)?64:32;out.rows=(dimensions&16)?64:32;out.planeAllocation[0]=(a&0x38)<<10;out.planeAllocation[1]=(b&7)<<13;out.backdrop=mainIp[0x1599]&63;SceneRenderer validate;if(!validate.configure(out,error))return false;layout=out;return true;
}
bool sceneColor(uint16_t raw,SceneColorEncoding encoding,uint8_t& r,uint8_t& g,uint8_t& b,std::string& error){
 error.clear();r=g=b=0;if((raw&0xf111)||(encoding!=kSceneRGB888&&encoding!=kSceneReferenceRGB565)){error="unsupported normal palette word/encoding";return false;}
 r=uint8_t(((raw>>1)&7)*34);g=uint8_t(((raw>>5)&7)*34);b=uint8_t(((raw>>9)&7)*34);
 if(encoding==kSceneReferenceRGB565){unsigned rr=r>>3,gg=g>>2,bb=b>>3;r=uint8_t(rr*255/31);g=uint8_t(gg*255/63);b=uint8_t(bb*255/31);}return true;
}
SceneRenderer::SceneRenderer(){clear();}
void SceneRenderer::clear(){_patterns.clear();for(unsigned i=0;i<2;++i){_maps[i].assign(4096,0);_known[i].assign(4096,false);}_palette.clear();_layout=SceneLayout();_configured=_paletteKnown=false;}
void SceneRenderer::establishBlankAssets(){std::vector<uint8_t> blank(2048*32,0);std::string error;_patterns.assign(0,blank,error);for(unsigned i=0;i<2;++i){std::fill(_maps[i].begin(),_maps[i].end(),0);std::fill(_known[i].begin(),_known[i].end(),true);}}
bool SceneRenderer::establishSceneReset(SceneLayoutPreset preset,std::string& error){error.clear();if(preset<kSceneSquareLayout||preset>kSceneWideLayout){error="unknown source scene reset preset";return false;}SceneRenderer next=*this;GraphicsTransfer transfer;transfer.destination=0;transfer.delay=0;transfer.decoded.assign(preset==kSceneSquareLayout?0xb000:0xc000,0);if(!next.applyTransfer(transfer,error))return false;transfer.decoded.assign(preset==kSceneSquareLayout?0x2000:0x1000,0);transfer.destination=0xc000;if(!next.applyTransfer(transfer,error))return false;transfer.destination=0xe000;if(!next.applyTransfer(transfer,error))return false;*this=next;return true;}
bool SceneRenderer::configure(const SceneLayout& layout,std::string& error){error.clear();if(!layout.width||!layout.height||layout.width>320||layout.height>240||(layout.columns!=32&&layout.columns!=64)||(layout.rows!=32&&layout.rows!=64)||layout.columns*layout.rows>4096||layout.backdrop>=64){error="unestablished game scene layout";return false;}for(unsigned i=0;i<2;++i)if(layout.planeAllocation[i]!=0xc000&&layout.planeAllocation[i]!=0xe000){error="unestablished game plane allocation";return false;}for(size_t i=0;i<layout.viewBands.size();++i)if(!layout.viewBands[i].firstRow||layout.viewBands[i].firstRow>=layout.height||(i&&layout.viewBands[i-1].firstRow>=layout.viewBands[i].firstRow)){error="unordered or out-of-range semantic view bands";return false;}_layout=layout;_configured=true;return true;}
bool SceneRenderer::setPalette(const std::vector<uint16_t>& colors,unsigned first,std::string& error){error.clear();if((first!=0&&first!=64)||colors.size()<first+64){error="scene palette bank not established";return false;}for(unsigned i=0;i<64;++i){uint8_t r,g,b;if(!sceneColor(colors[first+i],kSceneRGB888,r,g,b,error))return false;}_palette.assign(colors.begin()+first,colors.begin()+first+64);_paletteKnown=true;return true;}
bool SceneRenderer::applyTransfer(const GraphicsTransfer& transfer,std::string& error){
 error.clear();if((transfer.destination&1)||transfer.decoded.empty()||(transfer.decoded.size()&1)||transfer.decoded.size()>65536-transfer.destination){error="unaligned/out-of-range original graphics allocation";return false;}
 if(!transfer.knownBytes.empty()&&transfer.knownBytes.size()!=transfer.decoded.size()){error="graphics copy ownership mask extent";return false;}
 bool patternAligned=!(transfer.destination&31)&&!(transfer.decoded.size()%32);
 // Startup paint tasks also publish individual name words. Their native map
 // ownership does not require a whole pattern and must preserve adjacent cells.
 if(!patternAligned&&transfer.destination<0xc000){error="partial pattern allocation requires its format owner";return false;}
 SceneRenderer next=*this;if(patternAligned&&!next._patterns.assign(transfer.destination/32,transfer.decoded,error))return false;
 if(patternAligned&&!transfer.knownBytes.empty())for(size_t tile=0;tile<transfer.decoded.size()/32;++tile){bool known=true;for(unsigned n=0;n<32;++n)known=known&&transfer.knownBytes[tile*32+n];if(!known)next._patterns.invalidate(transfer.destination/32+unsigned(tile),1);}
 for(size_t n=0;n<transfer.decoded.size();n+=2){unsigned address=unsigned(transfer.destination+n);if(address<0xc000)continue;unsigned allocation=address>=0xe000?0xe000:0xc000,index=(address-allocation)/2,slot=allocation==0xe000?0:1;next._maps[slot][index]=uint16_t(uint16_t(transfer.decoded[n])<<8|transfer.decoded[n+1]);next._known[slot][index]=transfer.knownBytes.empty()||(transfer.knownBytes[n]&&transfer.knownBytes[n+1]);}
 *this=next;return true;
}
bool SceneRenderer::fillPlaneCells(unsigned plane,size_t column,size_t row,size_t columns,size_t rows,uint16_t attribute,std::string& error){error.clear();if(!_configured||plane>=2||!columns||!rows||column>=_layout.columns||row>=_layout.rows||columns>_layout.columns-column||rows>_layout.rows-row){error="invalid semantic plane fill rectangle";return false;}unsigned slot=_layout.planeAllocation[plane]==0xe000?0:1;for(size_t y=row;y<row+rows;++y)for(size_t x=column;x<column+columns;++x){size_t at=y*_layout.columns+x;_maps[slot][at]=attribute;_known[slot][at]=true;}return true;}
bool SceneRenderer::planePixel(unsigned plane,size_t x,size_t y,uint8_t& color,bool& priority,std::string& error)const{
 color=0;priority=false;if(!_layout.planeEnabled[plane])return true;int sourceX=_layout.sourceX[plane],sourceY=_layout.sourceY[plane];for(const SceneViewBand& band:_layout.viewBands){if(y<band.firstRow)break;sourceX=band.sourceX[plane];sourceY=band.sourceY[plane];}int64_t w=int64_t(_layout.columns*8),h=int64_t(_layout.rows*8),sx=(int64_t(x)+sourceX)%w,sy=(int64_t(y)+sourceY)%h;if(sx<0)sx+=w;if(sy<0)sy+=h;unsigned slot=_layout.planeAllocation[plane]==0xe000?0:1;size_t index=size_t(sy/8)*_layout.columns+size_t(sx/8);if(!_known[slot][index]){error="visible scene references unestablished inherited plane cell";return false;}uint16_t attr=_maps[slot][index];priority=(attr&0x8000)!=0;return _patterns.sample(attr,unsigned(sx)%8,unsigned(sy)%8,color,error);
}
bool SceneRenderer::render(const std::vector<PlacedPiece>& pieces,SceneColorEncoding encoding,SceneImage& image,std::string& error,const std::vector<ScenePaletteSpan>* paletteSpans)const{
 image=SceneImage();error.clear();if(!_configured||!_paletteKnown){error=!_configured?"scene layout ownership unresolved":"scene palette inheritance unresolved";return false;}
 if(paletteSpans){if(paletteSpans->size()>1024){error="scene palette publication count exceeds bounded frame";return false;}size_t previous=0;bool first=true;for(const ScenePaletteSpan& span:*paletteSpans){if(span.row>=_layout.height||span.column>=_layout.width||span.firstColor>=64||span.colors.empty()||span.colors.size()>64-span.firstColor){error="scene palette publication outside owned display/color extent";return false;}size_t position=span.row*_layout.width+span.column;if(!first&&position<previous){error="scene palette publications are not in display order";return false;}first=false;previous=position;for(uint16_t raw:span.colors){uint8_t r,g,b;if(!sceneColor(raw,encoding,r,g,b,error))return false;}}}
 std::vector<uint16_t> displayedPalette=_palette;size_t publication=0;
 SceneImage out;out.width=_layout.width;out.height=_layout.height;size_t count=out.width*out.height;out.indexed.assign(count,_layout.backdrop);out.rgb.resize(count*3);std::vector<uint8_t> sprites(count,0),priorities(count,0),temporary(count,0);
 out.submittedPieces=std::min(size_t(64),pieces.size());out.spriteLimitReached=pieces.size()>64;
 // Earlier actor/piece order owns a sprite pixel even if a later sprite has
 // high priority. Plane priority is evaluated after this ownership selection.
 // Original game submissions have a16-piece/256-dot row budget. Selection
 // precedes transparency and horizontal clipping. A terminal piece can expose
 // only its left prefix, after its own flip has selected the source columns.
 std::vector<unsigned> selected(out.height,0),fetched(out.height,0);
 std::vector<bool> nonzeroPosition(out.height,false),masked(out.height,false);
 for(size_t i=0;i<out.submittedPieces;++i){
  std::fill(temporary.begin(),temporary.end(),0);PlacedPiece piece=pieces[i];piece.x=spriteCoordinate(piece.x);piece.y=spriteCoordinate(piece.y);
  std::vector<uint8_t> admitted(out.height,0);
  for(size_t row=0;row<out.height;++row){
   if(int64_t(row)<piece.y||int64_t(row)>=int64_t(piece.y)+piece.height||masked[row])continue;
   if(selected[row]>=16||fetched[row]>=256){out.spriteLimitReached=true;continue;}
   ++selected[row];unsigned width=std::min(piece.width,256-fetched[row]);if(width<piece.width)out.spriteLimitReached=true;fetched[row]+=piece.width;
   if(piece.x==-128){if(nonzeroPosition[row])masked[row]=true;continue;}
   nonzeroPosition[row]=true;admitted[row]=uint8_t(width);
  }
  if(!_patterns.draw(piece,temporary,out.width,out.height,error,&admitted))return false;
  for(size_t at=0;at<count;++at)if(temporary[at]&&!sprites[at]){sprites[at]=temporary[at];priorities[at]=(piece.attributes&0x8000)?1:0;}
 }
 for(size_t y=0;y<out.height;++y)for(size_t x=0;x<out.width;++x){size_t at=y*out.width+x;if(paletteSpans)while(publication<paletteSpans->size()){const ScenePaletteSpan& span=(*paletteSpans)[publication];if(span.row*out.width+span.column>at)break;for(size_t i=0;i<span.colors.size();++i)displayedPalette[span.firstColor+i]=span.colors[i];++publication;}bool enabled=_layout.displayEnabled;for(const SceneViewBand& band:_layout.viewBands){if(y<band.firstRow)break;enabled=band.displayEnabled;}if(!enabled){out.indexed[at]=0;out.rgb[at*3]=out.rgb[at*3+1]=out.rgb[at*3+2]=0;continue;}uint8_t a,b;bool pa,pb;if(!planePixel(0,x,y,a,pa,error)||!planePixel(1,x,y,b,pb,error))return false;uint8_t color=_layout.backdrop;if(b&&!pb)color=b;if(a&&!pa)color=a;if(sprites[at]&&!priorities[at])color=sprites[at];if(b&&pb)color=b;if(a&&pa)color=a;if(sprites[at]&&priorities[at])color=sprites[at];out.indexed[at]=color;uint8_t r,g,bl;if(!sceneColor(displayedPalette[color],encoding,r,g,bl,error))return false;out.rgb[at*3]=r;out.rgb[at*3+1]=g;out.rgb[at*3+2]=bl;}
 image=out;return true;
}
}
