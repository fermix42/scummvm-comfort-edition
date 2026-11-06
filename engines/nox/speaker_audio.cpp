/* SPDX-License-Identifier: GPL-3.0-or-later */
// Reused standalone synthesis, with no host I/O.
#include "speaker_audio.h"
#include <string.h>
namespace Nox {
void speaker_init(SpeakerAudio *s,speaker_sink sink,void *context){
  memset(s,0,sizeof(*s));s->state=1;s->last_input=1;s->sink=sink;s->context=context;
}
void speaker_advance(SpeakerAudio *s,uint64 cycle){
  if(cycle<s->cycle)return;
  uint64 remaining=(cycle-s->cycle)*NOX_AUDIO_HZ;s->cycle=cycle;
  while(remaining){
    uint32 room=NOX_CPU_HZ-s->phase;
    uint32 amount=remaining<room?(uint32)remaining:room;
    s->area+=s->state*(double)amount;s->phase+=amount;remaining-=amount;
    if(s->phase==NOX_CPU_HZ){
      /* Box integration reduces aliasing; remove the speaker's DC level. */
      double input=s->area/NOX_CPU_HZ;
      double filtered=input-s->last_input+0.995*s->last_output;
      s->last_input=input;s->last_output=filtered;s->area=0;s->phase=0;
      double scaled=filtered*7000.0;
      if(scaled>32767)scaled=32767;if(scaled< -32768)scaled=-32768;
      if(s->sink)s->sink((int16)scaled,s->context);s->samples++;
    }
  }
}
void speaker_toggle(SpeakerAudio *s,uint64 cycle){
  speaker_advance(s,cycle);s->state=-s->state;s->toggles++;
}

} // namespace Nox
