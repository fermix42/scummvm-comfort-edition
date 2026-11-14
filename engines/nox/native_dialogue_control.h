/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_CONTROL_H
#define NOX_NATIVE_DIALOGUE_CONTROL_H
#include "nox/native_random.h"
namespace Nox { namespace Native {
// Original Talk B253: negative continues, zero ends, positive ends on draw<T.
class DialogueControl {
public:
    static bool resolve(byte control,GameplayRandom *random,bool &terminal) {
        if (control&128) {terminal=false;return true;}
        if (!control) {terminal=true;return true;}
        if (!random) return false;
        byte roll;if (!random->next(1,255,roll)) return false;
        terminal=roll<control;return true;
    }
};
} }
#endif
