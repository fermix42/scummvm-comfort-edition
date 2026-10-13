/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "engines/grim/grim.h"
#include "engines/grim/savegame.h"
#include "engines/grim/ce_ps2_saves.h"
#include "engines/grim/emi/emi.h"

#include "common/system.h"
#include "common/savefile.h"
#include "common/translation.h"
#include "common/config-manager.h"

namespace Grim {

static const ADExtraGuiOptionsMap gameGuiOptions[] = {
	{
		GAMEOPTION_LOAD_DATAUSR,
		{
			_s("Load user patch (unsupported)"),
			_s("Load an user patch. Please note that the ScummVM team doesn't provide support for using such patches."),
			"datausr_load",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_SHOW_FPS,
		{
			_s("Show FPS"),
			_s("Show the current FPS-rate, while you play."),
			"show_fps",
			false,
			0,
			0
		}
	},

	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

class GrimMetaEngine : public AdvancedMetaEngine<Grim::GrimGameDescription> {
public:
	const char *getName() const override {
		return "grim";
	}

	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		return gameGuiOptions;
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const Grim::GrimGameDescription *desc) const override;

	bool hasFeature(MetaEngineFeature f) const override;
	bool hasFeatureForTarget(MetaEngineFeature f, const char *target) const override;
	SaveStateDescriptor querySaveMetaInfos(const char *target, int slot) const override;

	Common::KeymapArray initKeymaps(const char *target) const override;

	SaveStateList listSaves(const char *target) const override;
	Common::String getSavegameFile(int slot, const char *target = nullptr) const override;

};

Common::Error GrimMetaEngine::createInstance(OSystem *syst, Engine **engine, const Grim::GrimGameDescription *desc) const {
	const GrimGameDescription *gd = (const GrimGameDescription *)desc;

	if (gd->gameType == GType_MONKEY4) {
#ifdef ENABLE_MONKEY4
#if !defined(USE_MPEG2)
		if (gd->desc.platform == Common::kPlatformPS2) {
			return Common::Error(Common::kUnsupportedGameidError, _s("Escape from Monkey Island PS2 support required MPEG2 support, which is not compiled in"));
		}
#endif
		*engine = new EMIEngine(syst, gd->desc.flags, gd->gameType, gd->desc.platform, gd->desc.language);
#else
		return Common::Error(Common::kUnsupportedGameidError, _s("Escape from Monkey Island support is not compiled in"));
#endif
	} else {
		*engine = new GrimEngine(syst, gd->desc.flags, gd->gameType, gd->desc.platform, gd->desc.language);
	}

	return Common::kNoError;
}

bool GrimMetaEngine::hasFeature(MetaEngineFeature f) const {
	return
		(f == kSupportsListSaves) ||
		(f == kSupportsLoadingDuringStartup);
}

bool GrimMetaEngine::hasFeatureForTarget(MetaEngineFeature f, const char *target) const {
	if (target && ConfMan.get("gameid", target) == "monkey4" &&
	    Common::parsePlatform(ConfMan.get("platform", target)) == Common::kPlatformPS2 &&
	    (f == kSavesSupportMetaInfo || f == kSavesSupportThumbnail))
		return true;
	return hasFeature(f);
}

SaveStateDescriptor GrimMetaEngine::querySaveMetaInfos(const char *target, int slot) const {
	if (hasFeatureForTarget(kSavesSupportThumbnail, target))
		return CEPS2Saves::saveMetadata(this, CEPS2Saves::chooserFile(target, slot), slot);
	return AdvancedMetaEngine::querySaveMetaInfos(target, slot);
}

Common::KeymapArray GrimMetaEngine::initKeymaps(const char *target) const {
	Common::String gameId = ConfMan.get("gameid", target);

#ifdef ENABLE_GRIM
	if (gameId == "grim") {
		return Grim::GrimEngine::initKeymapsGrim(target);
	}
#endif
#ifdef ENABLE_MONKEY4
	if (gameId == "monkey4") {
		return Grim::GrimEngine::initKeymapsEMI(target);
	}
#endif

	return AdvancedMetaEngine::initKeymaps(target);
}

Common::String GrimMetaEngine::getSavegameFile(int slot, const char *target) const {
	if (target && ConfMan.get("gameid", target) == "monkey4" &&
	    Common::parsePlatform(ConfMan.get("platform", target)) == Common::kPlatformPS2) {
		return slot == kSavegameFilePattern ? "efmi*.ps2" : CEPS2Saves::chooserFile(target, slot);
	}
	return MetaEngine::getSavegameFile(slot, target);
}

SaveStateList GrimMetaEngine::listSaves(const char *target) const {
	Common::String gameId = ConfMan.get("gameid", target);
	Common::String extra = ConfMan.get("extra", target);
	const bool isDemo = extra.contains("Demo");
	Common::Platform platform = Common::parsePlatform(ConfMan.get("platform", target));
	Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
	Common::StringArray filenames;
	Common::String pattern = gameId == "monkey4" ? "efmi###.gsv" : "grim##.gsv";
	SaveStateList saveList;
	char str[256];
	int32 strSize;

	if (platform == Common::kPlatformPS2)
		pattern = "efmi###.ps2";

	if (isDemo)
		return saveList; // Demos do not support saving

	if (gameId == "monkey4" && platform == Common::kPlatformPS2) {
		filenames = CEPS2Saves::files(target);
		for (uint index = 0; index < filenames.size(); ++index) {
			SaveStateDescriptor desc = CEPS2Saves::saveMetadata(this, filenames[index], index);
			if (!desc.getDescription().empty())
				saveList.push_back(desc);
		}
		return saveList;
	}

	filenames = platform == Common::kPlatformPS2 ? CEPS2Saves::files(target) : saveFileMan->listSavefiles(pattern);

	int chooserSlot = 0;
	for (Common::StringArray::const_iterator file = filenames.begin(); file != filenames.end(); ++file) {
		// PS2 chooser IDs distinguish sessions; descriptions retain slots 1-4.
		int slotNum = platform == Common::kPlatformPS2 ? chooserSlot++ : atoi(file->c_str() + 4);
		const int playerSlot = platform == Common::kPlatformPS2 ? CEPS2Saves::playerSlot(*file) : slotNum;

		if (slotNum >= 0 && playerSlot >= 0) {
			SaveGame *savedState = SaveGame::openForLoading(*file);
			if (savedState && savedState->isCompatible()) {
				const uint32 sectionSize = savedState->beginSection(platform == Common::kPlatformPS2 ? 'PS2S' : 'SUBS');
				strSize = savedState->readLESint32();
				if (strSize <= 0 || strSize > (int32)sizeof(str) || sectionSize < 4 || (uint32)strSize > sectionSize - 4) {
					delete savedState;
					continue;
				}
				savedState->read(str, strSize);
				str[strSize - 1] = '\0';
				savedState->endSection();
				Common::String description(str);
				if (platform == Common::kPlatformPS2)
					description = Common::String::format(_("Slot %d - %s").encode().c_str(), playerSlot + 1, str);
				saveList.push_back(SaveStateDescriptor(this, slotNum, description));
			}
			delete savedState;
		}
	}

	Common::sort(saveList.begin(), saveList.end(), SaveStateDescriptorSlotComparator());
	return saveList;
}

} // End of namespace Grim

#if PLUGIN_ENABLED_DYNAMIC(GRIM)
	REGISTER_PLUGIN_DYNAMIC(GRIM, PLUGIN_TYPE_ENGINE, Grim::GrimMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(GRIM, PLUGIN_TYPE_ENGINE, Grim::GrimMetaEngine);
#endif
