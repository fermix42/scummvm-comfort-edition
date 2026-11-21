/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_AUDIO_RATE_H
#define SNATCHER_AUDIO_RATE_H
#include <cstdint>
#include <string>
#include <memory>
#include "audio_feed.h"
namespace Snatcher {
// Semantic sample timing supplied by a verified caller, independent of a chip.
// Positive rates from 1 to 384000 samples/second are accepted. No gain, polarity,
// loop or audible-playback qualification is implied by timing conversion.
struct RationalAudioRate {
 uint32_t numerator,denominator;
 RationalAudioRate(uint32_t n=0,uint32_t d=1):numerator(n),denominator(d){}
};
bool normalizeAudioRate(RationalAudioRate& rate,std::string& error);
bool nearestMixerRate(const RationalAudioRate& rate,uint32_t& rounded,std::string& error);
// Round up so the final source sample is retained. Zero input produces zero.
// Overflow is rejected transactionally; no floating-point duration drift.
bool resampledFrameCount(uint64_t sourceFrames,const RationalAudioRate& source,uint32_t targetRate,uint64_t& outputFrames,std::string& error);
bool audioDurationNanoseconds(uint64_t sourceFrames,const RationalAudioRate& source,uint64_t& nanoseconds,std::string& error);
// Exact source sample-age transport at an integer mixer rate. The linear
// interpolation and zero initial history retain the existing CE convention;
// this does not qualify the original chip/reference output reconstruction.
class RationalPcmTransport:public NativeAudioFeed {
public:
 bool archive(class NativeArchive&,std::string& error);
 RationalPcmTransport();
 bool start(const std::shared_ptr<NativeAudioFeed>& source,const RationalAudioRate& rate,uint32_t outputRate,std::string& error);
 bool restore(const std::shared_ptr<NativeAudioFeed>& source,const RationalAudioRate& rate,uint32_t outputRate,class NativeArchive& archive,std::string& error);
 uint32_t sourceRate()const override{return _outputRate;}
 size_t readFrames(int16_t* stereo,size_t frames)override;
 bool ended()const override;
 std::string error()const override;
 uint64_t renderedFrames()const{return _rendered;}
 uint64_t consumedFrames()const{return _consumed;}
 uint64_t sourcePosition()const{return _position;}
 uint64_t sourceBoundary()const{return _boundary;}
private:
 friend class EngineStateCodec;
 bool bind(const std::shared_ptr<NativeAudioFeed>& source,const RationalAudioRate& rate,uint32_t outputRate,bool continuation,std::string& error);
 std::shared_ptr<NativeAudioFeed> _source;uint32_t _outputRate,_step;
 uint64_t _position,_boundary,_rendered,_consumed;int16_t _last[2],_current[2];
};
}
#endif
