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
 * Known releases of the game (file MD5 checksums of the first 5000 bytes and sizes).
 */

namespace Gooka {

/** Supported game versions. */
static const ADGameDescription gameDescriptions[] = {
	// Czech CD release v1.0.3.0 (JRC Interactive, 1997)
	{
		"gooka",
		"",
		// Detection line for GAME.EXE, for reference: "GAME.EXE", "01b8d6541ba82dec14bd6067a6520bef", 289792,
		// MD5 checksum generation with md5sum < <(head -c5000 "filename")
		AD_ENTRY2s("0.BIN", "b9579c10827278aff76cfd954844a87d", 68677632,
					"1.BIN","01055407c2d36d44cfba42df11f324aa", 6123520),
		Common::CS_CZE,
		Common::kPlatformWindows,
		ADGF_UNSTABLE | ADGF_ADDENGLISH,
		GUIO2(GUIO_MIDIGM, GUIO_GAMEOPTIONS1)
	},

	AD_TABLE_END_MARKER
};

} // End of namespace Gooka
