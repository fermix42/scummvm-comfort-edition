/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_MOUSE_INPUT_H
#define SNATCHER_MOUSE_INPUT_H
#include <cstdint>
#include <vector>
namespace Snatcher {
enum MouseContextKind { kMouseNone, kMouseDialogue, kMouseCommands, kMouseEntry, kMouseTitle, kMouseOptions, kMouseSaveSlots, kMouseInvestigation, kMouseCredits, kMouseShooting, kMousePaused, kMouseContinue, kMouseStartupSkip };
struct MouseRect { int x,y,w,h; MouseRect(int a=0,int b=0,int c=0,int d=0):x(a),y(b),w(c),h(d){} bool contains(int px,int py)const{return px>=x&&py>=y&&px-x<w&&py-y<h;} };
struct MouseTarget { MouseRect rect; unsigned id; bool enabled; MouseTarget(MouseRect r=MouseRect(),unsigned i=0,bool e=true):rect(r),id(i),enabled(e){} };
struct MouseContext { MouseContextKind kind; uint64_t generation; bool ready,canCancel,deferActivation; std::vector<MouseTarget> targets; MouseContext():kind(kMouseNone),generation(0),ready(false),canCancel(false),deferActivation(false){} };
enum MouseEventKind { kMouseMove,kMouseLeftDown,kMouseLeftUp,kMouseRightDown,kMouseRightUp,kMouseWheel,kMouseFocusLost,kMouseFocusGained };
struct MouseEvent { MouseEventKind kind;int x,y,wheel;bool valid; MouseEvent(MouseEventKind k=kMouseMove,int a=0,int b=0,bool v=true,int w=0):kind(k),x(a),y(b),wheel(w),valid(v){} };
enum MouseActionKind { kMouseNoAction,kMouseHover,kMouseActivate,kMouseCancel,kMouseScroll };
struct MouseAction { MouseActionKind kind;unsigned target;int delta;MouseAction():kind(kMouseNoAction),target(0),delta(0){} };
// Coordinates are already transformed by ScummVM into game space. No host
// coordinate clamping: letterbox/outside events carry valid=false.
class MouseInput {
public:
 MouseInput();
 MouseAction handle(const MouseContext& context,const MouseEvent& event);
 void reset();
 bool held()const{return _left||_right;}
 bool focused()const{return _focused;}
private:
 bool _left,_right,_focused,_known;uint64_t _generation;MouseContextKind _kind;
};
// Exact DialogueWindow rows; page arrows use id=count / count+1. Hidden
// commands never acquire a target. Half-open rectangles prevent shared edges.
std::vector<MouseTarget> commandMouseTargets(unsigned count,unsigned selected,unsigned variant);
}
#endif
