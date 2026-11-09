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

/*
 * Meta engine: creates the GookaEngine instance for a detected game and
 * lists the savegames (ScummVM's extended save format, target.000 ...).
 */

#include "engines/advancedDetector.h"
#include "common/translation.h"

#include "gooka/gooka.h"
#include "gooka/detection.h"

/** Creates the engine for a detected game; saves use the default extended format helpers. */
class GookaMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "gooka";
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override;
	bool hasFeature(MetaEngineFeature f) const override;
	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		static const ADExtraGuiOptionsMap options[] = {
			{ GUIO_GAMEOPTIONS1, {
				_s("Scale videos to full height"),
				_s("Enlarge AVI videos to fit the screen while preserving their aspect ratio."),
				"gooka_scale_videos", false, 0, 0
			} },
			AD_EXTRA_GUI_OPTIONS_TERMINATOR
		};
		return options;
	}
};

// Listing, deleting, thumbnails, dates and play time come from the extended
// save header; a save can be picked in the launcher.
bool GookaMetaEngine::hasFeature(MetaEngineFeature f) const {
	return checkExtendedSaves(f) || f == kSupportsLoadingDuringStartup;
}

// Creates the engine.
Common::Error GookaMetaEngine::createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const {
	*engine = new Gooka::GookaEngine(syst, desc);
	return Common::kNoError;
}

#if PLUGIN_ENABLED_DYNAMIC(GOOKA)
	REGISTER_PLUGIN_DYNAMIC(GOOKA, PLUGIN_TYPE_ENGINE, GookaMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(GOOKA, PLUGIN_TYPE_ENGINE, GookaMetaEngine);
#endif
