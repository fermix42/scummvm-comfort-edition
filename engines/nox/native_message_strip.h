/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MESSAGE_STRIP_H
#define NOX_NATIVE_MESSAGE_STRIP_H
#include "nox/native_text_glyph.h"
namespace Nox { namespace Native {
class FixedMessageCatalog {
public:
    bool load(HdvArchive &archive) {
        Common::Array<byte> data,hazards;
        if (!archive.read("NA/GAME",data) || data.size()!=13824 ||
            READ_LE_UINT16(data.data()+0x1c63+21*2)!=0xf806 || READ_LE_UINT16(data.data()+0x1c63+22*2)!=0xf810 ||
            !archive.readRange("NA/BS.ROUTINES.BK2",0x2806,20,hazards) || hazards.size()!=20 || hazards[9] || hazards[19]) return false;
        _game=data;_hazardMessages=hazards;return true;
    }
    bool text(byte id, Common::Array<byte> &out) const {
        if (_game.size() != 13824 || id >= 41) return false;
        // World damage IDs21/22 are immutable resident-resource strings:
        // Drowning!/Attacked!, Main F806/F810. This typed catalog binding
        // does not expose banked memory or execute a message pointer.
        if (id==21 || id==22) {
            if (_hazardMessages.size()!=20) return false;
            out.resize(9);memcpy(out.data(),_hazardMessages.data()+(id-21)*10,9);return true;
        }
        const uint encoded = READ_LE_UINT16(_game.data() + 0x1c63 + id * 2);
        // Only immutable GAME-backed messages. Banked text, dynamic pointer
        // entries and external buffers require their own typed context.
        if (encoded < 0x6000 || encoded >= 0x9600) return false;
        const uint start = encoded - 0x6000; uint end = start;
        while (end < _game.size() && _game[end]) ++end;
        if (end >= _game.size() || end - start > 25) return false;
        Common::Array<byte> next; next.resize(end - start);
        if (next.size()) memcpy(next.data(),_game.data() + start,next.size()); out = next; return true;
    }
private:
    Common::Array<byte> _game,_hazardMessages;
};
inline bool showFixedMessageStrip(const FixedMessageCatalog &catalog, byte id, TextGlyphSurface &surface) {
    Common::Array<byte> text; if (!catalog.text(id,text)) return false;
    for (uint i = 0; i < text.size(); ++i) if ((text[i] & 127) < 32 && text[i] != 0x8d) return false;
    if (!surface.clearTextSpan(7,23,25) || !surface.setCursor(byte(7 + (25 - text.size()) / 2),23) || !surface.setStyle(3,0)) return false;
    for (uint i = 0; i < text.size(); ++i) if (!surface.print(text[i])) return false;
    return surface.setStyle(3,0);
}
} }
#endif
