/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_DECENSOR_H
#define SNATCHER_DECENSOR_H
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
namespace Snatcher {
struct DecensorSpan { uint32_t offset; std::vector<uint8_t> bytes; };
struct DecensorFile {
 std::string name,sourceMd5,targetMd5;
 uint32_t sourceSize,targetSize;
 std::vector<DecensorSpan> spans;
};
// Data-only sidecar. No executable patches, selectors, or arbitrary resource names.
inline bool parseDecensor(const std::vector<uint8_t>& data,std::vector<DecensorFile>& files,std::string& error){
 files.clear();error.clear();size_t at=0;bool valid=true;
 auto u32=[&](){if(at>data.size()||data.size()-at<4){valid=false;return uint32_t(0);}uint32_t n=0;for(unsigned j=0;j<4;++j)n|=uint32_t(data[at++])<<(8*j);return n;};
 auto text=[&](size_t n){if(at>data.size()||n>data.size()-at){valid=false;return std::string();}std::string s(data.begin()+at,data.begin()+at+n);at+=n;return s;};
 if(data.size()>1024*1024||text(8)!="SNDC0001"){error="invalid decensor sidecar header";return false;}
 unsigned count=u32();if(count!=5){error="decensor sidecar must contain exactly five approved resources";return false;}
 std::set<std::string> names;std::vector<DecensorFile> parsed;
 for(unsigned i=0;i<count&&valid;++i){
  DecensorFile f;f.name=text(u32());f.sourceSize=u32();f.targetSize=u32();f.sourceMd5=text(32);f.targetMd5=text(32);unsigned spans=u32();
  if((f.name!="DATA_F0.BIN"&&f.name!="DATA_H09.BIN"&&f.name!="DATA_G0.BIN"&&f.name!="DATA_P0.BIN"&&f.name!="SP25.BIN")||!names.insert(f.name).second||!f.sourceSize||f.sourceSize>262144||!f.targetSize||f.targetSize>262144||!spans||spans>32768){valid=false;break;}
  for(const std::string* s:{&f.sourceMd5,&f.targetMd5})if(s->size()!=32||s->find_first_not_of("0123456789abcdef")!=std::string::npos)valid=false;
  uint32_t end=0;
  for(unsigned j=0;j<spans&&valid;++j){DecensorSpan span;span.offset=u32();unsigned n=u32();if(!valid||!n||span.offset<end||span.offset>f.targetSize||n>f.targetSize-span.offset||n>data.size()-at){valid=false;break;}span.bytes.assign(data.begin()+at,data.begin()+at+n);at+=n;end=span.offset+n;f.spans.push_back(span);}
  parsed.push_back(f);
 }
 if(!valid||at!=data.size()||parsed.size()!=5){error="invalid decensor resource spans";return false;}
 files.swap(parsed);return true;
}
inline bool applyDecensor(const DecensorFile& file,const std::vector<uint8_t>& source,std::vector<uint8_t>& target,std::string& error){
 error.clear();if(source.size()!=file.sourceSize){error="decensor source size mismatch";return false;}
 std::vector<uint8_t> next=source;next.resize(file.targetSize,0);
 for(const auto& span:file.spans){if(span.offset>next.size()||span.bytes.size()>next.size()-span.offset){error="decensor span outside resource";return false;}std::copy(span.bytes.begin(),span.bytes.end(),next.begin()+span.offset);}
 target.swap(next);return true;
}
}
#endif
