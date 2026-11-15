/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_STARTUP_RANDOM_H
#define NOX_NATIVE_STARTUP_RANDOM_H
#include "common/random.h"
#include "nox/native_random.h"
namespace Nox { namespace Native {
// Hardware timing is replaced by the engine entropy service. Gameplay still
// uses the original-qualified owned RNG; replay inputs deliberately stay fixed.
class StartupRandom {
public:
    static bool initialize(GameplayRandom &random,bool replay,uint16 *observed=nullptr) {
        uint16 entropy=119;
        if (!replay) {Common::RandomSource source("nox-new-game");entropy=uint16(source.getRandomNumber(65535));}
        if (!random.initializeEntropy(entropy)) return false;
        if (observed) *observed=entropy;return true;
    }
};
} }
#endif
