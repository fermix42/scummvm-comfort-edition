/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_PARTY_CONTROL_H
#define NOX_NATIVE_BATTLE_PARTY_CONTROL_H
#include "common/events.h"
namespace Nox { namespace Native {
class BattlePartyControl {
public:
    static bool requested(const Common::KeyState &key,byte &choice) {
        const char *keys=")!@#$%^";
        const uint16 value=key.ascii?key.ascii:uint16(key.keycode);
        for (byte i=0;i<7;++i) if (value==uint16(keys[i])) {choice=i;return true;}
        if (key.flags&Common::KBD_SHIFT) {
            if (key.keycode==Common::KEYCODE_0) {choice=0;return true;}
            if (key.keycode>=Common::KEYCODE_1 && key.keycode<=Common::KEYCODE_6) {choice=byte(key.keycode-Common::KEYCODE_1+1);return true;}
        }
        return false;
    }
};
} }
#endif
