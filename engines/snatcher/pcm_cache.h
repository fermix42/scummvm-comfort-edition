/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_PCM_CACHE_H
#define SNATCHER_PCM_CACHE_H
#include "pcm.h"
namespace Snatcher {
struct PcmCacheFact {bool known=false;uint16_t id=0xffff;};
bool validateResidentCache(const PcmCacheFact& cache,const std::vector<PcmCatalogEntry>& catalog,std::string& error);
bool migrateLegacyPcmCache(bool known,uint16_t lastSpeech,uint16_t resident,bool bookmarkValid,uint16_t archivedSpeech,const std::vector<PcmCatalogEntry>& catalog,PcmCacheFact& current,PcmCacheFact& bookmark,std::string& error);
}
#endif
