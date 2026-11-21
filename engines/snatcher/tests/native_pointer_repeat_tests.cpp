/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../interaction.h"
#include <cstdio>
#include <cstdlib>
using namespace Snatcher;
static unsigned checks;
static void check(bool ok,const char* text){++checks;if(!ok){std::fprintf(stderr,"FAILED: %s\n",text);std::exit(1);}}
int main(){
 NativePointerRepeat repeat;const uint8_t right=16|8,left=16|4;
 check(repeat.poll(0,true,right,3)==128,"initial held input has no repeat delay");
 for(unsigned t=1;t<12;++t)check(!repeat.poll(t,true,right,3),"same hold cannot repeat within native visit interval");
 check(repeat.poll(12,true,right,3)==128,"ordinary held repeat");
 check(repeat.poll(13,true,left,3)==128,"rapid reversal is immediately responsive");
 check(repeat.poll(14,true,0,3)==0&&repeat.poll(14,true,right,3)==128,"release and repress have no stale edge");
 check(repeat.poll(60014,true,right,3)==128&&!repeat.poll(60014,true,right,3),"late visit stays bounded without work debt");
 check(!repeat.poll(60015,false,right,3)&&repeat.poll(60015,true,right,3)==128,"nested or other mode resets repeat owner");
 repeat.reset();check(repeat.poll(UINT32_MAX-5,true,right,3)==128&&repeat.poll(6,true,right,3)==128,"host timestamp wrap uses unsigned elapsed");
 for(int speed=0;speed<8;++speed){repeat.reset();check(repeat.poll(0,true,right,speed)==uint32_t(32*(speed+1)),"native speed setting quarter steps");}
 repeat.reset();check(repeat.poll(0,true,right,90)==128,"unknown speed retains native default");
 PointerInteraction pointer;std::string error;std::vector<uint8_t> map(720);for(unsigned i=0;i<720;++i)map[i]=uint8_t(i%11);
 check(pointer.loadMap(map,error)&&pointer.enter(error),"actual pointer owner");PointerInteractionResult result;
 check(pointer.sample(false,0,1,result,error),"initial source cache publication");PointerInteractionSnapshot before,after;
 check(pointer.snapshot(before,error),"initial snapshot");
 repeat.reset();for(unsigned t=0;t<60;t+=12)check(pointer.sample(false,right,repeat.poll(t,true,right,3),result,error)&&result.adjusting&&!result.published,"held native input retains original cache");
 check(pointer.snapshot(after,error)&&after.x16==5*65536&&after.selectedRegion==before.selectedRegion,"five admitted visits move five pixels with stale source cache");
 check(pointer.sample(false,0,1,result,error)&&result.published&&pointer.snapshot(after,error)&&after.selectedRegion==map[10*36+19],"release publishes actual displaced hotspot");
 auto saved=after;pointer.leave();repeat.reset();check(!repeat.poll(200,false,right,3)&&pointer.restore(saved,error),"mode exit and cold position restore");
 check(pointer.sample(true,right,repeat.poll(201,false,right,3),result,error)&&pointer.snapshot(after,error)&&after.x16==saved.x16,"nested menu does not move source pointer");
 // Preserve the source's ordered opposite-direction operations at its limits.
 saved.x16=-71*65536;check(pointer.restore(saved,error)&&pointer.sample(false,16|4|8,128,result,error)&&pointer.snapshot(after,error)&&after.x16==saved.x16,"right then left retains lower-bound source ordering");
 saved.x16=69*65536-512;check(pointer.restore(saved,error)&&pointer.sample(false,16|4|8,128,result,error)&&pointer.snapshot(after,error)&&after.x16==saved.x16-512,"upper boundary retains ordered source asymmetry");
 std::printf("PASS %u native pointer repeat checks\n",checks);return 0;
}
