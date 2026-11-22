/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scene_runtime.h"
#include "native_visual_jobs.h"
#include "runtime.h"
#include "text_entry.h"
#include <algorithm>
#include <sstream>
namespace Snatcher {
namespace {
uint16_t sourceWord(const uint8_t* p){return uint16_t(unsigned(p[0])*256+p[1]);}
int32_t wrapped(int32_t a,int32_t b){uint32_t value=uint32_t(a)+uint32_t(b);return value<=0x7fffffff?int32_t(value):int32_t(int64_t(value)-0x100000000LL);}
int32_t fixedWord(uint16_t value){int16_t signedValue=value<0x8000?int16_t(value):int16_t(int(value)-65536);return int32_t(signedValue)*65536;}
int integer(int32_t value){return value>=0?value/65536:int(-((-int64_t(value)+65535)/65536));}
}
SceneRuntime::SceneRuntime():_random(nullptr),_trace(nullptr){reset();}
void SceneRuntime::resetForModule(){SceneGraphicsQueue staging=_graphicsTasks;staging.cancelPending();const auto copies=_graphicsCopies;const uint8_t nameBlankPhase=_nameBlankPhase;const bool mainNameBlankPending=_mainNameBlankPending;const bool fontClear=_fontClearPending,fontModeKnown=_fontModeKnown,fontFixedWidth=_fontFixedWidth;const auto renderer=_renderer;const bool fullPlanePublicationEnabled=_fullPlanePublicationEnabled;const bool scrollPublicationKnown=_scrollPublicationKnown;const int16_t frameY[2]={_frameScrollY[0],_frameScrollY[1]};const bool displayedKnown=_displayedSubmissionKnown,pendingKnown=_environmentSubmissionKnown;const auto displayed=_displayedSubmission,pending=_environmentSubmission;EnvironmentVisualState history=_environmentState;bool known=_rowSamplingKnown,all=_allRowsKnown,compat=_legacyUnknownRows;uint8_t tail=_tailRowsKnown;int16_t rows[2][256];for(unsigned p=0;p<2;++p)for(unsigned r=0;r<256;++r)rows[p][r]=_rowX[p][r];reset();_nameBlankPhase=nameBlankPhase;_mainNameBlankPending=mainNameBlankPending;_fontClearPending=fontClear;_fontModeKnown=fontModeKnown;_fontFixedWidth=fontFixedWidth;_graphicsTasks=staging;_graphicsCopies=copies;_renderer=renderer;_fullPlanePublicationEnabled=fullPlanePublicationEnabled;_scrollPublicationKnown=scrollPublicationKnown;for(unsigned p=0;p<2;++p)_frameScrollY[p]=frameY[p];_displayedSubmissionKnown=displayedKnown;_displayedSubmission=displayed;_environmentSubmissionKnown=pendingKnown;_environmentSubmission=pending;_environmentState=history;_rowSamplingKnown=known;_allRowsKnown=all;_legacyUnknownRows=compat;_tailRowsKnown=tail;for(unsigned p=0;p<2;++p)for(unsigned r=0;r<256;++r)_rowX[p][r]=rows[p][r];}
bool SceneRuntime::establishBootSubmissionHistory(std::string& error){
 error.clear();if(_renderer.configured()||_environmentMode||_environmentSubmissionKnown){error="Boot submission allocation requires a fresh application owner";return false;}
 for(const auto& slot:_environmentState.previous)if(slot.known){error="Boot submission allocation cannot replace retained history";return false;}
 // A zero encoded sprite entry has the128-dot display origin removed here.
 // These are game compositor fields, not a BIOS or shared-memory emulator.
 for(auto& slot:_environmentState.previous){slot.known=true;slot.piece=PlacedPiece{-128,-128,8,8,0};}
 _displayedSubmission.clear();_displayedSubmissionKnown=true;_scrollPublicationKnown=true;_frameScrollY[0]=_frameScrollY[1]=0;
 // The qualified cold Word RAM allocation clears all four decoded caches.
 for(unsigned channel=0;channel<4;++channel)if(!_graphicsTasks.setStaging(channel,std::vector<uint8_t>(8192,0),error))return false;
 return true;
}
bool SceneRuntime::validateModuleEnvironmentHistory(uint16_t mode,std::string& error)const{
 error.clear();if(!mode)return true;
 if(mode>9||!_environmentTables.known){error="Environmental module restore requires its authored visual owner";return false;}
 const unsigned count=mode<=2?unsigned(_environmentTables.counts[mode])+1:33;
 // ED28 establishes slot0 itself; every other consumed entry must have an
 // actual prior submission owner before the first empty module frame.
 for(unsigned i=1;i<count;++i)if(!_environmentState.previous[i].known){std::ostringstream message;message<<"Environmental module restore requires prior submission slot "<<i;error=message.str();return false;}
 return true;
}
// SUB1013E and actor9D share cache980F; only actor9D has suppression.
bool SceneRuntime::requestCommandMusic(uint8_t command,std::string& error){error.clear();if(_musicCommands.size()>=64){error="unconsumed music request capacity";return false;}_lastMusicCommand=command==0x3d?0:command;_lastMusicKnown=true;_musicCommands.push_back(SceneMusicCommand(command,false));return true;}
void SceneRuntime::reset(){_pendingDialogueView=0;_fullPlanePublicationEnabled=true;_nameBlankPhase=0;_mainNameBlankPending=false;_scrollPublicationKnown=false;_frameScrollY[0]=_frameScrollY[1]=0;_fontModeKnown=_fontFixedWidth=false;_fontClearPending=false;_transferCompletionPending=false;_transferCopyOffset=0;_graphicsCopies.clear();_workerVisualBusy=_menuCursorPending=false;_menuCursorPendingPieces.clear();_displayedSubmissionKnown=false;_displayedSubmission.clear();_environmentTables=EnvironmentVisualTables();_environmentState=EnvironmentVisualState();_environmentMode=0;_environmentSubmissionKnown=false;_environmentSubmission.clear();_saveStamp=0;_saveResource=0xffff;_saveScene=0;_animationClock=AnimationClockCapture();_cdClock=0;_cdClockKnown=_cadenceKnown=false;_cadenceMask=0;_actors.clear();for(unsigned i=0;i<64;++i)_slots[i]=ActorSlot();_palettes.clear();_scroll.clear();_lineScroll.reset();_fullPlaneScrollOverride=_legacyLineComposition=_rowSamplingKnown=false;_frameLineSampling=_pendingSamplingReset=_allRowsKnown=_legacyUnknownRows=false;_pendingSecondaryMode=_tailRowsKnown=0;for(auto& plane:_rowX)for(auto& x:plane)x=0;_secondaryScrollFrozen=false;_secondaryScrollMemo=0;_phoneVisual=PhoneVisualState();_phoneFrame=PhoneFramePublication();_pendingPhoneRequest=0;_pendingPhoneKnown=false;_renderer.clear();_graphicsTasks.clear();_knownColors.assign(128,false);_pendingKnownColors.assign(128,false);_visibleKnownColors.assign(128,false);_pendingColors.assign(128,0);_visibleColors.assign(128,0);_displayedColors.assign(64,0);_frameDisplayColors.assign(64,0);_frameLowerColors.clear();_alternateColors.clear();_displayedHistoryKnown=_displayedDirty=_alternatePaletteActive=_computedPrimaryPosted=_pendingPrimaryPosted=_gameLayoutsKnown=false;_tallGameLayout=_wideGameLayout=SceneLayout();_pendingGameLayout=0;_menuCursorPieces.clear();_menuCursorOwned=_visualBusy=false;_visualThreshold=_visualOrigin=_visualCountdown=0;_visualCommand=_palettePublicationMode=_pendingPalettePublication=0;_visualKnown=_palettePublicationDirty=false;_dialogueLayout=DialogueLayout();_baseLayout=SceneLayout();_assets=SceneAssets();_pendingTransfers.clear();_effects.clear();_musicCommands.clear();_musicSuppressed=_musicSuppressionKnown=_lastMusicKnown=_lastSecondaryMusicKnown=false;_lastMusicCommand=_lastSecondaryMusicCommand=0;_transferChannel=_pendingTransferChannel=0;_transferChannelKnown=_pendingTransferChannelKnown=false;_scrollEnabled=false;_paletteKnown=_paletteSuppressed=_paletteSuppressionKnown=_effectsSuppressed=_soundKnown=_textPaletteRequested=_palettePosted=false;_sceneFlags=0;_sceneFlagsKnown=_palettePostedKnown=_palettePolicyKnown=_freshPaletteRequested=_keepSecondaryPalette=_textPaletteKnown=_resourceChangePending=false;_textPalette.clear();_paletteBank=_currentActor=_audioActivity=0;}
bool SceneRuntime::configure(const SceneLayout& layout,std::string& error){if(!_renderer.configure(layout,error))return false;_baseLayout=layout;return true;}
bool SceneRuntime::setPalette(const std::vector<uint16_t>& colors,unsigned bank,std::string& error){PaletteController palette=_palettes;SceneRenderer renderer=_renderer;if(!palette.setColors(colors,error)||!renderer.setPalette(colors,bank,error))return false;_palettes=palette;_renderer=renderer;_paletteBank=bank;_paletteKnown=true;_knownColors.assign(128,true);_pendingKnownColors=_visibleKnownColors=_knownColors;_pendingColors=_visibleColors=colors;_displayedColors.assign(colors.begin()+bank,colors.begin()+bank+64);_frameDisplayColors=_displayedColors;_displayedHistoryKnown=true;_displayedDirty=false;_frameLowerColors.clear();return true;}
bool SceneRuntime::establishFreshPalette(std::string& error){std::vector<uint16_t> colors=_palettes.colors();for(unsigned i=0;i<64;++i)colors[i]=0;for(unsigned i=80;i<128;++i)colors[i]=0;PaletteController palette=_palettes;SceneRenderer renderer=_renderer;if(!palette.setColors(colors,error)||!renderer.setPalette(colors,0,error))return false;_palettes=palette;_renderer=renderer;for(unsigned i=0;i<64;++i)_knownColors[i]=true;for(unsigned i=80;i<128;++i)_knownColors[i]=true;for(unsigned i=0;i<128;++i)if(i<64||i>=80){_pendingColors[i]=_visibleColors[i]=0;_pendingKnownColors[i]=_visibleKnownColors[i]=true;}_paletteBank=0;_paletteKnown=true;_palettePosted=true;_palettePostedKnown=true;_palettePolicyKnown=true;_freshPaletteRequested=_keepSecondaryPalette=false;_paletteSuppressed=true;_paletteSuppressionKnown=true;_displayedColors.assign(64,0);_frameDisplayColors=_displayedColors;_frameLowerColors.clear();_displayedHistoryKnown=true;_displayedDirty=false;return true;}
bool SceneRuntime::loadGameLayoutPresets(const uint8_t* mainIp,size_t size,std::string& error){
 SceneLayout tall,wide;if(!readSceneLayoutPreset(mainIp,size,kSceneTallLayout,tall,error)||!readSceneLayoutPreset(mainIp,size,kSceneWideLayout,wide,error))return false;_tallGameLayout=tall;_wideGameLayout=wide;_gameLayoutsKnown=true;return true;
}
bool SceneRuntime::requestGameLayout(uint16_t word,std::string& error){error.clear();if(!word)return true;if(!_gameLayoutsKnown){error="game layout request requires original Main presets";return false;}_pendingGameLayout=word;return true;}
bool SceneRuntime::establishSceneReset(SceneLayoutPreset preset,std::string& error){
 SceneRenderer renderer=_renderer;if(!renderer.establishSceneReset(preset,error))return false;_renderer=renderer;_scrollPublicationKnown=true;_frameScrollY[0]=_frameScrollY[1]=0;_displayedColors.assign(64,0);_frameDisplayColors=_displayedColors;_frameLowerColors.clear();_displayedHistoryKnown=true;_fullPlaneScrollOverride=true;_rowSamplingKnown=true;_frameLineSampling=false;for(auto& plane:_rowX)for(unsigned row=0;row<(preset==kSceneSquareLayout?256u:128u);++row)plane[row]=0;if(preset==kSceneSquareLayout){_allRowsKnown=true;_legacyUnknownRows=false;_tailRowsKnown=255;}_legacyLineComposition=false;return true;
}
void SceneRuntime::inheritLicenseRowHistory(){for(auto& plane:_rowX)for(auto& x:plane)x=0;_allRowsKnown=_rowSamplingKnown=true;_legacyUnknownRows=_legacyLineComposition=false;_tailRowsKnown=255;}
bool SceneRuntime::applyGameLayout(GameLayoutRequest request,std::string& error){
 error.clear();if(request==kGameLayoutUnchanged)return true;if(!_gameLayoutsKnown){error="game layout replacement requires source presets";return false;}
 SceneRuntime next=*this;SceneLayout layout=request==kGameLayoutTall?next._tallGameLayout:next._wideGameLayout;
 if(!next._renderer.configure(layout,error)||!next.establishSceneReset(request==kGameLayoutTall?kSceneTallLayout:kSceneWideLayout,error))return false;
 // Main15B0 resets displayed allocations, vertical and sprite publication.
 // It does not clear the Sub actor records or either computed color bank.
 next._baseLayout=layout;*this=next;return true;
}
bool SceneRuntime::setAlternatePalette(bool active,const std::vector<uint16_t>& colors,std::string& error){
 error.clear();if(active){if(colors.size()!=64){error="alternate palette requires64 original colors";return false;}for(uint16_t value:colors){uint8_t r,g,b;if(!sceneColor(value,kSceneRGB888,r,g,b,error))return false;}}
 if(_alternatePaletteActive&&!active)_displayedDirty=true;_alternatePaletteActive=active;_alternateColors=active?colors:std::vector<uint16_t>();return true;
}
bool SceneRuntime::publishDisplayedPalette(bool primaryChanged,std::string& error){
 error.clear();_displayedDirty=_displayedDirty||primaryChanged;
 // Main1A44 tests continuous policy and pointing-owner before consuming dirty.
 bool upload=_palettePublicationMode||_alternatePaletteActive||_displayedDirty;
 if(upload){bool known=true;if(_alternatePaletteActive)_displayedColors=_alternateColors;else{for(unsigned i=_paletteBank;i<_paletteBank+64;++i)known=known&&_visibleKnownColors[i];if(known)_displayedColors.assign(_visibleColors.begin()+_paletteBank,_visibleColors.begin()+_paletteBank+64);}if(!_palettePublicationMode&&!_alternatePaletteActive)_displayedDirty=false;_displayedHistoryKnown=known;}
 _frameDisplayColors=_displayedColors;_frameLowerColors.clear();
 // Main08E4 arms the common lower publisher for a view tag or continuous policy.
 if((_phoneFrame.known?_phoneFrame.tag:_phoneVisual.tag)||_palettePublicationMode){std::string detail;if(!_displayedHistoryKnown||!readLowerViewPalette(_visibleColors,_visibleKnownColors,_frameLowerColors,detail)){_displayedHistoryKnown=false;_frameLowerColors.clear();if((_phoneFrame.known?_phoneFrame.tag:_phoneVisual.tag)>=0x16){error="lower phone view requires known primary and secondary publication history";return false;}}else std::copy(_frameLowerColors.begin(),_frameLowerColors.end(),_displayedColors.begin()+16);}
 return true;
}
bool SceneRuntime::setTextPalette(const std::vector<uint16_t>& colors,std::string& error){error.clear();if(colors.size()!=8){error="text palette requires eight source colors";return false;}for(uint16_t word:colors){uint8_t r,g,b;if(!sceneColor(word,kSceneRGB888,r,g,b,error))return false;}_textPalette=colors;_textPaletteKnown=true;return true;}
bool SceneRuntime::clearPresentationPalette(std::string& error){
 error.clear();if(!_palettePostedKnown){error="application palette publication owner unresolved";return false;}
 SceneRuntime next=*this;next._paletteSuppressed=next._paletteSuppressionKnown=true;
 if(!next._palettePosted){
  if(!next._palettePolicyKnown){error="application palette reset policy unresolved";return false;}
  std::vector<uint16_t> colors=next._palettes.colors();if(colors.size()!=128||next._knownColors.size()!=128){error="application palette allocation unresolved";return false;}
  next._palettes.cancelEffects();unsigned first=next._freshPaletteRequested||next._textPaletteRequested?0:16;
  for(unsigned i=first;i<64;++i){colors[i]=0;next._knownColors[i]=true;}
  if(!next._keepSecondaryPalette)for(unsigned i=80;i<128;++i){colors[i]=0;next._knownColors[i]=true;}
  next._freshPaletteRequested=next._keepSecondaryPalette=false;next._palettePosted=next._computedPrimaryPosted=true;next._palettePublicationDirty=true;
  if(!next._palettes.setColors(colors,error))return false;
 }
 *this=next;return true;
}
bool SceneRuntime::resetApplicationPresentation(std::string& error){
 SceneRuntime next=*this;if(!next.clearPresentationPalette(error))return false;
 // EEB6 clears every80-byte actor record, including reserved actor0. Decoded
 // patterns, name tables, scroll, environment and other owners are untouched.
 next._actors.clear();for(unsigned i=0;i<64;++i){next._actors._actors[i].recordIndex=next._actors._actors[i].remaining=0;next._slots[i]=ActorSlot();next._slots[i].clearedKnown=true;}
 *this=next;return true;
}
bool SceneRuntime::prepareResourceChange(std::string& error){return prepareReplacement(true,error);}
bool SceneRuntime::advanceResourcePreparation(SceneResourcePreparation& job,bool& complete,std::string& error){
 error.clear();complete=false;if(graphicsServiceBusy())return true;
 if(job.stage!=SceneResourcePreparation::kPrepare){job.stage=SceneResourcePreparation::kPrepared;complete=true;return true;}
 if(!_sceneFlagsKnown){error="queued replacement scene flag owner unresolved";return false;}
 if(!(_sceneFlags&0x7f)){
  if(!_palettePostedKnown){error="queued replacement palette-posted owner unresolved";return false;}
  // Source10298 retries queue0F after F560; it does not retire actors,
  // cancel queued patterns or reset scrolling during this palette-only visit.
  if(!_palettePosted)return clearPresentationPalette(error);
 }
 if(!preparePresentationReplacement(error))return false;
 job.stage=SceneResourcePreparation::kWaitForReplacement;return true;
}
bool SceneRuntime::preparePresentationReplacement(std::string& error){return prepareReplacement(false,error);}
bool SceneRuntime::prepareReplacement(bool resourceHandshake,std::string& error){
 error.clear();if(graphicsServiceBusy()){error="resource change requires prior graphics service completion";return false;}if(!_sceneFlagsKnown){error="resource-change scene flag ownership unresolved";return false;}
 SceneRuntime next=*this;
 // Queue0F first tests the low seven flag bits before F5E8 tests all eight.
 if((next._sceneFlags&(resourceHandshake?0x7f:0xff))==0&&!next.clearPresentationPalette(error))return false;
 std::vector<uint16_t> colors=next._palettes.colors();
 next._palettes.cancelEffects();if(next._textPaletteRequested){if(!next._textPaletteKnown){error="resource change requires original text palette owner";return false;}for(unsigned i=0;i<8;++i){colors[i]=next._textPalette[i];next._knownColors[i]=true;}next._textPaletteRequested=false;next._computedPrimaryPosted=true;}if(!next._palettes.setColors(colors,error))return false;next._palettePublicationDirty=true;
 next._graphicsTasks.cancelPending();next._pendingDialogueView=0;next._scroll.clear();next._pendingSamplingReset=true;next._pendingSecondaryMode=0;next._legacyLineComposition=!next._rowSamplingKnown;next._secondaryScrollFrozen=false;next._secondaryScrollMemo=0;next._phoneVisual=PhoneVisualState();next._phoneFrame=PhoneFramePublication();next._pendingPhoneRequest=0;next._pendingPhoneKnown=false;next._baseLayout.viewBands.clear();next._baseLayout.displayEnabled=true;next._visualBusy=false;next._visualCountdown=0;next.clearMenuCursor();next._visualCommand=0;next._visualKnown=true;next._scrollEnabled=false;for(unsigned i=0;i<2;++i)next._baseLayout.sourceX[i]=next._baseLayout.sourceY[i]=0;
 for(unsigned actor=1;actor<64;++actor){const ActorControlState *state=next._actors.state(actor);if(state->active&&!(state->completionFlags&1)&&!next.retireActor(actor,error))return false;}
 // SUB E34E publishes the resource blanking request; Main FF32F0 clears
 // the first4KB of both game name allocations before the new packet loads.
 // Source flag bit1 suppresses this handshake. Pattern/cache owners below
 // C000 and the second4KB of each name allocation must survive.
 if(resourceHandshake&&!(next._sceneFlags&2)){GraphicsTransfer blank;blank.decoded.assign(4096,0);blank.destination=0xc000;if(!next._renderer.applyTransfer(blank,error))return false;blank.destination=0xe000;if(!next._renderer.applyTransfer(blank,error))return false;}
 // A direct F5E8 call retains the maps now, but F682 requests7887 for
 // the later E34E/Main service. It does not cancel that future blank.
 if(!resourceHandshake&&!(next._sceneFlags&2))next._nameBlankPhase=1;
 next._resourceChangePending=resourceHandshake&&(next._sceneFlags&2)==0;next._sceneFlags=0;next._sceneFlagsKnown=true;*this=next;return true;
}
bool SceneRuntime::paletteRangeKnown(const PaletteEffect& effect,std::string& error)const{size_t begin=effect.byteOffset/2,end=begin+size_t(effect.countMinusOne)+1;if(end>_knownColors.size()){error="palette effect outside owned colors";return false;}// Source replacement effects own their destination; relative reads are
 // validated in source slot order at execution, after preceding writes.
 return true;}
const AnimationState* SceneRuntime::animation(unsigned actor)const{return actor<64&&_slots[actor].owned?&_slots[actor].player.state():nullptr;}
void SceneRuntime::record(unsigned actor,const char *category,uint32_t value){if(_trace&&actor<64&&_slots[actor].owned)_trace->record(Cursor(_slots[actor].resource,uint32_t(_slots[actor].player.currentOffset())),category,actor,value);}
bool SceneRuntime::failActor(unsigned actor,const std::string& reason,std::string& error)const{std::ostringstream message;message<<"actor "<<actor;if(actor<64&&_slots[actor].owned)message<<" resource "<<_slots[actor].resource<<" sequence offset "<<_slots[actor].player.currentOffset();message<<": "<<reason;error=message.str();return false;}
bool SceneRuntime::setControllerMapping(uint8_t mapping,std::string& error){error.clear();if(mapping>=6){error="controller mapping outside six source settings";return false;}_dialogueLayout.substitutionKnown=true;_dialogueLayout.substitution=mapping;return true;}
bool SceneRuntime::inspectActor(unsigned actor,SceneActorView& view,std::string& error)const{
 error.clear();if(actor>=64){error="actor view outside native slots";return false;}SceneActorView next;const ActorSlot& slot=_slots[actor];const ActorControlState* control=_actors.state(actor);next.owned=slot.owned;next.cleared=slot.clearedKnown;next.phaseKnown=slot.owned||slot.clearedKnown;next.phase=control->phase;next.active=control->active;next.flags=control->flags;next.completionFlags=control->completionFlags;next.record=control->recordIndex;next.remaining=control->remaining;
 if(slot.owned){next.resource=slot.resource;next.sequence=slot.player._sequence;const AnimationPlayer& player=slot.player;next.record=player._first?0xffff:uint16_t(player._state.recordIndex);next.remaining=player._state.remaining;next.x=int16_t(integer(player._state.x16));next.y=int16_t(integer(player._state.y16));if(slot.data&&player._sequence<=slot.data->size()&&slot.data->size()-player._sequence>=8){next.markersKnown=true;for(unsigned i=0;i<8;++i)next.markers[i]=(*slot.data)[player._sequence+i];}}
 view=next;return true;
}
bool SceneRuntime::patchActor(unsigned actor,const SceneActorPatch& patch,std::string& error){
 error.clear();if(actor>=64||(patch.fields&~127u)){error="actor patch outside native slots/fields";return false;}ActorSlot next=_slots[actor];ActorController actors=_actors;if(patch.fields&SceneActorPatch::kPhase)actors._actors[actor].phase=patch.phase;
 if(!next.owned){if((patch.fields&(SceneActorPatch::kX|SceneActorPatch::kY))||((patch.fields&SceneActorPatch::kActive)&&patch.active)){error="active/position patch requires installed graphics owner";return false;}auto& c=actors._actors[actor];if(patch.fields&SceneActorPatch::kFlags)c.flags=patch.flags;if(patch.fields&SceneActorPatch::kPhase)c.phase=patch.phase;if(patch.fields&SceneActorPatch::kRecord)c.recordIndex=patch.record;if(patch.fields&SceneActorPatch::kRemaining)c.remaining=patch.remaining;if(patch.fields&SceneActorPatch::kActive)c.active=false;_actors=actors;return true;}
 if(patch.fields&SceneActorPatch::kRecord){if(patch.record!=0xffff&&!next.player.selectRecord(patch.record,error))return false;if(patch.record==0xffff){if(next.player._halted||!next.player._scrollRequests.empty()){error="actor record replacement has unresolved controls";return false;}next.player._state.recordIndex=0xffff;next.player._first=false;next.player._blocked=false;}}
 if(patch.fields&SceneActorPatch::kRemaining)next.player._state.remaining=patch.remaining;
 if(patch.fields&SceneActorPatch::kFlags){if(!actors.setFlags(actor,patch.flags,error))return false;next.player._state.hidden=(patch.flags&2)!=0;}
 if(patch.fields&SceneActorPatch::kActive)actors._actors[actor].active=patch.active;
 if(patch.fields&SceneActorPatch::kX)next.player.setPosition(kScrollX,int32_t(int64_t(patch.x)*65536+(uint32_t(next.player.state().x16)&65535)));
 if(patch.fields&SceneActorPatch::kY)next.player.setPosition(kScrollY,int32_t(int64_t(patch.y)*65536+(uint32_t(next.player.state().y16)&65535)));
 _slots[actor]=next;_actors=actors;return true;
}
void SceneRuntime::captureActorBookmark(std::array<uint8_t,64>& summary)const{
 for(unsigned i=0;i<64;++i){const auto& actor=*_actors.state(i);uint8_t value=0;if(actor.active||actor.reserved){value=128;if(actor.completionFlags&128)value|=64;else value|=uint8_t((actor.flags&3)|(actor.phase&12)|((actor.completionFlags&1)<<4)|(_slots[i].catchUp?32:0));}summary[i]=value;}
}
bool SceneRuntime::restoreActorBookmark(const std::array<uint8_t,64>& summary,std::string& error){
 error.clear();SceneRuntime next=*this;for(unsigned i=0;i<64;++i){auto& actor=next._actors._actors[i];uint8_t value=summary[i];if((!(value&128)&&value)||((value&64)&&value!=192)){error="invalid compact native actor summary";return false;}if(!actor.active&&!actor.reserved)continue;if(!(value&128)){if(!next.retireActor(i,error))return false;continue;}if(value&64)continue;actor.flags=uint8_t((actor.flags&252)|(value&3));actor.phase=uint8_t((actor.phase&243)|(value&12));actor.completionFlags=uint8_t((actor.completionFlags&254)|((value>>4)&1));next._slots[i].catchUp=(value&32)!=0;next._slots[i].player._state.hidden=(actor.flags&2)!=0;}*this=next;return true;
}
bool SceneRuntime::copyActorTemplate(unsigned actor,unsigned templateActor,std::string& error){
 error.clear();if(actor>=64||templateActor>=64||(!_slots[actor].owned&&!_slots[actor].clearedKnown)||!_slots[templateActor].owned){error="actor template requires a known target and installed template owner";return false;}ActorSlot next=_slots[actor];const ActorSlot& source=_slots[templateActor];if(next.clearedKnown){next.owned=true;next.clearedKnown=false;next.initializer.actorIndex=actor;next.player._state.active=true;next.player._state.recordIndex=_actors.state(actor)->recordIndex;next.player._state.remaining=_actors.state(actor)->remaining;next.player._first=false;}
 // SUB12BCA copies palette, duration, frame-table, sequence and cached geometry.
 // Target control graph, position, velocity, repeat and current record survive.
 next.systemSource=source.systemSource;next.resource=source.resource;next.data=source.data;next.initializer.frameTableOffset=source.initializer.frameTableOffset;next.initializer.sequenceOffset=source.initializer.sequenceOffset;next.player._resource=source.player._resource;next.player._sequence=source.player._sequence;next.player._state.paletteOverride=source.player._state.paletteOverride;next.player._state.remaining=source.player._state.remaining;next.frame=source.frame;next.frameKnown=source.frameKnown;_slots[actor]=next;return true;
}
bool SceneRuntime::setActorFlags(unsigned actor,uint8_t flags,std::string& error){error.clear();if(actor>=64||!_slots[actor].owned){error="actor flags require installed resource owner";return false;}return _actors.setFlags(actor,flags,error);}
bool SceneRuntime::selectActorRecord(unsigned actor,unsigned record,uint8_t flags,std::string& error){
 error.clear();if(actor>=64||!_slots[actor].owned){error="record selection requires installed scene actor";return false;}
 AnimationPlayer next=_slots[actor].player;ActorController actors=_actors;
 if(!next.selectRecord(record,error)||!actors.setFlags(actor,flags,error))return false;
 _slots[actor].player=next;_actors=actors;return true;
}
bool SceneRuntime::loadSharedGraphicsEffects(const uint8_t* data,size_t size,std::string& error){const size_t at=0xea78-0xd400;if(!data||size<at+16){error="shared graphics effects exceed SUBCODE extent";return false;}return _graphicsTasks.setDissolvePattern(data+at,16,error);}
bool SceneRuntime::queueGraphicsTasks(const std::vector<SceneGraphicsTask>& tasks,std::string& error){
 SceneGraphicsQueue next=_graphicsTasks;for(const SceneGraphicsTask& task:tasks)if(!next.enqueue(task,error))return false;_graphicsTasks=next;return true;
}
bool SceneRuntime::queueScenarioGraphicsInitialization(const uint8_t* subcode,size_t size,std::string& error,bool resumedGame){
 error.clear();if(transferBusy()||pendingGraphicsTasks()||_resourceChangePending){error="common scenario graphics initialization requires its application boundary";return false;}
 SceneGraphicsTask atlas,cursor;const size_t first=0xecb6-0xd400,second=0xecc4-0xd400;
 if(!readSceneGraphicsTask(subcode,size,first,0xd400,atlas,error)||!readSceneGraphicsTask(subcode,size,second,0xd400,cursor,error))return false;
 if(atlas.flags!=0x41||cursor.flags!=0x11||atlas.transfers.size()!=1||cursor.transfers.size()!=1||atlas.transfers[0].destination!=0xbce0||cursor.transfers[0].destination!=0xde00||atlas.copyLengths[0]!=800||cursor.copyLengths[0]!=512){error="common presentation asset ownership differs from source packet";return false;}
 // EC5A tests the startup resume selector785C. A resumed game posts7866;
 // F5E8 subsequently consumes that request and restores the eight menu colors.
 // A fresh game's first authored presentation owns its separate palette request.
 if(resumedGame&&!_textPaletteKnown){error="Original resume requires its source text palette";return false;}
 SceneRuntime next=*this;if(resumedGame)next._textPaletteRequested=true;if(!next.resetApplicationPresentation(error))return false;next.clearMenuCursor();
 // EC5A also initializes its hidden cursor actor. Its active menu geometry is
 // already represented by the semantic menu owner, not a second raw actor.
 next._slots[0].clearedKnown=false;
 if(!next.queueGraphicsTasks({atlas,cursor},error))return false;*this=next;return true;
}
bool SceneRuntime::queuePaletteEffect(const PaletteEffect& effect,std::string& error){
 if(!paletteRangeKnown(effect,error))return false;return _palettes.enqueue(effect,error);
}
bool SceneRuntime::beginApplicationFade(const PaletteEffect& effect,std::string& error){
 // SUB10096 pauses animation without changing the actors' render bit, then
 // E4A0 replaces outstanding palette work before the source fade is posted.
 SceneRuntime next=*this;
 for(unsigned actor=0;actor<64;++actor){const auto* state=next._actors.state(actor);if(!state||!next._actors.setFlags(actor,uint8_t(state->flags|1),error))return false;}
 next._palettes.cancelEffects();if(!next.queuePaletteEffect(effect,error))return false;
 *this=next;return true;
}
bool SceneRuntime::initializeActor(const ActorInitializer& initial,uint16_t resource,const std::shared_ptr<const std::vector<uint8_t> >& data,std::string& error,bool systemSource){
 const ActorControlState *previous=_actors.state(initial.actorIndex);if(!previous){error="actor initializer index outside native slots";return false;}if(previous->reserved)return true;
 ActorSlot next;next.owned=true;next.systemSource=systemSource;next.resource=resource;next.initializer=initial;next.data=data;if(!next.player.start(data->data(),data->size(),initial.sequenceOffset,initial.x,initial.y,error)||!_actors.initialize(initial.actorIndex,initial.flags,error))return false;
 size_t at=initial.sequenceOffset;uint8_t first=(*data)[at];if((first==0xa1||first==0xa7)&&data->size()-at>=8){next.player.setPaletteOverride((*data)[at+1]);at+=4;first=(*data)[at];}if(first<0x80){next.frameKnown=true;next.frame=first;}
 _slots[initial.actorIndex]=next;return true;
}
bool SceneRuntime::install(uint16_t resource,const std::vector<uint8_t>& bytes,unsigned index,std::string& error){
 error.clear();if(transferBusy()){error="scene installation requires previous transfer completion";return false;}SceneAssets assets;if(parseSceneAssets(bytes.data(),bytes.size(),index,16*1024*1024,assets,error)!=kSceneAssetsReady)return false;
 // SUB F6E8 replaces the pending transfer list/channel immediately. A later
 // family1 command in this packet supersedes it before the transfer worker runs.
 for(const SceneControl& control:assets.controls)if((control.family<5||control.family>9)){std::ostringstream message;message<<"scene controller family "<<unsigned(control.family)<<" at resource offset "<<control.commandOffset<<" requires its native owner";error=message.str();return false;}
 std::shared_ptr<const std::vector<uint8_t> > data(new std::vector<uint8_t>(bytes));SceneRuntime next=*this;if(!next.installStaged(resource,data,assets,error))return false;next._saveResource=resource;next._saveScene=index;next._saveStamp=UINT64_C(14695981039346656037);for(uint8_t value:bytes)next._saveStamp=(next._saveStamp^value)*UINT64_C(1099511628211);*this=next;return true;
}
bool SceneRuntime::installOverlay(uint16_t resource,const std::vector<uint8_t>& bytes,unsigned index,std::string& error){
 error.clear();if(resource!=_saveResource||resource>=97||transferBusy()||_resourceChangePending){error="PCM-linked presentation requires the current stable resource owner";return false;}
 uint64_t stamp=UINT64_C(14695981039346656037);for(uint8_t byte:bytes)stamp=(stamp^byte)*UINT64_C(1099511628211);if(stamp!=_saveStamp){error="PCM-linked presentation source identity mismatch";return false;}
 SceneAssets assets;if(parseSceneAssets(bytes.data(),bytes.size(),index,16*1024*1024,assets,error)!=kSceneAssetsReady)return false;
 for(const auto& control:assets.controls)if((control.family<5||control.family>9)){error="PCM-linked presentation controller owner unresolved";return false;}
 SceneRuntime staged=*this;SceneAssets primary=staged._assets;std::shared_ptr<const std::vector<uint8_t>> source(new std::vector<uint8_t>(bytes));
 // 13FC6 owns transient transfer channel1; F68E applies the authored list
 // without F5E8 scene teardown or changing the scenario's presentation latch.
 if(!staged.setTransferChannel(1,error)||!staged.installStaged(resource,source,assets,error)||!staged.setTransferChannel(0,error))return false;
 staged._assets=primary;*this=staged;return true;
}
bool SceneRuntime::pcmVisualStartReady(const std::vector<ActorInitializer>& actors)const {
 for(const auto& initial:actors) {
  const unsigned i=initial.actorIndex;
  if(i<64&&_slots[i].owned&&_actors.state(i)->active&&
     !(_actors.state(i)->flags&1)&&!_slots[i].player.streamStartReady())return false;
 }
 return !graphicsServiceBusy()&&!transferBusy();
}
bool SceneRuntime::installSystemPresentation(const std::vector<uint8_t>& bytes,unsigned index,unsigned position,std::string& error){
 error.clear();const size_t table=0x144c6-0xd400;if(position>=18||bytes.size()<table+18*4){error="system presentation position outside original table";return false;}if(transferBusy()||_resourceChangePending){error="system presentation requires completed resource/transfer ownership";return false;}SceneAssets assets;if(parseSystemPresentationAssets(bytes.data(),bytes.size(),index,16*1024*1024,assets,error)!=kSceneAssetsReady)return false;
 for(const SceneControl& control:assets.controls)if((control.family<5||control.family>9)){error="system presentation controller owner unresolved";return false;}
 SceneRuntime next=*this;SceneAssets current=next._assets;std::shared_ptr<const std::vector<uint8_t> > source(new std::vector<uint8_t>(bytes));if(!next.installStaged(0xffff,source,assets,error,true))return false;next._assets=current;ActorSlot& actor=next._slots[1];if(!actor.owned){error="system presentation did not establish required actor1";return false;}
 actor.player.setPosition(kScrollX,fixedWord(sourceWord(bytes.data()+table+position*4)));actor.player.setPosition(kScrollY,fixedWord(sourceWord(bytes.data()+table+position*4+2)));*this=next;return true;
}
bool SceneRuntime::installStaged(uint16_t resource,const std::shared_ptr<const std::vector<uint8_t> >& data,const SceneAssets& assets,std::string& error,bool systemSource){
 size_t actorIndex=0,paletteIndex=0,linkIndex=0,controlIndex=0;
 for(const SceneAssetCommand& command:assets.commands){
  if(command.family==0){if(paletteIndex>=assets.palettes.size()){error="scene palette command ownership mismatch";return false;}const PaletteEffect& effect=assets.palettes[paletteIndex++];if(!paletteRangeKnown(effect,error)||!_palettes.enqueue(effect,error))return false;}
  else if(command.family==1){_pendingTransferChannel=_transferChannel;_pendingTransferChannelKnown=_transferChannelKnown;_pendingTransfers.clear();for(const GraphicsTransfer& transfer:assets.transfers)if(transfer.recordOffset>=command.payloadBegin&&transfer.recordOffset<command.payloadEnd)_pendingTransfers.push_back(transfer);_palettePosted=false;_palettePostedKnown=true;}
  else if(command.family==2){size_t count=(command.payloadEnd-command.payloadBegin)/12;for(size_t i=0;i<count;++i){if(actorIndex>=assets.actors.size()||!initializeActor(assets.actors[actorIndex++],resource,data,error,systemSource))return false;}}
  else if(command.family==3||command.family==4){size_t count=(command.payloadEnd-command.payloadBegin)/2;for(size_t i=0;i<count;++i){if(linkIndex>=assets.links.size()){error="scene link ownership mismatch";return false;}const ActorLink& link=assets.links[linkIndex++];if(!_actors.link(link.parent,link.child,error))return false;for(unsigned axis=0;axis<2;++axis)if(!updateVelocity(link.child,axis,error))return false;}}
  else if(command.family>=5&&command.family<=7){if(controlIndex>=assets.controls.size()){error="scene text variant ownership mismatch";return false;}_dialogueLayout.variant=uint8_t(assets.controls[controlIndex++].value);if(_dialogueLayout.variant>1)_dialogueLayout.variant=2;}
  else if(command.family==8){if(controlIndex>=assets.controls.size()||!requestGameLayout(assets.controls[controlIndex++].value,error))return false;}
  else if(command.family==9){if(controlIndex>=assets.controls.size()||!setTransferChannel(assets.controls[controlIndex++].value,error))return false;}
 }
 if(actorIndex!=assets.actors.size()||paletteIndex!=assets.palettes.size()||linkIndex!=assets.links.size()){error="unowned scene packet records";return false;}_assets=assets;_resourceChangePending=false;return true;
}
bool SceneRuntime::setTransferChannel(unsigned channel,std::string& error){error.clear();if(channel>=4){error="unsupported graphics transfer staging channel";return false;}_transferChannel=channel;_transferChannelKnown=true;return true;}
bool SceneRuntime::commitTransfers(std::string& error){
 error.clear();
 // E34E gives7887 priority over the dependent decoder. A posted request
 // cannot be acknowledged by repeating the Sub API before Main consumes it.
 if(_nameBlankPhase==1){_mainNameBlankPending=true;_nameBlankPhase=2;return true;}
 if(_nameBlankPhase==2){if(!_mainNameBlankPending)_nameBlankPhase=0;return true;}
 if(!transferBusy())return true;if(!_transferChannelKnown){error="graphics transfer copy policy inheritance unresolved";return false;}
 // E34E waits until Main has consumed the previous descriptor. F32C always
 // decodes dependent transfers into cache0; controller9 selects chunking,
 // not a cache number. Completion is observed on the next worker visit.
 if(!_graphicsCopies.empty())return true;
 if(_pendingTransfers.empty()){_transferCompletionPending=false;return true;}
 const GraphicsTransfer& transfer=_pendingTransfers.front();if(transfer.delay){error="nonzero graphics-transfer parameter requires its scheduling owner";return false;}
 SceneGraphicsQueue staging=_graphicsTasks;if(!_transferCopyOffset&&!staging.setStaging(0,transfer.decoded,error))return false;
 if(_transferCopyOffset>=transfer.decoded.size()){error="dependent graphics copy cursor outside decoded allocation";return false;}
 const size_t remaining=transfer.decoded.size()-_transferCopyOffset;const size_t count=_transferChannel?std::min(size_t(2048),remaining):remaining;
 if(!count||count>8192||count>65536u-transfer.destination-_transferCopyOffset){error="dependent graphics copy extent invalid";return false;}
 SceneGraphicsCopy copy;copy.channel=0;copy.offset=uint16_t(_transferCopyOffset);copy.count=uint16_t(count);copy.destination=uint16_t(transfer.destination+_transferCopyOffset);
 _graphicsTasks=staging;_graphicsCopies.push_back(copy);_transferCompletionPending=true;_transferCopyOffset+=unsigned(count);
 if(_transferCopyOffset==transfer.decoded.size()){_pendingTransfers.erase(_pendingTransfers.begin());_transferCopyOffset=0;}
 return true;
}
bool SceneRuntime::descendants(unsigned actor,std::vector<unsigned>& result,std::string& error)const{
 result.clear();if(actor>=64){error="actor index outside native slots";return false;}result.push_back(actor);bool seen[64]={false};seen[actor]=true;for(size_t i=0;i<result.size();++i){unsigned child=_actors.state(result[i])->head;while(child){if(child>=64||seen[child]){error="actor ownership cycle/bounds";return false;}seen[child]=true;result.push_back(child);child=_actors.state(child)->next;}}return true;
}
bool SceneRuntime::updateVelocity(unsigned actor,unsigned axis,std::string& error){std::vector<unsigned> list;if(!descendants(actor,list,error))return false;for(unsigned index:list){ActorSlot& slot=_slots[index];if(!slot.owned)continue;unsigned parent=_actors.state(index)->parent;slot.effectiveVelocity[axis]=parent&&_slots[parent].owned?wrapped(slot.baseVelocity[axis],_slots[parent].effectiveVelocity[axis]):slot.baseVelocity[axis];}return true;}
bool SceneRuntime::retireActor(unsigned actor,std::string& error){if(actor>=64){error="retired actor outside native slots";return false;}record(actor,"actor_retired");if(!_actors.retire(actor,error))return false;_actors._actors[actor].recordIndex=_actors._actors[actor].remaining=0;_slots[actor]=ActorSlot();_slots[actor].clearedKnown=true;return true;}
bool SceneRuntime::restartActor(unsigned actor,std::string& error){ActorSlot& slot=_slots[actor];return !slot.owned||slot.player.restartSequence(error);}
ActorActionResult SceneRuntime::actorAction(unsigned actor,unsigned action,std::string& error){
 if(action!=5)return _actors.applyAction(actor,action,error);
 std::vector<unsigned> targets;if(!descendants(actor,targets,error))return kActorActionError;SceneRuntime next=*this;ActorActionResult result=next._actors.applyAction(actor,action,error);if(result!=kActorActionApplied)return result;for(unsigned target:targets)if(!next.restartActor(target,error))return kActorActionError;*this=next;return result;
}
AnimationCommandResult SceneRuntime::handle(void *owner,const AnimationCommand& command,std::string& error){return static_cast<SceneRuntime*>(owner)->command(command,error);}
bool SceneRuntime::randomDuration(void *owner,uint16_t lower,uint16_t span,uint16_t& duration,std::string& error){SceneRuntime& scene=*static_cast<SceneRuntime*>(owner);uint16_t draw=0;if(!scene._random){error="animation duration requires shared RNG owner";return false;}if(!scene._random->next(draw,error))return false;duration=uint16_t(lower+((uint32_t(draw)*span)>>16));scene.record(scene._currentActor,"animation_random",draw);return true;}
AnimationCommandResult SceneRuntime::command(const AnimationCommand& command,std::string& error){
 ActorSlot& slot=_slots[_currentActor];unsigned op=command.opcode;record(_currentActor,"animation_command",op);
 if(op==0x84||op==0x85){unsigned axis=op==0x84?0:1;std::vector<unsigned> targets;if(!descendants(_currentActor,targets,error))return kAnimationCommandError;int32_t current=axis?slot.player.state().y16:slot.player.state().x16;uint32_t rawDelta=uint32_t(fixedWord(command.value))-(uint32_t(current)&0xffff0000u);int32_t delta=rawDelta<=0x7fffffff?int32_t(rawDelta):int32_t(int64_t(rawDelta)-0x100000000LL);for(unsigned target:targets)if(_slots[target].owned){const AnimationState& state=_slots[target].player.state();_slots[target].player.setPosition(axis?kScrollY:kScrollX,wrapped(axis?state.y16:state.x16,delta));}return kAnimationCommandAdvance;}
 if(op==0x86||op==0x87){unsigned axis=op==0x86?0:1;slot.baseVelocity[axis]=wrapped(fixedWord(command.value),int32_t(command.argument)*256);if(!updateVelocity(_currentActor,axis,error))return kAnimationCommandError;return kAnimationCommandAdvance;}
 if(op==0x88){if(!_paletteSuppressionKnown){error="palette suppression ownership unresolved";return kAnimationCommandUnsupported;}if(_paletteSuppressed)return kAnimationCommandAdvance;int64_t begin=int64_t(command.sequenceOffset)+int16_t(command.value);if(begin<0||uint64_t(begin)>=slot.data->size()){error="palette effect outside actor resource";return kAnimationCommandError;}PaletteEffect effect;if(!readPaletteEffect(slot.data->data(),slot.data->size(),size_t(begin),slot.data->size(),effect,error,slot.systemSource?0xd400:0x28000))return kAnimationCommandError;if(!paletteRangeKnown(effect,error))return kAnimationCommandUnsupported;if(!_palettes.enqueue(effect,error))return _palettes.activeEffects()==12?kAnimationCommandBlocked:kAnimationCommandError;return kAnimationCommandAdvance;}
 if(op==0x89){_palettes.cancelEffects();return kAnimationCommandAdvance;}
 if(op==0x8a){int64_t begin=int64_t(command.sequenceOffset)+int16_t(command.value);if(begin<0||uint64_t(begin)>=slot.data->size()){error="graphics task outside actor resource";return kAnimationCommandError;}SceneGraphicsTask task;if(!readSceneGraphicsTask(slot.data->data(),slot.data->size(),size_t(begin),slot.systemSource?0xd400:0x28000,task,error))return kAnimationCommandUnsupported;if(!_graphicsTasks.enqueue(task,error))return _graphicsTasks.pending()==7?kAnimationCommandBlocked:kAnimationCommandError;return kAnimationCommandAdvance;}
 if(op==0x8b)return kAnimationCommandAdvance;
 if(op>=0x90&&op<=0x93){if(_actors.applyAction(_currentActor,1,error)==kActorActionError)return kAnimationCommandError;return kAnimationCommandAdvance;}
 if(op==0x94){if(_actors.applyAction(command.argument,0,error)==kActorActionError)return kAnimationCommandError;return kAnimationCommandAdvance;}
 if(op==0x95){if(command.argument==_currentActor)return kAnimationCommandRetireSelf;return retireActor(command.argument,error)?kAnimationCommandAdvance:kAnimationCommandError;}
 if(op==0x96)return _actors.setFlags(command.argument,3,error)?kAnimationCommandAdvance:kAnimationCommandError;
 if(op==0x99||op==0x9a){slot.cdDeadline=uint32_t(command.argument)<<24|uint32_t(command.value)<<8;if(!_actors.setFlags(_currentActor,uint8_t(_actors.state(_currentActor)->flags|4),error))return kAnimationCommandError;return kAnimationCommandAdvance;}
 if(op==0x9b||op==0x9c){if(!_actors.markFlags(_currentActor,op==0x9b?1:128,error))return kAnimationCommandError;_sceneFlags=op==0x9b?command.argument:_actors.state(_currentActor)->completionFlags;_sceneFlagsKnown=true;return kAnimationCommandAdvance;}
 if(op==0x9d){if(!_musicSuppressionKnown){error="animation music suppression owner unresolved";return kAnimationCommandUnsupported;}if(!_musicSuppressed){if(!requestCommandMusic(command.argument,error))return kAnimationCommandError;}return kAnimationCommandAdvance;}
 if(op==0x9f){if(!_soundKnown){error="effect activity/suppression ownership unresolved";return kAnimationCommandUnsupported;}if(!_effectsSuppressed&&!(_audioActivity&7)){if(_effects.size()>=64){error="unconsumed animation effect request capacity";return kAnimationCommandError;}_effects.push_back(command.argument);}return kAnimationCommandAdvance;}
 if(op==0xa2)return transferBusy()?kAnimationCommandBlocked:kAnimationCommandAdvance;
 if(op==0xa3){_textPaletteRequested=true;return kAnimationCommandAdvance;}
 if(op==0xa4){_palettePosted=true;_palettePostedKnown=true;return kAnimationCommandAdvance;}
 // FCC8 waits for stream-start bit1; it does not wait for silence.
 if(op==0xa9){if(!_soundKnown){error="audio stream-start predicate owner unresolved";return kAnimationCommandUnsupported;}return (_audioActivity&2)?kAnimationCommandAdvance:kAnimationCommandBlocked;}
 if(op==0xad){if(_musicCommands.size()>=64){error="unconsumed animation secondary music request capacity";return kAnimationCommandError;}_lastSecondaryMusicCommand=command.argument==255?0:command.argument;_lastSecondaryMusicKnown=true;_musicCommands.push_back(SceneMusicCommand(command.argument,true));return kAnimationCommandAdvance;}
 if(op==0xae){_freshPaletteRequested=true;return kAnimationCommandAdvance;}
 if(op==0xa5||op==0xa6){SceneGraphicsTask task;task.flags=op==0xa5?0x25:0xa4;GraphicsTransfer transfer;transfer.recordOffset=transfer.sourceOffset=transfer.encodedEnd=0;transfer.destination=transfer.delay=0;task.transfers.push_back(transfer);task.stagingOffsets.push_back(0);task.copyLengths.push_back(32);task.stagingSources.push_back(std::vector<uint8_t>());if(!_graphicsTasks.enqueue(task,error))return _graphicsTasks.pending()==7?kAnimationCommandBlocked:kAnimationCommandError;return kAnimationCommandAdvance;}
 if(op==0xab)return requestPalettePublication(command.argument,error)?kAnimationCommandAdvance:kAnimationCommandError;
 if(op==0xac){slot.catchUp=true;return kAnimationCommandAdvance;}
 if(op==0xaf){slot.cadenceMask=command.argument;return kAnimationCommandAdvance;}
 if(op==0xaa){if(!applyAnimationClockCapture(command.argument,_animationClock,error))return kAnimationCommandUnsupported;record(_currentActor,"animation_clock_capture",_animationClock.captured);return kAnimationCommandAdvance;}
 if(op==0xa8)return requestVisualCommand(command.argument,error)?kAnimationCommandAdvance:kAnimationCommandUnsupported;
 return kAnimationCommandUnsupported;
}
bool SceneRuntime::advanceCoordinatorClock(std::string& error,bool* admitted){error.clear();if(admitted)*admitted=false;if(graphicsServiceBusy()||_resourceChangePending)return true;if(!_animationClock.clockKnown){error="animation coordinator clock requires explicit fresh or supplied source context";return false;}_animationClock.sourceTicks=uint16_t(unsigned(_animationClock.sourceTicks)+1);if(admitted)*admitted=true;return true;}
bool SceneRuntime::fontPublicationPending()const{
 if(_fontClearPending||_pendingDialogueView)return true;
 for(const auto& copy:_graphicsCopies){unsigned end=unsigned(copy.destination)+copy.count;if(copy.destination<8192||(copy.destination<0xee80&&end>0xec80))return true;}
 return false;
}
bool SceneRuntime::beginFrame(bool paused,std::string& error,int presentationCounter){
 error.clear();if(paused)return true;
 NativeVisualRequest request;request.clearFont=_fontClearPending;request.clearNames=_mainNameBlankPending;request.copies=_graphicsCopies;
 if(_pendingGameLayout){GameLayoutRequest layout=decodeGameLayoutRequest(_pendingGameLayout);if(layout!=kGameLayoutUnchanged){if(!_gameLayoutsKnown){error="visual layout job requires original presets";return false;}request.resetLayout=true;request.layout=layout==kGameLayoutTall?_tallGameLayout:_wideGameLayout;request.preset=layout==kGameLayoutTall?kSceneTallLayout:kSceneWideLayout;}}
 NativeVisualCommit publication(request);
 // Resolve shared sources at admission, then complete atomic native operations
 // on unpublished allocations. Failure leaves requests and visible state intact.
 if(!publication.admit(_graphicsTasks,error)||!publication.complete(_renderer,error))return false;
 if(_pendingDialogueView){uint8_t command=_pendingDialogueView;_pendingDialogueView=0;if(!requestVisualCommand(command,error))return false;}
 // Main323E reports the prior controller result; SubD74E owns788C.
 // A controller completing now cannot release the earlier worker report.
 _workerVisualBusy=_visualBusy;
 // Main0794/1750 consumes the completed submission. Sub ED28 computes its
 // successor later; publishing it at render time exposes transitions early.
 if(_environmentSubmissionKnown){_displayedSubmission=_environmentSubmission;_displayedSubmissionKnown=true;}
 // Main3CC6 skips vertical publication while E078 owns a line effect.
 if(_fullPlanePublicationEnabled&&!_lineScroll.state().active){_scrollPublicationKnown=true;for(unsigned p=0;p<2;++p)_frameScrollY[p]=int16_t(integer(_scroll.state().positions[p].y16));}
 // Main3D2A/3D3E publishes only the pair at row0 in full-plane mode.
 // Main1610 clears the first128 row pairs at a source layout reset.
 // Later rows without a source writer retain legacy composition, not invented zeros.
 if(_rowSamplingKnown&&!_legacyLineComposition){
  // Main3CB0 publishes the previously computed sampling selector before the
  // secondary worker changes it. Keep this frame's selector with its rows;
  // a newly initialized/hidden/restored/retired effect owns the next frame.
  _frameLineSampling=_lineScroll.state().active&&_lineScroll.state().lineEnabled&&!_fullPlaneScrollOverride;
  if(_frameLineSampling){const auto& line=_lineScroll.state();for(unsigned row=0;row<line.publishedPrefix;++row){_rowX[1][row+8]=int16_t(line.published[row]);if(row>=120)_tailRowsKnown|=uint8_t(1u<<(row-120));}}
  else if(_fullPlanePublicationEnabled)for(unsigned plane=0;plane<2;++plane)_rowX[plane][0]=int16_t(_baseLayout.sourceX[plane]-integer(_scroll.state().positions[plane].x16));
 }
 if(_pendingSamplingReset){_lineScroll.reset();_fullPlaneScrollOverride=true;_pendingSamplingReset=false;}
 const uint8_t secondary=_pendingSecondaryMode;_pendingSecondaryMode=0;if(secondary&&secondary<3)_fullPlanePublicationEnabled=true;
 if(secondary>=3){if(!_lineScroll.start(secondary,error))return false;_fullPlaneScrollOverride=false;_secondaryScrollMemo=0;}
 if(!beginPhonePublication(_phoneVisual,_phoneFrame,error)||!updateVisual(error))return false;if(_lineScroll.state().active&&!_secondaryScrollFrozen&&secondary<3){bool wasLine=_lineScroll.state().lineEnabled;if(!_lineScroll.tick(presentationCounter<0?uint16_t(_lineScroll.state().lastGlobalCounter+1):uint16_t(presentationCounter),error))return false;if(_lineScroll.state().mode==5&&wasLine!=_lineScroll.state().lineEnabled)_fullPlaneScrollOverride=!_lineScroll.state().lineEnabled;_secondaryScrollMemo=0;}if(secondary&&secondary<3)_fullPlaneScrollOverride=true;if(_lineScroll.state().completed&&!requestVisualCommand(0xff,error))return false;bool forcePrimary=_pendingPalettePublication==255;if(_pendingPalettePublication){if(_pendingPalettePublication==255){_palettePublicationMode=0;_palettePublicationDirty=true;}else _palettePublicationMode=_pendingPalettePublication;_pendingPalettePublication=0;}
 bool dirty=_computedPrimaryPosted||_pendingPrimaryPosted||_palettePublicationDirty||_visibleColors!=_pendingColors||_visibleKnownColors!=_pendingKnownColors;
 bool primaryChanged=forcePrimary||_pendingPrimaryPosted;if(_palettePublicationMode||dirty){_pendingPrimaryPosted=_computedPrimaryPosted;_computedPrimaryPosted=false;for(unsigned i=0;i<64;++i)if(_visibleColors[i]!=_pendingColors[i]||_visibleKnownColors[i]!=_pendingKnownColors[i])primaryChanged=true;_visibleColors=_pendingColors;_visibleKnownColors=_pendingKnownColors;_pendingColors=_palettes.colors();_pendingKnownColors=_knownColors;_palettePublicationDirty=false;}if(!publishDisplayedPalette(primaryChanged,error))return false;
 // Native completion preserves clear -> layout reset -> copy order. The
 // old hardware-dependent byte progress is deliberately not simulated.
 // Main0754 uploads the palette before Main07AA/3100 handles controller8.
 // Its1538/1540 reset clears displayed colors AFTER that upload. A continuous
 // palette owner must not repaint the reset until the following publication.
 if(_pendingGameLayout){GameLayoutRequest layoutRequest=decodeGameLayoutRequest(_pendingGameLayout);if(!applyGameLayout(layoutRequest,error))return false;_pendingGameLayout=0;}
 // Main17C4 consumes completed sources after the layout reset. The Sub
 // worker may have changed shared decoded caches since descriptors were posted.
 if(!publication.commit(_renderer,error)||!publication.continuationAllowed())return false;
 _fontClearPending=_mainNameBlankPending=false;_graphicsCopies.clear();
 if(!_rowSamplingKnown||_legacyLineComposition)_frameLineSampling=_lineScroll.state().active&&_lineScroll.state().lineEnabled&&!_fullPlaneScrollOverride;
 return true;
}
bool SceneRuntime::tick(bool paused,std::string& error){return beginFrame(paused,error)&&updateObjects(paused,error);}
bool SceneRuntime::updateObjects(bool paused,std::string& error,unsigned elapsedUpdates){
 error.clear();if(paused)return true;if(elapsedUpdates>4096){error="animation elapsed update count exceeds native bounded owner";return false;}if(!elapsedUpdates)elapsedUpdates=1;
 if(!graphicsServiceBusy()&&!_resourceChangePending&&!updateActorObjects(elapsedUpdates,error))return false;
 if(_scrollEnabled&&!_scroll.tick(error))return false;if(!transferBusy()){_paletteSuppressed=false;_paletteSuppressionKnown=true;uint8_t posted=0;if(!_palettes.tick(error,&posted,&_knownColors))return false;if(posted)_palettePublicationDirty=true;if(posted&1)_computedPrimaryPosted=true;}
 SceneGraphicsQueue tasks=_graphicsTasks;std::vector<GraphicsTransfer> ready;std::vector<SceneGraphicsCopy> copies;if(!tasks.tick(transferBusy(),ready,error,&copies))return false;_graphicsTasks=tasks;_graphicsCopies.insert(_graphicsCopies.end(),copies.begin(),copies.end());return true;
}
bool SceneRuntime::initializeBookmarkedActors(const std::array<uint8_t,64>& summary,std::string& error){
 error.clear();uint16_t resource;unsigned list;if(!resourceIdentity(resource,list)||_resourceChangePending||blankingPending()){error="bookmark actor initialization requires installed presentation ownership";return false;}
 // Queue20 calls F7F0 directly after F68E. The ordinary D6F6/7886 gate
 // belongs to its caller, not F7F0, so new dependent copies do not defer
 // this actor-only visit. ED28, palettes and graphics tasks run later once.
 for(uint8_t value:summary)if((!(value&128)&&value)||((value&64)&&value!=192)){error="invalid compact native actor summary";return false;}
 if(!updateActorObjects(1,error))return false;
 return restoreActorBookmark(summary,error);
}
bool SceneRuntime::updateActorObjects(unsigned elapsedUpdates,std::string& error){
 for(unsigned i=0;i<64;++i){
  for(unsigned repeat=0;repeat<elapsedUpdates;++repeat){const ActorControlState* control=_actors.state(i);ActorSlot& slot=_slots[i];if(!control->active||!slot.owned)break;
   if(slot.cadenceMask||slot.catchUp){if(!_cadenceKnown){error="animation cadence mask requires source update context";return failActor(i,error,error);}if(_cadenceMask)break;}
   if(control->flags&4){if(!_cdClockKnown){error="animation CD deadline requires actual source-consumed clock";return failActor(i,error,error);}if(_cdClock<slot.cdDeadline)break;if(!_actors.setFlags(i,uint8_t(control->flags&~4u),error))return false;}
   if(control->flags&1)break;
   // Source F8E2 leaves the actual actor bit1 unchanged on a held97.
   // Public UI/actor actions can replace that flag between sequence updates;
   // use its current owner rather than restoring the player's stale shadow.
   slot.player._state.hidden=(control->flags&2)!=0;
   _currentActor=i;AnimationResult result=slot.player.tick(error,handle,this,randomDuration);if(result==kAnimationUnsupported||result==kAnimationError)return failActor(i,error,error);
   unsigned markers=slot.player.takeCompletionMarkers();if(markers){if(!_actors.markCompletion(i,error))return false;record(i,"actor_completion_marker",markers);}
   // Animation8C..8F use the same F3F8 secondary mailbox as explicit plane
   // positioning. Updating only Sub's producer leaves Main E076 disabled after
   // a title/line effect, so the scene keeps displaying its old scroll origin.
   for(const ScrollRequest& request:slot.player.takeScrollRequests()){if(!requestVisualCommand(request.kind==ScrollRequest::kPosition?1:2,error))return false;if(!(request.kind==ScrollRequest::kPosition?_scroll.queuePosition(request.plane,request.axis,request.value,error):_scroll.setVelocity(request.plane,request.axis,request.value,error)))return false;}
   if(result==kAnimationFinished||result==kAnimationSelfRetired){if(!retireActor(i,error))return false;if(result==kAnimationSelfRetired){
     // Source95 clears the target then increments the current record. No
     // authored sequence survives self-clear. The inactive duration is a
     // canonical semantic zero, not a reproduction of the original low/vector page.
     _actors._actors[i].recordIndex=1;
    }break;}const AnimationState& state=slot.player.state();uint8_t flags=_actors.state(i)->flags;if(!_actors.setFlags(i,uint8_t((flags&~2u)|(state.hidden?2:0)),error))return false;
   if(!slot.catchUp)break;
  }
 }
 // F850..F898 publishes the final record's frame after ALL sequence updates.
 // Controls (including97 and blocked waits) retain the old pointer, which can
 // still be absent. Publication precedes the pause test; motion follows it.
 for(unsigned i=0;i<64;++i)if(_slots[i].owned&&_actors.state(i)->active&&!(_slots[i].cadenceMask&_cadenceMask)){
  ActorSlot& slot=_slots[i];unsigned record=slot.player.state().recordIndex;if(record==0xffff)record=0;
  size_t offset=slot.player._sequence+size_t(record)*4;
  if(offset>slot.player._resource.size()||slot.player._resource.size()-offset<4)return failActor(i,"truncated published animation record",error);
  uint8_t frame=slot.player._resource[offset];if(frame<0x80){slot.frame=frame;slot.frameKnown=true;}
  if(!(_actors.state(i)->flags&1)){AnimationState state=slot.player.state();slot.player.setPosition(kScrollX,wrapped(state.x16,slot.effectiveVelocity[0]));slot.player.setPosition(kScrollY,wrapped(state.y16,slot.effectiveVelocity[1]));}
 }
 return true;
}
SceneLayout SceneRuntime::currentRenderLayout()const{SceneLayout layout=_baseLayout;for(unsigned i=0;i<2;++i){layout.sourceX[i]-=integer(_scroll.state().positions[i].x16);layout.sourceY[i]-=_scrollPublicationKnown?_frameScrollY[i]:integer(_scroll.state().positions[i].y16);}
 if(_phoneFrame.known&&_phoneFrame.program!=PhoneFramePublication::kDelegate){std::vector<PhoneVisualBand> bands;std::string detail;if(phoneVisualBands(_phoneFrame,_phoneVisual,unsigned(layout.height),bands,detail)&&!bands.empty()){layout.viewBands.clear();for(unsigned p=0;p<2;++p)layout.sourceY[p]=p?bands[0].planeBY:bands[0].planeAY;for(size_t i=1;i<bands.size();++i){SceneViewBand b;b.firstRow=bands[i].begin;for(unsigned p=0;p<2;++p){b.sourceX[p]=layout.sourceX[p];b.sourceY[p]=p?bands[i].planeBY:bands[i].planeAY;}layout.viewBands.push_back(b);}}}
 if((_rowSamplingKnown&&!_legacyLineComposition)||(_lineScroll.state().active&&_lineScroll.state().lineEnabled&&!_fullPlaneScrollOverride)){
  const bool owned=_rowSamplingKnown&&!_legacyLineComposition;
  std::vector<SceneViewBand> original=layout.viewBands;layout.viewBands.clear();SceneViewBand current;current.displayEnabled=layout.displayEnabled;for(unsigned p=0;p<2;++p){current.sourceX[p]=layout.sourceX[p];current.sourceY[p]=layout.sourceY[p];}size_t index=0;SceneViewBand previous;
  for(unsigned row=0;row<layout.height;++row){while(index<original.size()&&original[index].firstRow<=row)current=original[index++];SceneViewBand line=current;line.firstRow=row;
   if(owned){for(unsigned p=0;p<2;++p){unsigned sample=_frameLineSampling?row:0;bool known=_allRowsKnown||sample<128||(p==1&&sample<136&&(_tailRowsKnown&(1u<<(sample-128))));if(known)line.sourceX[p]=_rowX[p][sample];}}
   else line.sourceX[1]+=_lineScroll.sourceX(row);
   if(!row){for(unsigned p=0;p<2;++p){layout.sourceX[p]=line.sourceX[p];layout.sourceY[p]=line.sourceY[p];}}
   else if(line.sourceX[0]!=previous.sourceX[0]||line.sourceX[1]!=previous.sourceX[1]||line.sourceY[0]!=previous.sourceY[0]||line.sourceY[1]!=previous.sourceY[1]||line.displayEnabled!=previous.displayEnabled)layout.viewBands.push_back(line);previous=line;
  }
 }
 return layout;}
bool SceneRuntime::publishedPlanePosition(unsigned plane,int16_t& x,int16_t& y,std::string& error)const{
 error.clear();if(plane>=2||!_scrollEnabled){error="Published plane position is not owned";return false;}
 x=int16_t(integer(_scroll.state().positions[plane].x16));y=int16_t(integer(_scroll.state().positions[plane].y16));return true;
}
bool SceneRuntime::queueAbsolutePlanePosition(unsigned plane,int16_t x,int16_t y,std::string& error){
 // F3F8(1)/F47C: the absolute marker is shared by both planes. Keep the
 // other plane's pending request and velocity; its arithmetic remains global.
 ScrollState next=_scroll;if(!next.queuePosition(plane,kScrollX,x,error)||!next.queuePosition(plane,kScrollY,y,error))return false;
 next.nextUpdate(kScrollAbsolute);_scroll=next;return requestVisualCommand(1,error);
}
bool SceneRuntime::actorPieces(unsigned actor,std::vector<PlacedPiece>& pieces,std::string& error,int record)const{
 pieces.clear();error.clear();if(actor>=64||!_slots[actor].owned){error="mouse geometry requires owned actor";return false;}
 const ActorSlot& slot=_slots[actor];AnimationPlayer player=slot.player;unsigned frame=slot.frame;
 if(record>=0){if(!player.selectRecord(unsigned(record),error))return false;player._state.remaining=0;player._first=false;player._paused=false;player._halted=false;AnimationResult result=player.tick(error,[](void*,const AnimationCommand& command,std::string&)->AnimationCommandResult{return command.opcode>=0x90&&command.opcode<=0x93?kAnimationCommandAdvance:kAnimationCommandUnsupported;});if(result!=kAnimationFrame){if(error.empty())error="mouse candidate record has no static frame";return false;}frame=player.state().frameIndex;}
 else if(!slot.frameKnown){error="mouse actor geometry has no published frame";return false;}
 std::vector<FramePiece> geometry;if(!readFrameGeometry(slot.data->data(),slot.data->size(),slot.initializer.frameTableOffset,frame,slot.data->size()/6,geometry,error))return false;
 FramePlacement placement;const AnimationState& state=player.state();unsigned flags=state.styleWord&255;placement.x=integer(state.x16);placement.y=integer(state.y16);placement.flipX=(flags&1)!=0;placement.flipY=(flags&2)!=0;SceneLayout layout=currentRenderLayout();for(unsigned plane=0;plane<2;++plane)if(flags&(4u<<plane)){placement.x-=layout.sourceX[plane];placement.y-=layout.sourceY[plane];}return placeFrame(geometry,placement,slot.data->size()/6,pieces,error);
}
bool SceneRuntime::collectPieces(const SceneLayout& layout,std::vector<PlacedPiece>& prefix,std::vector<PlacedPiece>& suffix,std::string& error)const{
 prefix.clear();suffix.clear();if(_menuCursorOwned){prefix=_menuCursorPieces;for(auto& p:prefix)p.ownerActor=0;}for(unsigned i=_menuCursorOwned?1u:0u;i<64&&prefix.size()+suffix.size()<=64;++i){std::vector<PlacedPiece>& pieces=i<32?prefix:suffix;const ActorControlState* control=_actors.state(i);const ActorSlot& slot=_slots[i];if(!control->active||(control->flags&2)||!slot.owned||!slot.frameKnown)continue;std::vector<FramePiece> frame;if(!readFrameGeometry(slot.data->data(),slot.data->size(),slot.initializer.frameTableOffset,slot.frame,slot.data->size()/6,frame,error))return failActor(i,error,error);FramePlacement placement;const AnimationState& state=slot.player.state();unsigned flags=state.styleWord&255;placement.x=integer(state.x16);placement.y=integer(state.y16);placement.flipX=(flags&1)!=0;placement.flipY=(flags&2)!=0;for(unsigned plane=0;plane<2;++plane)if(flags&(4u<<plane)){placement.x-=layout.sourceX[plane];placement.y-=layout.sourceY[plane];}placement.overridePalette=(state.paletteOverride&128)!=0;placement.palette=state.paletteOverride&3;std::vector<PlacedPiece> placed;if(!placeFrame(frame,placement,slot.data->size()/6,placed,error))return failActor(i,error,error);for(auto& p:placed)p.ownerActor=uint8_t(i);size_t n=std::min(placed.size(),size_t(65-prefix.size()-suffix.size()));pieces.insert(pieces.end(),placed.begin(),placed.begin()+n);}return true;}
bool SceneRuntime::setEnvironmentMode(uint16_t mode,std::string& error){error.clear();if(mode>9||(mode&&!_environmentTables.known)){error="Environmental mode requires its authored visual owner";return false;}_environmentMode=mode;_environmentSubmissionKnown=false;return true;}
bool SceneRuntime::commitEnvironmentFrame(uint16_t counter,std::string& error){
 error.clear();if(!_renderer.configured()){if(!_environmentMode)return true;error="Environmental submission has no scene layout owner";return false;}
 std::vector<PlacedPiece> prefix,suffix,submission;if(!collectPieces(currentRenderLayout(),prefix,suffix,error)||!composeEnvironmentVisual(_environmentTables,_environmentMode,counter,prefix,suffix,_environmentState,submission,error))return false;
 _environmentSubmission.swap(submission);_environmentSubmissionKnown=true;
 // The menu task resumes from1103C only after the view report.1104E
 // activates its cursor after the current ED28 emission, never retroactively.
 if(_menuCursorPending&&!visualBusy()){_menuCursorPieces.swap(_menuCursorPendingPieces);_menuCursorPendingPieces.clear();_menuCursorOwned=!_menuCursorPieces.empty();_menuCursorPending=false;}
 return true;
}
bool SceneRuntime::render(SceneColorEncoding encoding,SceneImage& image,std::string& error,unsigned hiddenActor){
 image=SceneImage();if(_frameLineSampling&&!_allRowsKnown&&!_legacyUnknownRows){error="line sampling requires established original later-row history";return false;}if(!validatePhoneVisualState(_phoneVisual,error)||!validatePhoneFramePublication(_phoneFrame,error))return false;if(!_renderer.configured()){error="scene layout ownership unresolved";return false;}if(!_displayedHistoryKnown)for(unsigned i=_paletteBank;i<_paletteBank+64;++i)if(!_visibleKnownColors[i]){error="visible scene palette inheritance unresolved";return false;}SceneLayout layout=currentRenderLayout();
 if(!_renderer.configure(layout,error)||!_renderer.setPalette(_displayedHistoryKnown?_frameDisplayColors:_visibleColors,_displayedHistoryKnown?0:_paletteBank,error))return false;
 std::vector<PlacedPiece> pieces;if(_displayedSubmissionKnown)pieces=_displayedSubmission;else if(_environmentMode){if(!_environmentSubmissionKnown){error="Environmental visual submission has not reached its worker frame";return false;}pieces=_environmentSubmission;}else{std::vector<PlacedPiece> suffix;if(!collectPieces(layout,pieces,suffix,error))return false;pieces.insert(pieces.end(),suffix.begin(),suffix.end());}

 if(hiddenActor<64)pieces.erase(std::remove_if(pieces.begin(),pieces.end(),[hiddenActor](const PlacedPiece& p){return p.ownerActor==hiddenActor;}),pieces.end());
 std::vector<ScenePaletteSpan> spans;if(!_frameLowerColors.empty()){std::vector<uint16_t> source(128,0);std::copy(_frameLowerColors.begin(),_frameLowerColors.end(),source.begin()+80);if(!lowerViewPaletteSpans(source,std::vector<bool>(128,true),spans,error))return false;}return _renderer.render(pieces,encoding,image,error,&spans);
}
bool SceneRuntime::dialogueLayout(DialogueLayout& out,std::string& error)const{error.clear();if(!_dialogueLayout.established){error="inherited dialogue layout ownership unresolved";return false;}out=_dialogueLayout;if(_fontModeKnown)out.fixedWidth=_fontFixedWidth;return true;}
bool SceneRuntime::prepareDialogue(DialogueWindow& window,std::string& error)const{
 error.clear();if(window._canvasFresh)return true;if(_fontModeKnown&&window._fontModeBound&&!window._fontModePending)window._layout.fixedWidth=_fontFixedWidth;
 SceneGraphicsCopy copy;copy.channel=3;copy.count=8192;GraphicsTransfer patterns,names;if(!_graphicsTasks.resolveCopy(copy,patterns,error))return false;copy.channel=2;copy.offset=0x1000;copy.count=512;if(!_graphicsTasks.resolveCopy(copy,names,error))return false;
 if(std::find(patterns.knownBytes.begin(),patterns.knownBytes.end(),false)!=patterns.knownBytes.end()||std::find(names.knownBytes.begin(),names.knownBytes.end(),false)!=names.knownBytes.end()){error="dialogue continuation requires its shared decoded canvas owner";return false;}
 window._patterns=patterns.decoded;window._names=names.decoded;return true;
}
bool SceneRuntime::prepareTextEntry(TextEntry& entry,std::string& error)const{
 error.clear();if(_fontModeKnown&&!entry._graphicsFont.modeChanged)entry._graphicsFont.fixedWidth=_fontFixedWidth;if(entry._graphicsFresh)return true;SceneGraphicsCopy copy;copy.channel=3;copy.count=8192;GraphicsTransfer patterns,names;if(!_graphicsTasks.resolveCopy(copy,patterns,error))return false;copy.channel=2;copy.offset=0x1000;copy.count=512;if(!_graphicsTasks.resolveCopy(copy,names,error))return false;
 if(std::find(patterns.knownBytes.begin(),patterns.knownBytes.end(),false)!=patterns.knownBytes.end()||std::find(names.knownBytes.begin(),names.knownBytes.end(),false)!=names.knownBytes.end()){error="entry font continuation requires its shared decoded canvas owner";return false;}entry._graphicsCanvas=patterns.decoded;entry._graphicsNames=names.decoded;return true;
}
bool SceneRuntime::stageTextEntry(TextEntry& entry,std::string& error){
 error.clear();SceneRuntime next=*this;const auto& source=entry._graphicsSource;if(entry._graphicsFont.modeChanged){next._fontModeKnown=true;next._fontFixedWidth=entry._graphicsFont.fixedWidth;}
 if(entry._graphicsResetCaption){PaletteEffect clear;if(!readPaletteEffect(source.data(),source.size(),0x139d0-0xd400,source.size(),clear,error,0xd400))return false;if(clear.type!=10||clear.byteOffset!=160||clear.countMinusOne!=47){error="entry caption reset differs from its original palette owner";return false;}if(!next.queuePaletteEffect(clear,error))return false;next.deactivateCaptionActors();next.clearMenuCursor();}
 if(entry._graphicsPolicy&&!next.requestPalettePublication(255,error))return false;
 if(entry._graphicsPalette){PaletteEffect palette;if(!readPaletteEffect(source.data(),source.size(),(entry._mode==3?0x11382:0x1138c)-0xd400,source.size(),palette,error,0xd400)||!next.queuePaletteEffect(palette,error))return false;}
 if(entry._graphicsView>=0&&!next.requestVisualCommand(uint16_t(entry._graphicsView),error))return false;
 if(entry._graphicsCanvas.size()!=8192||entry._graphicsNames.size()!=512){error="entry font allocations are not established";return false;}
 if((entry._graphicsClearCanvas||entry._graphicsDirtyRows)&&!next._graphicsTasks.setStaging(3,entry._graphicsCanvas,error))return false;
 if(entry._graphicsClearBuffer&&!next._graphicsTasks.writeStaging(3,3072,std::vector<uint8_t>(800,0),error))return false;
 if(entry._graphicsNamesPending&&!next._graphicsTasks.writeStaging(2,0x1000,entry._graphicsNames,error))return false;
 std::vector<GraphicsTransfer> copies;auto post=[&](uint16_t destination,const std::vector<uint8_t>& bytes){GraphicsTransfer t;t.recordOffset=t.sourceOffset=t.encodedEnd=0;t.delay=0;t.destination=destination;t.decoded=bytes;copies.push_back(t);};
 if(entry._graphicsNamesPending)post(0xec80,entry._graphicsNames);
 if(entry._graphicsClearCanvas){next._fontClearPending=true;post(8160,std::vector<uint8_t>(entry._graphicsCanvas.end()-32,entry._graphicsCanvas.end()));}
 if(entry._graphicsDirtyRows){unsigned first=0,last=7;while(!(entry._graphicsDirtyRows&(1u<<first)))++first;while(!(entry._graphicsDirtyRows&(1u<<last)))--last;post(uint16_t(first*1024),std::vector<uint8_t>(entry._graphicsCanvas.begin()+first*1024,entry._graphicsCanvas.begin()+(last+1)*1024));}
 if(!next.applyDialogueTransfers(copies,error))return false;*this=next;if(entry._graphicsClearCanvas||entry._graphicsDirtyRows)entry._graphicsFresh=false;entry._graphicsResetCaption=entry._graphicsPolicy=entry._graphicsPalette=entry._graphicsNamesPending=entry._graphicsClearCanvas=entry._graphicsClearBuffer=false;entry._graphicsDirtyRows=0;entry._graphicsView=-1;entry._graphicsFont.modeChanged=false;return true;
}
bool SceneRuntime::stageDialogue(DialogueWindow& window,std::string& error){
 std::vector<GraphicsTransfer> full;if(!window.transfers(full,error))return false;SceneRuntime next=*this;if(window._fontModePending){next._fontModeKnown=true;next._fontFixedWidth=window._layout.fixedWidth;}
 // The producer works in shared caches. Main clears displayed font bytes
 // separately; glyph publication copies only its first through last dirty row.
 if((window._clearCanvasPending||window._dirtyRows)&&!next._graphicsTasks.setStaging(3,full[0].decoded,error))return false;if(window._namesDirty&&!next._graphicsTasks.writeStaging(2,0x1000,full[1].decoded,error))return false;
 if(window._captionResetPending){PaletteEffect clear;const size_t at=0x13622-0xd400;if(!readPaletteEffect(window._subcode.data(),window._subcode.size(),at,window._subcode.size(),clear,error,0xd400))return false;if(clear.type!=10||clear.byteOffset!=160||clear.countMinusOne!=47){error="dialogue secondary palette reset differs from source owner";return false;}if(!next.queuePaletteEffect(clear,error))return false;next.deactivateCaptionActors();next.clearMenuCursor();}
 if(window._policyPending&&!next.requestPalettePublication(255,error))return false;
 if(window._atlasPending){SceneGraphicsTask atlas;if(!readSceneGraphicsTask(window._subcode.data(),window._subcode.size(),0xecb6-0xd400,0xd400,atlas,error)||!next.queueGraphicsTasks({atlas},error))return false;}
 std::vector<GraphicsTransfer> copies;
 if(window._namesDirty)copies.push_back(full[1]);
 if(window._clearCanvasPending){next._fontClearPending=true;GraphicsTransfer tail=full[0];tail.destination=8160;tail.decoded.assign(full[0].decoded.end()-32,full[0].decoded.end());copies.push_back(tail);}
 if(window._promptNamesDirty){
  // Source1383C fills the two raw task records before E846. They stage
  // eight bytes in cache1; they do not rewrite the base font name canvas.
  SceneGraphicsTask prompt;prompt.flags=0x52;const size_t records=0x1387c-0xd400+size_t(window._layout.variant)*24;
  for(unsigned row=0;row<2;++row){unsigned destination=sourceWord(window._subcode.data()+records+6+row*10);if(destination<0xec80||destination>0xee7c){error="prompt name copy exceeds its source allocation";return false;}GraphicsTransfer transfer=full[1];transfer.destination=uint16_t(destination);size_t at=destination-0xec80;transfer.decoded.assign(full[1].decoded.begin()+at,full[1].decoded.begin()+at+4);prompt.transfers.push_back(transfer);prompt.stagingOffsets.push_back(uint16_t(row*4));prompt.copyLengths.push_back(4);prompt.stagingSources.push_back({});}
  if(!next._graphicsTasks.enqueue(prompt,error))return false;
 }
 if(window._dirtyRows){unsigned first=0,last=7;while(!(window._dirtyRows&(1u<<first)))++first;while(!(window._dirtyRows&(1u<<last)))--last;GraphicsTransfer rows=full[0];rows.destination=uint16_t(first*1024);rows.decoded.assign(full[0].decoded.begin()+first*1024,full[0].decoded.begin()+(last+1)*1024);copies.push_back(rows);}
 if(window._menuCursorPending){std::vector<PlacedPiece> pieces;if(!window.menuPieces(pieces,error)||!next.setMenuCursorPieces(pieces,error))return false;}
 if(!window._taskOwned&&!window._menuTask)for(size_t i=2;i<full.size();++i)copies.push_back(full[i]);
 if(!next.applyDialogueTransfers(copies,error))return false;*this=next;if(window._fontModePending)window._fontModeBound=true;if(window._clearCanvasPending||window._dirtyRows)window._canvasFresh=false;window._dirtyRows=0;window._namesDirty=window._clearCanvasPending=window._promptNamesDirty=false;window._captionResetPending=window._policyPending=window._atlasPending=window._menuCursorPending=window._fontModePending=false;return true;
}
bool SceneRuntime::applyDialogueTransfers(const std::vector<GraphicsTransfer>& transfers,std::string& error){
 SceneGraphicsQueue staging=_graphicsTasks;auto copies=_graphicsCopies;
 for(const auto& transfer:transfers){SceneGraphicsCopy copy;copy.destination=transfer.destination;if(transfer.decoded.empty()||transfer.decoded.size()>8192){error="invalid dialogue copy allocation";return false;}copy.count=uint16_t(transfer.decoded.size());
  // Source13D4E/13D8C: the canvas is cache3, its names cache2+1000.
  if(transfer.destination<8192&&transfer.decoded.size()<=8192-transfer.destination){copy.channel=3;copy.offset=transfer.destination;if(!staging.writeStaging(3,copy.offset,transfer.decoded,error))return false;}
  else if(transfer.destination==0xec80&&transfer.decoded.size()==512){copy.channel=2;copy.offset=0x1000;if(!staging.writeStaging(2,copy.offset,transfer.decoded,error))return false;}
  else{copy.bytes=transfer.decoded;}
  copies.push_back(copy);
 }
 _graphicsTasks=staging;_graphicsCopies=copies;return true;
}
bool SceneRuntime::establishDialogueView(unsigned mode,std::string& error){
 error.clear();if(mode!=9&&mode!=14&&mode!=15&&mode!=16){error="unrecovered dialogue presentation mode";return false;}if(_baseLayout.width!=256||_baseLayout.height!=224||_baseLayout.columns!=32||_baseLayout.rows!=64){error="dialogue window requires original tall layout";return false;}
 unsigned first=mode==9?144:mode==14?160:mode==15?72:168;int origin=mode==9?256:mode==14?240:mode==15?328:232;return applyLowerWindow(first,origin,mode==9,error);
}
bool SceneRuntime::applyLowerWindow(unsigned first,int origin,bool both,std::string& error){
 SceneLayout layout=_baseLayout;layout.viewBands.clear();SceneViewBand lower;lower.firstRow=first;for(unsigned p=0;p<2;++p){lower.sourceX[p]=layout.sourceX[p]-integer(_scroll.state().positions[p].x16);lower.sourceY[p]=p==0||both?origin:layout.sourceY[p]-integer(_scroll.state().positions[p].y16);}layout.viewBands.push_back(lower);SceneRenderer next=_renderer;if(!next.configure(layout,error))return false;_baseLayout=layout;_renderer=next;return true;
}
bool SceneRuntime::requestPalettePublication(uint8_t command,std::string& error){error.clear();_pendingPalettePublication=command;return true;}
bool SceneRuntime::setPlaneAllocation(unsigned plane,unsigned allocation,std::string& error){
 error.clear();if(plane>=2||(allocation!=0xc000&&allocation!=0xe000)){error="invalid source plane allocation";return false;}SceneLayout layout=_baseLayout;layout.planeAllocation[plane]=allocation;SceneRenderer next=_renderer;if(!next.configure(layout,error))return false;_baseLayout=layout;_renderer=next;return true;
}
bool SceneRuntime::applyCinematicShutter(unsigned amplitude,std::string& error){
 if(_visualCommand<8||_visualCommand>12||amplitude>5){error="invalid cinematic shutter owner";return false;}SceneLayout layout=_baseLayout;layout.viewBands.clear();layout.displayEnabled=true;layout.sourceX[0]=layout.sourceX[1]=layout.sourceY[0]=layout.sourceY[1]=0;
 // FF0DB0/E30/E4C/E68/E86 use eight-pixel rows. The first
 // source variant changes plane A at row0; later variants leave prior text
 // visible until their owned negative/positive split. Plane B is unchanged.
 unsigned negative=_visualCommand==8?0:72+24*unsigned(_visualCommand-9),positive=_visualCommand==8?56:negative+8,blank=positive+8;
 if(negative){SceneViewBand band;band.firstRow=negative;band.sourceY[0]=-int(amplitude);layout.viewBands.push_back(band);}else layout.sourceY[0]=-int(amplitude);
 SceneViewBand second;second.firstRow=positive;second.sourceY[0]=int(amplitude);layout.viewBands.push_back(second);SceneViewBand bottom;bottom.firstRow=blank;bottom.displayEnabled=false;layout.viewBands.push_back(bottom);
 SceneRenderer next=_renderer;if(!next.configure(layout,error))return false;_baseLayout=layout;_renderer=next;return true;
}
bool SceneRuntime::applyPhoneView(std::string& error){
 // Active physical phone bands are a frame-local override. Only an actual
 // source cleanup establishes a replacement ordinary base layout.
 if(_phoneVisual.mode)return true;SceneLayout layout=_baseLayout;layout.viewBands.clear();layout.sourceY[0]=integer(_scroll.state().positions[0].y16);layout.sourceY[1]=_phoneVisual.publishedPlaneBY+integer(_scroll.state().positions[1].y16);SceneRenderer next=_renderer;if(!next.configure(layout,error))return false;_baseLayout=layout;_renderer=next;return true;
}
// Main FF396E/FF39EA: source alphabet/input panel split and lower window.
bool SceneRuntime::applyEntryView(bool closing,std::string& error){
 SceneLayout layout=_baseLayout;layout.viewBands.clear();
 const unsigned phase=unsigned(_visualCountdown);
 const bool grid=!closing||phase<4;
 auto band=[&](unsigned row,int y){SceneViewBand b;b.firstRow=row;for(unsigned p=0;p<2;++p){b.sourceX[p]=layout.sourceX[p]-integer(_scroll.state().positions[p].x16);b.sourceY[p]=y;}layout.viewBands.push_back(b);};
 if(grid){int gridY=closing?(_visualThreshold==101?338:437-_visualThreshold):320;band(100,gridY);SceneViewBand world;world.firstRow=unsigned(_visualThreshold+1);for(unsigned p=0;p<2;++p){world.sourceX[p]=layout.sourceX[p]-integer(_scroll.state().positions[p].x16);world.sourceY[p]=layout.sourceY[p]-integer(_scroll.state().positions[p].y16);}layout.viewBands.push_back(world);}
 if(closing||phase>=2)band(144,_visualOrigin);
 SceneRenderer next=_renderer;if(!next.configure(layout,error))return false;_renderer=next;_baseLayout=layout;return true;
}
bool SceneRuntime::requestVisualCommand(uint16_t command,std::string& error){
 // Sub F416 disables scroll production; Main33FE freezes only the secondary
 // owner. Retain its table/counters and the independent primary mailbox.
 error.clear();uint8_t low=uint8_t(command);
 // SubF416 owns0/1/2 in the secondary mailbox8F0B and the scroll producer.
 // Main34CA selects full-plane publication without replacing primary E060,
 // its window bands, transition progress or a pending phone request.
 if(low<=2){_pendingSecondaryMode=low;if(low)_legacyLineComposition=!_rowSamplingKnown;_scrollEnabled=low!=0;_scroll.nextUpdate(low==1?kScrollAbsolute:kScrollRelative);return true;}
 if(low==0xfd){_pendingSecondaryMode=0;_secondaryScrollFrozen=true;_scrollEnabled=false;return true;}if(!_visualKnown){error="inherited visual command owner unresolved";return false;}
 if((low>=3&&low<=0x1d)||low==0xfc||low==0xfe||low==0xff)_pendingDialogueView=0;
 if((low==0x1c||low==0x1d)&&!_displayedHistoryKnown){error="phone visual requires established displayed palette history";return false;}
 if((low>=0x17&&low<=0x1d)||((_phoneVisual.mode||_pendingPhoneKnown)&&(low==0x0f||low==0xfe))){_pendingPhoneRequest=low;_pendingPhoneKnown=true;return true;}
 bool supported=(low>=3&&low<=0x13)||(low>=0x14&&low<=0x16)||low==0xfc||low==0xfe||low==0xff;
 if(!supported){std::ostringstream message;message<<"visual command "<<unsigned(low)<<" requires its native transition/scroll owner";error=message.str();return false;}
 // Independent secondary distortion/scroll does not replace the primary
 // mailbox. Other primary/global commands cancel a not-yet-accepted phone.
 if(!(low>=1&&low<=6)){_pendingPhoneKnown=false;_pendingPhoneRequest=0;}
 if(_phoneVisual.mode&&low!=1&&!(low>=3&&low<=6)){int16_t published=_phoneVisual.publishedPlaneBY;bool temporary=_phoneVisual.temporaryDialogue;_phoneVisual=PhoneVisualState();_phoneVisual.publishedPlaneBY=published;_phoneVisual.temporaryDialogue=temporary;_baseLayout.viewBands.clear();}
 // F3F8 uses the low byte. Queue acceptance is distinct from completion of a
 // transition. Only closed native visual owners are accepted here.
 // Main FF34CA selects full-plane scrolling and the game transfer owner.
 // E076 changes the platform publication path, not scene pixels or duration.
 // Main34E0..34EA clears both E070 and E071 during initialization.
 // The active mode is therefore not the duplicate-request memo.
 if(low>=3&&low<=6){if(_secondaryScrollMemo==low)return true;_secondaryScrollMemo=low;_scrollEnabled=false;_legacyLineComposition=!_rowSamplingKnown;_pendingSecondaryMode=low;_secondaryScrollFrozen=false;return true;}
 if(low==7){if(_visualCommand==low)return true;_visualCommand=low;_visualBusy=true;_visualThreshold=5;_visualCountdown=0;return true;}
 if(low>=8&&low<=12){if(_visualCommand==low)return true;_visualCommand=low;_visualBusy=true;_visualCountdown=23;_scroll.clear();_scrollEnabled=false;return applyCinematicShutter(5,error);}
 if(low==0x0d){if(_visualCommand==low)return true;if(_baseLayout.width!=256||_baseLayout.height!=224||_baseLayout.columns!=32||_baseLayout.rows!=64){error="menu transition requires original tall layout";return false;}_visualCommand=low;_visualBusy=true;_visualThreshold=252;_visualOrigin=144;_baseLayout.viewBands.clear();return true;}
 if(low==0x0e||low==0x13){if(_visualCommand==low)return true;if(_baseLayout.width!=256||_baseLayout.height!=224||_baseLayout.columns!=32||_baseLayout.rows!=64){error="menu close requires original tall layout";return false;}_visualCommand=low;_visualBusy=true;_visualCountdown=0;return true;}
 if(low==0x10||low==0x11){if(_visualCommand==low)return true;if(_baseLayout.width!=256||_baseLayout.height!=224||_baseLayout.columns!=32||_baseLayout.rows!=64){error="entry panel requires original tall layout";return false;}_visualCommand=low;_visualBusy=true;_visualCountdown=0;_visualThreshold=low==0x10?100:117;_visualOrigin=low==0x10?0:112;return true;}
 if(low==0x12){if(_visualCommand==low)return true;if(_baseLayout.width!=256||_baseLayout.height!=224||_baseLayout.columns!=32||_baseLayout.rows!=64){error="alternate menu requires original tall layout";return false;}_visualCommand=low;_visualBusy=true;_visualOrigin=144;return applyLowerWindow(160,_visualOrigin,false,error);}
 if(low>=0x14&&low<=0x16){if(!establishDialogueView(unsigned(low)-6,error))return false;_visualCommand=low;_visualBusy=false;return true;}
 if(low==0x0f){if(_visualCommand==0x17){error="dialogue visual command requires interrupted mode17 owner";return false;}if(!establishDialogueView(9,error))return false;_visualCommand=low;_visualBusy=false;return true;}
 if(low==0xfc||low==0xff){_phoneVisual=PhoneVisualState();for(unsigned p=0;p<2;++p)_baseLayout.sourceX[p]=_baseLayout.sourceY[p]=0;_pendingSamplingReset=true;_pendingSecondaryMode=0;_legacyLineComposition=!_rowSamplingKnown;_secondaryScrollFrozen=false;_secondaryScrollMemo=0;_visualCountdown=0;_baseLayout.displayEnabled=true;_baseLayout.viewBands.clear();_scroll.clear();_scrollEnabled=false;_visualCommand=0;_visualBusy=false;return true;}
 if(low==0xfe){if(_visualCommand==0x17){error="visual resume from mode17 unresolved";return false;}_visualCountdown=0;_baseLayout.displayEnabled=true;_baseLayout.viewBands.clear();_visualCommand=0;_visualBusy=false;return true;}
 std::ostringstream message;message<<"visual command "<<unsigned(low)<<" requires its native transition/scroll owner";error=message.str();return false;
}
bool SceneRuntime::requestDialogueView(uint16_t speakerWord,std::string& error){if(!_dialogueLayout.established){error="dialogue visual layout owner unresolved";return false;}if(uint8_t(speakerWord)==1)return requestVisualCommand(0x15,error);return requestVisualCommand(_dialogueLayout.variant==0?0x0f:_dialogueLayout.variant==1?0x14:0x16,error);}
bool SceneRuntime::postDialogueView(uint16_t command,std::string& error){
 error.clear();if(!_visualKnown||!_dialogueLayout.established||(command!=0x0f&&command!=0x14&&command!=0x15&&command!=0x16)){error="invalid posted dialogue view owner";return false;}
 _pendingDialogueView=uint8_t(command);return true;
}
bool SceneRuntime::requestMenuClose(std::string& error){if(!_dialogueLayout.established){error="menu close layout owner unresolved";return false;}return requestVisualCommand(_dialogueLayout.variant?0x13:0x0e,error);}
bool SceneRuntime::requestMenuView(std::string& error){if(!_dialogueLayout.established){error="menu visual layout owner unresolved";return false;}return requestVisualCommand(_dialogueLayout.variant?0x12:0x0d,error);}
bool SceneRuntime::updateVisual(std::string& error){error.clear();
 if(_pendingPhoneKnown){uint8_t command=_pendingPhoneRequest;_pendingPhoneKnown=false;_pendingPhoneRequest=0;PhoneVisualRequestResult result=requestPhoneVisual(_phoneVisual,command,error);if(result==kPhoneVisualInvalid)return false;if(result==kPhoneVisualStartOrdinaryDialogue){if(!applyPhoneView(error)||!establishDialogueView(9,error))return false;_visualCommand=0x0f;_visualBusy=false;return true;}if(result==kPhoneVisualDelegate){if(_phoneVisual.mode){int16_t shadow=_phoneVisual.publishedPlaneBY;bool temporary=_phoneVisual.temporaryDialogue;_phoneVisual=PhoneVisualState();_phoneVisual.publishedPlaneBY=shadow;_phoneVisual.temporaryDialogue=temporary;_baseLayout.viewBands.clear();}if(!requestVisualCommand(command,error))return false;}else{_visualCommand=_phoneVisual.mode;_visualBusy=phoneVisualBusy(_phoneVisual);if(!_phoneVisual.mode)return applyPhoneView(error);}}
 if(_phoneVisual.mode){if(!tickPhoneVisual(_phoneVisual,error))return false;_visualCommand=_phoneVisual.mode;_visualBusy=phoneVisualBusy(_phoneVisual);return applyPhoneView(error);}if(!_visualBusy)return true;
 if(_visualCommand==0x10){switch(_visualCountdown){case 0:_visualThreshold=100;_visualOrigin=0;++_visualCountdown;break;case 1:_visualThreshold+=3;if(_visualThreshold>=117){_visualThreshold=117;++_visualCountdown;}break;case 2:_visualOrigin=0;++_visualCountdown;break;case 3:_visualOrigin+=10;if(_visualOrigin>=112){_visualOrigin=112;++_visualCountdown;}break;default:_visualBusy=false;break;}return applyEntryView(false,error);}
 if(_visualCommand==0x11){switch(_visualCountdown){case 0:_visualThreshold=117;_visualOrigin=112;++_visualCountdown;break;case 1:_visualThreshold-=3;if(_visualThreshold<100){_visualThreshold=101;++_visualCountdown;}break;case 2:_visualOrigin=112;++_visualCountdown;break;case 3:++_visualCountdown;break;case 4:_visualOrigin-=10;if(_visualOrigin<0){_visualOrigin=0;++_visualCountdown;}break;default:_visualBusy=false;_visualCommand=0;_baseLayout.viewBands.clear();return _renderer.configure(_baseLayout,error);}return applyEntryView(true,error);}
// Main3898/38F2 and3A64/3AAC: close is a distinct source owner, not
 // cancellation of the open layout. Keep init, nineteen-pixel steps and the
 // final report separate, just as Sub11E2A waits for7892 before returning.
 if(_visualCommand==0x0e||_visualCommand==0x13){bool ordinary=_visualCommand==0x0e;
  if(_visualCountdown==0){_visualThreshold=140;_visualOrigin=ordinary?256:240;_visualCountdown=1;}
  else if(_visualCountdown==1){if(ordinary)_visualThreshold+=19;_visualOrigin-=19;if(_visualOrigin<144){_visualOrigin=144;_visualCountdown=2;}}
  else{_visualBusy=false;_visualCommand=0;_visualCountdown=_visualThreshold=_visualOrigin=0;_baseLayout.viewBands.clear();return _renderer.configure(_baseLayout,error);}
  if(!ordinary)return applyLowerWindow(160,_visualOrigin,false,error);
  SceneLayout layout=_baseLayout;layout.viewBands.clear();if(_visualThreshold+1<int(layout.height)){SceneViewBand band;band.firstRow=unsigned(_visualThreshold+1);band.sourceY[0]=band.sourceY[1]=_visualOrigin;layout.viewBands.push_back(band);}SceneRenderer staged=_renderer;if(!staged.configure(layout,error))return false;_renderer=staged;_baseLayout=layout;return true;
 }
 if(_visualCommand==0x12){_visualOrigin+=19;if(_visualOrigin>=240){_visualOrigin=240;_visualBusy=false;}return applyLowerWindow(160,_visualOrigin,false,error);}if(_visualCommand>=8&&_visualCommand<=12){
 if(--_visualCountdown<0){_visualCountdown=0;_visualBusy=false;if(_visualCommand==12){_visualCommand=0;_baseLayout.viewBands.clear();_baseLayout.displayEnabled=true;_baseLayout.sourceY[0]=_baseLayout.sourceY[1]=0;return true;}}
 return applyCinematicShutter(unsigned(_visualCountdown)/4,error);
 }if(_visualCommand==7){
 SceneLayout layout=_baseLayout;layout.viewBands.clear();
 if(!_visualCountdown){
  // Main380E ->3438 initializes before the next H-blank program runs.
  // E076 and published E700/EB00 clear; Sub8F30 positions are retained.
  _visualCountdown=1;_fullPlanePublicationEnabled=false;_scrollPublicationKnown=true;_frameScrollY[0]=_frameScrollY[1]=0;
  for(unsigned p=0;p<2;++p){_rowX[p][0]=0;layout.sourceX[p]=layout.sourceY[p]=0;}
  _pendingSecondaryMode=0;_fullPlaneScrollOverride=true;_frameLineSampling=false;layout.displayEnabled=false;
 }else{--_visualThreshold;layout.displayEnabled=true;
 if(_visualThreshold>0){int offset=_visualThreshold>=2?48:16;unsigned first=_visualThreshold>=2?64:96,last=_visualThreshold>=2?159:127;layout.sourceY[0]=layout.sourceY[1]=offset;SceneViewBand blank;blank.firstRow=first;blank.displayEnabled=false;layout.viewBands.push_back(blank);SceneViewBand lower;lower.firstRow=last;lower.sourceY[0]=lower.sourceY[1]=-offset;layout.viewBands.push_back(lower);}
 else{layout.sourceY[0]=layout.sourceY[1]=0;if(_visualThreshold<0){_visualBusy=false;_visualCommand=0;}}
 }
 SceneRenderer next=_renderer;if(!next.configure(layout,error))return false;_renderer=next;_baseLayout=layout;return true;
 }if(_visualCommand!=0x0d){error="active visual transition owner unresolved";return false;}_visualThreshold-=19;_visualOrigin+=19;if(_visualOrigin>=256){_visualThreshold=136;_visualOrigin=256;_visualBusy=false;}SceneLayout layout=_baseLayout;layout.viewBands.clear();if(_visualThreshold+1<int(layout.height)){SceneViewBand lower;lower.firstRow=size_t(_visualThreshold+1);lower.sourceY[0]=lower.sourceY[1]=_visualOrigin;layout.viewBands.push_back(lower);}SceneRenderer next=_renderer;if(!next.configure(layout,error))return false;_renderer=next;_baseLayout=layout;return true;}
void SceneRuntime::deactivateCaptionActors(){for(unsigned actor=5;actor<15;++actor){_actors._actors[actor].active=false;_actors._actors[actor].reserved=false;}}
bool SceneRuntime::setMenuCursorPieces(const std::vector<PlacedPiece>& pieces,std::string& error){error.clear();if(_slots[0].owned){error="menu cursor cannot overwrite an independently owned actor0";return false;}if(pieces.empty()||pieces.size()>64){error="menu cursor piece ownership outside native submission limit";return false;}for(const PlacedPiece& piece:pieces)if(!piece.width||!piece.height||piece.width>32||piece.height>32||piece.width%8||piece.height%8){error="invalid menu cursor geometry";return false;}_menuCursorPendingPieces=pieces;_menuCursorPending=true;return true;}

}
