/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DAYLIGHT_H
#define NOX_NATIVE_DAYLIGHT_H
#include "nox/native_clock.h"
#include "nox/native_visibility.h"
namespace Nox { namespace Native {
// Ordinary world-input daylight producer, distinct from instant map-entry light.
// Transport speed is the source's 367E control, not the current light level.
class Daylight {
public:
    static bool advance(byte type,bool permanentDark,byte speed,const TurnClock &clock,LightState &light) {
        if (light.mode>3 || light.level>4 || clock.hour>=0x24 || (clock.hour&15)>9 ||
                clock.minute>=0x60 || (clock.minute&15)>9) return false;
        if (type==2 || permanentDark) return true;
        const bool boundary=clock.minute==0x30 || (clock.minute==0x31 && speed>=4);
        if (!boundary) return true;
        if (clock.hour==5) light=LightState(0,0,0);
        else if (clock.hour==0x20) light=LightState(2,4,0);
        return true;
    }
    static bool permanent(byte map,const byte *maps,uint size,bool &dark) {
        if (!maps) return false;
        bool next=false;
        for (uint i=0;i<size;++i) {
            if (maps[i]&128) {dark=next;return true;}
            if (maps[i]==map && map) next=true;
        }
        return false;
    }
};
} }
#endif
