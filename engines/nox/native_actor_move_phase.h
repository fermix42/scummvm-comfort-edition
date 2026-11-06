/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_MOVE_PHASE_H
#define NOX_NATIVE_ACTOR_MOVE_PHASE_H
#include "nox/native_world.h"
namespace Nox { namespace Native {
// Intermediate semantic operation after decision/route state changes and
// before position/motion commit. It may replace a move with a ranged request.
class ActorMovePhase {
public:
    virtual ~ActorMovePhase() {}
    virtual bool apply(World &world,uint slot,byte &direction)=0;
};
} }
#endif
