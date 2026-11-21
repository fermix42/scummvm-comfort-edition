/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../pcm_cache.h"
#include <cstdio>
#include <cstdlib>
using namespace Snatcher;
static unsigned checks=0;static void check(bool b,const char* message){++checks;if(!b){std::fprintf(stderr,"FAIL PCM cache: %s\n",message);std::exit(1);}}
int main(){std::vector<PcmCatalogEntry> catalog(3);for(unsigned i=0;i<3;++i)catalog[i].id=uint16_t(i+1);catalog[0].mode=1;catalog[1].mode=2;catalog[2].mode=0x82;std::string error;PcmCacheFact current,bookmark;
 for(uint16_t stream:{uint16_t(2),uint16_t(3)}){check(migrateLegacyPcmCache(true,stream,1,true,stream,catalog,current,bookmark,error)&&current.known&&current.id==0xffff&&bookmark.known&&bookmark.id==0xffff,"legacy stream proves both source caches cleared");}
 check(migrateLegacyPcmCache(true,0xffff,1,true,0xffff,catalog,current,bookmark,error)&&current.known&&current.id==1&&!bookmark.known,"actual current bank recoverable, archival bank remains unknown");
 check(migrateLegacyPcmCache(true,0xffff,0xffff,false,0xffff,catalog,current,bookmark,error)&&current.known&&current.id==0xffff&&!bookmark.known,"source reset and absent bookmark");
 check(migrateLegacyPcmCache(false,0xffff,1,true,0xffff,catalog,current,bookmark,error)&&!current.known&&!bookmark.known,"missing legacy producer ownership is not inferred from resident data");
 current.known=bookmark.known=true;current.id=bookmark.id=1;check(!migrateLegacyPcmCache(true,1,1,true,0xffff,catalog,current,bookmark,error)&&current.id==1&&bookmark.id==1,"wrong stream class rejects transactionally");
 check(!migrateLegacyPcmCache(true,0xffff,2,true,0xffff,catalog,current,bookmark,error)&&current.id==1,"wrong resident class rejects transactionally");
 check(!migrateLegacyPcmCache(true,0xffff,1,true,1,catalog,current,bookmark,error)&&bookmark.id==1,"historical bank cannot be guessed from old stream field");
 check(validateResidentCache(current,catalog,error),"corrected bank cache validates");current.id=2;check(!validateResidentCache(current,catalog,error),"speech is never a new resident bookmark");current.known=false;check(validateResidentCache(current,catalog,error),"unknown cache stays unknown");
 std::printf("PCM cache migration: %u checks passed\n",checks);return 0;}
