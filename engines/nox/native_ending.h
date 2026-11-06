/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENDING_H
#define NOX_NATIVE_ENDING_H
#include "nox/native_opening_backdrop.h"
#include "nox/native_text_window.h"
#include "nox/native_world.h"
namespace Nox { namespace Native {
//Authored Ancestral Plane notice1, selected by R:99B6..99CB. The original
//ending repeats its acknowledgement loop permanently; it never awards ordinary
//loot, advances levels or resumes exploration after an acknowledgement.
class EndingPresentation {
public:
    static bool selected(byte map,byte sprite) {return map==8 && sprite==133;}
    static bool revelation(byte map,byte field56,byte field59) {return map==98 && (field56&64) && !(field59&2);}
    static bool paint(HdvArchive &archive,const World &world,HgrFrame &frame) {
        if (!world.ready() || world.mapType()!=0 || world.mapId()!=8) return false;
        return paintNotice(archive,world,1,frame);
    }
    static bool paintRevelation(HdvArchive &archive,const World &world,HgrFrame &frame) {
        return world.ready() && revelation(world.mapId(),world.field(56),world.field(59)) && paintNotice(archive,world,0,frame);
    }
private:
    static bool paintNotice(HdvArchive &archive,const World &world,byte index,HgrFrame &frame) {
        Common::Array<byte> text;if (!world.notice(index,text) || text.empty()) return false;
        HgrFrame image=frame;OpeningBackdrop chrome;TextWindow window;
        if (!chrome.load(archive) || !OpeningBackdrop::renderEndingFrame(image) ||
            !window.begin(archive,text.data(),text.size(),5,30,6,17,11,&image) || !window.ended() || window.waiting() ||
            !window.paint(image) || !chrome.paintCaption(image,6,14)) return false;
        frame=image;return true;
    }
};
} }
#endif
