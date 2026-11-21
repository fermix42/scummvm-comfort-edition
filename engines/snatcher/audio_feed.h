/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_AUDIO_FEED_H
#define SNATCHER_AUDIO_FEED_H
#include <cstddef>
#include <cstdint>
#include <string>
namespace Snatcher {
// A native source-owned stereo S16 stream. Audio callbacks consume only native
// prefetched samples. They never dispatch gameplay or interpret game code.
class NativeAudioFeed {
public:
 virtual ~NativeAudioFeed(){}
 virtual bool jobIdentity(const void*,unsigned&,uint64_t&)const{return false;}
 virtual uint32_t sourceRate()const=0;
 virtual size_t readFrames(int16_t* stereo,size_t frames)=0;
 // Only an actual source end marker and consumed queued samples establish end.
 virtual bool ended()const=0;
 // A failed/unknown source is not represented by fabricated end or silence.
 virtual std::string error()const=0;
};
}
#endif
