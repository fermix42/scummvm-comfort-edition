/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_AUDIO_PERCUSSION_H
#define SNATCHER_AUDIO_PERCUSSION_H
#include "audio_runtime.h"
namespace Snatcher {
// Native music percussion, distinct from SFX slots and speech activity. The
// original music tracks6/7 dispatch these two fixed-drum source owners.
struct PcmPercussionRequest {
 uint8_t track,id,level5,gain;bool mono,attenuate;
 PcmPercussionRequest():track(6),id(0),level5(0),gain(255),mono(false),attenuate(false){}
};
class PcmPercussionAssets {
public:
 bool load(const uint8_t* index,size_t indexSize,const uint8_t* drums,size_t drumSize,std::string& error);
 bool prepare(const PcmPercussionRequest& request,PreparedPcmEffect& effect,bool& hasSound,std::string& error)const;
 bool loaded()const{return !_drums.empty();}
private:std::vector<PcmEffect> _effects;std::vector<uint8_t> _drums;
};
class PcmPercussionOwner {
public:
 explicit PcmPercussionOwner(AudioController& controller):_controller(controller){}
 bool start(const PcmPercussionRequest& request,const PreparedPcmEffect& effect,AudioOwner owner,uint32_t generation,bool& accepted,std::string& error);
 bool consume(const AudioCompletion& completion);
 void reset();
 void silence(){_controller.silenceCurrent();}
 size_t activeCount()const;
private:
 struct Slot {AudioToken token;uint8_t priority;Slot():priority(0){}};
 AudioController& _controller;Slot _slots[2];
};
}
#endif
