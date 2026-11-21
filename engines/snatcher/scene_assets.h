/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_SCENE_ASSETS_H
#define SNATCHER_SCENE_ASSETS_H
#include "presentation.h"
namespace Snatcher {
struct SceneDirectory {
 size_t tableOffset,tableBytes,gapBytes;
 std::vector<uint16_t> relativeEntries;
 SceneDirectory():tableOffset(0),tableBytes(0),gapBytes(0){}
};
// The original directory count is not recovered. This returns only the bounded,
// self-consistent candidate prefix independently specified in Phase 2.
bool readSceneDirectory(const uint8_t *data,size_t size,SceneDirectory& out,std::string& error);
struct GraphicsTransfer {
 size_t recordOffset,sourceOffset,encodedEnd;
 uint16_t destination,delay;
 std::vector<uint8_t> decoded;std::vector<bool> knownBytes;
};
bool readGraphicsTransfers(const uint8_t *data,size_t size,size_t begin,size_t end,size_t maxDecodedBytes,std::vector<GraphicsTransfer>& out,std::string& error,uint32_t resourceBase=0x28000);
// Completed copies retain their source allocation until the Main publisher.
// Channel4 denotes immutable, independently decoded presentation bytes.
struct SceneGraphicsCopy {
 uint8_t channel;uint16_t offset,count,destination;std::vector<uint8_t> bytes;
 SceneGraphicsCopy():channel(4),offset(0),count(0),destination(0){}
};
struct SceneGraphicsTask {uint8_t flags,progress;size_t record;std::vector<GraphicsTransfer> transfers;std::vector<uint16_t> stagingOffsets,copyLengths;std::vector<std::vector<uint8_t> > stagingSources;SceneGraphicsTask():flags(0),progress(0),record(0){} };
// Native copy tasks recovered from E81A/E93C/E8C6. The resource owner supplies
// its format base, never guest memory; raw and compressed records
// update four bounded decoded-source caches. Reuse requires known cache bytes;
// no source pointer is executed or mapped as guest memory.
bool readSceneGraphicsTask(const uint8_t *data,size_t size,size_t begin,uint32_t resourceBase,SceneGraphicsTask& task,std::string& error);
class SceneGraphicsQueue {
public:
 SceneGraphicsQueue(){clear();}
 void clear();
 void cancelPending(){_slots.assign(7,SceneGraphicsTask());}
 bool setDissolvePattern(const uint8_t* data,size_t size,std::string& error);
 bool setStaging(unsigned channel,const std::vector<uint8_t>& bytes,std::string& error);
 bool writeStaging(unsigned channel,unsigned offset,const std::vector<uint8_t>& bytes,std::string& error);
 bool resolveCopy(const SceneGraphicsCopy& copy,GraphicsTransfer& transfer,std::string& error)const;
 size_t pending()const;
 bool enqueue(const SceneGraphicsTask& task,std::string& error);
 // One original task update. Bit6 tasks permit later occupied slots to update;
 // ordinary tasks stop the scan after their completion. Busy preserves all slots.
 bool tick(bool transferBusy,std::vector<GraphicsTransfer>& ready,std::string& error,std::vector<SceneGraphicsCopy>* completed=nullptr);
private:
 friend class SceneStateCodec;std::vector<uint8_t> _dissolve;std::vector<SceneGraphicsTask> _slots;std::vector<std::vector<uint8_t> > _buffers;std::vector<std::vector<bool> > _known;std::vector<std::vector<uint8_t> > _zeroMask;
};
struct ActorLink {unsigned parent,child;};
struct SceneControl {uint8_t family;uint16_t value;size_t commandOffset;};
struct SceneAssetCommand {uint8_t family;size_t offset,payloadBegin,payloadEnd;};
enum SceneAssetResult { kSceneAssetsReady,kSceneAssetsUnsupported,kSceneAssetsInvalid };
struct SceneAssets {
 size_t listBegin,listEnd,unsupportedOffset;
 bool terminated;uint8_t terminator;int unsupportedOpcode;
 std::vector<SceneAssetCommand> commands;
 std::vector<GraphicsTransfer> transfers;
 std::vector<ActorInitializer> actors;
 std::vector<ActorLink> links;
 std::vector<PaletteEffect> palettes;
 std::vector<SceneControl> controls;
 SceneAssets():listBegin(0),listEnd(0),unsupportedOffset(0),terminated(false),terminator(0),unsupportedOpcode(-1){}
};
SceneAssetResult parseSystemPresentationAssets(const uint8_t* subcode,size_t size,unsigned index,size_t maxDecodedBytes,SceneAssets& out,std::string& error);
// Ready means specified asset staging, never a claim of complete scene behavior.
// Families 0..9 have typed staging contracts. Controls preserve their recovered
// value and command identity; staging never applies unknown controller policy.
// Any failure is transactional.
// Apply only the actor/palette portion, preserving original inter-family order.
// Transfers and scene controller policies remain explicitly owned by the caller.
// Both outputs are unchanged on failure.
bool applySceneActorsAndPalettes(const SceneAssets& assets,ActorController& actors,PaletteController& palettes,std::string& error);
SceneAssetResult parseSceneAssets(const uint8_t *data,size_t size,unsigned displayIndex,size_t maxDecodedBytes,SceneAssets& out,std::string& error);
}
#endif
