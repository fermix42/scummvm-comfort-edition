/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MESSAGE_STRIP_VALIDATION_H
#define NOX_NATIVE_MESSAGE_STRIP_VALIDATION_H
#include "nox/native_message_strip.h"
#include "nox/native_text_glyph_validation.h"
namespace Nox {
inline bool validateNativeMessageStrip(HdvArchive &archive, Common::SeekableReadStream &input, Common::SeekableReadStream &initial,
        Common::SeekableReadStream &continuation, Common::SaveFileManager &files, bool cold, Common::String &report, Common::String &error) {
    Native::FixedMessageCatalog catalog; if (!catalog.load(archive)) return false;
    Common::String first,second;
    if (continuation.readUint32BE() != MKTAG('N','M','S','C') || continuation.readUint32LE() != 1 || !readTextGlyphDigest(continuation,first) || !readTextGlyphDigest(continuation,second)) return false;
    const byte column = continuation.readByte(),row = continuation.readByte(),mask = continuation.readByte(),inverse = continuation.readByte();
    if (continuation.pos() != continuation.size()) return false;
    if (cold) {
        Native::TextGlyphSurface surface; Common::InSaveFile *in = files.openForLoading("nox-native-message-strip.bin");
        const bool ok = in && surface.load(*in,archive); delete in;
        if (!ok || textGlyphDigest(surface) != first || !Native::showFixedMessageStrip(catalog,37,surface) || textGlyphDigest(surface) != second ||
            surface.column() != column || surface.row() != row || surface.mask() != mask || surface.inverse() != inverse) return false;
        report += "native-message-strip\tphase=cold\tactual_result_restored=1\tcontinued_message36_to37=1\tcaller_turn_UI=0\tcpu_execution=0\n"; return true;
    }
    if (input.readUint32BE() != MKTAG('N','M','S','T') || input.readUint32LE() != 1 || input.readUint32LE() != 76 || initial.size() != 16384) return false;
    byte background[16384]; if (initial.read(background,16384) != 16384) return false;
    Native::TextGlyphSurface surfaces[2]; if (!surfaces[0].initialize(archive,1) || !surfaces[1].initialize(archive,2)) return false;
    bool checkpoint = false;
    for (uint i = 0; i < 76; ++i) {
        const byte variant = input.readByte(),id = input.readByte(),length = input.readByte(); Common::Array<byte> expectedText,actualText; expectedText.resize(length);
        if (variant < 1 || variant > 2 || input.read(expectedText.data(),length) != length || !catalog.text(id,actualText) || actualText.size() != length || (length && memcmp(actualText.data(),expectedText.data(),length))) return false;
        const byte afterColumn = input.readByte(),afterRow = input.readByte(),afterMask = input.readByte(),afterInverse = input.readByte(); Common::String originalCpu,expected;
        if (!readTextGlyphDigest(input,originalCpu) || !readTextGlyphDigest(input,expected)) return false;
        Native::TextGlyphSurface &surface = surfaces[variant - 1];
        if (!surface.importPages(background,background + 8192,8192) || !Native::showFixedMessageStrip(catalog,id,surface) || textGlyphDigest(surface) != expected ||
            surface.column() != afterColumn || surface.row() != afterRow || surface.mask() != afterMask || surface.inverse() != afterInverse) {
            error = Common::String::format("Native fixed message strip differs case%u variant%u id%u cursor%u/%u row%u/%u",i,uint(variant),uint(id),uint(surface.column()),uint(afterColumn),uint(surface.row()),uint(afterRow)); return false;
        }
        if (variant == 1 && id == 36) {
            if (checkpoint || expected != first) return false;
            Common::OutSaveFile *out = files.openForSaving("nox-native-message-strip.bin",false); if (!out) return false;
            bool ok = surface.save(*out); out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false; checkpoint = true;
            Native::HgrFrame frame; Common::Array<byte> rgb; if (!frame.importImage(surface.page(0),8192)) return false; frame.renderRGB(rgb);
            out = files.openForSaving("nox-native-message36.ppm",false); if (!out) return false;
            out->writeString("P6\n560 384\n255\n"); out->write(rgb.data(),rgb.size()); out->finalize(); ok = !out->err(); delete out; if (!ok) return false;
        }
    }
    if (!checkpoint || input.err() || input.pos() != input.size()) return false;
    report += "native-message-strip\tphase=warm\tcases=76\timmutable_messages=38\tfonts=2\tapproach_message36=1\tactual_saved_scene_background=1\tclear_center_text_cursor=1\tcomplete_owned_HGR_pages=2\tcaller_turn_UI=0\tdynamic_banked_messages=0\tcpu_execution=0\n"; return true;
}
}
#endif
