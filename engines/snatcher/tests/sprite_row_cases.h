/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_TEST_SPRITE_ROW_CASES_H
#define SNATCHER_TEST_SPRITE_ROW_CASES_H
#include "../scene_renderer.h"
inline std::vector<Snatcher::PlacedPiece> spriteRowCase(unsigned index){
 using Snatcher::PlacedPiece;std::vector<PlacedPiece> pieces;
 if(index==8){pieces.push_back(PlacedPiece(-128,64,8,8,16));for(unsigned i=0;i<15;++i)pieces.push_back(PlacedPiece(i*8,64,8,8,1));pieces.push_back(PlacedPiece(160,64,8,8,2));}
 else if(index==9){pieces.push_back(PlacedPiece(-128,64,32,8,16));for(unsigned i=0;i<7;++i)pieces.push_back(PlacedPiece(i*32,64,32,8,16));pieces.push_back(PlacedPiece(224,64,24,8,16));pieces.push_back(PlacedPiece(80,64,16,8,1));}
 else if(index<2){for(unsigned i=0;i<16;++i)pieces.push_back(PlacedPiece(i*8,64,8,8,index?16:1));pieces.push_back(PlacedPiece(160,64,8,8,2));}
 else if(index<6){unsigned n=index==3||index==4?7:8;for(unsigned i=0;i<n;++i)pieces.push_back(PlacedPiece(index==5?-80:int(i*32),64,32,8,16));if(n==7)pieces.push_back(PlacedPiece(224,64,24,8,16));pieces.push_back(PlacedPiece(80,64,16,8,index==4?0x801:1));}
 else{if(index==7)pieces.push_back(PlacedPiece(-80,64,8,8,16));pieces.push_back(PlacedPiece(-128,64,8,8,16));pieces.push_back(PlacedPiece(80,64,16,8,1));}
 return pieces;
}
inline bool spriteRowRenderer(Snatcher::SceneRenderer& renderer,std::string& error){
 using namespace Snatcher;SceneLayout layout;layout.width=256;layout.height=224;layout.columns=layout.rows=32;layout.planeEnabled[0]=layout.planeEnabled[1]=false;if(!renderer.configure(layout,error))return false;renderer.establishBlankAssets();std::vector<uint16_t> palette(64,0);palette[1]=0x00e;palette[2]=0x0e0;palette[3]=0xe00;palette[4]=0xeee;if(!renderer.setPalette(palette,0,error))return false;GraphicsTransfer transfer;transfer.destination=32;transfer.decoded.resize(128);for(unsigned i=0;i<128;++i)transfer.decoded[i]=uint8_t((i/32+1)*17);return renderer.applyTransfer(transfer,error);
}
#endif
