/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_PCM_SHARED_H
#define SNATCHER_PCM_SHARED_H
#include "audio_runtime.h"
#include <functional>
namespace Snatcher {
// Native game-owned PCM pairs and shared copy publication. No guest machine,
// CPU, register interface or hardware timing is exposed or executed here.
class NativePcmBus:public NativeAudioFeed,public std::enable_shared_from_this<NativePcmBus> {
public:
 bool archive(class NativeArchive&,std::string& error);
 NativePcmBus(AudioBackend&,const PreparedPcmStream&,const std::shared_ptr<PcmStreamFeed>&);
 NativePcmBus(AudioBackend&,const std::shared_ptr<ResidentPcmStaging>&,const PcmIdleContext&);
 ~NativePcmBus();
 bool initialize(std::string&);void activate();void silence();void cancel();
 void deactivate();bool finishStream(std::string&);bool idleContext(PcmIdleContext&,std::string&)const;
 bool service(std::string&);bool residentMode(std::string&);
 uint8_t priority(unsigned)const;bool effectActivity()const;
 bool owns(const std::shared_ptr<ResidentPcmStaging>&)const;
 size_t pendingSourceBytes()const;
 size_t playbackCursor(unsigned slot)const;
 void noteEffectRequest();
 bool replace(const PreparedPcmEffect&,const std::function<bool(const std::shared_ptr<NativeAudioFeed>&,std::string&)>&,std::string&);
 std::shared_ptr<NativeAudioFeed> streamJob();
 std::shared_ptr<NativeAudioFeed> restoreJob(unsigned slot,uint64_t serial);
 bool identifyJob(const std::shared_ptr<NativeAudioFeed>&,unsigned& slot,uint64_t& serial)const;
 uint32_t sourceRate()const override;size_t readFrames(int16_t*,size_t)override;
 bool ended()const override;std::string error()const override;
private:
 struct Impl;class Job;std::unique_ptr<Impl> _impl;
 bool jobEnded(unsigned,uint64_t)const;
};
}
#endif
