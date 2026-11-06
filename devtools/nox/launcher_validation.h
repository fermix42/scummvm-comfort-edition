/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_LAUNCHER_VALIDATION_H
#define NOX_LAUNCHER_VALIDATION_H
#include "common/events.h"
#include "common/system.h"
#include "gui/dialog.h"
#include "gui/widget.h"
namespace GUI {
// Isolated acceptance driver. It presses real buttons; launcher selection,
// session creation and engine dispatch retain their ordinary implementation.
class NoxLauncherValidationSource : public Common::EventSource {
	Widget &_start;
	Common::EventDispatcher &_dispatcher;
	uint32 _next=0;
	bool _up=false;
	Common::Point _point;
public:
	uint clicks=0;
	NoxLauncherValidationSource(Widget &start,Common::EventManager &events)
		:_start(start),_dispatcher(*events.getEventDispatcher()) {_dispatcher.registerSource(this,false);}
	~NoxLauncherValidationSource(){_dispatcher.unregisterSource(this);}
	bool allowMapping() const override{return false;}
	bool pollEvent(Common::Event &event) override{
		if(_up){_up=false;event.type=Common::EVENT_LBUTTONUP;event.mouse=_point;return true;}
		if(clicks>=2)return false;
		const uint32 now=g_system->getMillis();
		if(!_next){_next=now+200;return false;}
		if(now<_next)return false;
		if(!clicks){
			if(!_start.isEnabled())return false;
			_point=Common::Point(_start.getAbsX()+_start.getWidth()/2,_start.getAbsY()+_start.getHeight()/2);
		}else{
			// CEPlaythroughNameDialog's existing Start button. Keep its
			// generated human name and leave all session logic untouched.
			_point=Common::Point(380,92);
		}
		++clicks;_next=now+200;_up=true;event.type=Common::EVENT_LBUTTONDOWN;event.mouse=_point;return true;
	}
};
static NoxLauncherValidationSource *noxLauncherValidationSource=nullptr;
}
#endif
