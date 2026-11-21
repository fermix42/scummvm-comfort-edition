/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_AUDIO_OUTPUT_H
#define SNATCHER_AUDIO_OUTPUT_H
#include <cstdint>
#include <string>
#include <vector>
namespace Audio {class Mixer;}
namespace Snatcher {
// Semantic output-domain state, independent of chip/object memory layouts.
struct NativeAudioOutputState {
 uint32_t version,rate;uint64_t frames;int64_t inputQ16[2],outputQ16[2];
 NativeAudioOutputState():version(1),rate(44100),frames(0){inputQ16[0]=inputQ16[1]=outputQ16[0]=outputQ16[1]=0;}
};
// Final reference measurement domain: one fixed high-pass coupling of the
// summed sources, followed by S16 saturation. No per-song/channel adjustment.
class NativeAudioOutput {
public:
 NativeAudioOutput(){}
 void reset(){_state=NativeAudioOutputState();}
 NativeAudioOutputState snapshot()const{return _state;}
 bool restore(const NativeAudioOutputState&,std::string& error);
 bool process(const int32_t* stereo,int16_t* output,uint32_t frames,std::string& error);
private:NativeAudioOutputState _state;
};
#ifndef SNATCHER_AUDIO_CORE_ONLY
// A private CE mixer collects only this engine's sources. The one outer stream
// is truly audio-only: no timing callbacks, scenarios, machine state or devices.
class CeAudioOutputOwner {
public:
 explicit CeAudioOutputOwner(Audio::Mixer&);~CeAudioOutputOwner();
 Audio::Mixer& mixer();
 bool start(std::string& error);void stop();void reset();void setPaused(bool);
 std::string error()const;
 bool snapshot(NativeAudioOutputState&,std::string& error)const;
 bool restore(const NativeAudioOutputState&,std::string& error);
 bool renderOffline(uint32_t frames,std::vector<int16_t>& pcm,std::string& error);
private:
 struct Impl;Impl *_impl;
 CeAudioOutputOwner(const CeAudioOutputOwner&);CeAudioOutputOwner& operator=(const CeAudioOutputOwner&);
};
#endif
}
#endif
