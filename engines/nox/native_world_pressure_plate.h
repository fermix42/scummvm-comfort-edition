/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_PRESSURE_PLATE_H
#define NOX_NATIVE_WORLD_PRESSURE_PLATE_H
#include "nox/native_world_presentation.h"
namespace Nox { namespace Native {
// Source EA35 after completed actor/object paint,before clock and hazards.
class WorldPressurePlate {
public:
    static bool apply(World &world,const WorldScene &scene) {
        if (!world.ready()) return false;
        if (scene.actors.lookup[93]!=255) return true;
        const byte slot=scene.objects.lookup[93];if (slot==255) return true;
        if (slot>252 || (slot&3)) return false;
        const byte *object=world.currentTables()->data()+1024+slot;
        if (object[3]!=54) return true;
        return world.activatePlate(slot)==kGateAllowed;
    }
};
} }
#endif
