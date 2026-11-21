/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_STATE_CODEC_H
#define SNATCHER_STATE_CODEC_H
#include <string>
#include <cstddef>
#include <vector>
#include <cstdint>
namespace Common {class WriteStream;class SeekableReadStream;}
namespace Snatcher {
// A versioned verified-state fragment, deliberately not a complete game save.
// No guest memory prefix, CPU state, guessed scene or pending callback is stored.
bool encodeStateFragment(const std::vector<uint8_t>& packed,const std::string& discSha256,std::vector<uint8_t>& encoded,std::string& error);
bool decodeStateFragment(const uint8_t *data,size_t size,const std::string& expectedDisc,std::vector<uint8_t>& packed,std::string& error);
// Envelope only: payloads must be produced/validated by their semantic owners.
// Presence of sections is not proof that a complete gameplay snapshot exists.
enum NativeSaveSectionId {kSaveStory=1,kSaveNarrative=2,kSaveScene=3,kSaveAnimation=4,kSaveTimers=5,kSaveAudio=6,kSaveShooting=7,kSaveInput=8,kSaveEvents=9,kSaveGameServices=10,kSaveThumbnail=11};
struct NativeSaveSection {
 NativeSaveSectionId id;
 uint16_t version;
 bool required;
 std::vector<uint8_t> bytes;
 NativeSaveSection(NativeSaveSectionId i=kSaveStory):id(i),version(1),required(false){}
};
struct NativeSaveEnvelope {std::vector<NativeSaveSection> sections;};
// Limits apply before allocation. Required IDs are specified by the caller's
// current safe-boundary schema; no guessed default snapshot schema is implied.
bool encodeNativeSave(const NativeSaveEnvelope& save,const std::string& disc,const std::vector<NativeSaveSectionId>& required,std::vector<uint8_t>& encoded,std::string& error);
bool decodeNativeSave(const uint8_t *data,size_t size,const std::string& disc,const std::vector<NativeSaveSectionId>& required,NativeSaveEnvelope& save,std::string& error);
#ifndef SNATCHER_STATE_CORE_ONLY
bool writeNativeSave(Common::WriteStream& stream,const NativeSaveEnvelope& save,const std::string& disc,const std::vector<NativeSaveSectionId>& required,std::string& error);
bool readNativeSave(Common::SeekableReadStream& stream,const std::string& disc,const std::vector<NativeSaveSectionId>& required,NativeSaveEnvelope& save,std::string& error);
bool writeStateFragment(Common::WriteStream& stream,const std::vector<uint8_t>& packed,const std::string& disc,std::string& error);
bool readStateFragment(Common::SeekableReadStream& stream,const std::string& disc,std::vector<uint8_t>& packed,std::string& error);
#endif
}
#endif
