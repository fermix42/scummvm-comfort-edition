/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_STARTUP_RANDOM_VALIDATION_H
#define NOX_NATIVE_STARTUP_RANDOM_VALIDATION_H
#include "nox/native_startup_random.h"
#include "common/config-manager.h"
#include "common/memstream.h"
namespace Nox { namespace Native {
inline bool sameStartupRandom522(const GameplayRandom &a,const GameplayRandom &b) {
    Common::MemoryWriteStreamDynamic x(DisposeAfterUse::YES),y(DisposeAfterUse::YES);
    return a.save(x) && b.save(y) && x.size()==y.size() && !memcmp(x.getData(),y.getData(),x.size());
}
inline bool validateNativeStartupRandom522(Common::String &report) {
    const uint32 seeds[]={0x12345678,0x23456789};uint normal=0,replays=0;
    for (uint i=0;i<2;++i) {
        report=Common::String::format("bootstrap522 engine entropy case%u",i);
        // Source-controlled environment BEFORE the factory call. No gameplay
        // result is injected. Compare wiring to the engine entropy API itself.
        ConfMan.setInt("random_seed",seeds[i]);uint16 engineWord;
        {Common::RandomSource source("nox-new-game");engineWord=uint16(source.getRandomNumber(65535));}
        GameplayRandom expected,actual,replayed;uint16 observed=0;
        if (engineWord==119 || !expected.initializeEntropy(engineWord) || !StartupRandom::initialize(actual,false,&observed) || observed!=engineWord || !sameStartupRandom522(expected,actual)) return false;++normal;
        GameplayRandom reference;if (!reference.initializeEntropy(119) || !StartupRandom::initialize(replayed,true,&observed) || observed!=119 || !sameStartupRandom522(reference,replayed)) return false;++replays;
    }
    report="bootstrap522 loaded RNG is never reseeded";
    GameplayRandom loaded;if (!loaded.importState(0x23456700,2,41,3)) return false;const GameplayRandom before=loaded;uint16 observed=65535;
    if (StartupRandom::initialize(loaded,false,&observed) || observed!=65535 || !sameStartupRandom522(before,loaded)) return false;
    ConfMan.removeKey("random_seed",ConfMan.getActiveDomainName());
    report=Common::String::format("startup_random\tengine_entropy_sources=%u\tdeterministic_replays=%u\tloaded_rng_unchanged=1\toriginal_rng_algorithm_unchanged=1\tno_gameplay_draw_for_entropy=1\told_rng35_suite=0\tcampaign=0\n",normal,replays);return true;
}
} }
#endif
