#ifndef SNATCHER_DISC_H
#define SNATCHER_DISC_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace Snatcher {
enum SectorLayout { kMode1Raw2352, kIso2048 };
class ByteSource { public: virtual ~ByteSource() {} virtual uint64_t size() const=0; virtual bool readAt(uint64_t offset,void *data,size_t bytes) const=0; };
struct DiscFile { std::string name; uint32_t extent,size; };
struct CueTrack {
 unsigned number,fileIndex;bool audio,hasIndex0;uint32_t index0,index1;
 CueTrack():number(0),fileIndex(0),audio(false),hasIndex0(false),index0(0),index1(0){}
};
struct CueInfo { std::string binaryFile; std::vector<std::string> binaryFiles; bool filesResolved; uint32_t dataTrackStart,lastIndex;std::vector<CueTrack> tracks; CueInfo():filesResolved(true),dataTrackStart(0),lastIndex(0){} };
bool parseCue(const std::string &text,CueInfo &info,std::string &error);
// Resolve file-relative indexes onto a virtual concatenation of local BINs.
bool resolveCueFiles(CueInfo& cue,const std::vector<uint64_t>& sizes,std::string& error);
// INDEX00 belongs to the next
// track's stored pregap; no PREGAP/POSTGAP silence is synthesized.
bool validateCueTracks(const CueInfo& cue,uint64_t binaryBytes,std::string& error);
bool cueAudioFrameCount(const CueInfo& cue,uint64_t binaryBytes,unsigned track,uint32_t& frames,std::string& error);
bool readCueAudioFrames(const ByteSource& source,const CueInfo& cue,unsigned track,uint32_t first,uint32_t count,std::vector<uint8_t>& output,std::string& error);
class DiscReader {
public:
 DiscReader(const ByteSource &source,SectorLayout layout,uint32_t startSector=0);
 bool open(std::string &error);
 const std::vector<DiscFile> &files() const { return _files; }
 bool readFile(const std::string &name,std::vector<uint8_t> &out,std::string &error) const;
	// Exact physical sector allocation; logical file readers remain unchanged.
	bool readFileAllocation(const std::string &name,std::vector<uint8_t> &out,std::string &error) const;
 bool readBoot(std::vector<uint8_t> &out,std::string &error) const;
private:
 const ByteSource &_source; SectorLayout _layout; uint32_t _start,_volume; bool _open; std::vector<DiscFile> _files;
 bool readLogical(uint64_t offset,void *data,size_t bytes) const;
 bool readExtent(uint32_t extent,uint32_t size,std::vector<uint8_t> &out,std::string &error) const;
};
}
#endif
