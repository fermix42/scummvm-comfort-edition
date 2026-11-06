/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TEXT_GLYPH_H
#define NOX_NATIVE_TEXT_GLYPH_H
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
// Authored glyph/cursor ownership. Wrapping, scrolling, paging and message
// continuation belong to the text-window owner, not this drawing primitive.
class TextGlyphSurface {
public:
    TextGlyphSurface() : _variant(0),_column(0),_row(0),_mask(3),_inverse(0) {}
    bool initialize(HdvArchive &archive, byte variant) {
        if (_variant || !_font.load(archive,variant)) return false; _variant = variant; return true;
    }
    bool importPages(const byte *first, const byte *second, uint size) {
        return _variant && size == 8192 && _pages[0].importImage(first,size) && _pages[1].importImage(second,size);
    }
    bool setCursor(byte column, byte row) {
        if (!_variant || column >= 40 || row >= 24) return false; _column = column; _row = row; return true;
    }
    bool setStyle(byte mask, byte inverse) {
        if (!_variant || mask > 3 || (inverse != 0 && inverse != 127)) return false; _mask = mask; _inverse = inverse; return true;
    }
    bool clearTextSpan(uint column, uint row, uint width) {
        if (!_variant || column >= 40 || row >= 24 || !width || column + width > 40) return false;
        return _pages[0].clearTextSpan(column,row,width) && _pages[1].clearTextSpan(column,row,width);
    }
    bool scrollTextRows(uint column,uint firstRow,uint endRow,uint width) {
        return _variant && _pages[0].scrollTextRows(column,firstRow,endRow,width) && _pages[1].scrollTextRows(column,firstRow,endRow,width);
    }
    bool print(byte character) {
        if (!_variant) return false;
        if (character == 0x8d) return true;
        if (_column >= 40 || _row >= 24 || !_font.glyph(character)) return false;
        for (uint page = 0; page < 2; ++page) if (_mask & (1U << page)) {
            if (!_pages[page].drawGlyph(_font,character,_column,_row,_inverse)) return false;
        }
        ++_column; _mask = 3; _inverse = 0; return true;
    }
    byte column() const { return _column; } byte row() const { return _row; }
    byte mask() const { return _mask; } byte inverse() const { return _inverse; }
    const byte *page(uint index) const { return index < 2 ? _pages[index].data() : nullptr; }
    bool save(Common::WriteStream &out) const {
        if (!_variant) return false;
        out.writeUint32BE(MKTAG('N','T','G','S')); out.writeUint16LE(1); out.writeByte(_variant); out.writeByte(_column); out.writeByte(_row); out.writeByte(_mask); out.writeByte(_inverse);
        out.write(_pages[0].data(),8192); out.write(_pages[1].data(),8192); return !out.err();
    }
    bool load(Common::SeekableReadStream &in, HdvArchive &archive) {
        if (in.size() - in.pos() != 16395 || in.readUint32BE() != MKTAG('N','T','G','S') || in.readUint16LE() != 1) return false;
        TextGlyphSurface next; const byte variant = in.readByte(); if (!next.initialize(archive,variant)) return false;
        next._column = in.readByte(); next._row = in.readByte(); next._mask = in.readByte(); next._inverse = in.readByte();
        if (next._column > 40 || next._row >= 24 || next._mask > 3 || (next._inverse != 0 && next._inverse != 127)) return false;
        byte data[8192];
        for (uint page = 0; page < 2; ++page) if (in.read(data,8192) != 8192 || !next._pages[page].importImage(data,8192)) return false;
        if (in.err()) return false; *this = next; return true;
    }
private:
    GlyphFont _font; HgrFrame _pages[2]; byte _variant,_column,_row,_mask,_inverse;
};
} }
#endif
