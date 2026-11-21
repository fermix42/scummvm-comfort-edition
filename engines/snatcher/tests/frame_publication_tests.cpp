/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../frame_publication.h"
#include <cstdio>
#include <algorithm>
int main(){
 unsigned failures=0,checks=0;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",name);}};
 Snatcher::FramePublication frame;frame.prepare(4,3);
 std::fill(frame.pixels(),frame.pixels()+12,0);
 check(frame.publish(),"first black frame must upload");
 uint16_t *storage=frame.pixels();
 for(unsigned i=0;i<10000;++i){frame.prepare(4,3);if(frame.publish())++failures;}
 check(storage==frame.pixels(),"unchanged dimensions reuse conversion storage");
 check(frame.uploads()==1&&frame.unchanged()==10000,"idle visits submit exactly once");
 frame.pixels()[11]=1;check(frame.publish(),"last pixel change uploads");
 check(!frame.publish(),"changed frame is retained");
 frame.pixels()[11]=0;check(frame.publish(),"removed mouse/overlay pixel uploads");
 frame.invalidate();check(frame.publish(),"backend invalidation forces unchanged upload");
 frame.prepare(3,4);check(frame.publish(),"same pixel count with new geometry uploads");
 check(!frame.publish(),"new geometry retained");
 frame.prepare(2,2);check(frame.publish(),"resize uploads");
 std::printf("frame publication: %u checks and 10000 idle visits, %u failures\n",checks,failures);
 return failures?1:0;
}
