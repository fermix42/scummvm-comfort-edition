/* SPDX-License-Identifier: GPL-3.0-or-later */
// Reused synthesis from the Nox standalone conversion; no host audio I/O.
/* Apple II one-bit speaker, driven by emulated CPU cycles. */
#ifndef NOX_SPEAKER_AUDIO_H
#define NOX_SPEAKER_AUDIO_H
#include "common/scummsys.h"
namespace Nox {
#define NOX_CPU_HZ 1023000u
#define NOX_AUDIO_HZ 48000u
typedef void (*speaker_sink)(int16 sample,void *context);
typedef struct {
  uint64 cycle,samples,toggles;
  uint32 phase;
  int state;
  double area,last_input,last_output;
  speaker_sink sink;
  void *context;
} SpeakerAudio;
void speaker_init(SpeakerAudio *s,speaker_sink sink,void *context);
void speaker_advance(SpeakerAudio *s,uint64 cycle);
void speaker_toggle(SpeakerAudio *s,uint64 cycle);
} // namespace Nox
#endif
