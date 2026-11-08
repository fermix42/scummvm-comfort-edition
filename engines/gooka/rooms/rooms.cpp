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
 * Room registry: creates the ported room handlers and, for rooms that are
 * not ported yet, a data-only handler so their exits and walk maps work.
 */

#include "gooka/rooms/room_data.h"
#include "gooka/rooms/rooms.h"

namespace Gooka {

/** Room that only provides its static descriptor (no room logic yet). */
class DataRoom : public Room {
public:
	DataRoom(int id, const RoomDescriptor *desc) : Room(id), _desc(desc) {}

	const RoomDescriptor *getDescriptor() override { return _desc; }
	int handleEvent(int event, int param) override { return 0; }

private:
	const RoomDescriptor *_desc;
};

// Ported rooms first; every other id with static data gets a DataRoom.
void createRooms(Common::Array<Room *> &rooms) {
	createPortedRooms(rooms);

	for (int i = 0; kRoomIds[i]; i++) {
		bool ported = false;
		for (uint j = 0; j < rooms.size(); j++) {
			if (rooms[j]->getId() == kRoomIds[i]) {
				ported = true;
				break;
			}
		}
		if (!ported)
			rooms.push_back(new DataRoom(kRoomIds[i], findRoomData(kRoomIds[i])));
	}
}

} // End of namespace Gooka
