/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Channel-based WAV playback (see sound.h).
 */

#include "common/debug.h"
#include "common/ptr.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "audio/decoders/wave.h"

#include "gooka/detection.h"
#include "gooka/resource.h"
#include "gooka/sound.h"

namespace Gooka {

SoundManager::SoundManager(Audio::Mixer *mixer, ResourceManager *res) : _mixer(mixer), _res(res) {
	for (int i = 0; i < kNumChannels; i++) {
		_channels[i].wavId = -1;
		_channels[i].startTime = 0;
		_channels[i].priority = 0;
		_channels[i].loop = false;
		_channels[i].owner = nullptr;
	}
}

SoundManager::~SoundManager() {
	stopEverything();
}

// 10^(dB/20) with dB = volume / 100.
byte SoundManager::toMixerVolume(int volume) {
	if (volume >= 0)
		return Audio::Mixer::kMaxChannelVolume;
	if (volume <= -10000)
		return 0;
	return (byte)(Audio::Mixer::kMaxChannelVolume * pow(10.0, volume / 2000.0));
}

// The original attenuates the opposite side by |pan| hundredths of dB.
int8 SoundManager::toMixerBalance(int pan) {
	if (pan == 0)
		return 0;
	const double attenuation = 1.0 - pow(10.0, -ABS(pan) / 2000.0);
	const int balance = (int)(127 * attenuation);
	return (int8)(pan < 0 ? -balance : balance);
}

SoundManager::Channel *SoundManager::getChannel(const int *handle) const {
	if (!handle || *handle < 0 || *handle >= kNumChannels)
		return nullptr;
	return const_cast<Channel *>(&_channels[*handle]);
}

bool SoundManager::isChannelPlaying(const Channel &ch) const {
	return _mixer->isSoundHandleActive(ch.handle);
}

// Channel choice follows FUN_0040a2e0; the owner handle of a taken-over channel is reset.
bool SoundManager::play(int *handle, int wavId, int priority, bool loop, int volume, int pan,
		Audio::Mixer::SoundType type) {
	if (wavId <= 0)
		return false;

	// Detach the caller's handle from its previous channel
	Channel *previous = getChannel(handle);
	if (previous && previous->owner == handle)
		previous->owner = nullptr;
	if (handle)
		*handle = kNoSound;

	int chosen = -1;
	int freeChannel = -1;
	int oldestChannel = -1;
	uint32 oldestTime = _channels[0].startTime;
	bool haveOldest = false;
	for (int i = 0; i < kNumChannels; i++) {
		const Channel &ch = _channels[i];
		const bool playing = isChannelPlaying(ch);
		if (ch.wavId == wavId && !playing) {
			chosen = i;
			break;
		}
		if (!playing)
			freeChannel = i;
		if (ch.priority == 0 && (!haveOldest || ch.startTime < oldestTime)) {
			oldestTime = ch.startTime;
			oldestChannel = i;
			haveOldest = true;
		}
	}
	if (chosen == -1)
		chosen = freeChannel != -1 ? freeChannel : oldestChannel;
	if (chosen == -1)
		return false;

	Common::SeekableReadStream *s = _res->get(kResWave, wavId);
	if (!s)
		return false;
	Audio::RewindableAudioStream *wav = Audio::makeWAVStream(s, DisposeAfterUse::YES);
	if (!wav) {
		warning("Gooka: cannot decode WAV %d", wavId);
		return false;
	}

	Channel &ch = _channels[chosen];
	_mixer->stopHandle(ch.handle);
	if (ch.owner && ch.owner != handle)
		*ch.owner = kNoSound;

	Audio::AudioStream *stream = wav;
	if (loop)
		stream = Audio::makeLoopingAudioStream(wav, 0);
	_mixer->playStream(type, &ch.handle, stream, -1, toMixerVolume(volume), toMixerBalance(pan));

	ch.wavId = wavId;
	ch.startTime = g_system->getMillis();
	ch.priority = priority;
	ch.loop = loop;
	ch.owner = handle;
	if (handle)
		*handle = chosen;

	debugC(2, kDebugSound, "Sound %d on channel %d%s", wavId, chosen, loop ? " (loop)" : "");
	return true;
}

void SoundManager::stop(int *handle) {
	Channel *ch = getChannel(handle);
	if (ch)
		_mixer->stopHandle(ch->handle);
}

bool SoundManager::isPlaying(const int *handle) const {
	const Channel *ch = getChannel(handle);
	return ch && isChannelPlaying(*ch);
}

void SoundManager::setPan(int *handle, int pan) {
	Channel *ch = getChannel(handle);
	if (ch)
		_mixer->setChannelBalance(ch->handle, toMixerBalance(pan));
}

void SoundManager::stopAll() {
	for (int i = 0; i < kNumChannels; i++) {
		if (!_channels[i].loop)
			_mixer->stopHandle(_channels[i].handle);
	}
}

void SoundManager::stopEverything() {
	for (int i = 0; i < kNumChannels; i++)
		_mixer->stopHandle(_channels[i].handle);
}

// Duration = data bytes / (rate * bytes per sample frame), as the original computes it.
uint32 SoundManager::getDuration(int wavId) const {
	Common::ScopedPtr<Common::SeekableReadStream> s(_res->get(kResWave, wavId));
	if (!s)
		return 0;

	int size, rate;
	byte flags;
	if (!Audio::loadWAVFromStream(*s, size, rate, flags) || rate <= 0)
		return 0;

	const int bytesPerFrame = ((flags & Audio::FLAG_16BITS) ? 2 : 1) * ((flags & Audio::FLAG_STEREO) ? 2 : 1);
	return (uint32)((uint64)size * 1000 / (rate * bytesPerFrame));
}

} // End of namespace Gooka
