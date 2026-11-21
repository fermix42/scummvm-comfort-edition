/* ScummVM - Graphic Adventure Engine
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef BACKENDS_GRAPHICS_MOUSE_BOUNDS_H
#define BACKENDS_GRAPHICS_MOUSE_BOUNDS_H

#include <float.h>
#include <limits.h>

namespace Graphics {
// Preserve pre-clamp active-area provenance independently of cursor visibility
// and capture. Match SDL's existing window-to-drawable float rounding exactly.
inline bool mouseOutsideActiveArea(int windowX,int windowY,float dpiScale,
                                  int left,int top,int right,int bottom) {
 if(windowX<0||windowY<0||!(dpiScale>0&&dpiScale<=FLT_MAX)||right<=left||bottom<=top)return true;
 const float scaledX=windowX*dpiScale+0.5f,scaledY=windowY*dpiScale+0.5f;
 if(!(scaledX>=0&&double(scaledX)<=double(INT_MAX))||!(scaledY>=0&&double(scaledY)<=double(INT_MAX)))return true;
 const int x=int(scaledX),y=int(scaledY);
 return x<left||x>=right||y<top||y>=bottom;
}
}
#endif
