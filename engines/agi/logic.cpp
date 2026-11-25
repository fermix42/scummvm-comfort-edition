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

#include "agi/agi.h"
#include "common/md5.h"
#include "common/memstream.h"

namespace Agi {

/**
 * Decode logic resource
 * This function decodes messages from the specified raw logic resource
 * into a message list.
 * @param logicNr  The number of the logic resource to decode.
 */
int AgiEngine::decodeLogic(int16 logicNr) {
	AgiLogic &logic = _game.logics[logicNr];
	AgiDir &dirLogic = _game.dirLogic[logicNr];

	// bytecode section:
	// u16  bytecode size
	// u8[] bytecode
	uint16 bytecodeSize = READ_LE_UINT16(logic.data);

	// message section:
	// u8       message count
	// u16      messages size (2 + offsets + strings)
	// u16[]    string offsets (relative to message section + 1)
	// string[] strings (null terminated, possibly encrypted)
	int messageSectionPos = 2 + bytecodeSize;
	uint8 messageCount = logic.data[messageSectionPos];
	uint16 messagesSize = READ_LE_UINT16(logic.data + messageSectionPos + 1);
	int stringOffsetsPos = messageSectionPos + 3;
	int stringsPos = stringOffsetsPos + (2 * messageCount);
	int stringsSize = messagesSize - 2 - (2 * messageCount);

	// Two OS-9 collection resources retained plaintext messages when their
	// packed size equalled their unpacked size. Limit the exception to their
	// complete decoded payloads so other releases still use normal decryption.
	bool plaintextCoCoMessages = false;
	if (getPlatform() == Common::kPlatformCoCo3 && !(dirLogic.flags & RES_COMPRESSED)) {
		const char *expectedMD5 = nullptr;
		if (getGameID() == GID_KQ2 && logicNr == 115 && dirLogic.len == 173)
			expectedMD5 = "6de726a1006be47ec03cd83c8996fcdf";
		else if (getGameID() == GID_MIXEDUP && logicNr == 43 && dirLogic.len == 462)
			expectedMD5 = "4385eef161ffefef3eeab94350f50b1b";
		if (expectedMD5) {
			Common::MemoryReadStream resource(logic.data, dirLogic.len);
			plaintextCoCoMessages = Common::computeStreamMD5AsString(resource) == expectedMD5;
		}
	}

	// Compressed resources and the exact plaintext exceptions need no XOR.
	if (!(dirLogic.flags & RES_COMPRESSED) && messageCount > 0 && !plaintextCoCoMessages) {
		decrypt(logic.data + stringsPos, stringsSize);
	}

	// reset logic pointers
	logic.sIP = 2;
	logic.cIP = 2;
	logic.size = messageSectionPos; // exclude messages from logic size

	// allocate list of pointers to message texts. last entry is null.
	logic.numTexts = messageCount;
	logic.texts = (const char **)calloc(1 + logic.numTexts, sizeof(char *));
	if (logic.texts == nullptr) {
		free(logic.data);
		logic.data = nullptr;
		logic.numTexts = 0;
		return errNotEnoughMemory;
	}

	// populate list of pointers to message texts
	for (int i = 0; i < messageCount; i++) {
		int stringOffset = READ_LE_UINT16(logic.data + stringOffsetsPos + (i * 2));
		if (stringOffset != 0) {
			// offset is relative to the message section + 1
			stringOffset += messageSectionPos + 1;
			logic.texts[i] = (const char *)(logic.data + stringOffset);
		} else {
			// TODO: does this happen? when is a string offset zero?
			logic.texts[i] = "";
		}
	}

	// set loaded flag
	dirLogic.flags |= RES_LOADED;
	return errOK;
}

/**
 * Unload logic resource
 * This function unloads the specified logic resource, freeing any
 * memory chunks allocated for this resource.
 * @param logicNr  The number of the logic resource to unload
 */
void AgiEngine::unloadLogic(int16 logicNr) {
	AgiLogic &logic = _game.logics[logicNr];
	AgiDir &dirLogic = _game.dirLogic[logicNr];

	if (dirLogic.flags & RES_LOADED) {
		free(logic.data);
		logic.data = nullptr;
		free(logic.texts);
		logic.texts = nullptr;
		logic.numTexts = 0;
		dirLogic.flags &= ~RES_LOADED;
	}

	logic.sIP = 2;
	logic.cIP = 2;
}

} // End of namespace Agi
