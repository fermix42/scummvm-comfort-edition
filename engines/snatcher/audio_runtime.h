/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_AUDIO_RUNTIME_H
#define SNATCHER_AUDIO_RUNTIME_H
#include "pcm.h"
#include "audio_feed.h"
#include "pcm_refill.h"
#include "audio_rate.h"
#include <memory>
#include <deque>
#include <map>
namespace Audio {class Mixer;}
namespace Snatcher {
enum PcmSourceKind {kDrumSource,kResidentSource};
struct PcmEffect {uint16_t id,step;uint8_t priority,channel,level,pan;PcmSourceKind source;size_t headerOffset;};
bool parsePcmEffects(const uint8_t *index,size_t size,std::vector<PcmEffect>& effects,std::string& error);
bool resolvePcmEffect(const PcmEffect& effect,const std::vector<PcmSample>& drums,const std::vector<PcmSample>& resident,PcmSample& sample,std::string& error);
enum PcmEffectControlKind {kNoPcmEffectRequest,kPcmEffectRecord,kPcmEffectVerifiedNoOp,kPcmEffectMonoOn,kPcmEffectMonoOff,kPcmEffectStop};
struct PcmEffectControl {PcmEffectControlKind kind;uint8_t id;};
PcmEffectControl planPcmEffectControl(uint16_t command,size_t recordCount);

enum PcmRequestKind {kLoadResidentBank,kStartStream};
struct PcmRequest {PcmRequestKind kind;PcmCatalogEntry entry;};
bool planPcmRequest(const std::vector<PcmCatalogEntry>& catalog,uint16_t id,PcmRequest& request,std::string& error);
enum FmControlKind {kNoFmRequest,kRequestFmFade,kQueueFmCommand};
struct FmControl {FmControlKind kind;uint8_t command;bool resetFade;};
// Command routing only. No sequence decoder/synthesizer or fade timing implied.
FmControl planFmControl(uint16_t command,bool resetOnOrdinaryCommand);
// Source SUB15F18 publishes both logical gain bytes through immutable15FBA.
bool readFmGainPublication(const uint8_t* sub,size_t size,std::vector<uint8_t>& out,std::string& error);
// Four source-defined activity flags. Unknown bits remain unknown until their
// owning native subsystem establishes them; resident completion clears only bit2.
class AudioActivity {
public:
 AudioActivity():_known(0),_active(0){}
 void clearForSoundReset(){_known=15;_active=0;}
 void setKnownFlag(unsigned bit,bool active);
 void beginResidentTransfer(){setKnownFlag(2,true);}
 void completeResidentTransfer(){setKnownFlag(2,false);}
 void forgetFlag(unsigned bit){if(bit<4)_known&=uint8_t(~(1u<<bit));}
 bool snapshot(uint8_t& bits,std::string& error,uint8_t requiredMask=15)const;
 uint8_t knownMask()const{return _known;}
private:
 friend class EngineStateCodec;uint8_t _known,_active;
};
enum AudioWaitResult {kAudioWaitPending,kAudioWaitFinished,kAudioWaitUnknown};
// Queue0A contract: up to3600 caller ticks, then one advancement tick. This
// does not establish update frequency or infer mixer inactivity as story state.
class AudioActivityWait {
public:
 AudioActivityWait():_remaining(3600),_advance(false),_done(false){}
 AudioWaitResult tick(const AudioActivity& activity,std::string& error);
 uint16_t remaining()const{return _remaining;}
private:
 friend class EngineStateCodec;uint16_t _remaining;bool _advance,_done;
};
// Game-level command/gain scheduling recovered from the original sound coordinator.
// One update is one caller-owned sound tick, not an assumed wall-clock duration.
// Gain bytes are logical controls; they are not CE mixer volumes or FM registers.
struct MusicControlPacket {uint8_t command,musicGain,effectGain;};
enum MusicEnvelope {kMusicSteady,kMusicDuck,kMusicRestore};
class MusicControl {
public:
 MusicControl();
 bool submit(uint16_t command,bool resetFade,std::string& error);
 void beginSpeechDuck();void endSpeechDuck();
 bool tick(bool attenuateEffects,MusicControlPacket& packet);
 uint8_t gain()const{return _gain;}
 bool fading()const{return _fade;}
 MusicEnvelope envelope()const{return _envelope;}
 size_t pending()const{return _commands.size();}
private:
 friend class EngineStateCodec;
 std::deque<uint8_t> _commands;uint8_t _gain,_effectGain,_sentGain,_sentEffect,_counter;
 bool _fade;MusicEnvelope _envelope;
};
// Disc adapter supplies validated, pregap-excluded S16LE CD-DA sectors.
// This converts the standardized source domain, not a chip/device model.
bool decodeCdAudioFrames(const uint8_t *data,size_t size,std::vector<int16_t>& pcm,std::string& error);

// This profile must be supplied by a calibrated caller; nominal header rates
// and candidate exports never create a playable profile automatically.
struct AudioProfile {
 bool rateVerified,polarityVerified,gainVerified,loopVerified;
 uint32_t rate;uint8_t channels,volume;bool loop;
 RationalAudioRate exactRate;
 AudioProfile():rateVerified(false),polarityVerified(false),gainVerified(false),loopVerified(false),rate(0),channels(0),volume(0),loop(false){}
};
// Source-level stream conversion, not a sound-device/register model. The caller
// must qualify rate/polarity/gain/termination before enabling actual playback.
// Source catalog envelope/pan controls are applied during conversion; the output
// profile must therefore use stereo and full CE volume (no second gain stage).
AudioProfile sourcePcmStreamProfile();
struct PreparedPcmStream {
 PcmCatalogEntry source;PcmSample sample;std::vector<int16_t> pcm;AudioProfile profile;
 // sample.sampleCount retains the encoded payload extent. Source stream-copy
 // boundary markers replace one code per32KiB ring before native playback.
 size_t playbackFrames,droppedSourceFrames;
 bool sourceValidated=false;uint8_t effectiveLevel=0,effectivePan=0;
 PreparedPcmStream():source(),sample(),playbackFrames(0),droppedSourceFrames(0){}
};
bool preparePcmStream(const PcmCatalogEntry& entry,const uint8_t *bank,size_t size,
 const AudioProfile& calibrated,bool mono,PreparedPcmStream& stream,std::string& error,bool quiet=false);
struct PreparedPcmEffect {
 PcmEffect source;PcmSample sample;std::vector<int16_t> pcm;AudioProfile profile;
 std::vector<uint8_t> sourceCodes;uint8_t effectiveLevel=0,effectivePan=0;
 PreparedPcmEffect():source(),sample(){}
};
AudioProfile sourcePcmEffectProfile(const PcmEffect& effect);
// Source is exactly the selected resident bank or drum allocation, not the
// complete catalog blob. scale0 preserves source level; other bytes multiply
// level/256 per the original optional effect-volume operand.
bool preparePcmEffect(const PcmEffect& effect,const uint8_t *source,size_t size,
 const AudioProfile& calibrated,bool mono,uint8_t scale,PreparedPcmEffect& out,std::string& error,bool quiet=false);
// Actual known source allocation: effects skip16 bytes and terminate on FF,
// even when speech has replaced the prefix with data having no sample header.
bool preparePcmStagingEffect(const PcmEffect&,const uint8_t*,size_t,const AudioProfile&,bool,uint8_t,PreparedPcmEffect&,std::string&,bool quiet=false);
// Explicit caller-selected CE volume; no original fade/mix policy is inferred.
AudioProfile cdAudioProfile(uint8_t volume);
struct AudioOwner {uint16_t resource;uint32_t node;AudioOwner(uint16_t r=0,uint32_t n=0):resource(r),node(n){}};
struct AudioToken {uint64_t id;uint32_t generation;AudioToken(uint64_t i=0,uint32_t g=0):id(i),generation(g){}};
enum AudioResult {kAudioFinished,kAudioCancelled};
struct AudioCompletion {AudioToken token;AudioOwner owner;AudioResult result;};
// Quiescent game-level copier context. No active voice/buffer/device state.
struct PcmIdleContext {uint32_t version=1;bool known=false;uint8_t mode=0,copyMode=0;};
bool validatePcmIdleContext(const PcmIdleContext&,std::string& error);
bool encodePcmIdleContext(const PcmIdleContext&,std::vector<uint8_t>&,std::string&);
bool decodePcmIdleContext(const std::vector<uint8_t>&,PcmIdleContext&,std::string&);
enum AudioRole {kAudioSpeech,kAudioEffect,kAudioMusic,kAudioSharedPcm};
class AudioBackend {
public:
 virtual ~AudioBackend(){}
 virtual bool play(const std::vector<int16_t>& pcm,const AudioProfile& profile,AudioRole role,uint64_t& handle,std::string& error)=0;
 virtual bool playFeed(const std::shared_ptr<NativeAudioFeed>&,const AudioProfile&,AudioRole,uint64_t& handle,std::string& error){handle=0;error="Native prefetched audio feeds are unsupported by this backend";return false;}
 virtual void stop(uint64_t handle)=0;
 virtual void pause(uint64_t handle,bool paused)=0;
 virtual bool active(uint64_t handle)const=0;
 virtual uint32_t outputRate()const{return 44100;}
 virtual unsigned roleVolume(AudioRole)const{return 256;}
};
// Single engine-thread controller. It polls mixer state; no mixer callback
// executes gameplay. Completion records are delivered once, in stable token
// order within each poll, and retain explicit caller-owned generation/owner.
class AudioController {
public:
 AudioController(AudioBackend& backend,size_t maxActive=8,size_t completionCapacity=64);
 ~AudioController();
 bool start(const std::vector<int16_t>& pcm,const AudioProfile& profile,AudioRole role,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& error);
 bool startFeed(const std::shared_ptr<NativeAudioFeed>& feed,const AudioProfile& profile,AudioRole role,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& error);
 // Source-owned logical jobs observed from a shared real playback owner.
 // They acquire no dummy sound handle and never submit fabricated silence.
 bool observe(const std::shared_ptr<NativeAudioFeed>& job,AudioRole,AudioOwner,uint32_t,AudioToken&,std::string&);
 bool canAdmit(size_t count,std::string& error)const;
 bool physicalActive(AudioToken token)const;
 AudioBackend& backend(){return _backend;}
 std::string feedError()const;
 bool cancel(AudioToken token,std::string& error);
 void cancelGeneration(uint32_t generation);
 void cancelRole(AudioRole role);
 // New-game/session barrier, independent of narrative generation changes.
 void reset();
 void setPaused(bool paused);
 // Source PCM FF disables current channels while preserving logical jobs.
 void silenceCurrent();
 void poll();
 bool take(AudioCompletion& completion);
 size_t activeCount()const{return _active.size();}
 size_t completionCount()const{return _complete.size();}
private:
 friend struct FmCheckpointCodec;
 friend class EngineStateCodec;
 struct Playing {AudioToken token;AudioOwner owner;uint64_t handle;AudioRole role;std::shared_ptr<NativeAudioFeed> feed;bool silenced=false,logical=false;};
 AudioBackend& _backend;size_t _maxActive,_capacity;uint64_t _next;bool _paused;
 std::map<uint64_t,Playing> _active;std::deque<AudioCompletion> _complete;
 void finish(std::map<uint64_t,Playing>::iterator it,AudioResult result,bool stop);
 AudioController(const AudioController&);AudioController& operator=(const AudioController&);
};
// Main-thread ownership of the single original stream. Mixer completion is
// consumed separately from queue dispatch; only this exact token may clear
// playback bit1. A scene transition alone never guesses that speech ended.
// Existing resident/effect requests share the original stream allocation and
// are interrupted on a newly accepted stream; CD music remains independent.
class NativePcmBus;
class PcmStreamOwner {
public:
 PcmStreamOwner(AudioController& controller,AudioActivity& activity,MusicControl *music=nullptr);
 bool start(const PreparedPcmStream& stream,AudioOwner owner,uint32_t generation,AudioToken& token,std::string& error,const std::shared_ptr<PcmStreamFeed>& feed=std::shared_ptr<PcmStreamFeed>());
 bool serviceRefills(std::string& error);
 bool consume(const AudioCompletion& completion);
 bool cancel(std::string& error);
 void reset();
 bool active()const{return _token.id!=0;}
 AudioToken token()const{return _token;}
 uint16_t catalogId()const{return _catalog;}
 uint8_t priority()const;
 std::shared_ptr<NativePcmBus> sharedBus()const{return _bus;}
 bool enterResidentMode(std::string& error,const std::shared_ptr<ResidentPcmStaging>& staging=std::shared_ptr<ResidentPcmStaging>());
 bool ensureBusOutput(AudioOwner,uint32_t,bool& created,std::string&);
 void finishBusAdmission(bool created,bool accepted);
 bool captureIdleContext(PcmIdleContext&,std::string&)const;
 bool prepareIdleContext(const PcmIdleContext&,const std::shared_ptr<ResidentPcmStaging>&,std::shared_ptr<NativePcmBus>&,std::string&)const;
 void commitIdleContext(const PcmIdleContext&,const std::shared_ptr<NativePcmBus>&);
private:
 friend class EngineStateCodec;
 AudioController& _controller;AudioActivity& _activity;MusicControl *_music;
 AudioToken _token;uint16_t _catalog;uint8_t _mode,_priority;
 std::shared_ptr<PcmStreamFeed> _feed;
 std::shared_ptr<NativePcmBus> _bus;AudioToken _busToken;
 PcmIdleContext _idleContext;
 void stopped(bool restoreMusic,bool retainContext=false);
};
struct PcmEffectVoiceSnapshot {
 bool metadataKnown,prefetchOwned;bool terminalSeen;uint64_t sourceCursor,queuedFrames,renderedFrames,droppedUnits;uint8_t channel;uint16_t effect,bank;PcmSourceKind source;size_t payloadOffset,sampleCount;AudioToken token;
 PcmEffectVoiceSnapshot():metadataKnown(false),prefetchOwned(false),terminalSeen(false),sourceCursor(0),queuedFrames(0),renderedFrames(0),droppedUnits(0),channel(0),effect(0),bank(0xffff),source(kResidentSource),payloadOffset(0),sampleCount(0){}
};
class PcmEffectOwner {
public:
 PcmEffectOwner(AudioController& controller,AudioActivity& activity,PcmStreamOwner *stream=nullptr);
 // A lower-priority request is dispatched but does not replace the existing
 // channel; accepted=false/token0 reflects source rejection, not completion.
 bool start(const PreparedPcmEffect& effect,uint16_t bank,AudioOwner owner,uint32_t generation,bool& accepted,AudioToken& token,std::string& error);
 bool consume(const AudioCompletion& completion);
 void reset();
 void silence();
 size_t activeCount()const;
 // Source mode1 overwrites only its staging extent and does not cancel voices.
 // Conservative admission: independent/copied/disjoint payloads only. Streams
 // have a separate shared-allocation guard owned by the caller.
 bool residentReplacementSafe(uint16_t newBank,size_t newByteLength,const std::shared_ptr<ResidentPcmStaging>& staging=std::shared_ptr<ResidentPcmStaging>())const;
 bool preflight(const PcmEffect& record,bool& admitted,std::string& error);
 bool startFeed(const PreparedPcmEffect&,uint16_t bank,const std::shared_ptr<PcmResidentFeed>&,const std::shared_ptr<ResidentPcmStaging>&,AudioOwner,uint32_t generation,bool& accepted,AudioToken&,std::string& error);
 bool serviceRefills(std::string& error);
 std::vector<PcmEffectVoiceSnapshot> voices()const;
private:
 friend class EngineStateCodec;
 struct Slot {AudioToken token;uint8_t priority;uint16_t bank;PcmEffectVoiceSnapshot voice;std::shared_ptr<PcmResidentFeed> feed;std::shared_ptr<NativePcmBus> bus;Slot():priority(0),bank(0xffff){}};
 AudioController& _controller;AudioActivity& _activity;PcmStreamOwner *_stream;Slot _slots[2];
 bool startOwned(const PreparedPcmEffect&,uint16_t,AudioOwner,uint32_t,bool&,AudioToken&,std::string&,const std::shared_ptr<PcmResidentFeed>&);
};
#ifndef SNATCHER_AUDIO_CORE_ONLY
// Thin CE adapter. The mixer owns copied raw streams; this object owns handles.
// It does not know scenarios or dispatch events.
class CeAudioBackend:public AudioBackend {
public:
 explicit CeAudioBackend(Audio::Mixer& mixer);~CeAudioBackend() override;
 bool play(const std::vector<int16_t>&,const AudioProfile&,AudioRole,uint64_t&,std::string&) override;
 bool playFeed(const std::shared_ptr<NativeAudioFeed>&,const AudioProfile&,AudioRole,uint64_t&,std::string&) override;
 void stop(uint64_t) override;void pause(uint64_t,bool) override;bool active(uint64_t)const override;
 bool snapshotPlayback(uint64_t,std::vector<uint8_t>&,std::string&)const;
 bool restorePlayback(const std::shared_ptr<NativeAudioFeed>&,const std::vector<uint8_t>&,AudioRole,uint64_t&,std::string&,bool validateOnly=false);
 uint32_t outputRate()const override;
 unsigned roleVolume(AudioRole)const override;
private:
 friend class EngineStateCodec;
 struct Impl;Impl *_impl;
};
#endif
}
#endif
