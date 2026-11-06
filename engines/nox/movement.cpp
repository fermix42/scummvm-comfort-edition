/* SPDX-License-Identifier: GPL-3.0-or-later */
// Reuses the existing native sector movement rule and guarded compatibility
// adapter. Byte signatures are represented by hashes, not game instruction data.
#include "nox/runtime.h"
namespace Nox {
struct NoxMapCell { byte sector,column,row; };
static bool matchesCode(const byte *data,uint length,uint32 expected) {
 uint32 hash=2166136261u;for(uint i=0;i<length;++i)hash=(hash^data[i])*16777619u;
 return hash==expected;
}
static bool nox_advance_cell(uint direction,const NoxMapCell *before,NoxMapCell *after,int *crossed) {
 if(direction>=4||before->column>=16||before->row>=16)return false;
 *after=*before;*crossed=0;
 switch(direction){
 case 0: if(after->row)--after->row;else{after->row=15;after->sector-=16;*crossed=1;}break;
 case 1: if(after->row<15)++after->row;else{after->row=0;after->sector+=16;*crossed=1;}break;
 case 2: if(after->column<15)++after->column;else{after->column=0;++after->sector;*crossed=1;}break;
 case 3: if(after->column)--after->column;else{after->column=15;--after->sector;*crossed=1;}break;
 }return true;
}
bool Runtime::nativeMovement(){
  static const uint16 starts[4]={0xed77,0xee3c,0xeef8,0xefad};
  static const uint16 stops[4]={0xed8d,0xee54,0xef0a,0xefbd};
  static const byte lengths[4]={22,24,18,16};
  static const uint32 hashes[4]={3636077106u,3600636360u,229685385u,1014389203u};
  if(pc<0xe000||!ram||alt||read_aux||write_aux||(p&D))return 0;
  unsigned direction;for(direction=0;direction<4;direction++)if(pc==starts[direction])break;
  if(direction==4||!matchesCode(high[0]+pc-0xe000,lengths[direction],hashes[direction]))return 0;
  NoxMapCell before={mainram[0x6cf5],mainram[0x6cf7],mainram[0x6cf6]},after;int crossed;
  if(!nox_advance_cell(direction,&before,&after,&crossed))return 0;
  /* Preserve just the legacy caller's ABI while the remainder is migrated. */
  if(direction==0&&crossed){a=before.sector;p|=C;arithmetic(16,1);}
  if(direction==1){cmp((byte)(before.row+1),16);if(crossed){a=before.sector;p&=~C;arithmetic(16,0);}}
  if(direction==2)cmp((byte)(before.column+1),16);
  a=direction<2?after.row:after.column;
  if(direction==0||direction==3||crossed)nz(a);
  mainram[0x6cf5]=after.sector;mainram[0x6cf7]=after.column;mainram[0x6cf6]=after.row;
  pc=stops[direction];
  static const unsigned instructions[4][2]={{3,9},{4,10},{4,7},{3,6}};
  static const unsigned clocks[4][2]={{13,30},{15,32},{15,26},{13,24}};
  // timed_step already charged the first DEC/INC (six clocks).
  cpu_cycles+=clocks[direction][crossed]-6;total_steps+=instructions[direction][crossed]-1;
  nativeCalls++;return 1;
}

} // namespace Nox
