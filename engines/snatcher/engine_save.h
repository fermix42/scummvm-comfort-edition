/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_ENGINE_SAVE_H
#define SNATCHER_ENGINE_SAVE_H
#include <string>
#include "common/str.h"
namespace Common {class SeekableReadStream;class SaveFileManager;}
namespace Snatcher {
struct NativeSaveSection;struct LegacyInputHeader;
bool nativeSaveDescription(Common::SeekableReadStream& stream,std::string& description);
bool nativeAutosave(Common::SaveFileManager& manager,const Common::String& file);
bool migrateReservedNativeSaves(Common::SaveFileManager& manager,const Common::String& target,std::string& error);
void recordNativeSaveCreated(Common::SaveFileManager& manager,const Common::String& file,int slot,const Common::String& description);
// Offline staged bytes only; never applies state to a running Engine.
bool nativeInputCompatibilityProbe(const NativeSaveSection& source,const LegacyInputHeader& header,NativeSaveSection& migrated,std::string& error);
}
#endif
