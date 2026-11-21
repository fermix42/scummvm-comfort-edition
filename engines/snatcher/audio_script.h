/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_AUDIO_SCRIPT_H
#define SNATCHER_AUDIO_SCRIPT_H
#include <cstdint>
#include <string>
namespace Snatcher {
enum ScriptCdTransport {kScriptCdUnknown,kScriptCdIdle,kScriptCdPlaying,kScriptCdPaused};
enum ScriptAudioWaitResult {kScriptAudioWaitPending,kScriptAudioWaitFinished,kScriptAudioWaitError};
struct ScriptAudioWaitState {uint32_t version=1;uint16_t queue=0,remaining=0;bool active=false;};
// Script queues11/19/25 are distinct from PCM activity waits. Primary/secondary
// are source command caches, deliberately independent of renderer activity.
class ScriptAudioWait {
public:
 void reset(){_state=ScriptAudioWaitState();}
 bool start(unsigned queue,std::string& error);
 ScriptAudioWaitResult tick(uint8_t primary,uint8_t secondary,ScriptCdTransport cd,std::string& error);
 const ScriptAudioWaitState& state()const{return _state;}
 bool restore(const ScriptAudioWaitState& state,std::string& error);
private:ScriptAudioWaitState _state;
};
}
#endif
