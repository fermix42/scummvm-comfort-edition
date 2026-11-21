/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_PCM_REFILL_H
#define SNATCHER_PCM_REFILL_H
#include "audio_feed.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>
namespace Snatcher {
struct ResidentPcmMutex;
// Source stream transfer plan: up to32 sectors first, then alternating
//16-sector halves. Returns the exact final written prefix after completion.
bool completedPcmStreamPrefix(const uint8_t* bytes,size_t size,std::vector<uint8_t>& prefix,std::string& error);
enum ResidentPcmWriterKind { kPcmWriterNone=0,kPcmWriterResident=1,kPcmWriterStream=2 };
struct ResidentPcmStagingSnapshot {
 size_t capacity;std::vector<uint8_t> knownBytes;uint16_t bank;uint64_t generation;ResidentPcmWriterKind writerKind;uint16_t writerId;
 ResidentPcmStagingSnapshot():capacity(0),bank(0xffff),generation(0),writerKind(kPcmWriterNone),writerId(0){}
};
// Game-owned resident resource allocation. A new bank replaces only its prefix;
// previously loaded tail bytes retain their actual identity and contents.
class ResidentPcmStaging {
public:
 explicit ResidentPcmStaging(size_t capacity);
 ~ResidentPcmStaging();
 bool replacePrefix(uint16_t bank,const uint8_t* bytes,size_t size,std::string& error);
 // Playback starts only after the initial <=32-sector transfer completes.
 // Fully preloaded streams have no subsequent allocation writer to await.
 bool beginStream(uint16_t id,const uint8_t* bytes,size_t size,bool& pending,std::string& error);
 bool publishStreamTransfer(uint16_t id,size_t destination,const uint8_t* bytes,size_t size,std::string& error);
 // Completed source transfer map; active/aborted partial histories are separate.
 bool completeStream(uint16_t id,const uint8_t* bytes,size_t size,std::string& error);
 // Complex transfer history is explicitly unknown; no old prefix is reused.
 bool invalidateStream(uint16_t id,std::string& error);
 bool copyKnownBytes(std::vector<uint8_t>& bytes,std::string& error)const;
 bool snapshot(ResidentPcmStagingSnapshot& state,std::string& error)const;
 bool restore(const ResidentPcmStagingSnapshot& state,std::string& error);
 // bank is the last resident producer identity, not the current prefix writer.
 size_t capacity()const;size_t knownBytes()const;uint16_t bank()const;uint64_t generation()const;
private:
 friend class PcmResidentFeed;
 friend class NativePcmBus;
 bool window(size_t offset,size_t maximum,std::vector<uint8_t>& bytes,std::string& error)const;
 std::unique_ptr<ResidentPcmMutex> _mutex;std::vector<uint8_t> _bytes;size_t _known;uint16_t _bank;uint64_t _generation;ResidentPcmWriterKind _writerKind;uint16_t _writerId;
};
struct PcmStreamFeedSnapshot {
 size_t copiedCodes,consumedFrames,queuedFrames,publishedSourceBytes,pendingSourceOffset,pendingBytes;bool terminalSeen,cancelled;
 PcmStreamFeedSnapshot():copiedCodes(0),consumedFrames(0),queuedFrames(0),publishedSourceBytes(0),pendingSourceOffset(0),pendingBytes(0),terminalSeen(false),cancelled(false){}
};
struct PcmStreamSourceState {
 std::shared_ptr<ResidentPcmStaging> staging;std::vector<uint8_t> source;
 PcmStreamFeedSnapshot progress;size_t nextSource=0,nextTrigger=0,pendingDestination=0;
};
// Native resource delivery replaces the asynchronous disc service. Request,
// service completion and publication remain separate, driven by source copy
// progress rather than elapsed time or a fitted physical-drive delay.
class PcmStreamFeed:public NativeAudioFeed {
public:
 bool archive(class NativeArchive&,std::string& error,const std::shared_ptr<ResidentPcmStaging>& staging={});
 PcmStreamFeed();~PcmStreamFeed();
 bool start(const std::shared_ptr<ResidentPcmStaging>&,uint16_t,const uint8_t*,size_t,const std::vector<int16_t>&,size_t,uint32_t,std::string&);
 bool publishInitial(std::string& error);
 bool serviceRefill(std::string& error);
 void cancel();
 PcmStreamFeedSnapshot snapshot()const;
 bool sourceState(PcmStreamSourceState&,bool includeBytes,std::string&)const;
 uint32_t sourceRate()const override;
 size_t readFrames(int16_t*,size_t)override;
 bool ended()const override;
 std::string error()const override;
private:
 bool prefetch(std::string&);
 std::unique_ptr<ResidentPcmMutex> _mutex;std::shared_ptr<ResidentPcmStaging> _staging;
 std::vector<uint8_t> _source;std::vector<int16_t> _pcm;std::deque<int16_t> _queue;
 PcmStreamFeedSnapshot _state;uint16_t _id;uint32_t _rate;size_t _encoded,_nextSource,_nextTrigger,_pendingDestination;std::string _error;
};
struct ResidentPcmFeedParameters {
 size_t payloadOffset;uint32_t rate;uint8_t level,pan,scale;bool mono,quiet;
 ResidentPcmFeedParameters():payloadOffset(0),rate(0),level(0),pan(0),scale(0),mono(false),quiet(false){}
};
struct ResidentPcmFeedSnapshot {
 bool started,terminalSeen,finished;size_t sourceCursor,terminalOffset,queuedFrames;uint64_t copiedUnits,consumedUnits,renderedFrames,droppedSourceUnits,refills;std::string error;
 ResidentPcmFeedSnapshot():started(false),terminalSeen(false),finished(false),sourceCursor(0),terminalOffset(0),queuedFrames(0),copiedUnits(0),consumedUnits(0),renderedFrames(0),droppedSourceUnits(0),refills(0){}
};
// Semantic source prefetch, not a sound-chip/register or general device model.
// Worker methods copy/convert source data; mixer callbacks consume that queue.
class PcmResidentFeed:public NativeAudioFeed {
public:
 bool archive(class NativeArchive&,std::string& error,const std::shared_ptr<ResidentPcmStaging>& staging={});
 PcmResidentFeed();
 ~PcmResidentFeed();
 bool start(const std::shared_ptr<ResidentPcmStaging>& staging,const ResidentPcmFeedParameters& parameters,std::string& error);
 bool serviceRefill(std::string& error);
 bool owns(const std::shared_ptr<ResidentPcmStaging>& staging)const;
 ResidentPcmFeedSnapshot snapshot()const;
 uint32_t sourceRate()const override;
 size_t readFrames(int16_t* stereo,size_t frames)override;
 bool ended()const override;
 std::string error()const override;
private:
 struct Frame {int16_t left,right;uint64_t ordinal;};
 bool prefetch(std::string& error);
 std::unique_ptr<ResidentPcmMutex> _mutex;std::shared_ptr<ResidentPcmStaging> _staging;ResidentPcmFeedParameters _parameters;
 std::deque<Frame> _queue;ResidentPcmFeedSnapshot _state;
};
}
#endif
