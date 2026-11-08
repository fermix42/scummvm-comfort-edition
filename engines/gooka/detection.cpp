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
 * Game detection: identifies Gooka by MD5 of GAME.EXE and 0.BIN (see detection_tables.h).
 */

#include "base/plugins.h"

#include "engines/advancedDetector.h"

#include "gooka/detection.h"
#include "gooka/detection_tables.h"

/** Game IDs handled by this engine. */
static const PlainGameDescriptor gookaGames[] = {
	{ "gooka", "Gooka" },
	{ nullptr, nullptr }
};

/** Debug channels selectable with --debugflags. */
static const DebugChannelDef debugFlagList[] = {
	{ Gooka::kDebugResource, "resource", "Resource loading" },
	{ Gooka::kDebugGraphics, "graphics", "Graphics" },
	{ Gooka::kDebugScript, "script", "Script execution" },
	{ Gooka::kDebugSound, "sound", "Sound" },
	DEBUG_CHANNEL_END
};

/** Detection plugin: matches the files listed in detection_tables.h. */
class GookaMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	GookaMetaEngineDetection() : AdvancedMetaEngineDetection(Gooka::gameDescriptions, gookaGames) {
	}

	const char *getName() const override {
		return "gooka";
	}

	const char *getEngineName() const override {
		return "Gooka";
	}

	const char *getOriginalCopyright() const override {
		return "Gooka (C) 1997 JRC Interactive";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

REGISTER_PLUGIN_STATIC(GOOKA_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, GookaMetaEngineDetection);
