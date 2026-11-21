/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_NATIVE_ARCHIVE_H
#define SNATCHER_NATIVE_ARCHIVE_H
#include <cstddef>
#include <cstdint>
#include <vector>
#include <limits>
#include <string>
#include <deque>
namespace Snatcher {
// Bounded big-endian semantic owner fields, never object-memory images.
struct NativeArchive {
 bool reading,ok;size_t cursor,limit;std::vector<uint8_t> bytes;
 explicit NativeArchive(size_t maximum=4*1024*1024):reading(false),ok(maximum<=64*1024*1024),cursor(0),limit(maximum){}
 explicit NativeArchive(const std::vector<uint8_t>& b,size_t maximum=4*1024*1024):reading(true),ok(maximum<=64*1024*1024&&b.size()<=maximum),cursor(0),limit(maximum),bytes(b.size()<=maximum&&maximum<=64*1024*1024?b:std::vector<uint8_t>()){}
 template<class T> void field(T& value){if(!ok)return;uint64_t n=reading?0:uint64_t(value);if(reading){n=0;if(cursor>bytes.size()||sizeof(T)>bytes.size()-cursor){ok=false;return;}for(size_t i=0;i<sizeof(T);++i)n=(n<<8)|bytes[cursor++];value=T(n);}else{for(size_t i=sizeof(T);i;--i)bytes.push_back(uint8_t(n>>((i-1)*8)));if(bytes.size()>limit)ok=false;}}
 void flag(bool& v){uint8_t b=reading?0:(v?1:0);field(b);if(!ok)return;if(b>1){ok=false;return;}if(reading)v=b!=0;}
 template<class T>void number(T& v){uint32_t n=reading?0:uint32_t(v);if(!reading&&uint64_t(v)>UINT32_MAX){ok=false;return;}field(n);if(!ok)return;if(reading){if(uint64_t(n)>uint64_t(std::numeric_limits<T>::max())){ok=false;return;}v=T(n);}}
 template<class T>void vector(std::vector<T>& values,size_t limit){if(!ok)return;uint32_t count=uint32_t(values.size());field(count);if(count>limit){ok=false;return;}if(reading){if(count>(bytes.size()-cursor)/sizeof(T)){ok=false;return;}values.resize(count);}for(size_t i=0;i<count;++i)field(values[i]);}
 void flags(std::vector<bool>& values,size_t limit){if(!ok)return;uint32_t count=uint32_t(values.size());field(count);if(count>limit){ok=false;return;}if(reading){if(count>bytes.size()-cursor){ok=false;return;}values.resize(count);}for(size_t i=0;i<count;++i){bool v=values[i];flag(v);if(reading)values[i]=v;}}
 bool finished()const{return ok&&(!reading||cursor==bytes.size());}
 void text(std::string& s,size_t limit=1024){std::vector<uint8_t> b(s.begin(),s.end());vector(b,limit);if(reading)s.assign(b.begin(),b.end());}
 template<class T>void queue(std::deque<T>& q,size_t limit){std::vector<T> v(q.begin(),q.end());vector(v,limit);if(reading)q.assign(v.begin(),v.end());}
 template<class C,class F>void objects(C& c,size_t limit,F visit){uint32_t n=uint32_t(c.size());field(n);if(n>limit){ok=false;return;}if(reading){if(n>bytes.size()-cursor){ok=false;return;}c.resize(n);}for(auto& v:c)visit(v);}
};
}
#endif
