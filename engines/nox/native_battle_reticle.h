/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_RETICLE_H
#define NOX_NATIVE_BATTLE_RETICLE_H
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
// Authored mask/pixel frames; the Apple II row-pointer/page-switch machinery
// becomes direct native composition. No input, actor state or RNG is changed.
class BattleReticle {
public:
    BattleReticle() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;
        if (!archive.readRange("NA/BS.AUX.RTN.BK2",0x2e0f,256,data) || data.size()!=256) return false;
        memcpy(_frames,data.data(),256);_ready=true;return true;
    }
    bool paint(byte cell,byte phase,HgrFrame &image) const {
        if (!_ready || cell>=187 || cell%17>10 || phase>3) return false;
        const byte *data=_frames+phase*64;const uint left=(cell%17)*2,top=8+(cell/17)*16;
        for (uint y=0;y<16;++y) for (uint x=0;x<2;++x) {
            const byte old=image.data()[HgrFrame::rowOffset(top+y)+left+x];
            if (!image.putByte(left+x,top+y,byte((old&data[y*2+x])|data[32+y*2+x]))) return false;
        }
        return true;
    }
private:
    bool _ready;byte _frames[256];
};
} }
#endif
