/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "environment_visual.h"
#include <algorithm>
namespace Snatcher {
namespace {
uint16_t word(const uint8_t* p){return uint16_t(unsigned(p[0])*256+p[1]);}
unsigned phase(unsigned slot,unsigned clock){return ((slot+1)&3)?((slot*16+clock)/2)&127:(((slot^255)*8+clock)&127);}
PlacedPiece piece(unsigned y,unsigned x,unsigned side,uint16_t attributes){PlacedPiece p;p.x=int(x&511)-128;p.y=int(y&511)-128;p.width=p.height=side;p.attributes=attributes;return p;}
void append(std::vector<PlacedPiece>& out,const std::vector<PlacedPiece>& input){size_t n=std::min(input.size(),size_t(64-out.size()));out.insert(out.end(),input.begin(),input.begin()+n);}
}
bool readEnvironmentVisualTables(const uint8_t* sub,size_t size,EnvironmentVisualTables& out,std::string& error){
 error.clear();const size_t base=0xd400;if(!sub||size<0xf322-base){error="Environmental compositor source tables truncated";return false;}
 // Validate only the independently recovered dispatch family extents; table
 // bytes remain supplied by the owner's original resource, not native copies.
 if(word(sub+0xeede-base)!=0xfff0||word(sub+0xeee0-base)!=0x1e||word(sub+0xeee2-base)!=0x1e){error="Environmental compositor source directory mismatch";return false;}
 for(unsigned i=3;i<=9;++i)if(word(sub+0xeede-base+i*2)!=0x1fc){error="Environmental crossing family directory mismatch";return false;}
 EnvironmentVisualTables t;t.known=true;t.sourceStamp=UINT64_C(14695981039346656037);for(size_t i=0;i<size;++i)t.sourceStamp=(t.sourceStamp^sub[i])*UINT64_C(1099511628211);
 for(unsigned i=0;i<3;++i){t.counts[i]=sub[0xeef8-base+i];t.minX[i]=sub[0xf086-base+i];t.maxX[i]=sub[0xf08a-base+i];t.maxY[i]=sub[0xf08e-base+i];}
 if(t.counts[1]>63||t.counts[2]>63){error="Environmental piece count exceeds authored submission capacity";return false;}
 std::copy(sub+0xf092-base,sub+0xf0a2-base,t.drift.begin());std::copy(sub+0xf0ba-base,sub+0xf0da-base,t.initialX.begin());
 for(unsigned i=0;i<8;++i)t.smallTiles[i]=word(sub+0xf0a2-base+i*2);
 std::copy(sub+0xf292-base,sub+0xf302-base,t.crossingSteps.begin());std::copy(sub+0xf312-base,sub+0xf31a-base,t.crossingOffsets.begin());
 for(unsigned i=0;i<4;++i)t.crossingTiles[i]=word(sub+0xf31a-base+i*2);
 for(uint16_t tile:t.smallTiles)if(tile>2047){error="Environmental tile outside semantic pattern allocation";return false;}
 for(uint16_t tile:t.crossingTiles)if(tile>2047){error="Environmental crossing tile outside semantic pattern allocation";return false;}
 out=t;return true;
}
bool composeEnvironmentVisual(const EnvironmentVisualTables& t,uint16_t mode,uint16_t counter,const std::vector<PlacedPiece>& prefix,const std::vector<PlacedPiece>& suffix,EnvironmentVisualState& state,std::vector<PlacedPiece>& out,std::string& error){
 error.clear();if(mode>9){error="Environmental mode has no qualified original producer family";return false;}if(mode&&!t.known){error="Environmental compositor requires original authored tables";return false;}
 EnvironmentVisualState staged=state;std::vector<PlacedPiece> pieces;append(pieces,prefix);
 // ED28 clears only the first entry before submitting actors. The remainder
 // is retained game draw-list history, never a guessed reset on mode change.
 staged.previous[0].known=true;staged.previous[0].piece=piece(0,0,8,0);
 for(size_t i=0;i<pieces.size();++i){staged.previous[i].known=true;staged.previous[i].piece=pieces[i];}
 unsigned count=mode?(mode<=2?unsigned(t.counts[mode])+1:33):0;unsigned clock=counter/2;
 for(unsigned n=0;n<count&&pieces.size()<64;++n){unsigned slot=unsigned(pieces.size());const EnvironmentVisualSlot& old=staged.previous[slot];if(!old.known){error="Environmental effect inherits an unknown previous draw-list slot";return false;}
  unsigned x=uint8_t(old.piece.x),y=0;uint16_t attr;unsigned oldTile=old.piece.attributes&2047;
  if(mode<=2){unsigned tileIndex=(mode-1)*4+(((slot+x)/8)&3);bool inherited=oldTile==t.smallTiles[tileIndex];if(!inherited)x=t.initialX[slot&31];
   y=phase(slot,clock)+8;x=uint8_t(x+t.drift[(slot+clock)&15]);
   if(x<t.minX[mode]||x>=t.maxX[mode]||y>=t.maxY[mode])y=0;
   attr=uint16_t((inherited?(old.piece.attributes&0xf800):0x2000)|t.smallTiles[(mode-1)*4+(((slot+x)/8)&3)]);
  }else{bool inherited=oldTile==t.crossingTiles[((slot+x)/8)&3];if(!inherited)x=t.initialX[slot&31];
   y=phase(slot,clock);unsigned step=t.crossingSteps[(mode-3)*16+((slot+clock)&15)];unsigned test;
   if(x>=128){test=uint16_t(x+step);x=test;if(test>=216){x=128+t.crossingOffsets[counter&7];test=x;}}
   else{x=uint16_t(x-step);test=x;if(test<16){x=uint16_t(127-t.crossingOffsets[counter&7]);test=x;}}
   if(test<16)y=0;else if(y>=112)y=uint16_t(-4);
   attr=uint16_t((inherited?(old.piece.attributes&0xf800):0x4000)|t.crossingTiles[((slot+x)/8)&3]);
  }
  PlacedPiece p=piece(uint16_t(y+128),uint16_t(x+128),mode==1?8:16,attr);pieces.push_back(p);staged.previous[slot].known=true;staged.previous[slot].piece=p;
 }
 size_t boundary=pieces.size();append(pieces,suffix);for(size_t i=boundary;i<pieces.size();++i){staged.previous[i].known=true;staged.previous[i].piece=pieces[i];}
 state=staged;out.swap(pieces);return true;
}
}
