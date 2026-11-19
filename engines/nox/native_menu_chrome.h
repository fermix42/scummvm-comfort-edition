/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MENU_CHROME_H
#define NOX_NATIVE_MENU_CHROME_H
#include "nox/native_graphics.h"
#include "nox/native_party.h"
namespace Nox { namespace Native {
// Consistent keyboard navigation on native full-screen menus. These functions
// paint only; all selection and gameplay transactions remain with their owners.
class MenuChrome {
public:
    static bool text(const GlyphFont &font,HgrFrame &frame,uint x,uint y,const Common::String &value,byte inverse=0) {
        if (x<1 || x>38 || y>22) return false;
        for (uint i=0;i<value.size() && x+i<39;++i) if (!frame.drawGlyph(font,byte(value[i])<32?byte(' '):byte(value[i]),x+i,y,inverse)) return false;return true;
    }
    static bool separator(HgrFrame &frame,uint y) {
        for (uint x=0;x<40;++x) if (!frame.putByte(x,y,byte(x&1?170:213))) return false;return true;
    }
    static bool begin(const GlyphFont &font,HgrFrame &frame,const Common::String &title,uint footer=163) {
        HgrFrame next;
        for (uint y=0;y<192;++y) if (!next.putByte(0,y,129) || !next.putByte(39,y,160)) return false;
        if (!separator(next,0) || !separator(next,35) || !separator(next,footer) || !separator(next,191) || !text(font,next,2,1,title)) return false;frame=next;return true;
    }
    static bool row(const GlyphFont &font,HgrFrame &frame,uint y,const Common::String &value,bool selected) {
        for (uint i=0;i<38;++i) if (!frame.drawGlyph(font,i<value.size()?byte(value[i]):byte(' '),1+i,y,selected?127:0)) return false;return true;
    }
    static bool party(const GlyphFont &font,const Party &party,HgrFrame &frame,uint selected) {
        for (uint i=0;i<party.memberCount();++i) {
            const byte *r=party.member(i)->bytes;Common::String name;
            for (uint j=75;j<90 && r[j];++j) name+=char(r[j]&127);
            if (!row(font,frame,5+i*2,Common::String::format("%u %-15s HP%u MP%u",i+1,name.c_str(),READ_LE_UINT16(r+2),r[4]),i==selected)) return false;
        }
        return true;
    }
};
} }
#endif
