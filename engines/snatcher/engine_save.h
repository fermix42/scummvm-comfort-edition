/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_ENGINE_SAVE_H
#define SNATCHER_ENGINE_SAVE_H
#include <string>
namespace Common {class SeekableReadStream;}
namespace Snatcher {
struct NativeSaveSection;struct LegacyInputHeader;
bool nativeSaveDescription(Common::SeekableReadStream& stream,std::string& description);
// Offline staged bytes only; never applies state to a running Engine.
bool nativeInputCompatibilityProbe(const NativeSaveSection& source,const LegacyInputHeader& header,NativeSaveSection& migrated,std::string& error);
}
#endif
