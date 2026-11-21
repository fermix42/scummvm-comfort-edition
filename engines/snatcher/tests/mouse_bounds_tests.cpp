/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../backends/graphics/mouse_bounds.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
namespace {unsigned checks=0;void check(bool ok,const char* why){++checks;if(!ok){std::fprintf(stderr,"FAIL mouse active area: %s\n",why);std::exit(1);}}}
int main(){
 using Graphics::mouseOutsideActiveArea;
 check(!mouseOutsideActiveArea(100,50,1,100,50,900,750),"inclusive drawable origin");
 check(!mouseOutsideActiveArea(899,749,1,100,50,900,750),"last interior drawable pixel");
 check(mouseOutsideActiveArea(900,749,1,100,50,900,750)&&mouseOutsideActiveArea(899,750,1,100,50,900,750),"right and bottom edges excluded");
 check(mouseOutsideActiveArea(99,50,1,100,50,900,750)&&mouseOutsideActiveArea(100,49,1,100,50,900,750),"letterbox left and top excluded");
 check(!mouseOutsideActiveArea(50,25,2,100,50,900,750)&&!mouseOutsideActiveArea(449,374,2,100,50,900,750),"double DPI inside letterboxed drawable");
 check(mouseOutsideActiveArea(49,25,2,100,50,900,750)&&mouseOutsideActiveArea(450,25,2,100,50,900,750)&&mouseOutsideActiveArea(50,375,2,100,50,900,750),"double DPI border exclusion");
 check(!mouseOutsideActiveArea(80,40,1.25f,100,50,900,750)&&!mouseOutsideActiveArea(719,599,1.25f,100,50,900,750),"fractional DPI inside corners");
 check(mouseOutsideActiveArea(79,40,1.25f,100,50,900,750)&&mouseOutsideActiveArea(720,40,1.25f,100,50,900,750)&&mouseOutsideActiveArea(80,600,1.25f,100,50,900,750),"fractional DPI letterbox boundaries");
 check(!mouseOutsideActiveArea(1,1,.5f,1,1,2,2)&&mouseOutsideActiveArea(3,1,.5f,1,1,2,2),"same positive half-pixel rounding as SDL");
 check(mouseOutsideActiveArea(-1,0,.25f,0,0,256,224)&&mouseOutsideActiveArea(0,-1,.25f,0,0,256,224),"raw negative event stays outside even when float truncation would clamp to zero");
 check(mouseOutsideActiveArea(1,1,0,0,0,100,100)&&mouseOutsideActiveArea(1,1,-1,0,0,100,100)&&mouseOutsideActiveArea(1,1,std::numeric_limits<float>::infinity(),0,0,100,100)&&mouseOutsideActiveArea(1,1,std::numeric_limits<float>::quiet_NaN(),0,0,100,100),"invalid DPI cannot create a clickable origin");
 check(mouseOutsideActiveArea(32767,1,std::numeric_limits<float>::max(),0,0,100,100),"out-of-int float conversion safely rejected");
 check(mouseOutsideActiveArea(0,0,1,0,0,0,100)&&mouseOutsideActiveArea(0,0,1,0,0,100,0),"empty drawable has no interior");
 // Capture/visibility may retain and clamp this event. Its origin cannot be
 // recovered from that clamped coordinate, hence provenance must precede it.
 const bool beforeClamp=mouseOutsideActiveArea(9,30,1,10,20,100,90);
 check(beforeClamp&&!mouseOutsideActiveArea(10,30,1,10,20,100,90),"pre-clamp provenance survives an otherwise valid captured edge coordinate");
 // Exhaustive 1x geometry cross-check is independent half-open integer
 // containment; fractional behavior is qualified by explicit boundary cases.
 for(int y=-2;y<260;++y)for(int x=-2;x<300;++x){bool inside=x>=17&&x<273&&y>=13&&y<237;check(mouseOutsideActiveArea(x,y,1,17,13,273,237)==!inside,"exhaustive half-open active rectangle");}
 std::printf("mouse active-area geometry: %u checks,0 failures; no SDL or desktop execution\n",checks);
}
