#ifndef SNATCHER_RESOURCE_H
#define SNATCHER_RESOURCE_H

#include "snatcher/disc.h"

namespace Common { class FSNode; }
namespace Snatcher {
struct DecensorDiagnostics {
 uint64_t configurations=0,applications=0,cacheReads=0;
 size_t entries=0,bytes=0;
};

// Read-only CHDv5 / development CUE/BIN adapter. Identity comes from contents.
class GameResource {
public:
	GameResource();
	~GameResource();
	bool open(const Common::FSNode &root, std::string &error);
	void close();
	bool isOpen() const;
	bool isChd() const;
	bool cdAudioFrameCount(unsigned track,uint32_t& frames,std::string& error) const;
	bool readCdAudioFrames(unsigned track,uint32_t firstFrame,uint32_t frameCount,std::vector<uint8_t>& pcm,std::string& error) const;
	const std::vector<DiscFile> &files() const;
	bool readFile(const std::string &name, std::vector<uint8_t> &bytes, std::string &error) const;
	// Exact physical sector allocation; logical file readers remain unchanged.
	bool readFileAllocation(const std::string &name, std::vector<uint8_t> &bytes, std::string &error) const;
	bool readBoot(std::vector<uint8_t> &bytes, std::string &error) const;
	bool readResource(uint16_t id, std::vector<uint8_t> &bytes, std::string &error) const;
	bool configureDecensor(const std::vector<uint8_t> &sidecar, std::string &error);
	bool decensorEnabled() const;
	DecensorDiagnostics decensorDiagnostics() const;
	bool readOriginalResource(uint16_t id, std::vector<uint8_t> &bytes, std::string &error) const;
	size_t resourceCount() const;
private:
	GameResource(const GameResource &);
	GameResource &operator=(const GameResource &);
	struct Impl;
	Impl *_impl;
};
bool openGame(const Common::FSNode &root, GameResource &resource, std::string &error);

} // namespace Snatcher
#endif
