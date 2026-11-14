/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_STARTUP_MENU_H
#define NOX_NATIVE_STARTUP_MENU_H
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
class StartupMenu {
public:
    static bool render(const GlyphFont &font,HgrFrame &frame) {
        HgrFrame next;
        if (!text(next,font,13,5,"NOX ARCHAIST") || !text(next,font,10,10,"N  NEW GAME") ||
            !text(next,font,10,12,"C  CONTINUE / LOAD") || !text(next,font,10,14,"Q  QUIT")) return false;
        frame=next;return true;
    }
private:
    static bool text(HgrFrame &frame,const GlyphFont &font,uint x,uint y,const char *value) {
        for (uint i=0;value[i];++i) if (!frame.drawGlyph(font,byte(value[i]),x+i,y,0)) return false;
        return true;
    }
};
} }
#endif
