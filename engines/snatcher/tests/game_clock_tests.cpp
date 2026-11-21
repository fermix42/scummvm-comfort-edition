/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../game_clock.h"
#include <cstdio>
#include <limits>
int main(){using namespace Snatcher;unsigned checks=0,failures=0;auto check=[&](bool v,const char* m){++checks;if(!v){++failures;std::fprintf(stderr,"FAIL clock: %s\n",m);}};std::string error;GameClock a,b;uint32_t updates=9;
 check(!a.advance(1000,false,updates,error)&&updates==0,"unknown cadence never invents updates");check(!a.configure(0,1,error)&&!a.configure(1001,1,error),"bounded valid rate required");check(a.configure(599227434,10000000,error)&&b.configure(599227434,10000000,error),"observed USA game cadence explicitly supplied");
 check(a.advance(1000,false,updates,error)&&updates==59,"firstsecond fractional update count");uint64_t partitioned=0;for(unsigned i=0;i<1000;++i){check(b.advance(1,false,updates,error),"partitioned advance");partitioned+=updates;}check(partitioned==59&&a.snapshot().fraction==b.snapshot().fraction,"time partition invariance");
 GameClockState before=a.snapshot();check(a.advance(60000,true,updates,error)&&updates==0&&a.snapshot().fraction==before.fraction&&a.snapshot().ticks==before.ticks,"pause preserves fractional state without catchup");
 check(a.advance(9000,false,updates,error)&&a.snapshot().ticks==599,"tensecond rational total");GameClock restored;check(restored.restore(a.snapshot(),error)&&restored.advance(1000,false,updates,error),"owner snapshot restoration");uint32_t other;check(a.advance(1000,false,other,error)&&other==updates&&restored.snapshot().fraction==a.snapshot().fraction,"restored continuation deterministic");
 GameClockState invalid=a.snapshot();invalid.fraction=uint64_t(invalid.denominator)*1000;check(!a.restore(invalid,error),"malformed fraction transactional");invalid=a.snapshot();invalid.ticks=std::numeric_limits<uint64_t>::max();check(a.restore(invalid,error)&&!a.advance(1000,false,updates,error)&&a.snapshot().ticks==invalid.ticks,"tick overflow transactional");
 std::printf("game clock: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
