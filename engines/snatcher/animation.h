/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_ANIMATION_H
#define SNATCHER_ANIMATION_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace Snatcher {
enum AnimationResult { kAnimationFrame, kAnimationFinished, kAnimationPaused, kAnimationUnsupported, kAnimationError, kAnimationSelfRetired };
enum ScrollAxis { kScrollX, kScrollY };
struct ScrollRequest {
 enum Kind { kPosition, kVelocity };
 Kind kind;unsigned plane;ScrollAxis axis;int16_t value;
};
struct AnimationState {
 bool active,hidden; unsigned frameIndex,recordIndex;uint16_t remaining,styleWord;uint8_t paletteOverride;
 int32_t x16,y16,velocityX16,velocityY16;
 AnimationState():active(false),hidden(false),frameIndex(0),recordIndex(0),remaining(0),styleWord(0),paletteOverride(0),x16(0),y16(0),velocityX16(0),velocityY16(0){}
};
// Retirement is returned to the scene owner after tick unwinds; the command
// handler must not destroy the AnimationPlayer currently executing it.
enum AnimationCommandResult {kAnimationCommandAdvance,kAnimationCommandBlocked,kAnimationCommandUnsupported,kAnimationCommandError,kAnimationCommandRetireSelf};
struct AnimationCommand {uint8_t opcode,argument;uint16_t value;size_t sequenceOffset,resourceOffset;};
typedef AnimationCommandResult (*AnimationCommandHandler)(void *owner,const AnimationCommand& command,std::string& error);
typedef bool (*AnimationRandomHandler)(void *owner,uint16_t lower,uint16_t span,uint16_t& result,std::string& error);
struct AnimationClockCapture {
 bool clockKnown,captureKnown;uint16_t sourceTicks,captured;
 AnimationClockCapture():clockKnown(false),captureKnown(false),sourceTicks(0),captured(0){}
};
// Shared animation AA control: zero captures the caller-owned coordinator word;
// nonzero replaces the stored capture with the wrapping elapsed difference.
bool applyAnimationClockCapture(uint8_t argument,AnimationClockCapture& state,std::string& error);
// One tick is one original animation update, not an assumed video/host frame.
// Original resource controls are interpreted as game actions. Random durations,
// inter-actor commands and external event controls remain explicit unsupported.
class AnimationPlayer {
public:
 AnimationPlayer();
 bool start(const uint8_t *resource,size_t size,size_t sequenceOffset,int16_t x,int16_t y,std::string &error);
 AnimationResult tick(std::string &error,AnimationCommandHandler handler=nullptr,void *owner=nullptr,AnimationRandomHandler random=nullptr);
 void setStyleWord(uint16_t value){_state.styleWord=value;}
 void setPaletteOverride(uint8_t value){_state.paletteOverride=value;}
 void setPosition(ScrollAxis axis,int32_t value){if(axis==kScrollX)_state.x16=value;else _state.y16=value;}
 void setVelocity(ScrollAxis axis,int32_t value){if(axis==kScrollX)_state.velocityX16=value;else _state.velocityY16=value;}
 void pause(bool paused){_paused=paused;}
 void cancel();
 bool restartSequence(std::string& error);
 // Source UI owners replace the record cursor, preserving duration and repeat.
 bool selectRecord(unsigned record,std::string& error);
 const AnimationState &state()const{return _state;}
 // Delivered on the owning engine thread, never executed in an audio callback.
 // The scene owner applies global mode/ordering, which is not an actor field.
 std::vector<ScrollRequest> takeScrollRequests(){std::vector<ScrollRequest> out;out.swap(_scrollRequests);return out;}
 unsigned takeCompletionMarkers(){unsigned out=_completionMarkers;_completionMarkers=0;return out;}
 size_t currentOffset()const{return _sequence+size_t(_state.recordIndex)*4;}
private:
 friend class SceneStateCodec;
 friend class SceneRuntime;
 std::vector<uint8_t> _resource;size_t _sequence;AnimationState _state;bool _first,_paused,_halted,_blocked;uint8_t _repeat;AnimationResult _haltResult;std::string _haltError;
 std::vector<ScrollRequest> _scrollRequests;unsigned _completionMarkers;
 AnimationResult fail(AnimationResult result,const char *message,std::string &error);
};
}
#endif
