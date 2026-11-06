/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_STATUS_H
#define NOX_NATIVE_WORLD_STATUS_H
#include "nox/native_new_game.h"
#include "nox/native_opening_backdrop.h"
#include "nox/native_world_presentation.h"
#include "nox/native_lighting.h"
namespace Nox { namespace Native {
// Original authored caption/chrome, initial on-foot party image and owned
// Gold/Food/clock presentation. Transport/custom party appearances are separate.
class WorldStatus {
public:
    WorldStatus() : _ready(false),_type(255) {}
    bool load(HdvArchive &archive,byte type=8) {
        Common::Array<byte> game,footer;MapAssetRange range;
        if (!_chrome.load(archive) || !_font.load(archive,1) || !_painter.load(archive,type) || !archive.read("NA/GAME",game) ||
            !resolveMapAssets(game,type,range) || !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        if (!label(game,0x12ac,_gold) || !label(game,0x12a7,_food) || !label(game,0x1553,_am) || !label(game,0x1556,_pm)) return false;
        memcpy(_footer,footer.data(),72);_type=type;_ready=true;return true;
    }
    bool initializeFrame(HgrFrame &frame) const {
        HgrFrame next;if (!_ready || !_chrome.renderChrome(next)) return false;frame=next;return true;
    }
    bool paint(const NewGame &game,const WorldScene &scene,const LightState &light,bool notice,HgrFrame &frame) const {
        if (!_ready || !game.ready() || game.world.mapType()!=_type) return false;
        const byte appearance=game.appearance;
        if (appearance!=246 && appearance!=119 && appearance!=123 && appearance!=124 && appearance!=127 && appearance!=128 && appearance!=238) return false;
        HgrFrame next=frame;const byte ground=scene.actors.terrain[93];
        if (scene.actors.lookup[93]==255) {
            if (appearance==119) {
                const uint offsets[]={0,1,17,18};
                for (uint i=0;i<4;++i) if (!_painter.draw(next,byte(appearance+i),scene.actors.terrain[93+offsets[i]],light.mode,0,93+offsets[i],16)) return false;
            } else if (appearance!=246) {
                if (!_painter.draw(next,appearance,ground,light.mode,0,93,16)) return false;
            } else if (ground>=_footer[28] && ground<_footer[29]) {
                if (!_painter.draw(next,0,ground,light.mode,0,93,16)) return false;
            } else if ((ground>=_footer[40] && ground<_footer[41]) || (ground>=_footer[42] && ground<_footer[43])) {
                if (!_painter.draw(next,byte(ground+1),ground,light.mode,0,93,16)) return false;
            } else if (!_painter.draw(next,game.appearance,ground,light.mode,0,93,(ground==65 || ground==66 || ground==76)?8:16)) return false;
        }
        if (!notice && !paintStatus(game.gold,game.food,game.clock,next)) return false;
        frame=next;return true;
    }
    bool paintStatus(uint16 gold,uint16 food,const TurnClock &clock,HgrFrame &frame) const {
        if (!_ready) return false;ClockAdvance check;if (!advanceTurnClock(clock,check)) return false;
        for (uint row=1;row<13;++row) if (!frame.clearTextSpan(35,row,4)) return false;
        // Numeric fields have five positions; labels and clock have four.
        const Common::String goldText=Common::String::format("%u",uint(gold)),foodText=Common::String::format("%u",uint(food));
        // Original numeric padding advances the cursor without painting over
        // the border at column34. A fifth actual digit may occupy that cell.
        if (!draw(frame,_gold,35,1) || !draw(frame,_food,35,4) ||
            !draw(frame,goldText,39-goldText.size(),2) || !draw(frame,foodText,39-foodText.size(),5)) return false;
        const uint hour=(clock.hour>>4)*10+(clock.hour&15),minute=(clock.minute>>4)*10+(clock.minute&15);
        const uint display=hour%12?hour%12:12;
        return draw(frame,Common::String::format("%02u%02u",display,minute),35,7) && draw(frame,hour>=12?_pm:_am,36,8);
    }
private:
    static bool label(const Common::Array<byte> &data,uint start,Common::String &out) {
        if (start>=data.size()) return false;uint end=start;while (end<data.size() && data[end]) ++end;
        if (end==data.size() || end-start>4) return false;
        out.clear();for (uint i=start;i<end;++i) out+=char(data[i]);return true;
    }
    bool draw(HgrFrame &frame,const Common::String &value,uint column,uint row) const {
        for (uint i=0;i<value.size();++i) if (!frame.drawGlyph(_font,byte(value[i]),column+i,row,0)) return false;return true;
    }
    bool _ready;byte _type;byte _footer[72];Common::String _gold,_food,_am,_pm;OpeningBackdrop _chrome;GlyphFont _font;TilePainter _painter;
};
} }
#endif
