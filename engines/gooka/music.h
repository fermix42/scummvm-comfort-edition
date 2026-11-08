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
 * Background music: Windows MIDI streams (type 32750) played through ScummVM's MIDI layer.
 */

#ifndef GOOKA_MUSIC_H
#define GOOKA_MUSIC_H

#include "common/array.h"

#include "audio/midiplayer.h"

namespace Gooka {

class ResourceManager;

/**
 * Plays the RIFF 'MIDS' music resources. The original fed them to
 * midiStreamOut(); here they are converted to a standard MIDI file in
 * memory and played with MidiParser_SMF on the configured MIDI device.
 */
class Music : public Audio::MidiPlayer {
public:
	explicit Music(ResourceManager *res);
	~Music() override;

	/** Starts track `id` (no-op if it is already playing). */
	void play(uint16 id, bool loop = true);
	/** ID of the playing track, -1 if none. */
	int getCurrent() const { return _current; }
	/** Stops the music. */
	void stop() override;

	/**
	 * Converts a RIFF 'MIDS' stream to a format 0 standard MIDI file.
	 * Layout: 'fmt ' {u32 ticksPerQuarter, u32 maxBuffer, u32 flags},
	 * 'data' {u32 blocks; blocks x {u32 tickStart, u32 size, MIDIEVENT[]}},
	 * MIDIEVENT = {u32 delta, [u32 streamId], u32 event}.
	 */
	static bool convertToSMF(const byte *data, uint32 size, Common::Array<byte> &smf);

private:
	ResourceManager *_res;
	Common::Array<byte> _smf; ///< converted data of the playing track (the parser reads it in place)
	int _current;
};

} // End of namespace Gooka

#endif // GOOKA_MUSIC_H
