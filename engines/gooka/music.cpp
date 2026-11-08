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
 * MIDS to SMF conversion and music playback (see music.h).
 */

#include "common/debug.h"
#include "common/ptr.h"
#include "common/textconsole.h"

#include "audio/midiparser.h"

#include "gooka/detection.h"
#include "gooka/music.h"
#include "gooka/resource.h"

namespace Gooka {

// Opens the MIDI device chosen in the ScummVM options and resets it.
Music::Music(ResourceManager *res) : _res(res), _current(-1) {
	createDriver();

	if (_driver->open() == 0) {
		if (_nativeMT32)
			_driver->sendMT32Reset();
		else
			_driver->sendGMReset();
		_driver->setTimerCallback(this, &timerCallback);
	}
}

Music::~Music() {
}

void Music::stop() {
	Audio::MidiPlayer::stop();
	_current = -1;
}

// Loads, converts and starts a track; the parser keeps reading from _smf.
void Music::play(uint16 id, bool loop) {
	Common::StackLock lock(_mutex);

	if (_isPlaying && _current == id)
		return;

	stop();

	Common::ScopedPtr<Common::SeekableReadStream> s(_res->get(kResMusic, id));
	if (!s)
		return;

	Common::Array<byte> mids;
	mids.resize(s->size());
	s->read(mids.data(), mids.size());

	if (!convertToSMF(mids.data(), mids.size(), _smf)) {
		warning("Gooka: cannot convert music %d", id);
		return;
	}

	MidiParser *parser = MidiParser::createParser_SMF();
	if (!parser->loadMusic(_smf.data(), _smf.size())) {
		delete parser;
		return;
	}

	parser->setTrack(0);
	parser->setMidiDriver(this);
	parser->setTimerRate(_driver->getBaseTempo());
	parser->property(MidiParser::mpCenterPitchWheelOnUnload, 1);
	parser->property(MidiParser::mpAutoLoop, loop);
	_parser = parser;

	syncVolume();

	_isLooping = loop;
	_isPlaying = true;
	_current = id;
	debugC(1, kDebugSound, "Playing music %d", id);
}

/** Appends a MIDI variable-length quantity. */
static void writeVLQ(Common::Array<byte> &out, uint32 value) {
	byte buf[5];
	int n = 0;
	buf[n++] = value & 0x7F;
	while (value >>= 7)
		buf[n++] = 0x80 | (value & 0x7F);
	while (n > 0)
		out.push_back(buf[--n]);
}

// Walks the RIFF chunks, then turns every MIDIEVENT into an SMF event.
// Short messages and tempo changes are kept; NOPs only add their delta time.
bool Music::convertToSMF(const byte *data, uint32 size, Common::Array<byte> &smf) {
	if (size < 12 || READ_BE_UINT32(data) != MKTAG('R', 'I', 'F', 'F') || READ_BE_UINT32(data + 8) != MKTAG('M', 'I', 'D', 'S'))
		return false;

	uint32 timeFormat = 0, flags = 0;
	const byte *events = nullptr;
	uint32 eventsSize = 0;

	uint32 pos = 12;
	while (pos + 8 <= size) {
		const uint32 tag = READ_BE_UINT32(data + pos);
		uint32 len = READ_LE_UINT32(data + pos + 4);
		if (len > size - pos - 8)
			len = size - pos - 8;
		if (tag == MKTAG('f', 'm', 't', ' ') && len >= 12) {
			timeFormat = READ_LE_UINT32(data + pos + 8);
			flags = READ_LE_UINT32(data + pos + 16);
		} else if (tag == MKTAG('d', 'a', 't', 'a')) {
			events = data + pos + 8;
			eventsSize = len;
		}
		pos += 8 + len + (len & 1);
	}
	if (!events || !timeFormat || timeFormat > 0x7FFF || eventsSize < 4)
		return false;

	// MDS_F_NOSTREAMID: the events have no stream ID field
	const uint32 eventHeader = (flags & 1) ? 8 : 12;

	Common::Array<byte> track;
	uint32 pendingDelta = 0;
	const uint32 blocks = READ_LE_UINT32(events);
	pos = 4;
	for (uint32 b = 0; b < blocks && pos + 8 <= eventsSize; b++) {
		const uint32 blockSize = READ_LE_UINT32(events + pos + 4);
		pos += 8;
		const uint32 blockEnd = MIN(pos + blockSize, eventsSize);
		while (pos + eventHeader <= blockEnd) {
			const uint32 delta = READ_LE_UINT32(events + pos);
			const uint32 ev = READ_LE_UINT32(events + pos + eventHeader - 4);
			pos += eventHeader;
			pendingDelta += delta;

			if (ev & 0x80000000) {
				// MEVT_F_LONG: the parameter is the length of the data that follows
				pos += ((ev & 0xFFFFFF) + 3) & ~3;
				continue;
			}

			const byte kind = (ev >> 24) & 0x3F;
			const uint32 param = ev & 0xFFFFFF;
			if (kind == 0) {
				// MEVT_SHORTMSG: status + 1 or 2 data bytes
				const byte status = param & 0xFF;
				const byte type = status & 0xF0;
				int len = 0;
				if (type == 0xC0 || type == 0xD0)
					len = 2;
				else if (type >= 0x80 && type < 0xF0)
					len = 3;
				if (!len)
					continue;
				writeVLQ(track, pendingDelta);
				pendingDelta = 0;
				track.push_back(status);
				track.push_back((param >> 8) & 0x7F);
				if (len == 3)
					track.push_back((param >> 16) & 0x7F);
			} else if (kind == 1) {
				// MEVT_TEMPO: microseconds per quarter note -> meta event 0x51
				writeVLQ(track, pendingDelta);
				pendingDelta = 0;
				track.push_back(0xFF);
				track.push_back(0x51);
				track.push_back(0x03);
				track.push_back((param >> 16) & 0xFF);
				track.push_back((param >> 8) & 0xFF);
				track.push_back(param & 0xFF);
			}
		}
		pos = blockEnd;
	}

	// End of track
	writeVLQ(track, pendingDelta);
	track.push_back(0xFF);
	track.push_back(0x2F);
	track.push_back(0x00);

	const uint32 trackSize = track.size();
	const byte header[22] = {
		'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (byte)(timeFormat >> 8), (byte)(timeFormat & 0xFF),
		'M', 'T', 'r', 'k', (byte)(trackSize >> 24), (byte)(trackSize >> 16), (byte)(trackSize >> 8), (byte)trackSize
	};

	smf.resize(sizeof(header) + trackSize);
	memcpy(smf.data(), header, sizeof(header));
	memcpy(smf.data() + sizeof(header), track.data(), trackSize);
	return true;
}

} // End of namespace Gooka
