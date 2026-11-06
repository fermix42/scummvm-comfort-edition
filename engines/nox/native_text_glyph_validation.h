/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TEXT_GLYPH_VALIDATION_H
#define NOX_NATIVE_TEXT_GLYPH_VALIDATION_H
#include "nox/native_text_glyph.h"
#include "nox/native_graphics_validation.h"
namespace Nox {
inline bool readTextGlyphDigest(Common::SeekableReadStream &in, Common::String &digest) {
    digest.clear(); for (uint i = 0; i < 16; ++i) digest += Common::String::format("%02x",uint(in.readByte())); return !in.err();
}
inline Common::String textGlyphDigest(const Native::TextGlyphSurface &surface) {
    byte data[16384]; memcpy(data,surface.page(0),8192); memcpy(data + 8192,surface.page(1),8192); return nativeGraphicsDigest(data,16384);
}
inline bool validateNativeTextGlyph(HdvArchive &archive, Common::SeekableReadStream &input, Common::SeekableReadStream &continuation,
        Common::SaveFileManager &files, bool cold, Common::String &report, Common::String &error) {
    Common::String first,second,sourceFirst,sourceSecond;
    if (continuation.readUint32BE() != MKTAG('N','T','G','C') || continuation.readUint32LE() != 2 || !readTextGlyphDigest(continuation,sourceFirst) || !readTextGlyphDigest(continuation,sourceSecond) || !readTextGlyphDigest(continuation,first) || !readTextGlyphDigest(continuation,second)) return false;
    const byte expectedColumn = continuation.readByte(); if (expectedColumn != 21 || continuation.pos() != continuation.size()) return false;
    if (cold) {
        Native::TextGlyphSurface surface; Common::InSaveFile *in = files.openForLoading("nox-native-text-glyph.bin");
        const bool ok = in && surface.load(*in,archive); delete in;
        if (!ok || surface.column() != 20 || surface.row() != 10 || textGlyphDigest(surface) != first || !surface.print(0xc2) ||
            surface.column() != expectedColumn || textGlyphDigest(surface) != second || !surface.print(0x8d) || surface.column() != expectedColumn || textGlyphDigest(surface) != second) return false;
        report += "native-text-glyph\tphase=cold\tactual_result_restored=1\tcontinued_glyph_and_CR=1\tpartial_owner_only=1\tcpu_execution=0\n"; return true;
    }
    if (input.readUint32BE() != MKTAG('N','T','G','L') || input.readUint32LE() != 2 || input.readUint32LE() != 110592) return false;
    Native::TextGlyphSurface surfaces[2]; if (!surfaces[0].initialize(archive,1) || !surfaces[1].initialize(archive,2)) return false;
    byte firstPage[8192],secondPage[8192]; memset(firstPage,170,8192); memset(secondPage,85,8192); bool checkpoint = false;
    for (uint i = 0; i < 110592; ++i) {
        const byte variant = input.readByte(),character = input.readByte(),row = input.readByte(),column = input.readByte(),mask = input.readByte(),inverse = input.readByte(),afterColumn = input.readByte(),afterMask = input.readByte(),afterInverse = input.readByte();
        Common::String originalCpuDigest; byte pixels[16];
        if (variant < 1 || variant > 2 || row >= 24 || column >= 40 || !readTextGlyphDigest(input,originalCpuDigest) || input.read(pixels,16) != 16) return false;
        byte projected[16384]; memcpy(projected,firstPage,8192); memcpy(projected + 8192,secondPage,8192);
        for (uint page = 0; page < 2; ++page) for (uint line = 0; line < 8; ++line) projected[page * 8192 + Native::HgrFrame::rowOffset(row * 8 + line) + column] = pixels[page * 8 + line];
        const Common::String expected = nativeGraphicsDigest(projected,16384);
        Native::TextGlyphSurface &surface = surfaces[variant - 1];
        if (!surface.importPages(firstPage,secondPage,8192) || !surface.setCursor(column,row) || !surface.setStyle(mask,inverse) || !surface.print(character) ||
            surface.column() != afterColumn || surface.row() != row || surface.mask() != afterMask || surface.inverse() != afterInverse || textGlyphDigest(surface) != expected) {
            error = Common::String::format("Native text glyph differs case%u variant%u character%u row%u column%u mask%u inverse%u",i,uint(variant),uint(character),uint(row),uint(column),uint(mask),uint(inverse)); return false;
        }
        if (variant == 1 && character == 0xc1 && row == 10 && column == 19 && mask == 3 && inverse == 0) {
            if (checkpoint || expected != first) return false;
            Common::OutSaveFile *out = files.openForSaving("nox-native-text-glyph.bin",false); if (!out) return false;
            bool ok = surface.save(*out); out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false; checkpoint = true;
        }
    }
    if (!checkpoint || input.err() || input.pos() != input.size()) return false;
    report += "native-text-glyph\tphase=warm\tcases=110592\tfonts=2\tcharacters=96\ttext_rows=24\tcolumns=3\tpage_masks=4\tnormal_reverse=2\tcomplete_owned_HGR_pages=2\tnative_opaque_gaps_preserved=1\toriginal_CPU_full_digests_retained=1\tcursor_style_results=1\tfull_window_UI=0\tcpu_execution=0\n"; return true;
}
}
#endif
