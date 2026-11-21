/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_QUICK_DISPLAY_H
#define NOX_NATIVE_QUICK_DISPLAY_H
#include "nox/native_quick_battle.h"
namespace Nox { namespace Native {
// Ephemeral presentation only. The original scroll controls change the delay
// per character, not per attack. Keep its exact 14EB cycle budget at1.023MHz.
// The last remaining bytes are held back until the original printer budget elapses.
class QuickBattleDisplay {
public:
    QuickBattleDisplay() : cursor(0),stamp(0),holding(false),shown(0),speed(2),paused(false),remaining(0),fraction(0),characters(0),announcedTurn(0xffffffffU),victoryAnnounced(false) {}
    void reset() {*this=QuickBattleDisplay();}
    static uint32 characterCycles(byte speed) {return speed?1284U*(MIN<byte>(speed,3)*32U)+43U:18U;}
    bool ready(uint32 now,bool paced) {
        if (paced && paused) {stamp=now;return false;}
        if (!holding) return true;
        if (!paced) {holding=false;remaining=0;return true;}
        const uint32 elapsed=now-stamp;stamp=now;
        const uint64 budget=uint64(fraction)+uint64(elapsed)*1023;
        const uint32 cycles=characterCycles(speed);const uint64 complete=budget/cycles;
        if (complete>=remaining) {remaining=0;fraction=0;holding=false;return true;}
        remaining-=uint(complete);fraction=uint32(budget%cycles);return false;
    }
    bool control(byte key,uint32 now,bool paced) {
        // GAME $7B06 waits for and consumes the next key, regardless of its value.
        if (paused) {
            paused=false;
            stamp=now;
            return true;
        }
        if (key!='+' && key!='=' && key!='-' && key!='_' && key!='8') return false;
        ready(now,paced);
        if (key=='8') paused=true;
        else if (key=='+' || key=='=') {if (speed) --speed;}
        else if (speed<3) ++speed;
        // A partially elapsed source character retains its progress at the
        // new rate rather than restarting the complete action.
        fraction=MIN<uint32>(fraction,characterCycles(speed)-1);stamp=now;return true;
    }
    bool pending() const {return cursor<events.size();}
    void queue(const Common::Array<QuickBattleAction> &next) {events=next;cursor=0;}
    const QuickBattleAction &show(uint32 now) {stamp=now;holding=true;++shown;remaining=characters=0;fraction=0;return events[cursor++];}
    void beginText() {characters=2;for (uint i=3;i<6;++i) characters+=lines[i].size();remaining=characters;}
    void message(const Common::String &text,uint32 now) {
        stamp=now;holding=true;++shown;fraction=0;characters=text.size();
        lines[3]+=text;
        remaining=characters;
    }
    Common::Array<QuickBattleAction> events;uint cursor;uint32 stamp;bool holding;uint32 shown;byte speed;bool paused;uint remaining;uint32 fraction;uint characters;Common::String lines[6];uint32 announcedTurn;bool victoryAnnounced;
};
} }
#endif
