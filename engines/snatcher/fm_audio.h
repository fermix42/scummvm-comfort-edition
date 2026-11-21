/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_FM_AUDIO_H
#define SNATCHER_FM_AUDIO_H
#include "fm_player.h"
#include <limits>
namespace Audio {class Mixer;}
namespace Snatcher {
// Versioned reconstruction budgets: old packets retain their historical bound.
// FMAH7 changes source reset/request event publication; layouts1..6 retain
// their original producer policy, with the separated PCM lane retained from6.
// FMAH8 binds Gens/NFG1; FMAH1..7 bind Nuked, never reinterpreted.
inline uint64_t fmHistoryFrameLimit(unsigned version){return version==1||version==2?UINT64_C(158760000):version>=3&&version<=8?UINT64_C(3810240000):0;}
inline bool fmHistoryFramesAllowed(unsigned version,uint64_t frames){return fmHistoryFrameLimit(version)!=0&&frames<=fmHistoryFrameLimit(version);}
// V4 retains every event within a finite 64K-entry domain. At eleven bytes
// per event, even the maximum20KB generator and1472 pending samples leave
// more than290KiB below the native save section's1MiB allocation boundary.
// Older formats keep their original replay budget and byte layout.
inline size_t fmHistoryEventLimit(unsigned version){return version>=1&&version<=3?32768:(version>=4&&version<=8)?65536:0;}
inline bool fmHistoryEventsAllowed(unsigned version,uint64_t count){return fmHistoryEventLimit(version)!=0&&count<=fmHistoryEventLimit(version);}
// Optional physical resource bytes must be the real complete final ISO sector,
// preserving the separately identified logical resource. Never synthesize tail.
inline bool fmAllocationCompatible(const std::vector<uint8_t>& logical,const std::vector<uint8_t>& allocation,std::string& error){
 error.clear();if(logical.empty()||logical.size()>std::numeric_limits<size_t>::max()-2047||allocation.size()!=((logical.size()+2047)/2048)*2048||allocation.size()<logical.size()){error="FM allocation does not match its sector-owned logical resource extent";return false;}
 for(size_t i=0;i<logical.size();++i)if(logical[i]!=allocation[i]){error="FM allocation changes logical resource bytes";return false;}return true;
}

struct FmAudioSnapshot {
 FmCore core=kFmNuked;
 uint64_t frames,updates,nonzeroFrames,percussionEvents,primaryCompletionSerial,secondaryCompletionSerial,completionPublicationGeneration;uint8_t completionPublication;uint64_t semanticDriverDigest;uint16_t activeVoiceMask;uint8_t keyOnMask;size_t activePercussion,queuedCommands;bool active,paused,driverSuspended;std::string error;
 FmAudioSnapshot():frames(0),updates(0),nonzeroFrames(0),percussionEvents(0),primaryCompletionSerial(0),secondaryCompletionSerial(0),completionPublicationGeneration(0),completionPublication(0),semanticDriverDigest(0),activeVoiceMask(0),keyOnMask(0),activePercussion(0),queuedCommands(0),active(false),paused(false),driverSuspended(false){}
};
// All resource IO is completed by load. The mixer callback advances only native
// sound interpretation/generation; no story events or original machine code.
class CeFmAudioOwner {
public:
 explicit CeFmAudioOwner(Audio::Mixer& mixer,bool coupledOutput=true,FmCore core=kFmNuked);~CeFmAudioOwner();
 bool load(const std::vector<uint8_t>& main,const std::vector<uint8_t>& fm,const std::vector<uint8_t>& index,const std::vector<uint8_t>& drums,std::string& error,const std::vector<uint8_t>* subcode=nullptr,const std::vector<uint8_t>* fmAllocation=nullptr);
 bool start(uint16_t command,std::string& error);
 void setGains(uint8_t music,uint8_t effect);void setMono(bool mono);void setPercussionMono(bool mono);
 void setDriverSuspended(bool suspended);
 void setPaused(bool paused);void reset();FmAudioSnapshot snapshot()const;
 bool pcmOutputBusy()const;
 bool pcmSamplesPending()const;
 bool silencePcm(std::string& error);
 // Bounded validation advances the same native state while the device handle is
 // paused. It performs no file IO; caller stores any original-derived output privately.
 bool renderOffline(uint32_t frames,std::vector<int16_t>& pcm,std::string& error);
 bool canSnapshotState(std::string& error)const;
 bool snapshotState(std::vector<uint8_t>& bytes,std::string& error)const;
 bool snapshotLegacyState(std::vector<uint8_t>& bytes,std::string& error)const;
 bool restoreState(const std::vector<uint8_t>& bytes,std::string& error,uint64_t primaryObserved=0,uint64_t secondaryObserved=0,uint64_t publicationObserved=0);
private:
 struct Impl;Impl* _impl;FmCore _preferredCore;CeFmAudioOwner(const CeFmAudioOwner&);CeFmAudioOwner& operator=(const CeFmAudioOwner&);
};
}
#endif

