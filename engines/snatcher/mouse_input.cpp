/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mouse_input.h"
namespace Snatcher {
MouseInput::MouseInput(){reset();}
void MouseInput::reset(){_left=_right=false;_focused=true;_known=false;_generation=0;_kind=kMouseNone;}
MouseAction MouseInput::handle(const MouseContext& c,const MouseEvent& e){
 MouseAction a;
 if(e.kind==kMouseFocusLost){_focused=false;return a;}
 if(e.kind==kMouseFocusGained){_focused=true;return a;}
 // Always consume releases, even outside, during transitions and while paused.
 if(e.kind==kMouseLeftUp){_left=false;return a;}
 if(e.kind==kMouseRightUp){_right=false;return a;}
 bool duplicate=false;
 if(e.kind==kMouseLeftDown){duplicate=_left||_right;_left=true;}
 if(e.kind==kMouseRightDown){duplicate=_right||_left;_right=true;}
 if(!_known||_generation!=c.generation||_kind!=c.kind){_known=true;_generation=c.generation;_kind=c.kind;}
 bool deferred=c.deferActivation&&(e.kind==kMouseLeftDown||e.kind==kMouseRightDown);
 if(!_focused||duplicate||(!c.ready&&!deferred)||c.kind==kMouseNone||!e.valid)return a;
 const MouseTarget* hit=nullptr;for(const MouseTarget& t:c.targets)if(t.enabled&&t.rect.contains(e.x,e.y)){hit=&t;break;}
 if(e.kind==kMouseRightDown){if(c.canCancel)a.kind=kMouseCancel;return a;}
 if(e.kind==kMouseWheel){if(held())return a;if(hit&&e.wheel){a.kind=kMouseScroll;a.target=hit->id;a.delta=e.wheel;}return a;}
 if(hit){a.target=hit->id;if(e.kind==kMouseLeftDown)a.kind=kMouseActivate;else if(e.kind==kMouseMove&&!held())a.kind=kMouseHover;}
 return a;
}
std::vector<MouseTarget> commandMouseTargets(unsigned n,unsigned selected,unsigned variant){
 std::vector<MouseTarget> out;if(!n||n>8||selected>=n)return out;unsigned cap=variant?6:7,first=cap-1;bool paged=n>cap,second=paged&&selected>=first;int top=143+(variant?16:0);
 for(unsigned i=0;i<n;++i){if(paged&&(second?i<first:i>=first))continue;unsigned row=second?i-first+1:i;out.push_back(MouseTarget(MouseRect(16,top+int(row)*9,240,9),i));}
 if(paged)out.push_back(MouseTarget(MouseRect(116,top+int(second?0:first)*9,32,9),n+(second?1:0)));
 return out;
}
}
