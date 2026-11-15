/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PARTY_MOVE_COMPLETION_H
#define NOX_NATIVE_PARTY_MOVE_COMPLETION_H
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
// F053..F0E9: complete the actual position body before deferred battle entry.
// Quiet recovery terrain keeps the existing party image and countdown.
class PartyMoveCompletion {
public:
    static bool apply(byte arrived,byte quiet,byte &recovery,const HgrFrame &prior,HgrFrame &frame,bool *retained=nullptr) {
        if (recovery>16) return false;
        const bool preserve=arrived==quiet;
        if (preserve) {
            for (uint row=88;row<104;++row) for (uint column=16;column<18;++column)
                if (!frame.putByte(column,row,prior.data()[HgrFrame::rowOffset(row)+column])) return false;
        } else recovery=16;
        if (retained) *retained=preserve;return true;
    }
};
} }
#endif
