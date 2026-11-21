/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_GAME_SERVICE_BACKEND_H
#define SNATCHER_GAME_SERVICE_BACKEND_H
#include "game_service.h"
#include "common/str.h"
namespace Common {class SaveFileManager;}
namespace Snatcher {
// Original four-slot game service, owned by CE's portable save manager. These
// records do not share names or payloads with ordinary full-engine SNSV saves.
bool scanGameStorage(Common::SaveFileManager& manager,const Common::String& target,GameServiceState& state,std::string& error);
bool publishGameSave(Common::SaveFileManager& manager,const Common::String& target,unsigned slot,const GameSaveRecord& record,GameSaveOutcome& outcome,std::string& error);
bool readGameSave(Common::SaveFileManager& manager,const Common::String& target,unsigned slot,const std::string& disc,const std::vector<uint64_t>& stamps,GameSaveRecord& record,std::string& error);
}
#endif
