/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_CD_AUDIO_H
#define SNATCHER_CD_AUDIO_H
#include "audio_runtime.h"
namespace Snatcher {
class GameResource;
class CdFrameSource {
public:
 virtual ~CdFrameSource(){}
 virtual bool frameCount(unsigned track,uint32_t& frames,std::string& error)const=0;
 virtual bool readFrames(unsigned track,uint32_t first,uint32_t count,std::vector<uint8_t>& pcm,std::string& error)const=0;
};
class CdQueueBackend {
public:
 virtual ~CdQueueBackend(){}
 virtual bool begin(uint8_t volume,std::string& error)=0;
 virtual bool queue(const std::vector<uint8_t>& pcm,std::string& error)=0;
 virtual size_t queuedChunks()const=0;
 virtual void finishInput()=0;
 virtual bool active()const=0;
 virtual bool consumedFrames(uint64_t& frames)const{frames=0;return false;}
 virtual bool consumedSamples(uint64_t& samples)const{uint64_t frames=0;if(!consumedFrames(frames))return false;samples=frames*588;return true;}
 virtual void stop()=0;
 virtual void pause(bool paused)=0;
};
bool encodeCdRelativeClock(uint64_t frames,uint32_t& packed,std::string& error);
enum CdAudioResult {kCdAudioFinished,kCdAudioCancelled,kCdAudioFailed};
struct CdAudioCompletion {AudioToken token;AudioOwner owner;CdAudioResult result=kCdAudioFinished;std::string error;};
// A delivered, inactive transport retains its last source position independently
// of the scene's published clock. No queue, device cache or active PCM is owned.
struct CdIdleSnapshot {
 uint32_t version,track,total,nextFrame;uint64_t lastConsumed,nextToken;
 bool positionKnown,inputFinished,paused;
 CdIdleSnapshot():version(1),track(0),total(0),nextFrame(0),lastConsumed(0),nextToken(1),positionKnown(false),inputFinished(false),paused(false){}
};
struct CdPlaybackState {
 CdIdleSnapshot idle;bool active=false,resourcePaused=false,completionPending=false;uint8_t volume=255;
 AudioToken token;AudioOwner owner;CdAudioCompletion completion;uint64_t samples=0;std::vector<uint8_t> queued;
};
bool validateCdIdleSnapshot(const CdIdleSnapshot&,std::string& error);
// The caller owns source/backend and keeps them alive through pump destruction.
// Only engine-thread update() performs disc I/O. Each update performs at most
// maxQueuedChunks reads, even when a consumer drains concurrently. No track
// selection, fade, loop, scene policy or original timing is inferred here.
class CdAudioPump {
public:
 CdAudioPump(CdFrameSource& source,CdQueueBackend& backend,uint32_t chunkFrames=75,size_t maxQueuedChunks=4);
 ~CdAudioPump();
 bool start(unsigned track,uint8_t volume,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& error);
 bool update(std::string& error);
 bool cancel(AudioToken token,std::string& error);
 void cancelGeneration(uint32_t generation);
 bool reset(std::string& error);
 void setPaused(bool paused);
 // Game queue11 requests MSCPAUSEON before a presentation resource read.
 // Host pause updates must not resume this independently owned suspension.
 void pauseForResourceRead();
 bool releaseForResourceRead(std::string& error);
 bool take(CdAudioCompletion& completion);
 bool active()const{return _active;}
 bool paused()const{return _paused||_resourceReadPaused;}
 uint32_t inputFramesQueued()const{return _nextFrame;}
 uint32_t totalFrames()const{return _total;}
 bool consumedPosition(unsigned& track,uint64_t& frames)const;
 // SUB D5FE clears the visual clock unless the transport reports playing.
 // This query does not discard the separately retained consumed position.
 bool presentationPosition(unsigned& track,uint64_t& frames)const;
 uint64_t nextTokenCursor()const{return _nextToken;}
 bool captureIdle(CdIdleSnapshot&,std::string& error)const;
 bool capturePlayback(CdPlaybackState&,std::string&)const;
 bool preparePlayback(CdPlaybackState&,std::string&);
 bool commitPlayback(const CdPlaybackState&,std::string&);
 // Metadata validation may be staged before replacing any live audio owners.
 bool validateIdleSource(const CdIdleSnapshot&,std::string& error);
 bool restoreIdle(const CdIdleSnapshot&,std::string& error);
private:
 friend class SnatcherEngine;
 // Engine-only commit after immutable-source validation and reset; no I/O.
 void commitValidatedIdle(const CdIdleSnapshot&);
 CdFrameSource& _source;CdQueueBackend& _backend;
 uint32_t _chunk,_nextFrame,_total;size_t _maxChunks;uint64_t _nextToken;
 uint8_t _volume=255;uint64_t _sampleOrigin=0;
 unsigned _track;AudioToken _token;AudioOwner _owner;
 uint64_t _lastConsumed;bool _positionKnown;
 bool _active,_paused,_resourceReadPaused=false,_inputFinished,_completionPending,_inside;
 CdAudioCompletion _completion;
 bool fill(std::string& error);
 void finish(CdAudioResult result,const std::string& error);
 CdAudioPump(const CdAudioPump&);CdAudioPump& operator=(const CdAudioPump&);
};
#ifndef SNATCHER_AUDIO_CORE_ONLY
class ResourceCdFrameSource:public CdFrameSource {
public:
 explicit ResourceCdFrameSource(const GameResource& resource):_resource(resource){}
 bool frameCount(unsigned,uint32_t&,std::string&)const override;
 bool readFrames(unsigned,uint32_t,uint32_t,std::vector<uint8_t>&,std::string&)const override;
private:const GameResource& _resource;
};
class CeCdQueueBackend:public CdQueueBackend {
public:
 explicit CeCdQueueBackend(Audio::Mixer& mixer);~CeCdQueueBackend() override;
 bool begin(uint8_t,std::string&) override;
 bool queue(const std::vector<uint8_t>&,std::string&) override;
 size_t queuedChunks()const override;
 bool consumedFrames(uint64_t&)const override;
 bool consumedSamples(uint64_t&)const override;
 void finishInput() override;bool active()const override;void stop() override;void pause(bool) override;
private:struct Impl;Impl *_impl;
};
#endif
}
#endif
