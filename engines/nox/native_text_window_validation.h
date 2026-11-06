/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TEXT_WINDOW_VALIDATION_H
#define NOX_NATIVE_TEXT_WINDOW_VALIDATION_H
#include "nox/native_text_window.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeTextWindow(HdvArchive &archive,Common::SeekableReadStream &input,Common::SaveFileManager &files,bool cold,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','T','W','P') || input.readUint32LE()!=1 || input.readUint32LE()!=4) return false;
    const byte left=input.readByte(),width=input.readByte(),top=input.readByte(),bottom=input.readByte(),paging=input.readByte();
    Native::TextWindow window;
    if (cold) {
        Common::InSaveFile *in=files.openForLoading("nox-native-text-window.bin");if (!in) return false;
        const bool ok=window.load(*in,archive);delete in;if (!ok) return false;
    } else {
        Common::Array<byte> loader;if (!archive.read("NA/LOADER.P",loader) || loader.size()!=10514 || loader[8281]) return false;
        if (!window.begin(archive,loader.data()+7567,714,left,width,top,bottom,paging)) return false;
    }
    uint compared=0;
    for (uint page=0;page<4;++page) {
        const uint position=input.readUint16LE();const byte column=input.readByte(),row=input.readByte(),lines=input.readByte(),waiting=input.readByte(),ended=input.readByte();
        if (page) { Common::Event key;key.type=Common::EVENT_KEYDOWN;key.kbd=Common::KeyState(Common::KEYCODE_RETURN,13);if (!window.handle(key)) return false; }
        if (window.position()!=position || window.column()!=column || window.row()!=row || window.lines()!=lines || byte(window.waiting())!=waiting || byte(window.ended())!=ended) {
            error=Common::String::format("Native text window state differs page%u pointer%u/%u cursor%u,%u/%u,%u lines%u/%u waiting%u/%u",page,window.position(),position,uint(window.column()),uint(window.row()),uint(column),uint(row),uint(window.lines()),uint(lines),uint(window.waiting()),uint(waiting));return false;
        }
        for (uint plane=0;plane<2;++plane) for (uint y=uint(top)*8;y<(uint(bottom)+1)*8;++y) for (uint x=left;x<uint(left)+width;++x) {
            const byte expected=input.readByte(),actual=window.page(plane)[Native::HgrFrame::rowOffset(y)+x];++compared;
            if (actual!=expected) { error=Common::String::format("Native text window pixel differs page%u plane%u x%u y%u native%u original%u",page,plane,x,y,uint(actual),uint(expected));return false; }
        }
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES);if (!window.save(before)) return false;
        if (window.waiting()) {
            const Common::KeyCode ignored[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_LEFT,Common::KEYCODE_RIGHT,Common::KEYCODE_BACKSPACE};
            for (uint i=0;i<ARRAYSIZE(ignored);++i) { Common::Event key;key.type=Common::EVENT_KEYDOWN;key.kbd=Common::KeyState(ignored[i]);if (!window.handle(key)) return false; }
            Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES);if (!window.save(after) || before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size())) { error="Native text MORE refused-key state changed";return false; }
        }
        Common::MemoryReadStream codec(before.getData(),before.size());if (!window.load(codec,archive)) return false;
        if (!cold && !page) {
            Common::OutSaveFile *out=files.openForSaving("nox-native-text-window.bin",false);if (!out) return false;
            bool ok=window.save(*out);out->finalize();ok=ok&&!out->err();delete out;if (!ok) return false;
        }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+=Common::String::format("native-text-window\tphase=%s\tpages=4\twindow_bytes=%u\towned_scroll_word_wrap=1\tarrow_backspace_refusal=1\tactual_more_restored=%u\tcpu_execution=0\tfull_opening=0\n",cold?"cold":"warm",compared,uint(cold));return true;
}
}
#endif
