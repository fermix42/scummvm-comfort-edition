/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_STARTUP_MENU_H
#define NOX_NATIVE_STARTUP_MENU_H
#include "nox/native_opening_backdrop.h"
#include "common/events.h"
namespace Nox { namespace Native {
class StartupMenu {
public:
    enum Result { kStay,kNew,kContinue,kImport };
    StartupMenu() : _page(0),_offset(0),_pageEnd(0),_startFlags(0) {}
    bool load(HdvArchive &archive) {
        if (!_background.load(archive) || !archive.read("NA/LOADER.P",_loader) || _loader.size()!=10514) return false;
        const uint offsets[]={0xffd,0x100b,0x101c,0x102e,0x1045,0x1058,0x1068,0x1073,0x1082};
        for (uint i=0;i<9;++i) if (!read(offsets[i],_labels[i])) return false;
        return true;
    }
    byte startFlags() const {return _startFlags;}
    Result handle(const Common::Event &event,byte &selected,bool saved) {
        if (event.type!=Common::EVENT_KEYDOWN) return kStay;
        const Common::KeyCode key=event.kbd.keycode;
        if (_page==2) {
            if (key==Common::KEYCODE_ESCAPE) {_page=0;return kStay;}
            if (key==Common::KEYCODE_BACKSPACE || key==Common::KEYCODE_LEFT) {
                if (!_code.empty()) _code.deleteLastChar();
            } else if (key==Common::KEYCODE_RETURN) {
                if (_code.empty()) {_page=0;return kStay;}
                Common::String digits=_code;while (digits.size()<4) digits="0"+digits;
                const byte first=byte((digits[0]-'0')*16+digits[1]-'0');
                const byte second=byte((digits[2]-'0')*16+digits[3]-'0');
                for (uint i=0;i<3;++i) if (_loader[0x205a+i*2]==first && _loader[0x205b+i*2]==second) {
                    if (i<2) {_startFlags=byte(i?10:6);open(i?0x10b7:0x10a1);}
                    else {_startFlags=0;_page=0;}
                    return kStay;
                }
                _page=0;
            } else if (event.kbd.ascii>='0' && event.kbd.ascii<='9' && _code.size()<4) _code+=char(event.kbd.ascii);
            return kStay;
        }
        if (_page==1) {
            if (_pageEnd<_body.size() && key!=Common::KEYCODE_ESCAPE) {_offset=_pageEnd;return kStay;}
            _page=0;return kStay;
        }
        if (key==Common::KEYCODE_UP) selected=byte((selected+6)%7);
        else if (key==Common::KEYCODE_DOWN) selected=byte((selected+1)%7);
        else if (key==Common::KEYCODE_RETURN || key==Common::KEYCODE_1 || key==Common::KEYCODE_2) {
            if (selected==0) return saved?kContinue:kNew;
            if (selected==1) return kNew;
            if (selected==2) return kImport;
            if (selected==3) open(0x1322);
            else if (selected==4) open(0x13ba);
            else if (selected==5) open(0x156b);
            else {_page=2;_code.clear();}
        }
        return kStay;
    }
    bool render(const GlyphFont &font,HgrFrame &frame,byte selected=0,byte phase=0) {
        if (_loader.size()!=10514 || selected>6 || phase>3) return false;
        HgrFrame next;if (!_background.render(next,phase)) return false;
        if (!_page || _page==2) {
            for (uint i=0;i<7;++i) if (!center(font,next,_labels[i],7+i,i==selected?127:0)) return false;
            if (!center(font,next,_labels[7],16) || !center(font,next,_labels[8],17)) return false;
            if (_page==2) {
                Common::String label;
                if (!read(0x1095,label) || !line(font,next,label,7,23) || !line(font,next,_code,19,23)) return false;
            }
        } else {
            uint row=6,column=5;uint cursor=_offset;
            while (cursor<_body.size() && row<17) {
                const byte c=_body[cursor]&127;
                if (c==13) {++cursor;++row;column=5;continue;}
                uint end=cursor;
                while (end<_body.size() && _body[end]!=13 && _body[end]!=' ') ++end;
                if (end<_body.size() && _body[end]==' ') ++end;
                if (column>5 && column+end-cursor>29) {++row;column=5;if (row==17) break;}
                while (cursor<end) {
                    if (column==29) {++row;column=5;if (row==17) break;}
                    if (!next.drawGlyph(font,_body[cursor++],column++,row,0)) return false;
                }
            }
            _pageEnd=cursor;
        }
        frame=next;return true;
    }
private:
    bool read(uint offset,Common::String &text) const {
        text.clear();
        for (uint i=offset;i<_loader.size();++i) {
            const byte c=_loader[i]&127;if (!c) return true;
            if (c!=13 && c<32) return false;text+=char(c);
        }
        return false;
    }
    void open(uint offset) {_page=1;_offset=0;read(offset,_body);}
    static bool line(const GlyphFont &font,HgrFrame &frame,const Common::String &text,uint column,uint row,byte inverse=0) {
        for (uint i=0;i<text.size();++i) if (!frame.drawGlyph(font,text[i],column+i,row,inverse)) return false;
        return true;
    }
    static bool center(const GlyphFont &font,HgrFrame &frame,const Common::String &text,uint row,byte inverse=0) {
        return text.size()<24 && line(font,frame,text,5+(24-text.size())/2,row,inverse);
    }
    OpeningBackdrop _background;Common::Array<byte> _loader;Common::String _labels[9],_body,_code;
    byte _page;uint _offset,_pageEnd;byte _startFlags;
};
} }
#endif
