/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "presentation_bookmark.h"
#include "scene_runtime.h"
#include "interaction.h"
namespace Snatcher {
bool restorePresentationBookmarkPolicies(const NativePresentationBookmark& bookmark,
 SceneRuntime& scene,PointerInteraction& pointer,std::string& error){
 error.clear();
 if(!bookmark.valid||!bookmark.pcmKnown){error="Presentation restore requires a fresh source PCM bookmark capture";return false;}
 if(bookmark.interaction>1){error="Presentation restore requires a recovered interaction policy";return false;}
 SceneRuntime staged=scene;PointerInteraction next=pointer;PointerInteractionSnapshot state;
 if(!next.snapshot(state,error)||!staged.setEnvironmentMode(bookmark.ambient,error))return false;
 state.active=bookmark.interaction==1;
 if(!next.restore(state,error))return false;
 if(bookmark.suppressed)staged.setMusicSuppressed(true);
 scene=staged;pointer=next;return true;
}
}
