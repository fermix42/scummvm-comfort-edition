/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_ENGINE_SAVE_H
#define SNATCHER_ENGINE_SAVE_H
#include <string>
#include <vector>
#include <cstdint>
#include "common/str.h"
namespace Graphics {struct Surface;}
namespace Common {class SeekableReadStream;class SaveFileManager;}
namespace Snatcher {
struct NativeSaveSection;struct LegacyInputHeader;
class GameResource;struct Scenario;
bool alternateSaveScenario(GameResource& resources,Scenario& scenario,std::string& error);
bool nativeSaveDescription(Common::SeekableReadStream& stream,std::string& description,Graphics::Surface** thumbnail=nullptr);
// Standard RGB565 THMB bytes, kept in the same verified save generation.
bool captureSaveThumbnail(std::vector<uint8_t>& bytes);
Graphics::Surface* decodeSaveThumbnail(const std::vector<uint8_t>& bytes);
bool nativeAutosave(Common::SaveFileManager& manager,const Common::String& file);
bool migrateReservedNativeSaves(Common::SaveFileManager& manager,const Common::String& target,std::string& error);
void recordNativeSaveCreated(Common::SaveFileManager& manager,const Common::String& file,int slot,const Common::String& description);
// Offline staged bytes only; never applies state to a running Engine.
bool nativeInputCompatibilityProbe(const NativeSaveSection& source,const LegacyInputHeader& header,NativeSaveSection& migrated,std::string& error);
}
#endif
