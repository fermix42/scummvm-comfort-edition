/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_DIG_H
#define NOX_NATIVE_WORLD_DIG_H
#include "nox/native_world.h"
namespace Nox { namespace Native {
class WorldDig {
public:
    static bool available(byte tools) {return (tools&8)!=0;}
    // 255 is the source's successful coffin write/refresh; all other results
    // are authored refusal message IDs. No generated treasure or time action.
    static byte result(byte type,byte tile,bool object) {
        if (type==3 && (tile==91 || tile==92)) return object?255:18;
        if ((type==3 && tile==93) || (type==2 && (tile<64 || (tile>=70 && tile<74)))) return 27;
        return 18;
    }
    static bool open(World &world,byte slot,byte x,byte y) {
        if (!world.ready() || world.mapType()!=3 || slot==255 || (slot&3)) return false;
        byte *object=world._states[world._current].tables.data()+1024+slot;
        if (object[0]!=x || object[1]!=y || (object[2]!=91 && object[2]!=92)) return false;
        object[3]=41;return true;
    }
};
} }
#endif
