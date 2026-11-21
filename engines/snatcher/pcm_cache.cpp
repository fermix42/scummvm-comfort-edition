/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pcm_cache.h"
namespace Snatcher {
bool validateResidentCache(const PcmCacheFact& cache,const std::vector<PcmCatalogEntry>& catalog,std::string& error){error.clear();if(!cache.known)return true;if(cache.id==0xffff)return true;const auto* entry=findPcmEntry(catalog,cache.id);if(!entry||entry->mode!=1){error="PCM bookmark cache is not a validated resident bank";return false;}return true;}
bool migrateLegacyPcmCache(bool known,uint16_t lastSpeech,uint16_t resident,bool valid,uint16_t archivedSpeech,const std::vector<PcmCatalogEntry>& catalog,PcmCacheFact& current,PcmCacheFact& bookmark,std::string& error){
 error.clear();PcmCacheFact next,old;
 auto stream=[&](uint16_t id){const auto* entry=findPcmEntry(catalog,id);return entry&&(entry->mode==2||entry->mode==0x82);};
 if(known){next.known=true;if(lastSpeech!=0xffff){if(!stream(lastSpeech)){error="Legacy PCM cache does not identify a source stream";return false;}next.id=0xffff;}else next.id=resident;if(!validateResidentCache(next,catalog,error))return false;}
 if(valid&&archivedSpeech!=0xffff){if(!stream(archivedSpeech)){error="Legacy archived PCM cache is not a source stream";return false;}old.known=true;old.id=0xffff;}
 // ArchivedFFFF cannot distinguish historical bank identity from reset. Keep
 // that subowner unknown until a new actual native11/source capture replaces it.
 current=next;bookmark=old;return true;
}
}
