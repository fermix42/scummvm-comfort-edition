/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../pcm_visual.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
using namespace Snatcher;
namespace {unsigned checks;void check(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"PCM visual: %s\n",why);std::exit(1);}}}
int main(int argc,char** argv){std::string error;const size_t at=0x13fec-0xd400;std::vector<uint8_t> source(at+1198,255),table={17,19};source[at+53]=15;source[at+1197]=254;auto unchanged=table;
 check(!readPcmVisualTable(nullptr,source.size(),1197,table,error)&&table==unchanged,"null source rejects transactionally");check(!readPcmVisualTable(source.data(),source.size()-1,1197,table,error)&&table==unchanged,"last catalog byte must exist");check(!readPcmVisualTable(source.data(),source.size(),1196,table,error)&&table==unchanged&&!readPcmVisualTable(source.data(),source.size(),1198,table,error),"catalog identity closes table extent before adjacent code");check(readPcmVisualTable(source.data(),source.size(),1197,table,error)&&table.size()==1198,"full source catalog map independent of assets");bool mapped=true;uint8_t list=7;check(!lookupPcmVisual(table,0,mapped,list,error)&&mapped&&list==7&&!lookupPcmVisual(table,1198,mapped,list,error)&&mapped&&list==7,"invalid ID leaves prior plan unchanged");
 for(unsigned id=1;id<=1197;++id){check(lookupPcmVisual(table,uint16_t(id),mapped,list,error),"every valid catalog ID has an explicit byte owner");check(mapped==(id==53||id==1197)&&list==(id==53?15:id==1197?254:255),"FF sentinel distinct from every other display-list byte");}
 check(lookupPcmVisual(table,53,mapped,list,error)&&mapped&&list==15,"repeated request retains same mapping; no hidden last-ID dedup");std::vector<uint8_t> bad(1197);check(!lookupPcmVisual(bad,53,mapped,list,error),"unknown/truncated table fails closed");
 if(argc==3&&std::string(argv[1])=="--original"){std::ifstream f(argv[2],std::ios::binary);source.assign(std::istreambuf_iterator<char>(f),{});check(bool(f)||f.eof(),"original source opened");check(readPcmVisualTable(source.data(),source.size(),1197,table,error),"actual source mapping extent");unsigned count=0;for(unsigned id=1;id<=1197;++id){check(lookupPcmVisual(table,uint16_t(id),mapped,list,error),"actual catalog mapping");if(mapped){++count;check(list<=93,"observed source mapped-list extent");}if(id<=35)check(!mapped,"all resident banks have no implicit visual overlay");}check(count==1059,"actual full-catalog mapped count");check(lookupPcmVisual(table,53,mapped,list,error)&&mapped&&list==15,"actual53 uses current-resource list15");std::printf("PCM visual original:1197IDs,%u mapped,35 resident none\n",count);}
 else check(argc==1,"supported command line");std::printf("PCM visual: %u checks,0 failures\n",checks);return 0;}
