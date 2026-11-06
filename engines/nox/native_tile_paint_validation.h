/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TILE_PAINT_VALIDATION_H
#define NOX_NATIVE_TILE_PAINT_VALIDATION_H
#include "nox/native_tile_paint.h"
#include "nox/native_graphics_validation.h"
namespace Nox {
inline bool validateNativeTilePaint(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','T','P','T') || input.readUint32LE() != 1 || input.readUint32LE() != 32776) return false;
    byte seed[8192]; for (uint i = 0; i < sizeof(seed); ++i) seed[i] = byte(i * 37 + 11);
    Native::TilePainter painter; byte previous = 255;
    for (uint i = 0; i < 32776; ++i) {
        const byte type = input.readByte(), tile = input.readByte(), terrain = input.readByte(), mode = input.readByte(), phase = input.readByte(), height = input.readByte();
        const uint cell = input.readUint16LE(); Common::String expected;
        if (!readNativeGraphicsDigest(input, expected)) return false;
        if (type != previous) { if (!painter.load(archive, type)) return false; previous = type; }
        Native::HgrFrame frame;
        if (!frame.importImage(seed, sizeof(seed)) || !painter.draw(frame, tile, terrain, mode, phase, cell, height) || nativeGraphicsDigest(frame.data(), sizeof(seed)) != expected) {
            error = Common::String::format("Native tile raster differs at case%u", i); return false;
        }
    }
    Native::HgrFrame unchanged; if (!unchanged.importImage(seed, sizeof(seed))) return false;
    if (painter.draw(unchanged, 1, 1, 0, 0, 187, 16) || painter.draw(unchanged, 1, 1, 0, 0, 93, 0) || memcmp(unchanged.data(), seed, sizeof(seed)) || input.err() || input.pos() != input.size()) return false;
    report += "native-tile-paint\tcases=32776\tstyles=8\tanimation_phases=4\trow_heights=8,16\tfull_plane_bytes=8192\tatomic_refusal=1\tcpu_execution=0\n"; return true;
}
}
#endif
