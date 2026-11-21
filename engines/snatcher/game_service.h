/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_GAME_SERVICE_H
#define SNATCHER_GAME_SERVICE_H
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace Snatcher {
enum GameStorageAvailability {kGameStorageUnknown,kGameStorageUnavailable,kGameStorageReady};
enum GameSaveOutcome {kGameSaveCommitted,kGameSaveNoCapacity,kGameSaveIOFailure};
enum GameStorageQuery {kGameStorageStatus,kGameLastWriteResult,kGameSlot1,kGameSlot2,kGameSlot3,kGameSlot4};
struct GameSaveSlotFact {bool known,occupied;GameSaveSlotFact():known(false),occupied(false){} };
struct GameSaveTransaction {uint64_t serial;uint8_t slot,beforeCounter,stagedCounter;bool existing;GameSaveTransaction():serial(0),slot(0),beforeCounter(0),stagedCounter(0),existing(false){} };
struct GameServiceState {
 GameStorageAvailability availability;std::array<GameSaveSlotFact,4> slots;
 bool counterKnown,resultKnown,pending;uint8_t saveCounter;uint16_t lastWriteResult;uint64_t serial;
 GameSaveTransaction write;
 GameServiceState():availability(kGameStorageUnknown),counterKnown(false),resultKnown(false),pending(false),saveCounter(0),lastWriteResult(0),serial(0){}
};
// Facts must come from the CE-owned storage backend. Legacy unknown facts never
// become empty slots or a successful result merely by constructing this owner.
bool validateGameServiceState(const GameServiceState& state,std::string& error);
bool readGameStorageQuery(const GameServiceState& state,GameStorageQuery query,uint16_t& value,std::string& error);
bool beginGameSaveWrite(GameServiceState& state,unsigned slot,GameSaveTransaction& transaction,std::string& error);
bool finishGameSaveWrite(GameServiceState& state,const GameSaveTransaction& transaction,GameSaveOutcome outcome,std::string& error);
bool abortGameSaveWrite(GameServiceState& state,const GameSaveTransaction& transaction,std::string& error);
// Prepare the semantic state carried by a successfully written file. Publishing
// this preview into the live owner still requires an actual successful CE write.
bool previewCommittedGameSave(const GameServiceState& state,const GameSaveTransaction& transaction,GameServiceState& preview,std::string& error);
// Versioned quiescent owner section; active writes are deliberately not saveable.
bool encodeGameServiceState(const GameServiceState& state,std::vector<uint8_t>& bytes,std::string& error);
bool decodeGameServiceState(const std::vector<uint8_t>& bytes,GameServiceState& state,std::string& error);
struct GameSaveElapsed {uint8_t hours,minutes,seconds,frames;GameSaveElapsed():hours(0),minutes(0),seconds(0),frames(0){} };
struct GameSaveBookmark {
 bool known,quiet,suppressed;uint16_t presentation,pcm;uint8_t music,ambient,interaction;std::array<uint8_t,64> actors;
 GameSaveBookmark():known(false),quiet(false),suppressed(false),presentation(0),pcm(0xffff),music(0),ambient(0),interaction(0){actors.fill(0);}
};
struct GameSaveRecord {
 std::string disc;std::vector<uint64_t> scenarioStamps;std::vector<uint8_t> packed;
 bool contextKnown,sourcePolicy40,sourcePolicy80;uint8_t controllerMapping,pointerAnchorX,pointerAnchorY,counter;
 uint16_t resource,resumeEntry,progression;GameSaveElapsed elapsed;std::vector<uint16_t> cursorHistory;GameSaveBookmark bookmark;
 GameSaveRecord():contextKnown(false),sourcePolicy40(false),sourcePolicy80(false),controllerMapping(0),pointerAnchorX(0),pointerAnchorY(0),counter(0),resource(0),resumeEntry(0),progression(0){}
};
// Authored semantic record: no original backup-RAM header or guest memory blob.
// Source flags40/80 stay neutral until their setting owners are independently bound.
bool validateGameSaveRecord(const GameSaveRecord& record,std::string& error);
bool encodeGameSaveRecord(const GameSaveRecord& record,std::vector<uint8_t>& bytes,std::string& error);
bool decodeGameSaveRecord(const std::vector<uint8_t>& bytes,const std::string& expectedDisc,const std::vector<uint64_t>& expectedScenarioStamps,GameSaveRecord& record,std::string& error);
struct GameSaveResume {uint16_t resource,entry;GameSaveResume():resource(0),entry(0){} };
// Source14F12 chooses the first saved local entry (oldest), otherwise outerEntry.
// The caller validates the resulting entry against its decoded original graph.
bool resolveGameSaveResume(uint16_t resource,uint32_t outerEntry,const std::vector<uint32_t>& localEntries,GameSaveResume& resume,std::string& error);
}
#endif
