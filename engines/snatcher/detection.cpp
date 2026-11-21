#include "base/plugins.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "engines/metaengine.h"
#include "snatcher/resource.h"
#include "common/debug.h"

namespace {
const PlainGameDescriptor kSnatcherGames[] = {
	{"snatcher", "Snatcher"}, {nullptr, nullptr}
};
DetectedGame description() {
	DetectedGame game("snatcher", "snatcher", "Snatcher", Common::EN_ANY,
		Common::kPlatformSegaCD, "USA / experimental native engine");
	game.gameSupportLevel = kUnstableGame;
	game.preferredTarget = "snatcher-us-segacd";
	game.canBeAdded = true;
	return game;
}
class SnatcherMetaEngineDetection : public MetaEngineDetection {
public:
 const DebugChannelDef *getDebugChannels() const override {static const DebugChannelDef channels[]={{1,"resources","Native resource validation"},{2,"scenario","Native scenario parsing"},DEBUG_CHANNEL_END};return channels;}
	const char *getName() const override { return "snatcher"; }
	const char *getEngineName() const override { return "Snatcher native engine"; }
	const char *getOriginalCopyright() const override { return "Snatcher (C) Konami"; }
	PlainGameList getSupportedGames() const override { return PlainGameList(kSnatcherGames); }
	PlainGameDescriptor findGame(const char *id) const override {
		return id && Common::String(id) == "snatcher" ? kSnatcherGames[0] : PlainGameDescriptor::empty();
	}
	uint getMD5Bytes() const override { return 0; }
	int getGameVariantCount() const override { return 1; }
	void dumpDetectionEntries() const override {}
	DetectedGames detectGames(const Common::FSList &files, uint32 skipADFlags, bool skipIncomplete) override {
		(void)skipADFlags; (void)skipIncomplete;
		DetectedGames games;
		if (files.empty()) return games;
		Snatcher::GameResource resource; std::string error;
		const Common::FSNode root = files.begin()->getParent();
		if (Snatcher::openGame(root, resource, error)) {
			DetectedGame game = description(); game.path = root.getPath(); games.push_back(game);
		}
		return games;
	}
	Common::Error identifyGame(DetectedGame &game, const void **descriptor) override {
		if (descriptor) *descriptor = nullptr;
		if (!ConfMan.hasKey("path")) return Common::kNoGameDataFoundError;
		Snatcher::GameResource resource; std::string error;
		const Common::FSNode root(ConfMan.getPath("path"));
		if (!Snatcher::openGame(root, resource, error)) return Common::kNoGameDataFoundError;
		game = description(); game.path = root.getPath();
		return Common::kNoError;
	}
};
}
REGISTER_PLUGIN_STATIC(SNATCHER_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, SnatcherMetaEngineDetection);
