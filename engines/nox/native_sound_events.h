/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SOUND_EVENTS_H
#define NOX_NATIVE_SOUND_EVENTS_H
#include "common/array.h"
namespace Nox { namespace Native {
enum SpeakerCue { kSpeakerRefusal,kSpeakerSelection,kSpeakerImpact,kSpeakerLongLow,kSpeakerNotification,kSpeakerMagic,kSpeakerProjectile,kSpeakerActorHit,kSpeakerPartyHit,kCwSpeechFirst=128,kCwSpeechLast=255 };
// Transient presentation events: owned by one engine, never gameplay/RNG state.
typedef Common::Array<SpeakerCue> SoundEvents;
} }
#endif
