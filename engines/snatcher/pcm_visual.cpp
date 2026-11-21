/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pcm_visual.h"
namespace Snatcher {
namespace {const size_t kTableOffset=0x13fec-0xd400,kCatalogCount=1197;}
bool readPcmVisualTable(const uint8_t *sub,size_t size,size_t count,std::vector<uint8_t>& table,std::string& error){
 error.clear();if(!sub||count!=kCatalogCount||size<kTableOffset||size-kTableOffset<count+1){error="PCM visual source table/catalog extent unavailable";return false;}
 std::vector<uint8_t> next(sub+kTableOffset,sub+kTableOffset+count+1);table.swap(next);return true;
}
bool lookupPcmVisual(const std::vector<uint8_t>& table,uint16_t id,bool& mapped,uint8_t& displayList,std::string& error){
 error.clear();if(table.size()!=kCatalogCount+1||!id||id>kCatalogCount){error="PCM visual catalog identity unavailable";return false;}
 uint8_t value=table[id];mapped=value!=0xff;displayList=value;return true;
}
}
