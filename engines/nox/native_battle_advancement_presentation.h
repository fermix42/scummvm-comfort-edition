/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_ADVANCEMENT_PRESENTATION_H
#define NOX_NATIVE_BATTLE_ADVANCEMENT_PRESENTATION_H
#include "nox/native_battle_advancement_state.h"
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
// The original character sheet and prayer window, using the player's HDV text.
// Layout/field bindings are data semantics, never executable Apple II code.
class BattleAdvancementPresentation {
public:
    bool load(HdvArchive &archive) {
        Common::Array<byte> combat,aux;
        if (!archive.readRange("NA/SRTN.COMBAT.S",0x41fc,0x210,combat) || !archive.read("NA/BS.AUX.RTN.BK2",aux)) return false;
        const uint addresses[]={0xa00c,0xa0ec,0xa119,0xa145,0xa171,0xa1aa,0xa1b9,0xa1f8,0xa19b,0xa1a0,0xa1a5};
        for (uint i=0;i<ARRAYSIZE(addresses);++i) if (!text(combat,addresses[i]-0xa000,_prayer[i])) return false;
        const uint labels[]={0xe57a,0xe5e6,0xe5f5,0xe604,0xe611,0xe623,0xe635,0xe647,0xe650,0xe65a,0xe664,0xe66e,0xe67a};
        for (uint i=0;i<ARRAYSIZE(labels);++i) if (!text(aux,labels[i]-0xd000+1,_labels[i])) return false;
        const uint status[]={0xe582,0xe595,0xe58c,0xe587},gender[]={0xe5a5,0xe5aa,0xe5b1},race[]={0xe5c1,0xe5c7,0xe5d0,0xe5d4,0xe5dd};
        for (uint i=0;i<4;++i) if (!text(aux,status[i]-0xd000+1,_status[i])) return false;
        for (uint i=0;i<3;++i) if (!text(aux,gender[i]-0xd000+1,_gender[i])) return false;
        for (uint i=0;i<5;++i) if (!text(aux,race[i]-0xd000+1,_race[i])) return false;
        if (!text(aux,0x1569,_footer) || _footer.size()<8) return false;
        _footer.setChar('1',7);
        return true;
    }
    Common::String introduction(const BattleAdvancementState &s) const {
        const uint budget=uint(s.remaining)+s.points[0]+s.points[1]+s.points[2];
        return name(s.proposal)+_prayer[0]+Common::String::format("%u",budget)+"\r\r\r\r\r\r\r";
    }
    bool paint(const GlyphFont &font,const byte *original,const BattleAdvancementState &s,HgrFrame &frame) const {
        const byte *r=s.proposal;
        // Original sheet replaces the tactical field, retaining the party pane.
        for (uint y=0;y<192;++y) for (uint x=0;x<24;++x) if (!frame.putByte(x,y,0)) return false;
        for (uint y=4;y<189;++y) for (uint x=0;x<=20;++x) {
            const byte v=y==4 || y==188?byte((x&1)?170:213):x==0?129:x==20?160:0;
            if (!frame.putByte(x,y,v)) return false;
        }
        if (!line(font,frame,name(r),1,1) || !line(font,frame,_footer,1,22)) return false;
        const uint status=r[0]==255?3:r[0];if (status>3 || r[19]>2 || (r[20]&63)>4) return false;
        if (!line(font,frame,_labels[0]+_status[status],1,4) || !line(font,frame,_labels[1]+_gender[r[19]],1,5) ||
            !line(font,frame,_labels[2]+_race[r[20]&63],1,6) || !line(font,frame,_labels[3]+Common::String::format("%u",uint(original[1])),1,7) ||
            !line(font,frame,_labels[4]+Common::String::format("%5u / %u",READ_LE_UINT16(original+2),READ_LE_UINT16(original+72)),1,9) ||
            !line(font,frame,_labels[5]+Common::String::format("%5u / %u",READ_LE_UINT16(original+5),READ_LE_UINT16(original+67)),1,10) ||
            !line(font,frame,_labels[6]+Common::String::format("%5u / %u",uint(r[4]),uint(r[18])),1,11) ||
            !line(font,frame,_labels[7]+Common::String::format("%5u",uint(r[16]))+_labels[8]+Common::String::format("%u",uint(r[70])),1,13) ||
            !line(font,frame,_labels[9]+Common::String::format("%5u",uint(r[17])),1,14) ||
            !line(font,frame,_labels[10]+Common::String::format("%5u",uint(r[18])),1,15) ||
            !line(font,frame,_labels[11]+recordText(r+90,19),1,17) || !line(font,frame,_labels[12]+recordText(r+109,19),1,18)) return false;
        // INT refreshes MP through the original inverse label printer. That
        // label remains inverse on the confirmation and insufficient-prayer UI.
        if (s.ability==3 && !line(font,frame,_labels[6],1,11,127)) return false;
        // The source leaves the old level/threshold visible until Y commits.
        // Accepted allocations update their ability immediately in the sheet.
        for (uint row=14;row<23;++row) if (!frame.clearTextSpan(25,row,14)) return false;
        uint column=25,row=14;
        if (s.phase==4) {
            row=19;const Common::String message=introduction(s);
            return window(font,frame,message,column,row,uint(s.digits)*100+s.value);
        }
        Common::String prompt;
        if (s.phase==1) {
            if (s.ability>=3) return false;
            if (!window(font,frame,_prayer[1+s.ability],column,row)) return false;
            const uint inputColumn=column,inputRow=row;
            prompt="\r\r"+_prayer[4]+Common::String::format("%u",uint(s.remaining));
            if (!window(font,frame,prompt,column,row)) return false;
            // The original restores the cursor following the deity question,
            // rather than entering the answer after the Mins Left counter.
            if (s.digits && !line(font,frame,Common::String::format(s.digits==2?"%02u":"%u",uint(s.value)),inputColumn,inputRow)) return false;
        } else if (s.phase==2) {
            // Separate source print calls are word boundaries even when the
            // heading fills the row without a trailing CR or space.
            if (!window(font,frame,_prayer[5],column,row)) return false;
            prompt=_prayer[8]+Common::String::format("%u",uint(s.points[0]))+"\r"+
                _prayer[9]+Common::String::format("%u",uint(s.points[1]))+"\r"+_prayer[10]+Common::String::format("%u",uint(s.points[2]))+_prayer[7];
            if (!window(font,frame,prompt,column,row)) return false;
        } else if (s.phase==3) {
            if (!window(font,frame,_prayer[6],column,row)) return false;
        } else return false;
        // Invert the currently edited stat, just as the original field printer.
        if (s.phase==1) {
            const uint abilityRow=13+s.ability;
            const Common::String value=_prayer[8+s.ability];
            if (!line(font,frame,value,1,abilityRow,127)) return false;
        }
        return true;
    }
private:
    static Common::String recordText(const byte *r,uint count) {Common::String out;for (uint i=0;i<count && r[i];++i) out+=char(r[i]&127);return out;}
    static Common::String name(const byte *r) {return recordText(r+75,14);}
    static bool text(const Common::Array<byte> &data,uint offset,Common::String &out) {
        if (offset>=data.size()) return false;out.clear();
        for (uint i=offset;i<data.size();++i) {const byte c=data[i]&127;if (!c) return true;if (c<32 && c!=13) return false;out+=char(c);}return false;
    }
    static bool line(const GlyphFont &font,HgrFrame &frame,const Common::String &value,uint column,uint row,byte inverse=0) {
        if (column+value.size()>40 || row>=24) return false;
        for (uint i=0;i<value.size();++i) if (!frame.drawGlyph(font,byte(value[i]),column+i,row,inverse)) return false;return true;
    }
    static bool window(const GlyphFont &font,HgrFrame &frame,const Common::String &value,uint &column,uint &row,uint limit=65535) {
        uint at=0;
        auto newline=[&]()->bool {column=25;if (++row<22) return true;row=21;return frame.scrollTextRows(25,14,22,14);};
        while (at<value.size() && at<limit) {
            if (value[at]==13) {++at;if (!newline()) return false;continue;}
            uint end=at;while (end<value.size() && value[end]!=13 && end-at<14) {if (value[end++]==' ') break;}
            if (column+end-at>39 && !newline()) return false;
            while (at<end && at<limit) {if (!frame.drawGlyph(font,byte(value[at++]),column++,row,0)) return false;}
        }
        return true;
    }
    Common::String _prayer[11],_labels[13],_status[4],_gender[3],_race[5],_footer;
};
} }
#endif
