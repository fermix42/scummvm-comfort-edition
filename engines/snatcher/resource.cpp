#include "snatcher/resource.h"
#include "snatcher/chd.h"
#include "snatcher/decensor.h"
#include <map>
#include <algorithm>
#include "common/fs.h"
#include "common/stream.h"
#include "common/memstream.h"
#include "common/md5.h"
#include <limits>

namespace Snatcher {
namespace {
class FileSource : public ByteSource {
public:
 explicit FileSource(Common::SeekableReadStream *stream) { if(stream)_streams.push_back(stream);cacheSizes(); }
 explicit FileSource(const std::vector<Common::SeekableReadStream *>& streams):_streams(streams) {cacheSizes();}
 ~FileSource() override { for(auto* stream:_streams)delete stream; }
 // Open game assets are immutable. Refresh geometry only when reopening.
 uint64_t size() const override {return _size;}
 bool readAt(uint64_t offset,void *data,size_t bytes)const override {
  const uint64_t total=_size;if(!_valid||offset>total||bytes>total-offset)return false;
  byte* output=static_cast<byte*>(data);
  for(size_t i=0;i<_streams.size();++i){auto* stream=_streams[i];const uint64_t length=_lengths[i];if(offset>=length){offset-=length;continue;}
   size_t count=size_t(std::min<uint64_t>(bytes,length-offset));if(count>std::numeric_limits<uint32>::max())return false;
   stream->clearErr();if(!stream->seek(int64(offset))||stream->read(output,uint32(count))!=count||stream->err())return false;
   output+=count;bytes-=count;offset=0;if(!bytes)return true;
  }
  return bytes==0;
 }
private:
 std::vector<Common::SeekableReadStream *> _streams;
 std::vector<uint64_t> _lengths;uint64_t _size=0;bool _valid=true;
 void cacheSizes(){for(auto* stream:_streams){int64 length=stream?stream->size():-1;if(length<0||uint64_t(length)>UINT64_MAX-_size){_valid=false;_size=0;return;}_lengths.push_back(uint64_t(length));_size+=uint64_t(length);}}
};
bool digestMatches(const std::vector<uint8_t> &bytes, const char *md5) {
	if (bytes.empty() || bytes.size() > std::numeric_limits<uint32>::max()) return false;
	Common::MemoryReadStream stream(bytes.data(), uint32(bytes.size()));
	return Common::computeStreamMD5AsString(stream) == md5;
}
bool cueName(const Common::String &name) {
	return name.size() >= 4 && name.hasSuffixIgnoreCase(".cue");
}
}
struct GameResource::Impl {
	FileSource source;
	ChdSource chd;
	DiscReader disc;
	bool chdInput;
 CueInfo cue;
	std::vector<std::string> catalog;
	std::map<std::string,std::vector<uint8_t> > decensored;
 DecensorDiagnostics decensorStats;
	Impl(Common::SeekableReadStream *stream, uint32_t start,bool compressed=false) : source(stream),chd(source),disc(compressed?static_cast<const ByteSource&>(chd):static_cast<const ByteSource&>(source), kMode1Raw2352, start),chdInput(compressed) {}
 Impl(const std::vector<Common::SeekableReadStream *>& streams,const CueInfo& info):source(streams),chd(source),disc(source,kMode1Raw2352,info.dataTrackStart),chdInput(false),cue(info) {}
};
GameResource::GameResource() : _impl(0) {}
GameResource::~GameResource() { close(); }
void GameResource::close() { delete _impl; _impl = 0; }
bool GameResource::isOpen() const { return _impl != 0; }
bool GameResource::isChd() const {return _impl&&_impl->chdInput;}
bool GameResource::cdAudioFrameCount(unsigned track,uint32_t& frames,std::string& e)const {
 frames=0;e.clear();if(!_impl){e="CD audio disc is not open";return false;}
 if(!_impl->chdInput)return cueAudioFrameCount(_impl->cue,_impl->source.size(),track,frames,e);
 const std::vector<ChdTrack>& tracks=_impl->chd.tracks();if(!track||track>tracks.size()||!tracks[track-1].audio){e="Invalid CD audio track identity";return false;}
 const ChdTrack& t=tracks[track-1];if(t.pregap>t.frames){e="CD audio pregap exceeds track bounds";return false;}frames=t.frames-t.pregap;return true;
}
bool GameResource::readCdAudioFrames(unsigned track,uint32_t first,uint32_t count,std::vector<uint8_t>& out,std::string& e)const {
 out.clear();e.clear();if(!_impl){e="CD audio disc is not open";return false;}
 if(!_impl->chdInput)return readCueAudioFrames(_impl->source,_impl->cue,track,first,count,out,e);
 const std::vector<ChdTrack>& tracks=_impl->chd.tracks();if(!track||track>tracks.size()||!tracks[track-1].audio||!count||count>4096){e="Invalid or oversized CD audio request";return false;}
 const ChdTrack&t=tracks[track-1];uint32_t playable=t.frames-t.pregap;if(first>playable||count>playable-first){e="CD audio request outside playable track";return false;}
 out.resize(size_t(count)*2352);if(!_impl->chd.readAt(uint64_t(t.projectedStart+t.pregap+first)*2352,out.data(),out.size())){out.clear();e=_impl->chd.lastError();return false;}return true;
}
const std::vector<DiscFile> &GameResource::files() const {
	static const std::vector<DiscFile> empty;
	return _impl ? _impl->disc.files() : empty;
}
bool GameResource::readFileAllocation(const std::string& name,std::vector<uint8_t>& bytes,std::string& error)const{
 bytes.clear();error.clear();if(!_impl){error="game disc is not open";return false;}return _impl->disc.readFileAllocation(name,bytes,error);
}
bool GameResource::readFile(const std::string &name, std::vector<uint8_t> &bytes, std::string &error) const {
	bytes.clear(); error.clear();
	if (!_impl) { error = "game disc is not open"; return false; }
	auto found=_impl->decensored.find(name);if(found!=_impl->decensored.end()){bytes=found->second;++_impl->decensorStats.cacheReads;return true;}
	return _impl->disc.readFile(name, bytes, error);
}
bool GameResource::configureDecensor(const std::vector<uint8_t>& sidecar,std::string& error){
 if(!_impl){error="game disc is not open";return false;}std::vector<DecensorFile> files;if(!parseDecensor(sidecar,files,error))return false;
 std::map<std::string,std::vector<uint8_t> > patched;
 for(const auto& f:files){std::vector<uint8_t> original,target;if(!_impl->disc.readFile(f.name,original,error))return false;
  if(!digestMatches(original,f.sourceMd5.c_str())){error="decensor source identity mismatch: "+f.name;return false;}
  if(!applyDecensor(f,original,target,error))return false;
  if(!digestMatches(target,f.targetMd5.c_str())){error="decensor target identity mismatch: "+f.name;return false;}patched[f.name].swap(target);
 }
 _impl->decensored.swap(patched);++_impl->decensorStats.configurations;_impl->decensorStats.applications+=files.size();return true;
}
bool GameResource::decensorEnabled()const{return _impl&&!_impl->decensored.empty();}
DecensorDiagnostics GameResource::decensorDiagnostics()const{
 DecensorDiagnostics result;if(!_impl)return result;result=_impl->decensorStats;
 result.entries=_impl->decensored.size();for(const auto& file:_impl->decensored)result.bytes+=file.second.size();return result;
}
bool GameResource::readOriginalResource(uint16_t id,std::vector<uint8_t>& bytes,std::string& error)const{
 bytes.clear();if(!_impl||id>=_impl->catalog.size()){error="original resource identity outside catalog";return false;}return _impl->disc.readFile(_impl->catalog[id],bytes,error);
}
bool GameResource::readBoot(std::vector<uint8_t> &bytes, std::string &error) const {
	bytes.clear(); error.clear();
	if (!_impl) { error = "game disc is not open"; return false; }
	return _impl->disc.readBoot(bytes, error);
}
size_t GameResource::resourceCount() const {return _impl?_impl->catalog.size():0;}
bool GameResource::readResource(uint16_t id,std::vector<uint8_t>& bytes,std::string& error) const {
 bytes.clear();if(!_impl||id>=_impl->catalog.size()){error="resource identity outside verified catalog";return false;}return readFile(_impl->catalog[id],bytes,error);
}
bool GameResource::open(const Common::FSNode &root, std::string &error) {
	close(); error.clear();
	Common::FSList children;
	if (!root.isDirectory() || !root.getChildren(children, Common::FSNode::kListFilesOnly)) {
		error = "game path is not a readable directory"; return false;
	}
	Impl *matched = 0;
	for (Common::FSList::const_iterator it = children.begin(); it != children.end(); ++it) {
		const bool compressed=it->getName().hasSuffixIgnoreCase(".chd");
		if (!compressed&&!cueName(it->getName())) continue;
		Impl *candidate=nullptr;std::string detail;
		if(compressed){
		 Common::SeekableReadStream *stream=it->createReadStream();if(!stream)continue;
		 // Exact known release size is a cheap candidate filter, not identity.
		 if(stream->size()!=335210238){delete stream;continue;}
		 candidate=new Impl(stream,0,true);if(!candidate->chd.open(detail)){delete candidate;continue;}
		}else{
		Common::SeekableReadStream *cue = it->createReadStream();
		if (!cue) continue;
		const int64 length = cue->size();
		if (length <= 0 || length > 1024 * 1024) { delete cue; continue; }
		std::string text(size_t(length), '\0');
		const bool read = cue->read(&text[0], uint32(length)) == uint32(length) && !cue->err();
		delete cue;
		CueInfo info;
		if (!read || !parseCue(text, info, detail)) continue;
        // Retain one stream per local BIN; expose a virtual concatenation.
        // No original files are rewritten or loaded wholesale into memory.
        std::vector<Common::SeekableReadStream *> binaries;std::vector<uint64_t> sizes;
        bool complete=true;
        for(const std::string& name:info.binaryFiles){
         Common::SeekableReadStream* binary=root.getChild(name.c_str()).createReadStream();
         if(!binary){complete=false;break;}
         binaries.push_back(binary);if(binary->size()<0){complete=false;break;}sizes.push_back(uint64_t(binary->size()));
        }
        if(!complete||!resolveCueFiles(info,sizes,detail)){for(auto* binary:binaries)delete binary;continue;}
        candidate=new Impl(binaries,info);
		}
		std::vector<uint8_t> boot, subcode;
		const bool valid = candidate->disc.open(detail) && candidate->disc.readBoot(boot, detail) &&
			boot.size() == 32768 && digestMatches(boot, "3cf11682040bcc8722557203c99ee127") &&
			candidate->disc.readFile("SUBCODE.BIN", subcode, detail) &&
			digestMatches(subcode, "52b4dd7c99c511bc4f62249708ef2ea3");
		if (!valid) { delete candidate; continue; }
		// Recover resource identities from immutable original data, not a bundled
		// executable table. The verified format has 97 bounded 16-byte slots.
		bool catalogValid=subcode.size()>=0x432+97*16;
		for(unsigned id=0;catalogValid&&id<97;++id){
		 const uint8_t *slot=&subcode[0x432+id*16];size_t len=0;while(len<16&&slot[len])++len;
		 if(len==16){catalogValid=false;break;}while(len&&slot[len-1]==' ')--len;
		 std::string name(reinterpret_cast<const char *>(slot),len);bool exists=false;
		 for(size_t f=0;f<candidate->disc.files().size();++f)if(candidate->disc.files()[f].name==name)exists=true;
		 if(!exists){catalogValid=false;break;}candidate->catalog.push_back(name);
		}
		if(!catalogValid){delete candidate;continue;}
		if (matched) {
			delete candidate; delete matched;
			error = "multiple matching Snatcher disc sources; use a directory containing one disc"; return false;
		}
		matched = candidate;
	}
	if (!matched) { error = "no supported Snatcher USA CHD or single-BIN CUE disc found"; return false; }
	_impl = matched; return true;
}
bool openGame(const Common::FSNode &root, GameResource &resource, std::string &error) {
	return resource.open(root, error);
}
} // namespace Snatcher
