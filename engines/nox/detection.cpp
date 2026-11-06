/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "base/plugins.h"
#include "engines/advancedDetector.h"
#include "common/file.h"
#include "nox/resources.h"

namespace Nox {
static const PlainGameDescriptor games[] = {{"nox", "Nox Archaist"}, {nullptr, nullptr}};
// Resource signatures, rather than the mutable save sectors or filename,
// establish v119 ownership. Do not accept a filename-only table match.
static const ADGameDescription descriptions[] = {AD_TABLE_END_MARKER};
static const ADGameDescription baseV119 = {
	"nox", "v119", AD_ENTRY1(nullptr, nullptr), Common::EN_ANY,
	Common::kPlatformApple2, ADGF_NO_FLAGS, GUIO2(GUIO_NOMIDI,GUIO_GAMEOPTIONS1)
};

class NoxMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	NoxMetaEngineDetection() : AdvancedMetaEngineDetection(descriptions, games) {}
	const char *getName() const override { return "nox"; }
	const char *getEngineName() const override { return "Nox Archaist"; }
	const char *getOriginalCopyright() const override { return "Nox Archaist (C) 6502 Workshop"; }
	ADDetectedGame fallbackDetect(const FileMap &, const Common::FSList &fslist, ADDetectedGameExtraInfo **) const override {
		uint matches = 0;
		for (Common::FSList::const_iterator it = fslist.begin(); it != fslist.end(); ++it) {
			Common::String name = it->getName();
			name.toLowercase();
			if (it->isDirectory() || !name.hasSuffix(".hdv"))
				continue;
			Common::File stream;
			HdvArchive archive;
			if (stream.open(*it) && archive.open(stream) && archive.isBaseV119())
				++matches;
		}
		// An expansion may coexist with the base. Multiple base images require
		// the user to choose a game directory, never silently pick one.
		return matches == 1 ? ADDetectedGame(&baseV119) : ADDetectedGame();
	}
};
} // namespace Nox
REGISTER_PLUGIN_STATIC(NOX_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, Nox::NoxMetaEngineDetection);
