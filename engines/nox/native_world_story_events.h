/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_STORY_EVENTS_H
#define NOX_NATIVE_WORLD_STORY_EVENTS_H
#include "common/scummsys.h"
namespace Nox { namespace Native {
class WorldStoryEvents {
public:
    static bool shadowApproach(byte type,byte map,byte sector,byte temple,byte discovered) {
        return type<3 && !map && sector==169 && (temple&64) && !(discovered&1);
    }
};
} }
#endif
