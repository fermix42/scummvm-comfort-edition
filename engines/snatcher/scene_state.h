/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_SCENE_STATE_H
#define SNATCHER_SCENE_STATE_H
#include "scene_runtime.h"
#include "state_codec.h"
namespace Snatcher {
class SceneStateSource {public:virtual ~SceneStateSource(){}virtual bool loadSystemResource(std::vector<uint8_t>&,std::string& error){error="system resource source unavailable";return false;}virtual bool loadSceneResource(uint16_t resource,std::vector<uint8_t>& bytes,std::string& error)=0;};
class SceneStateCodec {
public:
 static void archiveLayout(class NativeArchive&,SceneLayout&);
 static bool renderer(class NativeArchive& archive,SceneRenderer& renderer,std::string& error);
 static bool encode(const SceneRuntime& scene,NativeSaveSection& section,std::string& error,bool randomStateOwned=false,bool activeOwners=false);
 static bool decode(const NativeSaveSection& section,SceneStateSource& source,SceneRuntime& scene,std::string& error);
 static bool encodeDialogue(const DialogueWindow& window,std::vector<uint8_t>& bytes,std::string& error);
 static bool decodeDialogue(const std::vector<uint8_t>& bytes,const std::vector<uint8_t>& subcode,DialogueWindow& window,std::string& error);
 static bool dialogueAssociation(const DialogueWindow& window,const std::vector<uint8_t>& source,size_t offset,uint16_t speaker,bool ready);
 static bool bindMenuCount(DialogueWindow& window,unsigned count,std::string& error);
 static bool menuAssociation(const DialogueWindow& window,unsigned selected,bool ready);
private:
 static bool scene(class NativeArchive& archive,SceneRuntime& state,SceneStateSource* source,std::string& error,bool activeOwners=false);
 static bool dialogue(class NativeArchive& archive,DialogueWindow& window);
};
}
#endif
