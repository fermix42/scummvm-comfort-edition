/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "chd.h"
#include "libchdr/include/libchdr/chd.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
namespace Snatcher {
namespace {
uint32_t be32(const uint8_t*p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
uint64_t be64(const uint8_t*p){return uint64_t(be32(p))<<32|be32(p+4);}
bool fail(std::string&e,const char*s){e=s;return false;}
struct Bridge {const ByteSource& source;uint64_t cursor;bool closed;Bridge(const ByteSource&s):source(s),cursor(0),closed(false){}};
uint64_t fileSize(void *p){return static_cast<Bridge*>(p)->source.size();}
size_t fileRead(void *out,size_t item,size_t count,void *p){Bridge&b=*static_cast<Bridge*>(p);if(b.closed||!item||count>std::numeric_limits<size_t>::max()/item)return 0;size_t bytes=count*item;if(b.cursor>b.source.size()||bytes>b.source.size()-b.cursor||!b.source.readAt(b.cursor,out,bytes))return 0;b.cursor+=bytes;return count;}
int fileClose(void *p){static_cast<Bridge*>(p)->closed=true;return 0;}
int fileSeek(void *p,int64_t offset,int whence){Bridge&b=*static_cast<Bridge*>(p);uint64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?b.cursor:whence==SEEK_END?b.source.size():UINT64_MAX;if(b.closed||base==UINT64_MAX)return -1;if(offset<0){uint64_t n=uint64_t(-(offset+1))+1;if(n>base)return -1;b.cursor=base-n;}else{if(uint64_t(offset)>b.source.size()||base>b.source.size()-uint64_t(offset))return -1;b.cursor=base+uint64_t(offset);}return 0;}
const core_file_callbacks callbacks={fileSize,fileRead,fileClose,fileSeek};
bool token(std::istringstream&in,const char *prefix,std::string&value){std::string s;if(!(in>>s)||s.compare(0,std::strlen(prefix),prefix))return false;value=s.substr(std::strlen(prefix));return !value.empty();}
bool number(std::istringstream&in,const char*prefix,uint32_t&v){std::string s;if(!token(in,prefix,s)||s.find_first_not_of("0123456789")!=std::string::npos)return false;std::istringstream n(s);uint64_t x;if(!(n>>x)||x>UINT32_MAX)return false;v=uint32_t(x);return true;}
}
struct ChdSource::Impl {
 Bridge bridge;chd_file *handle;uint32_t units,hunkBytes;uint64_t projectedBytes;
 std::vector<ChdTrack> tracks;mutable std::vector<uint8_t> cache;mutable uint32_t cachedHunk;mutable std::string error;
 Impl(const ByteSource&s):bridge(s),handle(nullptr),units(0),hunkBytes(0),projectedBytes(0),cachedHunk(UINT32_MAX){}
 bool hunk(uint32_t index) const {if(index==cachedHunk)return true;cachedHunk=UINT32_MAX;chd_error e=chd_read(handle,index,cache.data());if(e!=CHDERR_NONE){error=std::string("CHD hunk decode failed: ")+chd_error_string(e);return false;}cachedHunk=index;return true;}
};
ChdSource::ChdSource(const ByteSource&s):_impl(new Impl(s)){}
ChdSource::~ChdSource(){close();delete _impl;}
void ChdSource::close(){if(_impl->handle)chd_close(_impl->handle);_impl->handle=nullptr;_impl->cache.clear();_impl->tracks.clear();_impl->units=0;_impl->projectedBytes=0;_impl->cachedHunk=UINT32_MAX;_impl->bridge.closed=false;_impl->bridge.cursor=0;}
const std::vector<ChdTrack>& ChdSource::tracks()const{return _impl->tracks;}
const std::string& ChdSource::lastError()const{return _impl->error;}
uint64_t ChdSource::size()const{return _impl->projectedBytes;}
uint32_t ChdSource::unitCount()const{return _impl->units;}
bool ChdSource::open(std::string&e){
 close();e.clear();_impl->error.clear();const ByteSource&s=_impl->bridge.source;uint8_t raw[124];
 if(s.size()<124||!s.readAt(0,raw,sizeof(raw))||std::memcmp(raw,"MComprHD",8)||be32(raw+8)!=124||be32(raw+12)!=5)return fail(e,"Unsupported or truncated CHDv5 header");
 if(!be32(raw+16))return fail(e,"Uncompressed CHD outside validated decoder profile");
 for(unsigned i=16;i<32;i+=4){uint32_t codec=be32(raw+i);if(codec&&codec!=CHD_CODEC_CD_LZMA&&codec!=CHD_CODEC_CD_ZLIB&&codec!=CHD_CODEC_CD_FLAC)return fail(e,"CHD codec outside validated CD profile");}
 uint64_t logical=be64(raw+32),map=be64(raw+40),meta=be64(raw+48);uint32_t hunk=be32(raw+56),unit=be32(raw+60);
 if(!logical||logical>uint64_t(16)*1024*1024*1024||!hunk||hunk>512*1024||unit!=2448||hunk%2448||logical%2448||logical/2448>UINT32_MAX||(logical+hunk-1)/hunk>1000000||map<124||map>=s.size()||meta<124||meta>=s.size())return fail(e,"CHD CD geometry exceeds supported bounds");
 for(unsigned i=104;i<124;++i)if(raw[i])return fail(e,"Parent-dependent CHD unsupported");
 // Validate the metadata chain before library traversal: bounded lengths, cycle
 // rejection and a strict count avoid unbounded malformed-source work.
 std::set<uint64_t> visited;uint64_t pos=meta;
 while(pos){uint8_t m[16];if(visited.size()>=128||!visited.insert(pos).second||pos>s.size()||s.size()-pos<16||!s.readAt(pos,m,16))return fail(e,"Invalid CHD metadata chain");uint32_t len=be32(m+4)&0xffffff;if(len>4096||len>s.size()-pos-16)return fail(e,"CHD metadata length outside bounds");pos=be64(m+8);}
 chd_file *handle=nullptr;chd_error result=chd_open_core_file_callbacks(&callbacks,&_impl->bridge,CHD_OPEN_READ,nullptr,&handle);
 if(result!=CHDERR_NONE){_impl->bridge.closed=false;return fail(e,chd_error_string(result));}
 _impl->handle=handle;const chd_header *header=chd_get_header(handle);
 if(!header||header->hunkbytes!=hunk||header->logicalbytes!=logical||header->unitbytes!=2448){close();return fail(e,"CHD decoded header mismatch");}
 uint64_t stored=0,projected=0;
 for(unsigned i=0;i<100;++i){char text[512]={};uint32_t length=0,tag=0;uint8_t flags=0;result=chd_get_metadata(handle,CDROM_TRACK_METADATA2_TAG,i,text,sizeof(text)-1,&length,&tag,&flags);
  if(result==CHDERR_METADATA_NOT_FOUND)break;
  if(result!=CHDERR_NONE||length>=sizeof(text)){close();return fail(e,"Missing or invalid CHD track metadata");}
  text[length]=0;std::istringstream in(text);uint32_t n,frames,pregap,postgap;std::string type,sub,pgtype,pgsub,extra;
  if(!number(in,"TRACK:",n)||!token(in,"TYPE:",type)||!token(in,"SUBTYPE:",sub)||!number(in,"FRAMES:",frames)||!number(in,"PREGAP:",pregap)||!token(in,"PGTYPE:",pgtype)||!token(in,"PGSUB:",pgsub)||!number(in,"POSTGAP:",postgap)||(in>>extra)||n!=i+1||!frames||pregap>frames||postgap||sub!="NONE"||pgsub!="NONE"||(i==0&&(type!="MODE1_RAW"||pregap))||(i>0&&(type!="AUDIO"||pgtype!="VAUDIO"))){close();return fail(e,"Unsupported CHD track/pregap layout");}
  uint64_t padded=(uint64_t(frames)+3)&~uint64_t(3);if(stored+padded>logical/2448||projected+frames>UINT32_MAX){close();return fail(e,"CHD track exceeds logical units");}
  ChdTrack track={n,type=="AUDIO",frames,pregap,uint32_t(stored),uint32_t(projected)};_impl->tracks.push_back(track);stored+=padded;projected+=frames;
 }
 if(_impl->tracks.empty()||_impl->tracks.size()>99||stored!=logical/2448){close();return fail(e,"CHD track table does not cover stored units");}
 _impl->units=uint32_t(logical/2448);_impl->hunkBytes=hunk;_impl->projectedBytes=projected*2352;_impl->cache.resize(hunk);return true;
}
bool ChdSource::readUnit(uint32_t unit,void *data,std::string&e)const{
 e.clear();if(!_impl->handle||!data||unit>=_impl->units)return fail(e,"CHD unit outside source");uint64_t off=uint64_t(unit)*2448;uint32_t hn=uint32_t(off/_impl->hunkBytes);if(!_impl->hunk(hn)){e=_impl->error;return false;}std::memcpy(data,_impl->cache.data()+off%_impl->hunkBytes,2448);return true;
}
bool ChdSource::readAt(uint64_t offset,void *data,size_t bytes)const{
 _impl->error.clear();if(!_impl->handle||offset>size()||bytes>size()-offset||(!data&&bytes))return fail(_impl->error,"CHD projected range outside source");uint8_t *dest=static_cast<uint8_t*>(data);
 while(bytes){uint32_t sector=uint32_t(offset/2352);size_t within=size_t(offset%2352),count=std::min(bytes,2352-within);const ChdTrack *track=nullptr;
  for(size_t i=0;i<_impl->tracks.size();++i){const ChdTrack&t=_impl->tracks[i];if(sector>=t.projectedStart&&sector-t.projectedStart<t.frames){track=&t;break;}}
  if(!track)return fail(_impl->error,"CHD projected track unavailable");uint8_t frame[2448];std::string e;if(!readUnit(track->storedStart+sector-track->projectedStart,frame,e)){_impl->error=e;return false;}
  // CHD CD audio is big endian; the verified extracted BIN contract is
  // little-endian signed16 stereo. Map individual bytes for odd/partial reads.
  if(track->audio){for(size_t i=0;i<count;++i)dest[i]=frame[(within+i)^1];}
  else std::memcpy(dest,frame+within,count);
  dest+=count;offset+=count;bytes-=count;
 }return true;
}
}
