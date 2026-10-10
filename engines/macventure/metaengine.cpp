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

#include "base/plugins.h"

#include "engines/advancedDetector.h"
#include "common/config-manager.h"
#include "common/system.h"
#include "common/translation.h"

#include "macventure/macventure.h"

namespace MacVenture {

static const ADExtraGuiOptionsMap optionsList[] = {
	{
		GAMEOPTION_DEJA_VU_FREEZE_POLICE_TIMER,
		{
			_s("Freeze police timer"),
			_s("Prevent the police arrest timer from reaching the game-over state in Deja Vu"),
			"deja_vu_freeze_police_timer",
			false,
			0,
			0,
			kExtraGuiOptionFlagCheat
		}
	},
	{
		GAMEOPTION_DEJA_VU_RIG_SLOT_MACHINE,
		{
			_s("Rig slot machine"),
			_s("Force Deja Vu's slot machine roll to take the winning branch"),
			"deja_vu_rig_slot_machine",
			false,
			0,
			0,
			kExtraGuiOptionFlagCheat
		}
	},
	{
		GAMEOPTION_DEJA_VU_UNLIMITED_AMMO,
		{
			_s("Unlimited ammo"),
			_s("Prevent Deja Vu's gunshots from consuming loaded ammunition"),
			"deja_vu_unlimited_ammo",
			false,
			0,
			0,
			kExtraGuiOptionFlagCheat
		}
	},
	{
		GAMEOPTION_DEJA_VU_NO_ALLIGATORS,
		{
			_s("No alligators"),
			_s("Prevent Deja Vu's random sewer alligator encounter from triggering"),
			"deja_vu_no_alligators",
			false,
			0,
			0,
			kExtraGuiOptionFlagCheat
		}
	},
	{
		GAMEOPTION_DEJA_VU_UNLIMITED_INVENTORY,
		{
			_s("Unlimited inventory"),
			_s("Allow Deja Vu's trench coat inventory to exceed its normal capacity"),
			"deja_vu_unlimited_inventory",
			false,
			0,
			0,
			kExtraGuiOptionFlagCheat
		}
	},
	{
		GAMEOPTION_DEJA_VU_MUGGER_WONT_KILL,
		{
			_s("Mugger won't kill"),
			_s("Keep Deja Vu's mugger encounter active, but prevent repeated punches from reaching the death branch"),
			"deja_vu_mugger_wont_kill",
			false,
			0,
			0,
			kExtraGuiOptionFlagCheat
		}
	},
	{
		GAMEOPTION_PATCH_DEJA_VU_SPEECH_CANCEL,
		{
			_s("Patch: Speech cancel"),
			_s("Prevent canceled Deja Vu speech dialogs from being treated as spoken input"),
			"patch_deja_vu_speech_cancel",
			true,
			0,
			0,
			0
		}
	},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

const char *MacVentureEngine::getGameFileName() const {
	return _gameDescription->filesDescriptions[0].fileName;
}

} // End of namespace MacVenture


namespace MacVenture {

class MacVentureMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "macventure";
	}

	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		return MacVenture::optionsList;
	}

	bool isAdvancedExtraGuiOptionAllowedForTarget(const Common::String &target, const ADExtraGuiOptionsMap &entry) const override;

protected:
	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override;
	bool hasFeature(MetaEngineFeature f) const override;
	int getMaximumSaveSlot() const override;
};

bool MacVentureMetaEngine::isAdvancedExtraGuiOptionAllowedForTarget(const Common::String &target, const ADExtraGuiOptionsMap &entry) const {
	const bool isDejaVuOption =
		!strcmp(entry.guioFlag, GAMEOPTION_DEJA_VU_FREEZE_POLICE_TIMER) ||
		!strcmp(entry.guioFlag, GAMEOPTION_DEJA_VU_RIG_SLOT_MACHINE) ||
		!strcmp(entry.guioFlag, GAMEOPTION_DEJA_VU_UNLIMITED_AMMO) ||
		!strcmp(entry.guioFlag, GAMEOPTION_DEJA_VU_NO_ALLIGATORS) ||
		!strcmp(entry.guioFlag, GAMEOPTION_DEJA_VU_UNLIMITED_INVENTORY) ||
		!strcmp(entry.guioFlag, GAMEOPTION_DEJA_VU_MUGGER_WONT_KILL) ||
		!strcmp(entry.guioFlag, GAMEOPTION_PATCH_DEJA_VU_SPEECH_CANCEL);

	if (!isDejaVuOption)
		return true;

	return !ConfMan.hasKey("gameid", target) || ConfMan.get("gameid", target) == "deja_vu";
}

bool MacVentureMetaEngine::hasFeature(MetaEngineFeature f) const {
	return
		(f == kSupportsListSaves) ||
		(f == kSupportsLoadingDuringStartup) ||
		(f == kSupportsDeleteSave) ||
		(f == kSavesSupportMetaInfo) ||
		(f == kSavesSupportThumbnail) ||
		(f == kSavesSupportCreationDate) ||
		(f == kSimpleSavesNames) ||
		(f == kSavesSupportPlayTime) ||
		(f == kSavesUseExtendedFormat);
}

void MacVentureEngine::initializePath(const Common::FSNode &gamePath) {
	Engine::initializePath(gamePath);
	_gamePath = gamePath;
}

bool MacVentureEngine::hasFeature(EngineFeature f) const {
	return
		(f == kSupportsReturnToLauncher) ||
		(f == kSupportsLoadingDuringRuntime) ||
		(f == kSupportsSavingDuringRuntime);
}

int MacVentureMetaEngine::getMaximumSaveSlot() const { return 999; }

Common::Error MacVentureMetaEngine::createInstance(OSystem *syst, Engine **engine, const ADGameDescription *game) const {
	*engine = new MacVenture::MacVentureEngine(syst, game);
	return Common::kNoError;
}

} // End of namespace MacVenture

#if PLUGIN_ENABLED_DYNAMIC(MACVENTURE)
	REGISTER_PLUGIN_DYNAMIC(MACVENTURE, PLUGIN_TYPE_ENGINE, MacVenture::MacVentureMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(MACVENTURE, PLUGIN_TYPE_ENGINE, MacVenture::MacVentureMetaEngine);
#endif
