/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_OPENING_BACKDROP_H
#define NOX_NATIVE_OPENING_BACKDROP_H
#include "nox/native_tile_paint.h"
namespace Nox { namespace Native {
// Authored opening tile scene plus semantic header/sidebar/dialogue chrome.
// Resource addresses are decoded data references, never executable state.
class OpeningBackdrop {
public:
    OpeningBackdrop() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> loader,game,directory;
        if (!_painter.load(archive,0) || !_font.load(archive,1) || !archive.read("NA/LOADER.P",loader) || loader.size()!=10514 ||
            !archive.read("NA/GAME",game) || game.size()!=13824 || !archive.read("NA/BS.ROUTINES.BK2",directory) || directory.size()!=12286) return false;
        const uint pointer=READ_LE_UINT16(directory.data()+0x1956);
        if (pointer<0x6000 || pointer>=0x9600) return false;
        uint end=pointer-0x6000;while (end<game.size() && game[end]) ++end;
        if (end==game.size() || end-(pointer-0x6000)>28) return false;
        _caption.resize(end-(pointer-0x6000));if (!_caption.empty()) memcpy(_caption.data(),game.data()+pointer-0x6000,_caption.size());
        memcpy(_tiles,loader.data()+8294,187);_ready=true;return true;
    }
    bool render(HgrFrame &frame,byte animation=0) const {
        if (!_ready || animation>3) return false;
        HgrFrame next;
        for (uint cell=0;cell<187;++cell) if (!_painter.draw(next,0,_tiles[cell],1,animation,cell,16)) return false;
        if (!renderChrome(next)) return false;
        if (!box(next,4,30,40,152,213,170,129,160)) return false;
        frame=next;return true;
    }
    bool renderChrome(HgrFrame &frame) const {
        if (!_ready) return false;
        if (!box(frame,34,40,4,189,213,170,132,136) || !frame.putByte(39,4,138) || !frame.putByte(39,188,138) ||
            !line(frame,4,0,9) || !line(frame,4,28,6) || !line(frame,188,0,6) || !line(frame,188,32,2)) return false;
        for (uint i=0;i<_caption.size();++i) if (!frame.drawGlyph(_font,_caption[i],12+i,0,0)) return false;
        return true;
    }
    static bool renderNoticeFrame(HgrFrame &frame) {
        return box(frame,24,40,4,112,213,170,129,160);
    }
    static bool renderEndingFrame(HgrFrame &frame) {return box(frame,4,36,44,152,213,170,129,160);}
    bool paintCaption(HgrFrame &frame,uint row,uint left) const {
        if (!_ready || row>=24 || left>40 || _caption.size()>40-left) return false;
        for (uint i=0;i<_caption.size();++i) if (!frame.drawGlyph(_font,_caption[i],left+i,row,0)) return false;
        return true;
    }
    static bool renderConversationFrame(HgrFrame &frame) {
        // GAME7F79 configures the same authored border through row188.
        return box(frame,24,40,4,189,213,170,129,160);
    }
    static bool renderTempleFrame(HgrFrame &frame) {
        // Temple sets GAME7F79's left edge20, then draws its authored separator
        // at pixel107 before text21/14,width18 and the price rows19/21.
        if (!box(frame,20,40,4,189,213,170,129,160)) return false;
        return line(frame,107,20,20);
    }
private:
    static bool line(HgrFrame &frame,uint row,uint start,uint count) {
        for (uint i=0;i<count;++i) if (!frame.putByte(start+i,row,byte(213^((i&1)?127:0)))) return false;
        return true;
    }
    static bool box(HgrFrame &frame,uint left,uint right,uint top,uint bottom,byte even,byte odd,byte sideEven,byte sideOdd) {
        if (left>=right || right>40 || top>=bottom || bottom>192) return false;
        for (uint y=top;y<bottom;++y) for (uint x=left;x<right;++x) {
            const bool edge=y==top || y==bottom-1,side=x==left || x==right-1,alternate=(x-left)&1;
            const byte value=edge?(alternate?odd:even):side?(alternate?sideOdd:sideEven):0;
            if (!frame.putByte(x,y,value)) return false;
        }
        return true;
    }
    bool _ready;byte _tiles[187];Common::Array<byte> _caption;TilePainter _painter;GlyphFont _font;
};
} }
#endif
