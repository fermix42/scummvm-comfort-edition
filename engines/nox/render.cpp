/* SPDX-License-Identifier: GPL-3.0-or-later */
// Existing standalone raster conversion, adapted to a ScummVM surface.
#include "nox/runtime.h"
namespace Nox {
static const uint32 palette[16]={0x000000,0xe31e60,0x604ebd,0xff44fd,0x00a360,0x9c9c9c,0x14cffd,0xd0c3ff,0x607203,0xff6a3c,0x9c9c9c,0xffa0d0,0x14f53c,0xd0dd8d,0x72ffd0,0xffffff};
static void plot(Graphics::Surface &surface, int x, int y, uint32 color) {
 *static_cast<uint32 *>(surface.getBasePtr(x,y*2))=color;
 *static_cast<uint32 *>(surface.getBasePtr(x,y*2+1))=color;
}
static unsigned hgr_offset(unsigned row){return (row&7)*1024+((row>>3)&7)*128+(row>>6)*40;}
void Runtime::textRow(Graphics::Surface &surface, int row, uint32 millis){
  unsigned base=(video_page2&&!video_80store)?0x800:0x400;
  unsigned offset=(row&7)*128+(row>>3)*40;
  int columns=video_80col?80:40,scale=video_80col?1:2;
  for(int col=0;col<columns;col++){
    byte ch=(video_80col&&!(col&1)?auxram:mainram)[base+offset+(video_80col?col/2:col)];
    int inverse=ch<0x40,flash=ch>=0x40&&ch<0x80;
    if(flash&&((millis/267)&1))inverse=!inverse;
    for(int gy=0;gy<8;gy++)for(int gx=0;gx<7;gx++){
      int on=(font[ch&63][gy]&(0x40>>gx))!=0;
      uint32_t color=(on^inverse)?0xffffff:0;
      for(int k=0;k<scale;k++)plot(surface,col*7*scale+gx*scale+k,row*8+gy,color);
    }
  }
}
void Runtime::render(Graphics::Surface &surface, uint32 hostMillis){
  if(video_text){for(int row=0;row<24;row++)textRow(surface,row,hostMillis);return;}
  unsigned base=(video_page2&&!video_80store)?0x4000:0x2000;
  int rows=video_mixed?160:192;
  for(int row=0;row<rows;row++){
    unsigned offset=hgr_offset(row);
    if(!video_hires){
      unsigned textbase=(video_page2&&!video_80store)?0x800:0x400;
      unsigned t=(row/8&7)*128+(row/64)*40;
      for(int col=0;col<40;col++){byte b=mainram[textbase+t+col];uint32_t c=palette[(row&7)<4?(b&15):(b>>4)];for(int j=0;j<14;j++)plot(surface,col*14+j,row,c);}
    }else if(video_dhgr&&video_80col){
      byte bits[560];
      for(int col=0;col<40;col++)for(int k=0;k<7;k++){bits[col*14+k]=(auxram[base+offset+col]>>k)&1;bits[col*14+7+k]=(mainram[base+offset+col]>>k)&1;}
      for(int px=0;px<560;px++){
        unsigned i=px&~3;unsigned nib=bits[i]|bits[i+1]<<1|bits[i+2]<<2|bits[i+3]<<3;
        unsigned colorIndex=((nib<<1)|(nib>>3))&15;
        plot(surface,px,row,false?(bits[px]?0xffffff:0):palette[colorIndex]);
      }
    }else{
      byte bits[280],phase[280];
      for(int col=0;col<40;col++)for(int k=0;k<7;k++){byte b=mainram[base+offset+col];bits[col*7+k]=(b>>k)&1;phase[col*7+k]=b>>7;}
      for(int px=0;px<280;px++){
        uint32_t c=0;
        if(bits[px]){if(false||(px>0&&bits[px-1])||(px<279&&bits[px+1]))c=0xffffff;
          else c=phase[px]?(px&1?0xff6a3c:0x14cffd):(px&1?0x14f53c:0xff44fd);}
        plot(surface,px*2,row,c);plot(surface,px*2+1,row,c);
      }
    }
  }
  if(video_mixed)for(int row=20;row<24;row++)textRow(surface,row,hostMillis);
}

} // namespace Nox
