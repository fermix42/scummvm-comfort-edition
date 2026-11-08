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
 * Sound effects and speech: a small channel manager modelled on the
 * original DirectSound manager (DAT_00448468).
 */

#ifndef GOOKA_SOUND_H
#define GOOKA_SOUND_H

#include "audio/mixer.h"

namespace Gooka {

class ResourceManager;

/** Value of a sound handle that is not bound to a channel. */
enum {
	kNoSound = -1
};

/**
 * Plays WAV resources on a fixed number of channels.
 *
 * Callers identify a playing sound through an int handle they own (-1 =
 * none). The manager remembers the handle's address and resets it to -1
 * when the channel is taken over by another sound, like the original.
 * Volume and pan use DirectSound units (hundredths of dB, 0 = full volume,
 * -10000 = silent; pan -10000 = left .. 10000 = right).
 */
class SoundManager {
public:
	SoundManager(Audio::Mixer *mixer, ResourceManager *res);
	~SoundManager();

	/**
	 * Plays WAV `wavId` (FUN_0040a2e0). Reuses an idle channel that already
	 * holds the sample, else a free one, else the oldest low-priority one.
	 * @param handle   optional owner handle, receives the channel index
	 * @param priority non-zero protects the channel from being taken over
	 * @param loop     loop forever; looping sounds also survive stopAll()
	 * @return false if the sample is missing or no channel is available
	 */
	bool play(int *handle, int wavId, int priority = 0, bool loop = false, int volume = 0, int pan = 0,
		Audio::Mixer::SoundType type = Audio::Mixer::kSFXSoundType);
	/** Stops the sound bound to `handle` (FUN_0040a4c0). */
	void stop(int *handle);
	/** True while the sound bound to `handle` plays (FUN_0040a490). */
	bool isPlaying(const int *handle) const;
	/** Changes the pan of a playing sound (FUN_0040a470). */
	void setPan(int *handle, int pan);
	/** Stops every non-looping sound (FUN_0040a4e0). */
	void stopAll();
	/** Stops everything including loops (room changes, loading). */
	void stopEverything();

	/** Length of a WAV resource in milliseconds, 0 if missing. */
	uint32 getDuration(int wavId) const;

private:
	/** One playback channel (16-byte record in the original). */
	struct Channel {
		int wavId;                 ///< sample loaded on the channel, -1 = none
		Audio::SoundHandle handle; ///< mixer handle
		uint32 startTime;          ///< when playback started (oldest-channel choice)
		int priority;
		bool loop;
		int *owner;                ///< caller's handle, reset to -1 on takeover
	};

	static const int kNumChannels = 16;

	/** Converts DirectSound volume (hundredths of dB) to a mixer volume 0..255. */
	static byte toMixerVolume(int volume);
	/** Converts DirectSound pan to a mixer balance -127..127. */
	static int8 toMixerBalance(int pan);
	/** Channel bound to `handle`, nullptr if none. */
	Channel *getChannel(const int *handle) const;
	/** True while the channel's mixer handle is active. */
	bool isChannelPlaying(const Channel &ch) const;

	Audio::Mixer *_mixer;
	ResourceManager *_res;
	Channel _channels[kNumChannels];
};

} // End of namespace Gooka

#endif // GOOKA_SOUND_H
