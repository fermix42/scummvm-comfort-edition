/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_CHD_H
#define SNATCHER_CHD_H
#include "disc.h"
namespace Snatcher {
struct ChdTrack { unsigned number; bool audio; uint32_t frames,pregap,storedStart,projectedStart; };
// Single owning thread only. Compressed source must outlive this object.
// readAt projects the supported stored-pregap CD layout to unpadded 2352-byte
// sectors. It does not expose guest hardware or execute disc code.
class ChdSource : public ByteSource {
public:
 explicit ChdSource(const ByteSource& source);
 ~ChdSource();
 bool open(std::string& error);
 void close();
 uint64_t size() const override;
 bool readAt(uint64_t offset,void *data,size_t bytes) const override;
 bool readUnit(uint32_t unit,void *data,std::string& error) const;
 uint32_t unitCount() const;
 const std::vector<ChdTrack>& tracks() const;
 const std::string& lastError() const;
private:
 ChdSource(const ChdSource&);ChdSource&operator=(const ChdSource&);
 struct Impl;Impl *_impl;
};
}
#endif
