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

#include "common/debug.h"
#include "common/config-manager.h"
#include "common/endian.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "cine/cine.h"
#include "cine/msg.h"
#include "cine/various.h"

namespace Cine {

static bool shouldCleanupAmigaText() {
	return g_cine->getGameType() == Cine::GType_OS &&
		g_cine->getPlatform() == Common::kPlatformAmiga &&
		(!ConfMan.hasKey("cleanup_os_amiga_text") || ConfMan.getBool("cleanup_os_amiga_text"));
}

static Common::String stripAmigaMessageControls(const char *message) {
	Common::String result;

	for (const char *p = message; *p; ++p) {
		if (*p == '\\' && (p[1] == 'p' || p[1] == 's') && Common::isDigit(p[2])) {
			p += 2;
			while (Common::isDigit(p[1]))
				++p;
			while (p[1] == ' ')
				++p;
		} else if (*p == '\\' && Common::isDigit(p[1])) {
			++p;
			while (Common::isDigit(p[1]))
				++p;
			while (p[1] == ' ')
				++p;
		} else {
			result += *p;
		}
	}

	return result;
}

static Common::String normalizeMessage(const char *message, const char *msgName, uint index) {
	if (!shouldCleanupAmigaText())
		return message;

	Common::String result = stripAmigaMessageControls(message);

	if (g_cine->getLanguage() == Common::EN_GRB && scumm_stricmp(msgName, "BATEAU.MSG") == 0 &&
			index == 4 && result.hasPrefix("A very light Un tr")) {
		result = "A very light whistle tells you the bracelet is inflating.";
	}

	if (g_cine->getLanguage() == Common::EN_GRB && scumm_stricmp(msgName, "VILLE.MSG") == 0 &&
			index == 3 && result.hasSuffix("vous le prenez.")) {
		result = "OK. You take it.";
	}

	if (g_cine->getLanguage() == Common::EN_GRB && scumm_stricmp(msgName, "SALLE59.MSG") == 0) {
		if (index == 136 && result.hasPrefix("Vous actionnez")) {
			result = "You play with the on/off switch on the razor. It turns on.";
		} else if (index == 172 && result.hasSuffix("Docteur WHY ?")) {
			result = "\"Why the number 346, Doctor Why?\"";
		} else if (index == 174 && result.hasPrefix("Docteur WHY")) {
			result = "\"Doctor Why, a transmission from the Stealth.\"";
		}
	}

	if (g_cine->getLanguage() == Common::EN_GRB && scumm_stricmp(msgName, "DOUCHE.MSG") == 0) {
		if (index == 22 && result.hasPrefix("-Vous pouvez disposer")) {
			result = "\"You can leave.\"";
		} else if (index == 172 && result.hasSuffix("Docteur WHY ?")) {
			result = "\"Why the number 346, Doctor Why?\"";
		} else if (index == 174 && result.hasPrefix("Docteur WHY")) {
			result = "\"Doctor Why, a transmission from the Stealth.\"";
		} else if (index == 207 && result.hasPrefix("Ok! Vous enlacez")) {
			result = "OK! You secure the bomb with your elastic band.";
		}
	}

	return result;
}

int16 loadMsg(char *pMsgName) {
	uint32 sourceSize;

	checkDataDisk(-1);
	g_cine->_messageTable.clear();

	int16 foundFileIdx = findFileInBundle(pMsgName);
	if (foundFileIdx < 0) {
		warning("loadMsg(\"%s\"): Could not find file in bundle.", pMsgName);
		return -1;
	}

	byte *dataPtr = readBundleFile(foundFileIdx, &sourceSize);

	setMouseCursor(MOUSE_CURSOR_DISK);

	uint count = READ_BE_UINT16(dataPtr);
	uint messageLenPos = 2;
	uint messageDataPos = messageLenPos + 2 * count;

	// Read in the messages
	for (uint i = 0; i < count; i++) {
		// Read message's length
		uint messageLen = READ_BE_UINT16(dataPtr + messageLenPos);
		messageLenPos += 2;

		// Store the read message.
		// This code works around input data that has empty strings residing outside the input
		// buffer (e.g. message indices 58-254 in BATEAU.MSG in PROCS08 in Operation Stealth).
		if (messageDataPos < sourceSize) {
			g_cine->_messageTable.push_back(normalizeMessage((const char *)(dataPtr + messageDataPos), pMsgName, i));
		} else {
			if (messageLen > 0) { // Only warn about overflowing non-empty strings
				warning("loadMsg(%s): message (%d. / %d) is overflowing the input buffer. Replacing it with an empty string", pMsgName, i + 1, count);
			} else {
				debugC(5, kCineDebugPart, "loadMsg(%s): empty message (%d. / %d) resides outside input buffer", pMsgName, i + 1, count);
			}
			// Message resides outside the input buffer so we replace it with an empty string
			g_cine->_messageTable.push_back("");
		}
		// Jump to the next message
		messageDataPos += messageLen;
	}

	free(dataPtr);
	return 0;
}

} // End of namespace Cine
