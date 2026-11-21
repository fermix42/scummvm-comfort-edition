/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include "snatcher/engine.h"
#include "snatcher/native_visual_jobs.h"
#include "snatcher/engine_save.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/file.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/formats/json.h"
#include "engines/util.h"
#include "engines/metaengine.h"
#include "engines/ce_test_cli.h"
#include "engines/ce_achievements.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/paletteman.h"
#include "graphics/surface.h"
#include "graphics/cursorman.h"
#include "common/savefile.h"
#include "snatcher/state_codec.h"
#include "snatcher/event_state.h"
#include "snatcher/timing_state.h"
#include "snatcher/narrative_state.h"
#include "snatcher/audio_fm.h"
#include "audio/mixer_intern.h"
#include "base/version.h"
#include "gui/message.h"
#include "gui/saveload-dialog.h"
#ifdef USE_OPENGL
#include "graphics/opengl/debug.h"
#endif
#ifdef WIN32
#include "backends/platform/sdl/win32/process-metrics.h"
#endif
namespace Snatcher {
namespace {
uint64 testInstancesCreated=0,testDestructorsEntered=0,testLastDestructionSerial=0;
bool testLastDestructionDiscOpen=false;
}
void SnatcherEngine::noteTestInstanceCreated(){
 if(CETestCLI::active()&&testInstancesCreated<UINT64_MAX)_testInstanceSerial=++testInstancesCreated;
}
void SnatcherEngine::noteTestInstanceDestruction(){
 if(!_testInstanceSerial)return;
 if(testDestructorsEntered<UINT64_MAX)++testDestructorsEntered;
 testLastDestructionSerial=_testInstanceSerial;testLastDestructionDiscOpen=_resources.isOpen();
}
void SnatcherEngine::resetVisualObserver(){_visualObserver=VisualWorkerObserver();}
void SnatcherEngine::recordInvestigationFrame(){
 _investigationFrame.millis=_system->getMillis();_investigationFrame.frame=++_investigationFrames;
 if(_runtime){_investigationFrame.resource=_runtime->cursor().resource;_investigationFrame.node=_runtime->cursor().node;}
 _investigationFrame.pending=_events.pending();
 if(_investigationHistory.empty()||uint32_t(_investigationFrame.millis-_investigationLastSample)>=1000||_investigationHistory.back().resource!=_investigationFrame.resource){
#ifdef WIN32
  const Win32::ProcessMetrics metrics=Win32::processMetrics();_investigationFrame.processMetrics=metrics.known;_investigationFrame.workingSet=metrics.workingSet;_investigationFrame.privateBytes=metrics.privateBytes;_investigationFrame.handles=metrics.handles;_investigationFrame.cpu100ns=metrics.cpu100ns;_investigationFrame.availablePhysical=metrics.availablePhysical;
#endif
  if(_investigationHistory.size()==1800)_investigationHistory.pop_front();
  _investigationHistory.push_back(_investigationFrame);_investigationLastSample=_investigationFrame.millis;
  _investigationFrame=InvestigationSample();
 }
}
namespace {
// SaveFileManager keeps all evidence in the installation's managed save root.
// Finalize and read back before allowing the UI to report completion.
Common::JSONValue *uploadInfoJSON(const GraphicsDiagnostics::UploadInfo& info){
 Common::JSONObject object;
 object["texture_id"]=new Common::JSONValue((long long)info.texture);
 object["internal_format"]=new Common::JSONValue((long long)info.internalFormat);
 object["format"]=new Common::JSONValue((long long)info.format);object["type"]=new Common::JSONValue((long long)info.type);
 object["millis"]=new Common::JSONValue((long long)info.millis);object["duration_ms"]=new Common::JSONValue((long long)info.duration);
 object["allocated_width"]=new Common::JSONValue((long long)info.allocatedWidth);object["allocated_height"]=new Common::JSONValue((long long)info.allocatedHeight);
 object["source_width"]=new Common::JSONValue((long long)info.sourceWidth);object["source_height"]=new Common::JSONValue((long long)info.sourceHeight);
 object["pitch"]=new Common::JSONValue((long long)info.pitch);object["bytes_per_pixel"]=new Common::JSONValue((long long)info.bpp);
 object["dirty_left"]=new Common::JSONValue((long long)info.left);object["dirty_top"]=new Common::JSONValue((long long)info.top);
 object["dirty_right"]=new Common::JSONValue((long long)info.right);object["dirty_bottom"]=new Common::JSONValue((long long)info.bottom);
 object["uploaded_width"]=new Common::JSONValue((long long)info.sourceWidth);object["uploaded_height"]=new Common::JSONValue((long long)(info.bottom-info.top));
 object["uploaded_bytes"]=new Common::JSONValue((long long)info.bytes);
 return new Common::JSONValue(object);
}
Common::JSONValue *textureTimingsJSON(const GraphicsDiagnostics::Frame& frame){
 Common::JSONObject textures;
 for(unsigned role=0;role<GraphicsDiagnostics::kTextureRoleCount;++role){
  const auto& texture=frame.textures[role];Common::JSONObject object;
  for(unsigned op=0;op<GraphicsDiagnostics::kTextureOpCount;++op){
   const auto& timing=texture.operations[op];Common::JSONObject value;
   value["max_ms"]=new Common::JSONValue((long long)timing.maximum);value["total_ms"]=new Common::JSONValue((long long)timing.total);value["calls"]=new Common::JSONValue((long long)timing.calls);
   object[GraphicsDiagnostics::textureOpName(op)]=new Common::JSONValue(value);
  }
  object["uploads"]=new Common::JSONValue((long long)texture.uploads);object["uploaded_bytes"]=new Common::JSONValue((long long)texture.bytes);
  if(texture.uploads){object["last_upload"]=uploadInfoJSON(texture.last);object["slowest_upload"]=uploadInfoJSON(texture.slowest);}
  textures[GraphicsDiagnostics::textureRoleName(role)]=new Common::JSONValue(object);
 }
 return new Common::JSONValue(textures);
}
bool writeInvestigationJSON(Common::SaveFileManager& manager,const Common::String& name,const Common::JSONValue& value){
 Common::String bytes=value.stringify(true);Common::OutSaveFile* out=manager.openForSaving(name,false);if(!out)return false;
 bool good=out->write(bytes.c_str(),bytes.size())==bytes.size();out->finalize();good=good&&!out->err();delete out;if(!good)return false;
 Common::InSaveFile* in=manager.openForLoading(name);if(!in)return false;
 Common::String read;while(!in->eos()&&!in->err()){char block[4096];uint32 n=in->read(block,sizeof(block));read.append(block,block+n);}good=!in->err()&&read==bytes;delete in;return good;
}
}
bool SnatcherEngine::captureInvestigation(Common::String& message){
 _investigationComplete=false;_investigationStatus="Capturing investigation";
 auto* manager=_system->getSavefileManager();TimeDate now;_system->getTimeAndDate(now);
 const Common::String prefix=Common::String::format("%s-snatcher-investigation-%04d%02d%02d-%02d%02d%02d-%u-",ConfMan.getActiveDomainName().c_str(),now.tm_year+1900,now.tm_mon+1,now.tm_mday,now.tm_hour,now.tm_min,now.tm_sec,_system->getMillis());
 Common::String stem;do{stem=prefix+Common::String::format("%u",++_investigationSerial);}while(!manager->listSavefiles(stem+"*").empty());
 _investigationLastCapture=stem;
 Common::JSONObject report;report["format"]=new Common::JSONValue("snatcher-investigation/3");report["build"]=new Common::JSONValue(gScummVMFullVersion);
 report["snapshot_file"]=new Common::JSONValue(stem+".save");report["snapshot_status"]=new Common::JSONValue("See matching .result.json; this report is written before snapshot capture");
 report["engine"]=testCLIState();report["host_frames"]=new Common::JSONValue((long long)_investigationFrames);report["input_events"]=new Common::JSONValue((long long)_investigationEvents);report["mouse_moves"]=new Common::JSONValue((long long)_investigationMoves);
 report["current_interval_events"]=new Common::JSONValue((long long)_investigationFrame.events);report["current_poll_elapsed_ms"]=new Common::JSONValue((long long)uint32_t(_system->getMillis()-_investigationPollStart));
 Common::JSONObject settings;for(const char* key:{"gfx_mode","gfx_mode_submode","fullscreen","aspect_ratio","filtering","vsync","stretch_mode","shader","snatcher_fm_core","snatcher_decensor","snatcher_original_controls","autosave_period"})if(ConfMan.hasKey(key))settings[key]=new Common::JSONValue(ConfMan.get(key));report["settings"]=new Common::JSONValue(settings);
 Common::JSONArray history;
 for(const auto& s:_investigationHistory){Common::JSONObject row;row["millis"]=new Common::JSONValue((long long)s.millis);row["frame"]=new Common::JSONValue((long long)s.frame);row["max_poll_ms"]=new Common::JSONValue((long long)s.pollMillis);row["max_update_ms"]=new Common::JSONValue((long long)s.updateMillis);row["max_draw_ms"]=new Common::JSONValue((long long)s.drawMillis);row["max_present_ms"]=new Common::JSONValue((long long)s.presentMillis);row["events"]=new Common::JSONValue((long long)s.events);row["moves"]=new Common::JSONValue((long long)s.moves);row["resource"]=new Common::JSONValue((long long)s.resource);row["node"]=new Common::JSONValue((long long)s.node);row["pending_events"]=new Common::JSONValue((long long)s.pending);row["process_metrics_known"]=new Common::JSONValue(s.processMetrics);
  row["working_set"]=new Common::JSONValue((long long)s.workingSet);row["private_bytes"]=new Common::JSONValue((long long)s.privateBytes);row["handles"]=new Common::JSONValue((long long)s.handles);
  row["cpu_100ns"]=new Common::JSONValue((long long)s.cpu100ns);row["available_physical_bytes"]=new Common::JSONValue((long long)s.availablePhysical);
  Common::JSONObject graphics;graphics["backend"]=new Common::JSONValue(s.graphics.backend);graphics["renderer"]=new Common::JSONValue(s.graphics.renderer);graphics["vendor"]=new Common::JSONValue(s.graphics.vendor);graphics["version"]=new Common::JSONValue(s.graphics.version);graphics["video_driver"]=new Common::JSONValue(s.graphics.videoDriver);
  graphics["backend_reported_vsync"]=new Common::JSONValue((long long)s.graphics.vsync);graphics["drawable_width"]=new Common::JSONValue((long long)s.graphics.width);graphics["drawable_height"]=new Common::JSONValue((long long)s.graphics.height);graphics["frames"]=new Common::JSONValue((long long)s.graphicsFrames);
  for(unsigned i=0;i<GraphicsDiagnostics::kStageCount;++i){Common::JSONObject stage;stage["max_ms"]=new Common::JSONValue((long long)s.graphicsMax[i]);stage["total_ms"]=new Common::JSONValue((long long)s.graphicsTotal[i]);stage["calls"]=new Common::JSONValue((long long)s.graphicsCalls[i]);graphics[GraphicsDiagnostics::stageName(i)]=new Common::JSONValue(stage);}
  graphics["textures"]=textureTimingsJSON(s.graphics);
#ifdef OPENGL_DEBUG
  graphics["routine_gl_debug_polling"]=new Common::JSONValue(true);
#else
  graphics["routine_gl_debug_polling"]=new Common::JSONValue(false);
#endif
  row["graphics"]=new Common::JSONValue(graphics);
  history.push_back(new Common::JSONValue(row));}
 report["history"]=new Common::JSONValue(history);
 if(!writeInvestigationJSON(*manager,stem+".json",Common::JSONValue(report))){message="Capture FAILED: diagnostic report could not be finalized. Do not assume evidence was saved.";_investigationStatus=message;return false;}
 _system->displayMessageOnOSD(Common::U32String("Capturing investigation. Please wait for the completion dialog."));
 std::string error;bool saved=writeNativeSnapshot(stem+".save","F12 investigation recovery snapshot",error);
 Common::JSONObject result;result["report_file"]=new Common::JSONValue(stem+".json");result["snapshot_file"]=new Common::JSONValue(stem+".save");result["snapshot_validated"]=new Common::JSONValue(saved);result["error"]=new Common::JSONValue(error.c_str());
 bool finalized=writeInvestigationJSON(*manager,stem+".result.json",Common::JSONValue(result));
 _investigationComplete=saved&&finalized;_lastTickMillis=_system->getMillis();
 if(_investigationComplete)message="Capture complete - safe to restart.\nRecovery save and diagnostics were finalized and checked.\nFiles are in this installation's saves folder:\n"+stem;
 else if(finalized)message="Diagnostics saved, but recovery SAVE FAILED.\nThe report is available after restart.\n"+Common::String(error.c_str())+"\n"+stem;
 else message="Capture INCOMPLETE: report saved, but completion record failed.\n"+stem;
 _investigationStatus=message;return _investigationComplete;
}
void SnatcherEngine::enterVisualObserver(uint16_t counter){
 if(!_visualObserver.active)return;
 if(_visualObserver.serial==UINT64_MAX){_visualObserver.error="Visual observer serial exhausted";_visualObserver.active=false;return;}
 ++_visualObserver.serial;++_visualObserver.workers;
 if(uint16_t(_visualObserver.serial)!=counter){_visualObserver.error="Visual observer worker serial/counter mismatch";_visualObserver.active=false;}
 else if(_visualObserver.workers>_visualObserver.maxWorkers){_visualObserver.error="Visual observer worker horizon exceeded";_visualObserver.active=false;}
}
void SnatcherEngine::finishVisualObserver(uint16_t counter){
 if(!_visualObserver.active)return;
 uint8_t visual=0;if(!_scene.visualCommand(visual)){_visualObserver.error="Visual observer has no known visual command";_visualObserver.active=false;return;}
 for(auto& capture:_visualObserver.captures){
  if(!capture.firstKnown&&visual==capture.targetMode){capture.firstKnown=true;capture.firstTick=_visualObserver.serial;}
  if(!capture.firstKnown||capture.captured||_visualObserver.serial==capture.firstTick)continue;
  if(_visualObserver.serial!=capture.firstTick+1){_visualObserver.error="Visual observer missed fixed one-worker capture boundary";_visualObserver.active=false;return;}
  SceneRuntime snapshot=_scene;SceneImage image;std::string error;uint16_t resource;unsigned index;
  if(!snapshot.render(kSceneReferenceRGB565,image,error)||!_scene.resourceIdentity(resource,index)){_visualObserver.error=error.empty()?"Visual observer scene identity unavailable":error;_visualObserver.active=false;return;}
  capture.captureTick=_visualObserver.serial;capture.counter=counter;capture.visualMode=visual;
  const PhoneVisualState& phone=_scene.phoneVisualState();capture.phoneMode=phone.mode;capture.phonePhase=phone.phase;capture.paletteMode=_scene.palettePublicationMode();
  capture.resource=_runtime->cursor().resource;capture.node=_runtime->cursor().node;capture.sceneResource=resource;capture.sceneIndex=index;
  capture.rngFirst=_nativeRandom.state.first;capture.rngSecond=_nativeRandom.state.second;capture.rngDraws=_nativeRandom.draws;
  capture.width=image.width;capture.height=image.height;capture.rgb.swap(image.rgb);capture.captured=true;
 }
 _visualObserver.done=_visualObserver.captures[0].captured&&_visualObserver.captures[1].captured;
 if(_visualObserver.done)_visualObserver.active=false;
 else if(_visualObserver.workers>=_visualObserver.maxWorkers){_visualObserver.error="Visual observer worker horizon reached before both captures";_visualObserver.active=false;}
}
Common::JSONValue *SnatcherEngine::visualCaptureMetadata(const VisualWorkerCapture& c,const char* filename)const{
 Common::JSONObject o;o["target_mode"]=new Common::JSONValue((long long)c.targetMode);o["first_known"]=new Common::JSONValue(c.firstKnown);o["captured"]=new Common::JSONValue(c.captured);
 o["first_worker_tick"]=new Common::JSONValue((long long)c.firstTick);o["capture_worker_tick"]=new Common::JSONValue((long long)c.captureTick);o["counter16"]=new Common::JSONValue((long long)c.counter);
 o["visual_mode"]=new Common::JSONValue((long long)c.visualMode);o["phone_mode"]=new Common::JSONValue((long long)c.phoneMode);o["phone_phase"]=new Common::JSONValue((long long)c.phonePhase);o["palette_publication_mode"]=new Common::JSONValue((long long)c.paletteMode);
 o["resource"]=new Common::JSONValue((long long)c.resource);o["node"]=new Common::JSONValue((long long)c.node);o["scene_resource"]=new Common::JSONValue((long long)c.sceneResource);o["scene_index"]=new Common::JSONValue((long long)c.sceneIndex);
 o["rng_first"]=new Common::JSONValue((long long)c.rngFirst);o["rng_second"]=new Common::JSONValue((long long)c.rngSecond);o["rng_draws"]=new Common::JSONValue((long long)c.rngDraws);o["width"]=new Common::JSONValue((long long)c.width);o["height"]=new Common::JSONValue((long long)c.height);if(filename)o["filename"]=new Common::JSONValue(filename);return new Common::JSONValue(o);
}
bool SnatcherEngine::consumeFmCompletions(std::string& error){
 error.clear();if(!_fmLoaded)return true;FmAudioSnapshot state=_fm.snapshot();if(!state.error.empty()){error=state.error;return false;}if(state.primaryCompletionSerial<_primaryCompletionObserved||state.secondaryCompletionSerial<_secondaryCompletionObserved||state.completionPublicationGeneration<_completionPublicationObserved){error="native FM completion serial regressed";return false;}uint8_t serial=state.completionPublication&15;if(serial!=_completionSerialObserved){if(state.completionPublication&128){_primaryMusicCommand=0;_primaryMusicKnown=true;}if(state.completionPublication&64)_scene.clearSecondaryMusicCache();_completionSerialObserved=serial;}_completionPublicationObserved=state.completionPublicationGeneration;_primaryCompletionObserved=state.primaryCompletionSerial;_secondaryCompletionObserved=state.secondaryCompletionSerial;return true;
}
bool SnatcherEngine::capturePresentationBookmark(uint16_t presentation,NativePresentationBookmark& out,std::string& error)const{
 error.clear();if(presentation==0xffff){error="presentation bookmark has no source presentation latch";return false;}
 if(!_primaryMusicKnown||!_lastPcmKnown){error="presentation bookmark audio caches require their source owner after legacy load";return false;}
 NativePresentationBookmark next;next.valid=true;next.pcmKnown=true;next.presentation=presentation;next.stream=_lastPcmCommand;next.music=_primaryMusicCommand;next.ambient=_ambientMode&15;next.interaction=_interactionMode&3;next.quiet=_quietPcm;next.suppressed=_scene.musicSuppressed();_scene.captureActorBookmark(next.actors);out=next;return true;
}
bool SnatcherEngine::restoreBookmarkPolicies(std::string& error){
 if(!restorePresentationBookmarkPolicies(_bookmark,_scene,_pointerInteraction,error))return false;
 _ambientMode=_bookmark.ambient;_interactionMode=_bookmark.interaction;_quietPcm=_bookmark.quiet;return true;
}
bool SnatcherEngine::applySemanticOwner(const RuntimeRequest& r,std::string& error){
 error.clear();if(r.kind==kRequestSemanticSetting){switch(NarrativeSetting(r.first)){
 case kSettingTextEntrySession:_textEntrySession=r.second;break;case kSettingTextEntryLimit:_textEntryLimit=r.second;break;case kSettingPhoneEntryPolicy:_phoneEntryPolicy=r.second;break;
 case kSettingDialogueTop:if(!_scene.setDialogueTop(uint8_t(r.second),error))return false;break;
 case kSettingShootingParameter:_shooting.encounter.timing=r.second;break;
 case kSettingQuietPcm:_quietPcm=uint8_t(r.second)!=0;break;
 case kSettingAmbientPresentation:if(!_scene.setEnvironmentMode(r.second,error))return false;_ambientMode=r.second;break;
 default:error="unknown native semantic setting";return false;}}
 else if(r.kind==kRequestInteractionMode){if(r.first>1){error="Unrecovered original interaction policy";return false;}if(r.first==1){if(!_pointerInteraction.enter(error))return false;}else _pointerInteraction.leave();_interactionMode=r.first;_pointerAdjusting=false;}
 else if(r.kind==kRequestCoordinatorControl){uint16_t mode=uint16_t(r.first+2);if(mode!=2&&mode!=3&&mode!=4&&mode!=5){error="coordinator mode requires its original owner";return false;}_coordinatorMode=mode;if(mode==3)return true;if(mode==4){if(_returnTitleToken.id&&!(_returnTitleToken==r.token)){error="return-to-title token conflict";return false;}_returnTitleToken=r.token;return true;}}
 else if(r.kind==kRequestPresentationBookmark){if(!r.first){if(!capturePresentationBookmark(r.second,_bookmark,error))return false;}else{return restoreBookmarkPolicies(error);}}
 else{error="unknown semantic owner";return false;}
 _trace.record(r.owner,"native_semantic_setting",r.first,r.second);return _events.complete(r.token,false,error);
}
bool SnatcherEngine::updateBookmark(const RuntimeRequest& request,std::string& error){
 error.clear();if(!_bookmark.valid||!_bookmark.pcmKnown){error="presentation bookmark PCM cache is unknown; a fresh source capture is required";return false;}
 if(!_bookmarkToken.id){
  // 1500E emits queue0F/11, resident-bank restoration, then queue20.
  // Emission does not execute the newly appended queue in this VM visit.
  _bookmarkToken=request.token;_bookmarkPhase=0;_bookmarkPreparation=SceneResourcePreparation();_bookmarkData.clear();
  uint16_t resident;unsigned list;_bookmarkNeedsLoad=!_scene.resourceIdentity(resident,list)||resident!=(_bookmark.presentation>>8);
  return true;
 }
 if(!(_bookmarkToken==request.token)){error="presentation bookmark token conflict";return false;}
 if(!_bookmarkPhase){
  bool ready=false;if(!_scene.advanceResourcePreparation(_bookmarkPreparation,ready,error))return false;if(!ready)return true;
  if(_bookmarkNeedsLoad){_cd.pauseForResourceRead();_bookmarkPhase=1;return true;}
 }
 if(_bookmarkPhase==1&&_cd.active()&&!_cd.paused())return true;
 if(_bookmarkPhase<2){
  if(_bookmarkNeedsLoad&&!_cd.releaseForResourceRead(error))return false;
  _lastTickMillis=_system->getMillis();if(!_resources.readResource(_bookmark.presentation>>8,_bookmarkData,error))return false;_lastTickMillis=_system->getMillis();
  _bookmarkPhase=2;if(_bookmarkNeedsLoad)return true;
 }
 if(_bookmarkPhase==2){
  if(_bookmark.stream!=0xffff&&!loadResidentBank(_bookmark.stream,error))return false;
  _bookmarkPhase=3;_audioWait=AudioActivityWait();if(_bookmark.stream!=0xffff)return true;
 }
 if(_bookmark.stream!=0xffff){AudioWaitResult result=_audioWait.tick(_activity,error);if(result==kAudioWaitUnknown)return false;if(result!=kAudioWaitFinished)return true;}
 // Queue20 (105E8) installs, visits F7F0, then restores actor markers in
 // the same service invocation. This direct actor-only visit bypasses the
 // ordinary coordinator gate, even when F68E queued dependent graphics.
 if(!_scene.install(_bookmark.presentation>>8,_bookmarkData,_bookmark.presentation&255,error))return false;
 _sceneAssets=_scene.assets();_sceneAssetsReady=true;
 if(!_scene.initializeBookmarkedActors(_bookmark.actors,error))return false;
 if(_bookmark.music){_primaryMusicCommand=_bookmark.music;_primaryMusicKnown=true;if(!startFm(_bookmark.music,error))return false;}
 if(!_events.complete(request.token,false,error))return false;
 _trace.record(request.owner,"presentation_bookmark_restored",_bookmark.presentation);_bookmarkToken=EventToken();_bookmarkPhase=0;_bookmarkData.clear();return true;
}
bool SnatcherEngine::updateCounterSettlement(const RuntimeRequest& r,uint8_t counter,std::string& error){
 error.clear();if(!_counterToken.id){if(!startShootingCounterSettlement(0,_counterSettlement,error))return false;_counterToken=r.token;}else if(!(_counterToken==r.token)){error="counter settlement token conflict";return false;}
 SceneActorView first,second;if(!_scene.inspectActor(58,first,error)||!_scene.inspectActor(59,second,error))return false;if(!first.owned||!second.owned){error="counter settlement requires actual counter actor sources";return false;}
 uint16_t a=first.record,b=second.record;ShootingCounterSettlement state=_counterSettlement;ShootingCounterEffects effects;bool done=false;if(!tickShootingCounterSettlement((counter&1)==0,a,b,state,effects,done,error))return false;SceneRuntime scene=_scene;
 if(effects.writeRecords||effects.hideCounterActors){for(unsigned i=0;i<2;++i){SceneActorPatch p;p.fields=SceneActorPatch::kRecord;p.record=effects.hideCounterActors?0xffff:(i?b:a);if(effects.hideCounterActors){p.fields|=SceneActorPatch::kFlags;p.flags=3;}if(!scene.patchActor(58+i,p,error))return false;}}
 if(effects.clearCountdownRecords){for(unsigned i=56;i<58;++i){SceneActorPatch p;p.fields=SceneActorPatch::kRecord;p.record=0;if(!scene.patchActor(i,p,error))return false;}}
 _scene=scene;_counterSettlement=state;if(done){if(!_events.complete(r.token,false,error))return false;_counterToken=EventToken();_trace.record(r.owner,"shooting_counters_settled");}return true;
}
bool SnatcherEngine::read(NarrativeQuery query,uint16_t& value,std::string& error){
 error.clear();switch(query){
 case kQueryResponseStatus:case kQueryResponseWord0:case kQueryResponseWord1:case kQueryResponseWord2:case kQueryResponseWord3:case kQueryStorageStatus:{
  if(!ensureGameStorage(error))return false;
  GameStorageQuery storage=query==kQueryResponseStatus?kGameLastWriteResult:query==kQueryStorageStatus?kGameStorageStatus:GameStorageQuery(unsigned(kGameSlot1)+unsigned(query-kQueryResponseWord0));
  return readGameStorageQuery(_gameServices,storage,value,error);
 }
 case kQueryMenuContext:return _pointerInteraction.queryRegion(value,error);
 case kQueryShootingShots:value=_shooting.shots;break;
 case kQueryShootingTypeClearHits:value=_shooting.encounter.typeClearHits;break;
 case kQueryShootingTypeSetHits:value=_shooting.encounter.typeSetHits;break;
 case kQueryShootingMisses:value=_shooting.misses;break;
 case kQueryShootingCounter:value=_shooting.encounter.damage;break;
 case kQueryShootingResult:value=_shooting.allSpecialHit?1:0;break;
 case kQueryShootingInputToggle:value=_shooting.aimToggle;break;
 case kQueryAlternateShootingInput:value=_shooting.encounter.alternateInput?1:0;break;
 default:error="Narrative query requires its unrecovered response/menu/storage owner";return false;
 }return true;
}
bool SnatcherEngine::updateShooting(const RuntimeRequest& request,std::string& error){
 error.clear();if(_shootingToken.id&&!(request.token==_shootingToken)){error="shooting request token owner conflict";return false;}
 if(!_shootingToken.id){std::vector<uint8_t> sub;if(!_resources.readFile("SUBCODE.BIN",sub,error)||!readShootingEncounterData(sub.data(),sub.size(),_shootingData,error)||!readShootingAimLayout(sub.data(),sub.size(),_shootingAim,error)||!readShootingFireLayout(sub.data(),sub.size(),_shootingFire,error))return false;
  bool pointer=ConfMan.hasKey("snatcher_mouse_aim")&&ConfMan.getBool("snatcher_mouse_aim");uint16_t timing=_shooting.encounter.timing;if(!startShootingCoordinator(_shootingData,request.operands[0],pointer,timing,_shooting,error))return false;_shootingToken=request.token;_shootingProfileReady=false;_shootingPressed=0;_trace.record(request.owner,"shooting_started",request.operands[0],pointer);
 }
 ShootingCoordinatorInput input;input.profileReady=_shootingProfileReady;uint8_t effectActivity;if(!_activity.snapshot(effectActivity,error,8))return false;input.effectBusy=(effectActivity&8)!=0;input.held=_shootingHeld;input.pressed=_shootingPressed;_shootingPressed=0;input.pointerX=_pointerX;input.pointerY=_pointerY;input.absoluteAim=mouseEnabled()&&_mouseAimSelected&&_mousePositionKnown;input.mouseX=int16_t(_pointerX);input.mouseY=int16_t(_pointerY);input.scrollX=int16_t(_scene.scroll().positions[0].x16/65536);input.scrollY=int16_t(_scene.scroll().positions[0].y16/65536);input.actors.resize(64);
 for(unsigned i=0;i<64;++i){SceneActorView v;if(!_scene.inspectActor(i,v,error))return false;auto& a=input.actors[i];a.owned=v.owned;a.active=v.active;a.markersKnown=v.markersKnown;a.phaseKnown=v.phaseKnown;a.phase=v.phase;a.cleared=v.cleared;a.flags=v.flags;a.record=v.record;a.remaining=v.remaining;a.x=v.x;a.y=v.y;for(unsigned n=0;n<8;++n)a.markers[n]=v.markers[n];}
 NativeRandomOwner random=_nativeRandom;ShootingCoordinatorState state=_shooting;ShootingCoordinatorEffects effects;if(!tickShootingCoordinator(_shootingData,_shootingAim,_shootingFire,input,random,state,effects,error))return false;SceneRuntime scene=_scene;std::vector<uint8_t> sub;
 if(!effects.resources.empty()&&!_resources.readFile("SUBCODE.BIN",sub,error))return false;
 for(const ShootingTemplateRequest& action:effects.templates)if(!scene.copyActorTemplate(32+action.cell,26+action.index,error))return false;
 for(const ShootingActorMutation& action:effects.actors){SceneActorPatch patch;patch.fields=action.fields;patch.active=action.active;patch.flags=action.flags;patch.record=action.record;patch.remaining=action.remaining;patch.x=action.x;patch.y=action.y;if(!scene.patchActor(action.actor,patch,error))return false;}
 for(const ShootingResourceAction& action:effects.resources){if(action.kind==ShootingResourceAction::kProbeInput){_shootingProfileReady=true;continue;}if(action.kind==ShootingResourceAction::kCancelPalettes){scene.cancelPaletteEffects();continue;}
  if(action.kind==ShootingResourceAction::kPalette){PaletteEffect palette;if(!readPaletteEffect(sub.data(),sub.size(),action.dataOffset,sub.size(),palette,error,0xd400)||!scene.queuePaletteEffect(palette,error))return false;}
  else{std::vector<uint8_t> source=sub;if(action.overrideValue){if(action.dataOffset>source.size()||source.size()-action.dataOffset<8){error="shooting graphics destination outside original packet";return false;}source[action.dataOffset+6]=uint8_t(uint16_t(action.value)>>8);source[action.dataOffset+7]=uint8_t(action.value);}SceneGraphicsTask task;if(!readSceneGraphicsTask(source.data(),source.size(),action.dataOffset,0xd400,task,error)||!scene.queueGraphicsTasks(std::vector<SceneGraphicsTask>(1,task),error))return false;}
 }
 for(unsigned i=0;i<9;++i){SceneActorPatch target;target.fields=SceneActorPatch::kPhase;target.phase=state.encounter.targets[i].phaseAndType;if(!scene.patchActor(32+i,target,error))return false;}
 _scene=scene;_shooting=state;_nativeRandom=random;for(uint8_t command:effects.music)if(!startFm(command,error,false))return false;
 for(uint8_t effect:effects.effects){PendingEffect pending={effect,_residentBankId,_events.generation(),request.owner};_pendingEffects.push_back(pending);}
 _trace.record(request.owner,"shooting_update",_shooting.encounter.phase,_shooting.shots);
 if(effects.completed){if(!_events.complete(request.token,false,error))return false;_trace.record(request.owner,"shooting_completed",effects.failed,_shooting.encounter.damage);_shootingToken=EventToken();}return true;
}
FmCore configuredFmCore(){FmCore core=kFmGens;if(ConfMan.hasKey("snatcher_fm_core"))parseFmCore(ConfMan.get("snatcher_fm_core").c_str(),core);return core;}
Common::Error SnatcherEngine::run() {
 FmCore selected;if(ConfMan.hasKey("snatcher_fm_core")&&!parseFmCore(ConfMan.get("snatcher_fm_core").c_str(),selected))return Common::Error(Common::kUnknownError,"snatcher_fm_core must be gens or nuked");
 _ready=false;_code=_text=_unknown=0;_scenarios.clear();_resources.close();
 std::string reason;
 if(!_output.start(reason))return Common::Error(Common::kAudioDeviceInitFailed,Common::String(reason.c_str()));
 if(!openGame(Common::FSNode(ConfMan.getPath("path")),_resources,reason))
  return Common::Error(Common::kReadingFailed,Common::String(reason.c_str()));
 if(ConfMan.hasKey("snatcher_decensor")&&ConfMan.getBool("snatcher_decensor")){
  Common::File patch;
  if(!patch.open("snatcher-decensor.dat"))return Common::Error(Common::kReadingFailed,"Decensor option requires snatcher-decensor.dat in the game or extra-data search path");
  if(patch.size()<=0||patch.size()>1024*1024)return Common::Error(Common::kReadingFailed,"Invalid decensor sidecar size");
  std::vector<uint8_t> data(size_t(patch.size()));
  if(patch.read(data.data(),uint32(data.size()))!=data.size()||patch.err()||!_resources.configureDecensor(data,reason))return Common::Error(Common::kReadingFailed,Common::String(reason.empty()?"Unable to read decensor sidecar":reason.c_str()));
 }
 for(unsigned i=0;i<39;++i){
  std::vector<uint8_t> data;Scenario scenario;
  if(!_resources.readResource(uint16_t(0x39+i),data,reason)||!parseScenario(data.empty()?nullptr:&data[0],data.size(),scenario,reason)||!validateLocalTargets(scenario,reason))
   return Common::Error(Common::kReadingFailed,Common::String(reason.c_str()));
  _code+=scenario.codeBytes;_text+=scenario.textBytes;_unknown+=scenario.unknownCode+scenario.unknownText;_scenarios.push_back(scenario);
 }
 size_t crossTargets=0;if(!validateCrossTargets(_scenarios,crossTargets,reason))return Common::Error(Common::kReadingFailed,Common::String(reason.c_str()));
 std::vector<uint8_t> subcode;if(!_resources.readFile("SUBCODE.BIN",subcode,reason)||!_font.load(subcode.data(),subcode.size(),reason)||!_dialogueWindow.loadAssets(subcode.data(),subcode.size(),reason)||!readGameTextPalette(subcode.data(),subcode.size(),_textColors,reason))return Common::Error(Common::kReadingFailed,Common::String(reason.c_str()));
 if(!_scene.loadSharedGraphicsEffects(subcode.data(),subcode.size(),reason)||!_scene.establishBootSubmissionHistory(reason))return Common::Error(Common::kReadingFailed,Common::String(reason.c_str()));
 delete _runtime;_runtime=new NarrativeRuntime(_scenarios,_events,_trace);_runtime->setQuerySource(this);_trace.enable(true);
 initGraphics(640,240);_pixels.assign(640*240,0);const byte colors[]={12,16,24,230,235,240};_system->getPaletteManager()->setPalette(colors,0,2);
 if(!migrateReservedNativeSaves(*_system->getSavefileManager(),ConfMan.getActiveDomainName(),reason))return Common::Error(Common::kReadingFailed,Common::String(reason.c_str()));
 if(!startStartup(reason))return Common::Error(Common::kReadingFailed,Common::String(reason.c_str()));
 _ready=true;
 if(ConfMan.hasKey("save_slot")&&ConfMan.getInt("save_slot")>=0){Common::Error loaded=loadGameState(ConfMan.getInt("save_slot"));if(loaded.getCode()!=Common::kNoError)return loaded;}
 CursorMan.showMouse(false);
 while(!shouldQuit()){
  const uint32_t pollStart=_system->getMillis();
  _investigationPollStart=pollStart;
  Common::Event event;while(_eventMan->pollEvent(event)){
   ++_investigationEvents;++_investigationFrame.events;
   if(event.type==Common::EVENT_MOUSEMOVE){++_investigationMoves;++_investigationFrame.moves;}
   if((event.type==Common::EVENT_KEYDOWN||event.type==Common::EVENT_KEYUP)&&event.kbd.keycode==Common::KEYCODE_F12){
    if(event.type==Common::EVENT_KEYUP)_investigationKeyHeld=false;
    else if(!event.kbdRepeat&&!_investigationKeyHeld&&_mouseInput.focused()){
     _investigationKeyHeld=true;Common::String message;captureInvestigation(message);
     if(!CETestCLI::active()){PauseToken pause=pauseEngine();GUI::MessageDialog dialog(Common::U32String(message.c_str()));dialog.runModal();_investigationKeyHeld=false;}
     releaseInput();_lastTickMillis=_system->getMillis();
    }
    continue;
   }
   if(event.type==Common::EVENT_SCREEN_CHANGED)_framePublication.invalidate();
   if(event.type==Common::EVENT_KEYDOWN&&(event.mouseOutsideActiveArea||!_mouseInput.focused()))continue;
   if(event.type==Common::EVENT_KEYDOWN&&!event.kbdRepeat&&(event.kbd.keycode==Common::KEYCODE_F5||event.kbd.keycode==Common::KEYCODE_F7)){
    releaseInput();if(event.kbd.keycode==Common::KEYCODE_F5){_testSaveDialogOpen=true;saveGameDialog();_testSaveDialogOpen=false;}else{_testLoadDialogOpen=true;loadGameDialog();_testLoadDialogOpen=false;}releaseInput();_gameStorageScanned=false;_lastTickMillis=_system->getMillis();continue;
   }
   if((event.type==Common::EVENT_KEYDOWN||event.type==Common::EVENT_KEYUP)&&event.kbd.keycode==Common::KEYCODE_p){if(event.type==Common::EVENT_KEYUP)_pauseKeyHeld=false;else if(!_pauseKeyHeld&&!event.kbdRepeat){_pauseKeyHeld=true;if(_inputPause.isActive())_inputPause.clear();else _inputPause=pauseEngine();releaseInput();}continue;}
   if(event.type==Common::EVENT_FOCUS_LOST||event.type==Common::EVENT_FOCUS_GAINED||event.type==Common::EVENT_MOUSEMOVE||event.type==Common::EVENT_LBUTTONDOWN||event.type==Common::EVENT_LBUTTONUP||event.type==Common::EVENT_RBUTTONDOWN||event.type==Common::EVENT_RBUTTONUP||event.type==Common::EVENT_WHEELUP||event.type==Common::EVENT_WHEELDOWN){
    if(!processMouseEvent(event,reason))_blocker=reason;
    continue;
   }
   if(event.type==Common::EVENT_KEYDOWN&&!event.kbdRepeat&&!isPaused()&&_shooting.active&&(event.kbd.keycode==Common::KEYCODE_UP||event.kbd.keycode==Common::KEYCODE_DOWN||event.kbd.keycode==Common::KEYCODE_LEFT||event.kbd.keycode==Common::KEYCODE_RIGHT||event.kbd.keycode==Common::KEYCODE_z))_mouseAimSelected=false;
   if(_startupActive){
    if(event.type==Common::EVENT_KEYDOWN&&event.kbd.keycode==Common::KEYCODE_ESCAPE){_resources.close();return Common::kNoError;}
    if(event.type==Common::EVENT_KEYDOWN||event.type==Common::EVENT_KEYUP){uint8_t mask=0;switch(event.kbd.keycode){case Common::KEYCODE_UP:mask=1;break;case Common::KEYCODE_DOWN:mask=2;break;case Common::KEYCODE_LEFT:mask=4;break;case Common::KEYCODE_RIGHT:mask=8;break;case Common::KEYCODE_BACKSPACE:case Common::KEYCODE_x:mask=0x10;break;case Common::KEYCODE_c:mask=0x20;break;case Common::KEYCODE_z:mask=0x40;break;case Common::KEYCODE_SPACE:case Common::KEYCODE_RETURN:mask=0x80;break;default:break;}
     if(event.type==Common::EVENT_KEYUP)_startupHeld=uint8_t(_startupHeld&~mask);else if(!event.kbdRepeat&&!isPaused()){if(mask)_mouseStartupPending=false;_startupPressed=uint8_t(_startupPressed|(mask&~_startupHeld));_startupHeld=uint8_t(_startupHeld|mask);}
    }continue;
   }

   if(event.type==Common::EVENT_KEYDOWN||event.type==Common::EVENT_KEYUP){uint8_t mask=0;switch(event.kbd.keycode){case Common::KEYCODE_UP:mask=1;break;case Common::KEYCODE_DOWN:mask=2;break;case Common::KEYCODE_LEFT:mask=4;break;case Common::KEYCODE_RIGHT:mask=8;break;case Common::KEYCODE_BACKSPACE:case Common::KEYCODE_x:mask=0x10;break;case Common::KEYCODE_RETURN:case Common::KEYCODE_c:mask=0x20;break;case Common::KEYCODE_z:mask=0x40;break;case Common::KEYCODE_SPACE:mask=0x80;break;default:break;}if(event.type==Common::EVENT_KEYUP)_shootingHeld=uint8_t(_shootingHeld&~mask);else if(!event.kbdRepeat&&!isPaused()){_shootingPressed|=uint8_t(mask&~_shootingHeld);_entryGlobalPressed|=uint8_t(mask&~_shootingHeld);_shootingHeld|=mask;}}
   if(event.type==Common::EVENT_KEYDOWN||event.type==Common::EVENT_KEYUP){
    CommandButton button=kCommandConfirm;bool mapped=true;switch(event.kbd.keycode){case Common::KEYCODE_UP:button=kCommandUp;break;case Common::KEYCODE_DOWN:button=kCommandDown;break;case Common::KEYCODE_LEFT:button=kCommandLeft;break;case Common::KEYCODE_RIGHT:button=kCommandRight;break;case Common::KEYCODE_BACKSPACE:button=kCommandCancel;break;case Common::KEYCODE_RETURN:button=kCommandConfirm;break;default:mapped=false;break;}
    if(mapped){if(event.type==Common::EVENT_KEYDOWN&&!event.kbdRepeat){_mouseCommandPending=false;_entryMousePending=false;_entryMousePressed=0;}if(event.type==Common::EVENT_KEYUP||!event.kbdRepeat)_commandInput.setButton(button,event.type==Common::EVENT_KEYDOWN,reason);}
   }
   if(event.type==Common::EVENT_KEYDOWN&&event.kbd.keycode==Common::KEYCODE_SPACE&&!event.kbdRepeat&&!isPaused())_startPressed=true;
   if(event.type==Common::EVENT_KEYUP&&event.kbd.keycode==Common::KEYCODE_RETURN)_confirmHeld=false;
   if(event.type==Common::EVENT_KEYDOWN){
    if(event.kbd.keycode==Common::KEYCODE_ESCAPE){_resources.close();return Common::kNoError;}
    if(event.kbd.keycode==Common::KEYCODE_RETURN&&!event.kbdRepeat&&!_confirmHeld&&!isPaused()){_confirmHeld=true;if(_runtime->status()!=kRuntimeChoices&&nativeDialogueInputReady(_dialogueReady,_scene.visualBusy(),_scene.fontPublicationPending())&&!acknowledgeText(reason))_blocker=reason;}
   }
  }
  uint32_t stamp=_system->getMillis();_investigationFrame.pollMillis=std::max(_investigationFrame.pollMillis,uint32_t(stamp-pollStart));
  _output.setPaused(isPaused());_events.pause(isPaused());_audio.setPaused(isPaused());_cd.setPaused(isPaused());_fm.setPaused(isPaused());if(!isPaused())updateNative();else{releaseInput();_commandInput.discardPressed();_lastTickMillis=_system->getMillis();}
  uint32_t next=_system->getMillis();_investigationFrame.updateMillis=std::max(_investigationFrame.updateMillis,uint32_t(next-stamp));stamp=next;
  drawNative();next=_system->getMillis();_investigationFrame.drawMillis=std::max(_investigationFrame.drawMillis,uint32_t(next-stamp));stamp=next;
  GraphicsDiagnostics::begin(_investigationFrame.graphicsFrames==0);
  _system->updateScreen();GraphicsDiagnostics::end();
  const auto& graphics=GraphicsDiagnostics::state();_investigationFrame.graphics.copyIdentity(graphics);++_investigationFrame.graphicsFrames;
  for(unsigned role=0;role<GraphicsDiagnostics::kTextureRoleCount;++role)_investigationFrame.graphics.textures[role].merge(graphics.textures[role]);
  for(unsigned i=0;i<GraphicsDiagnostics::kStageCount;++i){_investigationFrame.graphicsMax[i]=std::max(_investigationFrame.graphicsMax[i],graphics.millis[i]);_investigationFrame.graphicsTotal[i]+=graphics.millis[i];_investigationFrame.graphicsCalls[i]+=graphics.calls[i];}
  next=_system->getMillis();_investigationFrame.presentMillis=std::max(_investigationFrame.presentMillis,uint32_t(next-stamp));recordInvestigationFrame();
  CETestCLI::frame();_system->delayMillis(10);
 }
 _resources.close();return Common::kNoError;
}
Common::JSONValue *SnatcherEngine::testCLIState(){
 Common::JSONObject o;
 o["investigation_capture"]=new Common::JSONValue(_investigationLastCapture);o["investigation_complete"]=new Common::JSONValue(_investigationComplete);o["investigation_status"]=new Common::JSONValue(_investigationStatus);o["investigation_samples"]=new Common::JSONValue((long long)_investigationHistory.size());
 const SceneLayout debugLayout=_scene.framePresentationLayout();Common::JSONObject sceneLayout;
 sceneLayout["columns"]=new Common::JSONValue((long long)debugLayout.columns);sceneLayout["rows"]=new Common::JSONValue((long long)debugLayout.rows);
 for(unsigned p=0;p<2;++p){sceneLayout[Common::String::format("x%u",p)]=new Common::JSONValue((long long)debugLayout.sourceX[p]);sceneLayout[Common::String::format("y%u",p)]=new Common::JSONValue((long long)debugLayout.sourceY[p]);sceneLayout[Common::String::format("allocation%u",p)]=new Common::JSONValue((long long)debugLayout.planeAllocation[p]);sceneLayout[Common::String::format("scroll_x%u",p)]=new Common::JSONValue((long long)_scene.scroll().positions[p].x16);sceneLayout[Common::String::format("scroll_y%u",p)]=new Common::JSONValue((long long)_scene.scroll().positions[p].y16);}
 o["scene_layout"]=new Common::JSONValue(sceneLayout);
 o["save_dialog_open"]=new Common::JSONValue(_testSaveDialogOpen);o["load_dialog_open"]=new Common::JSONValue(_testLoadDialogOpen);
 o["autosaves_created"]=new Common::JSONValue((long long)_testAutosaves);
 o["host_updates_last"]=new Common::JSONValue((long long)_hostUpdatesLast);
 o["host_suspended_updates"]=new Common::JSONValue((long long)_hostSuspendedUpdates);
 const auto decensorStats=_resources.decensorDiagnostics();
 o["decensor_enabled"]=new Common::JSONValue(_resources.decensorEnabled());
 o["decensor_applications"]=new Common::JSONValue((long long)decensorStats.applications);
 o["decensor_cache_reads"]=new Common::JSONValue((long long)decensorStats.cacheReads);
 o["decensor_cache_bytes"]=new Common::JSONValue((long long)decensorStats.bytes);
 o["frame_uploads"]=new Common::JSONValue((long long)_framePublication.uploads());
 o["frame_unchanged"]=new Common::JSONValue((long long)_framePublication.unchanged());
 o["digital_fine_guard"]=new Common::JSONValue(false);o["digital_fine_policy"]=new Common::JSONValue("native-slow-pointer");
 StartupSettings startupSettings;std::string startupSettingsError;bool startupSettingsKnown=_startup.settings(startupSettings,startupSettingsError);o["startup_settings_known"]=new Common::JSONValue(startupSettingsKnown);if(startupSettingsKnown){o["startup_control_mapping"]=new Common::JSONValue((long long)startupSettings.controlMapping);o["startup_mono"]=new Common::JSONValue(startupSettings.mono);}o["startup_option_row"]=new Common::JSONValue((long long)_startup.optionRow());o["mouse_startup_pending"]=new Common::JSONValue(_mouseStartupPending);o["startup_mouse_action_ready8"]=new Common::JSONValue(_startup.mouseActionReady(8));
 o["input_paused"]=new Common::JSONValue(_inputPause.isActive());o["engine_paused"]=new Common::JSONValue(isPaused());
 o["menu_close_pending"]=new Common::JSONValue(_closingCommand.kind!=kCommandNoResponse);o["mouse_command_pending"]=new Common::JSONValue(_mouseCommandPending);o["mouse_entry_pending"]=new Common::JSONValue(_entryMousePressed!=0);
 std::string mouseError;MouseContext mouseView=mouseContext(mouseError);o["mouse_context"]=new Common::JSONValue((long long)mouseView.kind);o["mouse_ready"]=new Common::JSONValue(mouseView.ready);o["mouse_layout_error"]=new Common::JSONValue(mouseError.c_str());Common::JSONArray mouseTargets;for(const MouseTarget& t:mouseView.targets){Common::JSONObject target;target["id"]=new Common::JSONValue((long long)t.id);target["x"]=new Common::JSONValue((long long)t.rect.x);target["y"]=new Common::JSONValue((long long)t.rect.y);target["width"]=new Common::JSONValue((long long)t.rect.w);target["height"]=new Common::JSONValue((long long)t.rect.h);target["enabled"]=new Common::JSONValue(t.enabled);mouseTargets.push_back(new Common::JSONValue(target));}o["mouse_targets"]=new Common::JSONValue(mouseTargets);
 o["shooting_absolute_aim"]=new Common::JSONValue(_shooting.absoluteAim);o["shooting_aim_x"]=new Common::JSONValue((long long)_shooting.aimX);o["shooting_aim_y"]=new Common::JSONValue((long long)_shooting.aimY);o["shooting_fire_target"]=new Common::JSONValue((long long)_shooting.fireTarget);
 SceneActorView fireView;std::string fireViewError;if(_scene.inspectActor(21,fireView,fireViewError)&&fireView.owned){o["shooting_fire_x"]=new Common::JSONValue((long long)fireView.x);o["shooting_fire_y"]=new Common::JSONValue((long long)fireView.y);o["shooting_fire_record"]=new Common::JSONValue((long long)fireView.record);}
 o["mouse_enabled"]=new Common::JSONValue(mouseEnabled());o["mouse_context_generation"]=new Common::JSONValue((long long)_mouseContextGeneration);o["mouse_position_known"]=new Common::JSONValue(_mousePositionKnown);o["mouse_x"]=new Common::JSONValue((long long)_mouseX);o["mouse_y"]=new Common::JSONValue((long long)_mouseY);o["mouse_held"]=new Common::JSONValue(_mouseInput.held());o["mouse_aim_selected"]=new Common::JSONValue(_mouseAimSelected);
 o["test_instance_serial"]=new Common::JSONValue((long long)_testInstanceSerial);o["test_instances_created"]=new Common::JSONValue((long long)testInstancesCreated);o["test_destructors_entered"]=new Common::JSONValue((long long)testDestructorsEntered);o["test_last_destruction_serial"]=new Common::JSONValue((long long)testLastDestructionSerial);o["test_last_destruction_disc_open"]=new Common::JSONValue(testLastDestructionDiscOpen);o["test_recreation_queued"]=new Common::JSONValue(_testRecreationQueued);o["test_chained_games_empty"]=new Common::JSONValue(ChainedGamesMan.empty());o["disc_open"]=new Common::JSONValue(_resources.isOpen());
 o["visual_observer_active"]=new Common::JSONValue(_visualObserver.active);o["visual_observer_done"]=new Common::JSONValue(_visualObserver.done);o["visual_observer_error"]=new Common::JSONValue(_visualObserver.error.c_str());o["visual_observer_serial"]=new Common::JSONValue((long long)_visualObserver.serial);o["visual_observer_workers"]=new Common::JSONValue((long long)_visualObserver.workers);Common::JSONArray observedVisuals;for(const auto& capture:_visualObserver.captures)observedVisuals.push_back(visualCaptureMetadata(capture));o["visual_observer_captures"]=new Common::JSONValue(observedVisuals);
 uint8_t secondaryMemo=0;bool secondaryKnown=_scene.lastAnimationMusic(secondaryMemo);o["sub_primary_fm_memo"]=new Common::JSONValue((long long)_primaryMusicCommand);o["sub_secondary_fm_memo_known"]=new Common::JSONValue(secondaryKnown);o["sub_secondary_fm_memo"]=new Common::JSONValue((long long)secondaryMemo);
 o["resident_stream_staging_pending"]=new Common::JSONValue((long long)_residentStreamPending);o["resident_staging_known_bytes"]=new Common::JSONValue((long long)(_residentStaging?_residentStaging->knownBytes():0));o["resident_staging_present"]=new Common::JSONValue(bool(_residentStaging));if(_residentStaging){ResidentPcmStagingSnapshot stage;std::string detail;if(_residentStaging->snapshot(stage,detail)){uint32_t digest=2166136261u;for(uint8_t value:stage.knownBytes)digest=(digest^value)*16777619u;o["resident_staging_digest"]=new Common::JSONValue((long long)digest);o["resident_staging_generation"]=new Common::JSONValue((long long)stage.generation);o["resident_staging_writer_kind"]=new Common::JSONValue((long long)stage.writerKind);o["resident_staging_writer_id"]=new Common::JSONValue((long long)stage.writerId);}}
 Common::JSONArray pendingEffects;for(const PendingEffect& effect:_pendingEffects){Common::JSONObject pending;pending["effect"]=new Common::JSONValue((long long)effect.id);pending["bank"]=new Common::JSONValue((long long)effect.bank);pending["resource"]=new Common::JSONValue((long long)effect.owner.resource);pending["node"]=new Common::JSONValue((long long)effect.owner.node);pendingEffects.push_back(new Common::JSONValue(pending));}o["pending_scene_effect_details"]=new Common::JSONValue(pendingEffects);
 o["credits_active"]=new Common::JSONValue(_creditsPhase!=0);o["credits_phase"]=new Common::JSONValue((long long)_creditsPhase);o["credits_index"]=new Common::JSONValue((long long)_credits.index());o["credits_scene"]=new Common::JSONValue((long long)_creditsScene);o["credits_waiting_start"]=new Common::JSONValue(_creditsPhase==3&&_credits.waitingStart());o["credits_finished"]=new Common::JSONValue(_credits.finished());o["credits_completion_serial"]=new Common::JSONValue((long long)_creditsCompletionSerial);o["credits_gate_tick"]=new Common::JSONValue((long long)_creditsGateTick);
 uint8_t visual=0;bool visualKnown=_scene.visualCommand(visual);o["scene_visual_known"]=new Common::JSONValue(visualKnown);o["scene_visual_command"]=new Common::JSONValue((long long)visual);const PhoneVisualState& phone=_scene.phoneVisualState();o["phone_visual_mode"]=new Common::JSONValue((long long)phone.mode);o["phone_visual_phase"]=new Common::JSONValue((long long)phone.phase);o["palette_publication_mode"]=new Common::JSONValue((long long)_scene.palettePublicationMode());o["display_bytes_per_pixel"]=new Common::JSONValue((long long)_system->getScreenFormat().bytesPerPixel);
 uint16_t progression=0;bool progressionKnown=_runtime&&_runtime->progressionSection(progression);o["progression_known"]=new Common::JSONValue(progressionKnown);o["progression_section"]=new Common::JSONValue((long long)progression);
 unsigned knownSlots=0,occupiedSlots=0;for(unsigned i=0;i<4;++i){if(_gameServices.slots[i].known)knownSlots|=1u<<i;if(_gameServices.slots[i].occupied)occupiedSlots|=1u<<i;}
 unsigned submissionKnown=0;for(const auto& slot:_scene.submissionHistory().previous)if(slot.known)++submissionKnown;
 o["submission_known_slots"]=new Common::JSONValue((long long)submissionKnown);o["environment_mode"]=new Common::JSONValue((long long)_scene.environmentMode());o["interaction_mode"]=new Common::JSONValue((long long)_interactionMode);
 o["game_storage_known"]=new Common::JSONValue(_gameServices.availability!=kGameStorageUnknown);o["game_storage_ready"]=new Common::JSONValue(_gameServices.availability==kGameStorageReady);o["game_save_counter_known"]=new Common::JSONValue(_gameServices.counterKnown);o["game_save_counter"]=new Common::JSONValue((long long)_gameServices.saveCounter);o["game_save_result_known"]=new Common::JSONValue(_gameServices.resultKnown);o["game_save_result"]=new Common::JSONValue((long long)_gameServices.lastWriteResult);o["game_slot_known_mask"]=new Common::JSONValue((long long)knownSlots);o["game_slot_occupied_mask"]=new Common::JSONValue((long long)occupiedSlots);o["game_slot_write_pending"]=new Common::JSONValue(_gameServices.pending);o["game_elapsed_origin"]=new Common::JSONValue((long long)_gameElapsedOrigin);o["startup_active"]=new Common::JSONValue(_startupActive);o["startup_continue_selected"]=new Common::JSONValue(_startup.continueSelected());o["startup_selected_slot"]=new Common::JSONValue((long long)_startup.selectedSlot());o["startup_pending_load_slot"]=new Common::JSONValue((long long)_startup.pendingLoadSlot());o["startup_loaded_slot"]=new Common::JSONValue((long long)_startup.loadedSlot());o["original_resume_pending"]=new Common::JSONValue(_originalResumePending);o["return_title_phase"]=new Common::JSONValue((long long)_returnTitlePhase);o["return_title_remaining"]=new Common::JSONValue((long long)_returnTitleRemaining);
 o["pcm_request_phase"]=new Common::JSONValue((long long)_pcmRequestPhase);o["resident_bank_id"]=new Common::JSONValue((long long)_residentBankId);o["stream_active"]=new Common::JSONValue(_streamOwner.active());o["bookmark_pcm_known"]=new Common::JSONValue(_bookmark.pcmKnown);Common::JSONArray pcmVoices;for(const auto& voice:_effectOwner.voices()){Common::JSONObject value;value["effect"]=new Common::JSONValue((long long)voice.effect);value["bank"]=new Common::JSONValue((long long)voice.bank);value["source"]=new Common::JSONValue((long long)voice.source);value["payload_offset"]=new Common::JSONValue((long long)voice.payloadOffset);value["sample_count"]=new Common::JSONValue((long long)voice.sampleCount);value["metadata_known"]=new Common::JSONValue(voice.metadataKnown);value["prefetch_owned"]=new Common::JSONValue(voice.prefetchOwned);value["refill_source_cursor"]=new Common::JSONValue((long long)voice.sourceCursor);value["refill_queued_frames"]=new Common::JSONValue((long long)voice.queuedFrames);value["rendered_source_frames"]=new Common::JSONValue((long long)voice.renderedFrames);value["refill_terminal_seen"]=new Common::JSONValue(voice.terminalSeen);value["dropped_source_units"]=new Common::JSONValue((long long)voice.droppedUnits);value["token"]=new Common::JSONValue((long long)voice.token.id);pcmVoices.push_back(new Common::JSONValue(value));}o["pcm_effect_voices"]=new Common::JSONValue(pcmVoices);o["primary_music_cache_known"]=new Common::JSONValue(_primaryMusicKnown);o["primary_music_cache"]=new Common::JSONValue((long long)_primaryMusicCommand);o["pcm_cache_known"]=new Common::JSONValue(_lastPcmKnown);o["pcm_cache"]=new Common::JSONValue((long long)_lastPcmCommand);o["bookmark_valid"]=new Common::JSONValue(_bookmark.valid);o["bookmark_phase"]=new Common::JSONValue((long long)_bookmarkPhase);o["entry_active"]=new Common::JSONValue(_entryOpen);o["entry_grid_index"]=new Common::JSONValue((long long)_textEntry.gridIndex());o["entry_mode"]=new Common::JSONValue((long long)_textEntry.mode());o["entry_cursor"]=new Common::JSONValue((long long)_textEntry.cursor());o["entry_session"]=new Common::JSONValue((long long)_textEntrySession);o["shooting_active"]=new Common::JSONValue(_shooting.active);o["shooting_result"]=new Common::JSONValue(_shooting.allSpecialHit);o["shooting_misses"]=new Common::JSONValue((long long)_shooting.misses);o["shooting_spawned"]=new Common::JSONValue((long long)_shooting.encounter.spawned);o["shooting_pointer_input"]=new Common::JSONValue(_shooting.encounter.alternateInput);o["shooting_fire_phase"]=new Common::JSONValue((long long)_shooting.fire.phase);o["shooting_aim_enabled"]=new Common::JSONValue(_shooting.encounter.inputGate!=0);Common::JSONArray targets;for(unsigned i=0;i<9;++i){Common::JSONObject target;target["cell"]=new Common::JSONValue((long long)i);target["phase"]=new Common::JSONValue((long long)_shooting.encounter.targets[i].phaseAndType);target["active"]=new Common::JSONValue(_shooting.encounter.targets[i].active);const auto& semantic=_shooting.encounter.targets[i];target["record"]=new Common::JSONValue((long long)semantic.record);target["remaining"]=new Common::JSONValue((long long)semantic.remaining);target["flags"]=new Common::JSONValue((long long)semantic.flags);target["markers_known"]=new Common::JSONValue(semantic.markersKnown);SceneActorView actual;std::string viewError;if(_scene.inspectActor(32+i,actual,viewError)){target["scene_owned"]=new Common::JSONValue(actual.owned);target["scene_phase_known"]=new Common::JSONValue(actual.phaseKnown);target["scene_phase"]=new Common::JSONValue((long long)actual.phase);target["scene_cleared"]=new Common::JSONValue(actual.cleared);target["scene_markers_known"]=new Common::JSONValue(actual.markersKnown);target["scene_active"]=new Common::JSONValue(actual.active);target["scene_record"]=new Common::JSONValue((long long)actual.record);target["scene_remaining"]=new Common::JSONValue((long long)actual.remaining);target["scene_flags"]=new Common::JSONValue((long long)actual.flags);}targets.push_back(new Common::JSONValue(target));}o["shooting_targets"]=new Common::JSONValue(targets);o["shooting_subphase"]=new Common::JSONValue((long long)_shooting.subphase);o["shooting_timer"]=new Common::JSONValue((long long)_shooting.timer);o["shooting_mode"]=new Common::JSONValue((long long)_shooting.encounter.mode);o["shooting_phase"]=new Common::JSONValue((long long)_shooting.encounter.phase);o["shooting_aim_cell"]=new Common::JSONValue((long long)_shooting.aimCell);o["shooting_shots"]=new Common::JSONValue((long long)_shooting.shots);o["shooting_damage"]=new Common::JSONValue((long long)_shooting.encounter.damage);o["shooting_set_hits"]=new Common::JSONValue((long long)_shooting.encounter.typeSetHits);o["shooting_clear_hits"]=new Common::JSONValue((long long)_shooting.encounter.typeClearHits);o["ok"]=new Common::JSONValue(true);o["ready"]=new Common::JSONValue(_ready);o["playable"]=new Common::JSONValue(false);
 std::string saveReason;o["can_save"]=new Common::JSONValue(saveBoundary(saveReason));o["save_boundary_reason"]=new Common::JSONValue(saveReason.c_str());o["save_error"]=new Common::JSONValue(_saveError.c_str());
 Common::JSONArray references;if(_runtime)for(uint16_t ref:_runtime->control().availableChoices())references.push_back(new Common::JSONValue((long long)ref));o["choice_references"]=new Common::JSONValue(references);
 Common::JSONArray path;if(_runtime)for(uint16_t ref:_runtime->control().selectedPath())path.push_back(new Common::JSONValue((long long)ref));o["selected_path"]=new Common::JSONValue(path);
 SceneRuntime imageOwner=_scene;SceneImage checkpointImage;std::string checkpointError;uint32_t imageDigest=2166136261u;if(!_startupActive&&imageOwner.render(kSceneRGB888,checkpointImage,checkpointError)){for(uint8_t value:checkpointImage.rgb)imageDigest=(imageDigest^value)*16777619u;o["scene_rgb_digest"]=new Common::JSONValue((long long)imageDigest);}
 o["rng_first"]=new Common::JSONValue((long long)_nativeRandom.state.first);o["rng_second"]=new Common::JSONValue((long long)_nativeRandom.state.second);o["rng_draws"]=new Common::JSONValue((long long)_nativeRandom.draws);
 uint16_t sceneResource;unsigned sceneIndex;if(_scene.resourceIdentity(sceneResource,sceneIndex)){o["scene_resource"]=new Common::JSONValue((long long)sceneResource);o["scene_index"]=new Common::JSONValue((long long)sceneIndex);}o["actor1_system_source"]=new Common::JSONValue(_scene.systemActor(1));
 o["resource_ids"]=new Common::JSONValue((long long)_resources.resourceCount());
 o["direct_chd"]=new Common::JSONValue(_resources.isChd());o["fixture"]=new Common::JSONValue(_fixture);o["blocker"]=new Common::JSONValue(_blocker.c_str());
 const FmAudioSnapshot fm=_fm.snapshot();o["fm_core"]=new Common::JSONValue(fmCoreName(fm.core));o["fm_frames"]=new Common::JSONValue((long long)fm.frames);o["fm_updates"]=new Common::JSONValue((long long)fm.updates);o["fm_nonzero_frames"]=new Common::JSONValue((long long)fm.nonzeroFrames);o["fm_percussion_events"]=new Common::JSONValue((long long)fm.percussionEvents);o["fm_active"]=new Common::JSONValue(fm.active);o["fm_error"]=new Common::JSONValue(fm.error.c_str());
 unsigned cdTrack;uint64_t cdPosition;bool cdKnown=_cd.consumedPosition(cdTrack,cdPosition);o["cd_clock_known"]=new Common::JSONValue(cdKnown);o["cd_track"]=new Common::JSONValue((long long)cdTrack);o["cd_consumed_frames"]=new Common::JSONValue((long long)cdPosition);
 o["cd_active"]=new Common::JSONValue(_cd.active());o["cd_paused"]=new Common::JSONValue(_cd.paused());o["presentation_request_pending"]=new Common::JSONValue(_presentationToken.id!=0);o["presentation_load_phase"]=new Common::JSONValue((long long)_presentationLoadPhase);o["cd_frames_queued"]=new Common::JSONValue((long long)_cd.inputFramesQueued());o["cd_total_frames"]=new Common::JSONValue((long long)_cd.totalFrames());
 o["pcm_stream_active"]=new Common::JSONValue(_streamOwner.active());o["pcm_stream_catalog"]=new Common::JSONValue((long long)_streamOwner.catalogId());o["pcm_profile_qualified"]=new Common::JSONValue(sourcePcmStreamProfile().gainVerified);o["startup_complete"]=new Common::JSONValue(_startupCompleted);o["startup_active"]=new Common::JSONValue(_startupActive);o["startup_phase"]=new Common::JSONValue((long long)_startup.phase());
 o["dialogue_mouse_pending"]=new Common::JSONValue(_mouseDialoguePending);o["dialogue_ready"]=new Common::JSONValue(_dialogueReady);o["dialogue_cursor"]=new Common::JSONValue((long long)_dialogueWindow.cursor());o["font_publication_pending"]=new Common::JSONValue(_scene.fontPublicationPending());o["dialogue_active"]=new Common::JSONValue(_dialogueWindow.active());o["dialogue_waiting"]=new Common::JSONValue(_dialogueWindow.waiting());o["dialogue_glyphs"]=new Common::JSONValue((long long)_dialogueWindow.revealedGlyphs());o["pcm_effects_active"]=new Common::JSONValue((long long)_effectOwner.activeCount());o["command_reference"]=new Common::JSONValue((long long)_commandInput.selectedReference());
 o["command_ready"]=new Common::JSONValue(_commandInput.active()&&!_scene.visualBusy());o["visual_busy"]=new Common::JSONValue(_scene.visualBusy());
 o["scene_transfer_busy"]=new Common::JSONValue(_scene.transferBusy());o["scene_blanking_pending"]=new Common::JSONValue(_scene.blankingPending());o["scene_graphics_service_busy"]=new Common::JSONValue(_scene.graphicsServiceBusy());o["resource_transition_pending"]=new Common::JSONValue(_scene.resourceChangePending());o["pending_graphics_tasks"]=new Common::JSONValue((long long)_scene.pendingGraphicsTasks());
 Common::JSONObject pending;if(_runtime)for(const RuntimeRequest& request:_runtime->requests())if(_events.awaiting(request.token)){pending["kind"]=new Common::JSONValue((long long)request.kind);pending["resource"]=new Common::JSONValue((long long)request.owner.resource);pending["node"]=new Common::JSONValue((long long)request.owner.node);pending["first"]=new Common::JSONValue((long long)request.first);pending["second"]=new Common::JSONValue((long long)request.second);break;}o["pending_native_request"]=new Common::JSONValue(pending);
 PointerInteractionSnapshot pointer;std::string pointerError;if(_pointerInteraction.snapshot(pointer,pointerError)){o["interaction_pointer_active"]=new Common::JSONValue(pointer.active);o["interaction_region_known"]=new Common::JSONValue(pointer.selectedKnown);o["interaction_region"]=new Common::JSONValue((long long)pointer.selectedRegion);o["interaction_x16"]=new Common::JSONValue((long long)pointer.x16);o["interaction_y16"]=new Common::JSONValue((long long)pointer.y16);}
 o["pending_scene_effects"]=new Common::JSONValue((long long)_pendingEffects.size());o["audio_error"]=new Common::JSONValue(_audioError.c_str());o["scene_visible"]=new Common::JSONValue(_sceneVisible);o["scene_fixture"]=new Common::JSONValue(_presentationFixture);o["render_error"]=new Common::JSONValue(_renderError.c_str());o["game_ticks"]=new Common::JSONValue((long long)_clock.snapshot().ticks);const ActorControlState *actor29=_scene.actor(29);o["actor29_active"]=new Common::JSONValue(actor29&&actor29->active);o["actor29_completion"]=new Common::JSONValue((long long)(actor29?actor29->completionFlags:0));const AnimationState* animation29=_scene.animation(29);o["actor29_frame"]=new Common::JSONValue((long long)(animation29?animation29->frameIndex:0xffff));o["actor29_record"]=new Common::JSONValue((long long)(animation29?animation29->recordIndex:0xffff));o["actor29_remaining"]=new Common::JSONValue((long long)(animation29?animation29->remaining:0));
 o["render_frames"]=new Common::JSONValue((long long)_renderFrames);o["render_diagnostic_frames"]=new Common::JSONValue((long long)_renderDiagnosticFrames);o["render_pending_scene_frames"]=new Common::JSONValue((long long)_renderPendingSceneFrames);o["render_retained_frames"]=new Common::JSONValue((long long)_renderRetainedFrames);
 o["scene_assets_ready"]=new Common::JSONValue(_sceneAssetsReady);o["scene_actor_count"]=new Common::JSONValue((long long)_sceneAssets.actors.size());o["scene_transfer_count"]=new Common::JSONValue((long long)_sceneAssets.transfers.size());
 if(_runtime){o["runtime_status"]=new Common::JSONValue((long long)_runtime->status());o["resource"]=new Common::JSONValue((long long)_runtime->cursor().resource);o["node"]=new Common::JSONValue((long long)_runtime->cursor().node);o["runtime_error"]=new Common::JSONValue(_runtime->diagnostic().c_str());uint32_t digest=2166136261u;for(uint8_t byte:_runtime->packed()){digest^=byte;digest*=16777619u;}o["packed_digest"]=new Common::JSONValue((long long)digest);o["choices"]=new Common::JSONValue((long long)_runtime->control().availableChoices().size());}
 Common::JSONArray entries;for(const TraceEntry& t:_trace.entries()){Common::JSONObject e;e["sequence"]=new Common::JSONValue((long long)t.sequence);e["resource"]=new Common::JSONValue((long long)t.cursor.resource);e["node"]=new Common::JSONValue((long long)t.cursor.node);e["category"]=new Common::JSONValue(t.category.c_str());e["a"]=new Common::JSONValue((long long)t.a);e["b"]=new Common::JSONValue((long long)t.b);entries.push_back(new Common::JSONValue(e));}o["trace"]=new Common::JSONValue(entries);
 o["scenarios"]=new Common::JSONValue((long long)_scenarios.size());o["code_bytes"]=new Common::JSONValue((long long)_code);o["text_bytes"]=new Common::JSONValue((long long)_text);o["unresolved_bytes"]=new Common::JSONValue((long long)_unknown);
 return new Common::JSONValue(o);
}
bool SnatcherEngine::updateFm(std::string& error){
 error=_output.error();if(!error.empty())return false;
 _fmControlTicked=true;MusicControlPacket packet;if(_music.tick(_quietPcm,packet)){
  if(!_fmLoaded)return true;
  _fm.setGains(packet.musicGain,packet.effectGain);if(packet.command&&!_fm.start(packet.command,error))return false;
 }
 const FmAudioSnapshot state=_fm.snapshot();if(!state.error.empty()){error=state.error;return false;}return true;
}
bool SnatcherEngine::postUiClick(std::string& error){if(!_scene.requestCommandMusic(29,error))return false;_trace.record(_runtime->cursor(),"ui_fm_packet",29,0);return true;}
bool SnatcherEngine::startFm(uint16_t command,std::string& error,bool resetFade){
 if(!loadFmAssets(error))return false;
 if(!_music.submit(command,resetFade,error))return false;return updateFm(error);
}
bool SnatcherEngine::startStartup(std::string& error,bool returnedFromGame){
 _commonGraphicsInitPending=false;
 releaseInput();
 resetCredits();
 resetVisualObserver();
 StartupSettings retained;
 if(returnedFromGame){DialogueLayout layout;if(!_scene.dialogueLayout(layout,error)||!layout.substitutionKnown){error="Return to title has no retained controller settings";return false;}retained=_originalInputSettings;retained.controlMapping=layout.substitution;retained.mono=_pcmMono;}
 _returnTitleToken=EventToken();_returnTitlePhase=_returnTitleRemaining=0;_returnTitleReady=false;_gameSaveCaptures.clear();_pcmRequestToken=EventToken();_pcmRequestPhase=0;_gameStorageScanned=false;_startupRecordReady=false;_startupRecordSlot=0;_originalResumePending=false;

 _presentationToken=EventToken();_presentationPreparation=SceneResourcePreparation();_presentationLoadPhase=0;_presentationNeedsLoad=false;_presentationData.clear();_bookmark=NativePresentationBookmark();_bookmarkToken=EventToken();_bookmarkPhase=0;_bookmarkPreparation=SceneResourcePreparation();_bookmarkNeedsLoad=false;_bookmarkData.clear();_lastPcmCommand=0xffff;_lastPcmKnown=true;_scriptAudioWait.reset();_scriptAudioToken=EventToken();_primaryMusicCommand=0;_primaryMusicKnown=true;_primaryCompletionObserved=_secondaryCompletionObserved=_completionPublicationObserved=0;_completionSerialObserved=0;_textEntry=TextEntry();_entryOpen=_entryPending=_entryKeySaved=false;_entryMousePending=false;_entryMousePressed=0;_pointerInteraction=PointerInteraction();_coordinatorMode=2;_interactionMode=_textEntrySession=_textEntryLimit=_phoneEntryPolicy=_ambientMode=0;_quietPcm=false;_pointerAdjusting=false;_counterSettlement=ShootingCounterSettlement();_counterToken=EventToken();_shooting=ShootingCoordinatorState();_shootingToken=EventToken();_shootingHeld=_shootingPressed=_entryGlobalPressed=_entryFramePressed=0;_startupError.clear();_startupActive=false;_startupCompleted=false;_startupHeld=_startupPressed=0;_startPressed=_workerStartPressed=false;
 if(!_cd.reset(error)||!_events.transition(error))return false;_fm.reset();_fm.setDriverSuspended(false);_fmControlTicked=false;_streamOwner.reset();_effectOwner.reset();_audio.reset();_music=MusicControl();_pcmMono=false;_fm.setMono(false);_residentBankId=0xffff;_residentStaging.reset();_residentStreamPending=0;
 delete _runtime;_runtime=new NarrativeRuntime(_scenarios,_events,_trace);_runtime->setQuerySource(this);_nativeRandom=NativeRandomOwner();_scene.resetForModule();_scene.establishFreshAnimationClock();_scene.establishFreshMusicContext();_scene.setRandomSource(&_nativeRandom);_runtime->setRandomSource(&_nativeRandom);_runtime->setQuerySource(this);_scene.setTrace(&_trace);_dialogueWindow.cancel();_dialogueReady=false;_pendingEffects.clear();_handled.clear();_delay.reset();_commandInput=CommandInput();_closingCommand=CommandResponse();_mouseDialoguePending=false;_mouseCommandPending=false;_sceneAssetsReady=false;_sceneVisible=false;_presentationFixture=false;_fixture=false;_blocker.clear();_audioError.clear();_renderError.clear();_activity=AudioActivity();_activity.clearForSoundReset();
 std::vector<uint8_t> boot,title,first,second,subcode;if(!_resources.readBoot(boot,error)||boot.size()<512||!_resources.readResource(4,title,error)||!_resources.readResource(3,first,error)||!_resources.readResource(2,second,error)||!_resources.readFile("SUBCODE.BIN",subcode,error))return false;
 if(!_scene.loadGameLayoutPresets(boot.data()+512,boot.size()-512,error)||!_scene.loadEnvironmentTables(subcode.data(),subcode.size(),error))return false;
 std::vector<uint8_t> hitMap;if(!readInteractionHitMap(subcode.data(),subcode.size(),hitMap,error)||!_pointerInteraction.loadMap(hitMap,error))return false;_pointerInteraction.sourceFreshReset();
 if(_catalog.empty()){std::vector<uint8_t> index;if(!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMLD_01.BIN",_pcmData,error)||!parsePcmCatalog(index.data(),index.size(),_pcmData.size(),_catalog,error))return false;}
 PcmRequest bank;if(!planPcmRequest(_catalog,30,bank,error)||bank.kind!=kLoadResidentBank){error="startup resident PCM bank30 unavailable";return false;}std::vector<PcmSample> samples;size_t tail;if(!parsePcmSamples(_pcmData.data()+bank.entry.offset,bank.entry.length,true,samples,tail,error))return false;_residentBankId=30;_lastPcmCommand=30;_lastPcmKnown=true;if(!loadResidentBank(30,error))return false;
 // Original four-slot records are distinct from CE full-engine saves. The
 // game-level device service maps to the actual portable CE backend; BIOS
 // firmware menus and backup-RAM hardware are not reproduced by this engine.
 StartupStorage storage;if(!startupStorage(storage,error))return false;
 if(!_startup.start(std::vector<uint8_t>(boot.begin()+512,boot.end()),title,first,second,subcode,storage,returnedFromGame,returnedFromGame?&retained:nullptr,error)||!_clock.configure(599227434,10000000,error))return false;
 if(returnedFromGame){_pcmMono=retained.mono;_fm.setMono(_pcmMono);_fm.setPercussionMono(_pcmMono);}
 _lastTickMillis=_system->getMillis();_startupActive=true;return true;
}
bool SnatcherEngine::stepStartup(uint16_t counter){
 if(!_startupError.empty()){_blocker=_startupError;return false;}
 std::string error;if(!consumeMouseStartup(error)){_startupError=_blocker=error;return false;}StartupFrame frame;frame.pressed=_startupPressed;_startupPressed=0;frame.sourceFrame=counter;frame.soundDriverReady=_fmLoaded;uint64_t consumed=0;frame.cdKnown=_cd.presentationPosition(frame.cdTrack,consumed);if(frame.cdKnown&&!encodeCdRelativeClock(consumed,frame.cdClock,error)){_startupError=error;_blocker=error;return false;}
 uint8_t activity;if(_activity.snapshot(activity,error,7))_scene.setSoundContext(_scene.effectsSuppressed(),activity);else _scene.clearSoundContext();
 if(!_startup.tick(frame,_scene,error)){_startupError=error;_blocker=error;return false;}
 if(!_scene.commitEnvironmentFrame(counter,error)){_startupError=_blocker=error;return false;}
 _fm.setDriverSuspended(_startup.soundDriverSuspended());
 for(unsigned track:_startup.takeTracks()){AudioToken token;if(!_cd.reset(error)){_startupError=error;_blocker=error;return false;}if(!_cd.start(track,64,AudioOwner(4,0),_events.generation(),token,error)){_startupError=error;_blocker=error;return false;}}
 for(uint16_t command:_startup.takeFm())if(!startFm(command,error)){_startupError=error;_blocker=error;return false;}
 for(uint16_t command:_startup.takePcm()){PendingEffect effect={command,_residentBankId,_events.generation(),Cursor(4,0)};if(!dispatchSceneEffect(effect,error)){_startupError=error;_blocker=error;return false;}}
 for(const SceneMusicCommand& command:_scene.takeMusicCommands())if(!startFm(command.command,error,command.resetFade)){_blocker=error;return false;}
 for(uint16_t command:_scene.takeEffects()){PendingEffect effect={command,_residentBankId,_events.generation(),Cursor(4,0)};if(!dispatchSceneEffect(effect,error)){_startupError=error;_blocker=error;return false;}}
 if(!_fmControlTicked&&!updateFm(error)){_startupError=error;_blocker=error;return false;}_sceneAssets=_scene.assets();_sceneAssetsReady=_startup.phase()>kStartupNativeLicense;_blocker.clear();
 if(_startup.pendingLoadSlot()&&!readStartupGameSlot(_startup.pendingLoadSlot(),error)){_startupError=error;_blocker=error;return false;}
 if(_startup.loaded()){StartupSettings settings;if(!_startupRecordReady||_startup.loadedSlot()!=_startupRecordSlot||!_startup.settings(settings,error)||!beginOriginalGameResume(_startupRecord,settings,error)){if(error.empty())error="Original startup load lost its record owner";_startupError=error;_blocker=error;return false;}return true;}
 if(_startup.finished()){StartupSettings settings;if(!_startup.settings(settings,error)||!startNative(Cursor(0x3f,0),false,error)||!_scene.setControllerMapping(uint8_t(settings.controlMapping),error)){_startupError=error;_blocker=error;return false;}_originalInputSettings=settings;_pcmMono=settings.mono;_fm.setMono(_pcmMono);_startupCompleted=true;_trace.record(Cursor(0x3f,0),"native_startup_completed");}
 return true;
}
bool SnatcherEngine::startNative(Cursor entry,bool fixture,std::string& error){
 _commonGraphicsInitPending=false;
 resetCredits();
 resetVisualObserver();
 _returnTitleToken=EventToken();_returnTitlePhase=_returnTitleRemaining=0;_returnTitleReady=false;_gameElapsedOrigin=0;_pcmRequestToken=EventToken();_pcmRequestPhase=0;_startupRecordReady=false;_startupRecordSlot=0;_originalResumePending=false;
 _gameSaveCaptures.clear();_gameServices=GameServiceState();_gameServices.counterKnown=_gameServices.resultKnown=true;_gameStorageScanned=false;
 _presentationToken=EventToken();_presentationPreparation=SceneResourcePreparation();_presentationLoadPhase=0;_presentationNeedsLoad=false;_presentationData.clear();_bookmark=NativePresentationBookmark();_bookmarkToken=EventToken();_bookmarkPhase=0;_bookmarkPreparation=SceneResourcePreparation();_bookmarkNeedsLoad=false;_bookmarkData.clear();_lastPcmCommand=0xffff;_lastPcmKnown=true;_scriptAudioWait.reset();_scriptAudioToken=EventToken();_primaryMusicCommand=0;_primaryMusicKnown=true;_primaryCompletionObserved=_secondaryCompletionObserved=_completionPublicationObserved=0;_completionSerialObserved=0;_textEntry=TextEntry();_entryOpen=_entryPending=_entryKeySaved=false;_entryMousePending=false;_entryMousePressed=0;_pointerInteraction=PointerInteraction();_coordinatorMode=2;_interactionMode=_textEntrySession=_textEntryLimit=_phoneEntryPolicy=_ambientMode=0;_quietPcm=false;_pointerAdjusting=false;_counterSettlement=ShootingCounterSettlement();_counterToken=EventToken();_shooting=ShootingCoordinatorState();_shootingToken=EventToken();_shootingHeld=_shootingPressed=_entryGlobalPressed=_entryFramePressed=0;_startupError.clear();_startupActive=false;_startupCompleted=false;_startupHeld=_startupPressed=0;_startPressed=_workerStartPressed=false;
 if(entry.resource<0x39||entry.resource>0x5f||entry.resource-0x39>=_scenarios.size()||!_scenarios[entry.resource-0x39].at(entry.node)){error="invalid native entry";return false;}
 _fm.reset();_fm.setDriverSuspended(false);_fmControlTicked=false;if(!_cd.reset(error))return false;_streamOwner.reset();_effectOwner.reset();_audio.reset();_music=MusicControl();_pcmMono=false;_fm.setMono(false);_residentBankId=0xffff;_residentStaging.reset();_residentStreamPending=0;_handled.clear();_nativeRandom=NativeRandomOwner();if(fixture)_scene.reset();else _scene.resetForModule();_scene.establishFreshAnimationClock();_scene.establishFreshMusicContext();_scene.setRandomSource(&_nativeRandom);_runtime->setRandomSource(&_nativeRandom);_runtime->setQuerySource(this);_scene.setTrace(&_trace);_dialogueWindow.cancel();_pendingEffects.clear();_delay.reset();_renderError.clear();_audioError.clear();_sceneVisible=false;_presentationFixture=false;if(!_clock.configure(599227434,10000000,error))return false;_lastTickMillis=_system->getMillis();if(!fixture){std::vector<uint8_t> boot;SceneLayout layout;if(!_resources.readBoot(boot,error)||boot.size()<512||!readSceneLayoutPreset(boot.data()+512,boot.size()-512,kSceneTallLayout,layout,error)||!_scene.loadGameLayoutPresets(boot.data()+512,boot.size()-512,error)||!_scene.configure(layout,error)||!_scene.establishSceneReset(kSceneTallLayout,error)||!_scene.establishFreshPalette(error))return false;std::vector<uint8_t> sub;if(!_resources.readFile("SUBCODE.BIN",sub,error)||!_scene.loadEnvironmentTables(sub.data(),sub.size(),error))return false;std::vector<uint8_t> hitMap;if(!readInteractionHitMap(sub.data(),sub.size(),hitMap,error)||!_pointerInteraction.loadMap(hitMap,error))return false;_pointerInteraction.sourceFreshReset();if(!_scene.setTransferChannel(0,error))return false;_scene.establishFreshModuleDialogueContext();_scene.establishSceneFlags(0);if(!_scene.setTextPalette(_textColors,error))return false;_scene.setSoundContext(false,0);}_activity=AudioActivity();if(!fixture)_activity.clearForSoundReset();_audioWait=AudioActivityWait();_sceneAssets=SceneAssets();_sceneAssetsReady=false;_blocker.clear();_fixture=fixture;_selection=_rootSelection=0;_commandInput=CommandInput();_closingCommand=CommandResponse();_mouseDialoguePending=false;_mouseCommandPending=false;_confirmHeld=false;_dialogueReady=false;_pixels.assign(640*240,0);if(!fixture){_commonGraphicsEntry=entry;_commonGraphicsInitPending=true;return true;}return _runtime->start(entry,std::vector<uint8_t>(352,0),error);
}
bool SnatcherEngine::showDialogue(const RuntimeRequest& request,std::string& error){
 std::vector<uint8_t> data;DialogueLayout layout;DialogueWindow window=_dialogueWindow;
 if(!_resources.readResource(request.owner.resource,data,error)||!_scene.dialogueLayout(layout,error)||!window.beginTask(data.data(),data.size(),size_t(0x3800)+request.second,request.first,layout,error))return false;
 _dialogueWindow=window;_dialogueToken=request.token;_dialogueReady=false;return true;
}
bool SnatcherEngine::updateTextEntry(uint8_t counter,std::string& error){
 error.clear();TextEntryEffects effects;
 if(!_entryOpen){std::vector<uint8_t> sub,data;if(!_resources.readFile("SUBCODE.BIN",sub,error)||!_resources.readResource(_runtime->cursor().resource,data,error)||!_textEntry.loadAssets(sub.data(),sub.size(),error))return false;std::vector<TextEntryChoice> choices;for(uint16_t reference:_runtime->control().availableChoices()){size_t at=0x3800+reference;TextEntryChoice choice;choice.reference=reference;while(at<data.size()&&data[at]!=255&&choice.label.size()<255)choice.label.push_back(data[at++]);if(at==data.size()||data[at]!=255){error="entry choice label outside original text";return false;}choices.push_back(choice);}TextEntrySettings settings;settings.session=_textEntrySession;settings.limit=uint8_t(_textEntryLimit);settings.phonePolicy=uint8_t(_phoneEntryPolicy);settings.mode=uint8_t(_interactionMode);if(!_textEntry.begin(choices,settings,effects,error))return false;_entryOpen=true;_entryPending=false;_entryCloseView=-1;_shootingPressed=0;_commandInput.close();_scene.clearMenuCursor();_dialogueWindow.cancel();_dialogueReady=false;
  if(_textEntry.active()){DialogueLayout layout;if(!_scene.dialogueLayout(layout,error)||!_textEntry.beginGraphics(layout.fixedWidth,error))return false;effects.view=-1;effects.changed=effects.cursorMoved=false;}
 }else if(_textEntry.graphicsBusy()){_shootingPressed=0;return true;}else if(_entryPending){if(_scene.visualBusy())return true;}
 else if(_textEntry.awaitingKey()){SceneActorView key;if(!_scene.inspectActor(62,key,error)||!key.owned){error="entry key completion requires its original actor62";return false;}if(!_textEntry.resumeKey(key.flags==3,effects,error))return false;_shootingPressed=0;}
 else{if(_scene.visualBusy()||_scene.fontPublicationPending()){_shootingPressed=0;return true;}TextEntryEffects mouseEffects;if(_entryMousePending){_entryMousePending=false;if(!_textEntry.mouseSelect(_entryMouseX,_entryMouseY,mouseEffects,error))return false;}uint8_t pressed=uint8_t(_entryFramePressed|_entryMousePressed);_shootingPressed=0;if(!_textEntry.step(pressed,_shootingHeld,counter,effects,error,true))return false;effects.cursorMoved=effects.cursorMoved||mouseEffects.cursorMoved;if(!effects.inputDeferred)_entryMousePressed=0;}
 return applyTextEntryEffects(effects,error);
}
bool SnatcherEngine::applyTextEntryEffects(TextEntryEffects& effects,std::string& error){
 if(!_textEntry.queueGraphics(effects,error))return false;
 if(effects.view>=0){if(effects.completed)_entryCloseView=effects.view;else if(!_scene.requestVisualCommand(uint16_t(effects.view),error))return false;}
 if(effects.cursorMoved){for(unsigned actor=62;actor<=63;++actor){SceneActorView current;if(!_scene.inspectActor(actor,current,error)||!current.owned){error="entry cursor requires its original scene actors62/63";return false;}SceneActorPatch cursor;cursor.fields=SceneActorPatch::kX|SceneActorPatch::kY|SceneActorPatch::kFlags;cursor.flags=actor==62?3:0;cursor.x=int16_t((uint16_t(current.x)&0xff00)|effects.cursorX);cursor.y=int16_t((uint16_t(current.y)&0xff00)|effects.cursorY);if(actor==63){cursor.fields|=SceneActorPatch::kRecord;cursor.record=_textEntry.mode()==3?0:2;}if(!_scene.patchActor(actor,cursor,error))return false;}}

 if(effects.keyActor>=0){SceneActorPatch key;key.fields=SceneActorPatch::kFlags;key.flags=0;if(effects.keyX>=0||effects.keyY>=0){SceneActorView current;if(!_scene.inspectActor(unsigned(effects.keyActor),current,error)||!current.owned){error="entry key position requires owned source actor";return false;}if(!_entryKeySaved){_entryKeyX=current.x;_entryKeyY=current.y;_entryKeySaved=true;}if(effects.keyX>=0){key.fields|=SceneActorPatch::kX;key.x=int16_t((uint16_t(current.x)&0xff00)|uint8_t(effects.keyX));}if(effects.keyY>=0){key.fields|=SceneActorPatch::kY;key.y=int16_t((uint16_t(current.y)&0xff00)|uint8_t(effects.keyY));}}if(effects.keyRecord>=0){key.fields|=SceneActorPatch::kRecord;key.record=uint16_t(effects.keyRecord);}if(!_scene.patchActor(unsigned(effects.keyActor),key,error))return false;}
 if(effects.cursorHidden||effects.restoreKeyPosition){SceneActorPatch cursor;cursor.fields=SceneActorPatch::kFlags;cursor.flags=effects.cursorHidden?3:0;if(!_scene.patchActor(63,cursor,error))return false;}
 if(effects.restoreKeyPosition){if(_entryKeySaved){SceneActorPatch key;key.fields=SceneActorPatch::kX|SceneActorPatch::kY;key.x=_entryKeyX;key.y=_entryKeyY;if(!_scene.patchActor(62,key,error))return false;_entryKeySaved=false;}}
 if(effects.click&&!postUiClick(error))return false;
 if(effects.completed)_entryPending=true;
 if(_entryPending&&!_textEntry.graphicsBusy()&&!_scene.fontPublicationPending()&&_entryCloseView>=0){
  for(unsigned actor=62;actor<=63;++actor){SceneActorPatch hide;hide.fields=SceneActorPatch::kFlags;hide.flags=3;if(!_scene.patchActor(actor,hide,error))return false;}
  if(!_textEntry.beginCloseGraphics(_entryCloseView,error))return false;_entryCloseView=-1;
 }
 if(_entryPending&&!_textEntry.graphicsBusy()&&!_scene.visualBusy()&&!_scene.fontPublicationPending()){_textEntrySession=_textEntry.settings().session;_textEntryLimit=_textEntry.settings().limit;_phoneEntryPolicy=_textEntry.settings().phonePolicy;_interactionMode=_textEntry.settings().mode;uint16_t reference=_textEntry.selectedReference();if(!_runtime->chooseReference(reference,error))return false;_entryOpen=_entryPending=false;_entryMousePending=false;_entryMousePressed=0;_trace.record(_runtime->cursor(),"player_entry_confirmed",reference);}
 return true;
}
bool SnatcherEngine::showMenu(bool opening,std::string& error){
 std::vector<uint8_t> data;DialogueLayout layout;DialogueWindow window=_dialogueWindow;SceneRuntime scene=_scene;
 if(_interactionMode&&(_interactionMode!=1||!_pointerInteraction.active())){error="text/pointer entry requires its original native interaction owner";return false;}
 if(!_resources.readResource(_runtime->cursor().resource,data,error)||!scene.dialogueLayout(layout,error)||!window.beginMenuTask(data.data(),data.size(),_runtime->control().availableChoices(),_selection,layout,opening,error))return false;
 _scene=scene;_dialogueWindow=window;_dialogueReady=false;return true;
}
bool SnatcherEngine::coordinateText(std::string& error){
 if(_entryOpen&&_textEntry.graphicsBusy()){TextEntryEffects effects;if(!_scene.prepareTextEntry(_textEntry,error)||!_textEntry.coordinateGraphics(_scene.visualControllerBusy(),effects,error)||!_scene.stageTextEntry(_textEntry,error))return false;return applyTextEntryEffects(effects,error);}
 if(!_dialogueWindow.active()||(_runtime&&_runtime->status()==kRuntimeChoices&&!_dialogueWindow.menuTaskOwned())||_scene.graphicsServiceBusy()||_scene.resourceChangePending())return true;
 if(!consumeMouseDialogue(error))return false;
 if(_dialogueWindow.menuTaskOwned()){DialogueWindow window=_dialogueWindow;if(!_scene.prepareDialogue(window,error)||!window.coordinateMenu(error))return false;return publishDialogue(window,error);}
 DialogueWindow window=_dialogueWindow;if(window.taskOwned()){if(!_scene.prepareDialogue(window,error)||!window.coordinateTask(error))return false;return publishDialogue(window,error);}
 if(!window.waiting())return true;
 // The dialogue task posts prompt jobs in D6F6, before E846 scans them.
 if(!_scene.prepareDialogue(window,error)||!window.tickPrompt(error)||!publishDialogue(window,error))return false;_textPromptUpdated=true;return true;
}
bool SnatcherEngine::advanceText(uint8_t counter,std::string& error){
 if(_entryOpen&&_textEntry.graphicsBusy()){if(!_scene.prepareTextEntry(_textEntry,error)||!_textEntry.consumeGraphics(counter,error))return false;return _scene.stageTextEntry(_textEntry,error);}
 if(_runtime&&_runtime->status()==kRuntimeChoices&&!_dialogueWindow.menuTaskOwned())return true;
 if(!_dialogueWindow.active())return true;
 DialogueWindow window=_dialogueWindow;
 if(window.taskOwned()||window.menuTaskOwned()){if(!_scene.prepareDialogue(window,error)||!window.tickGlyphs(false,error,!window.menuTaskOwned()&&(_entryFramePressed&0x20)!=0))return false;return publishDialogue(window,error);}
 if(_textPromptUpdated||window.waiting())return true;
 if(!_scene.prepareDialogue(window,error)||!window.tick(false,error,(_entryFramePressed&0x20)!=0))return false;return publishDialogue(window,error);
}
bool SnatcherEngine::acknowledgeText(std::string& error){
 if(!nativeDialogueInputReady(_dialogueReady,_scene.visualBusy(),_scene.fontPublicationPending()))return false;DialogueWindow window=_dialogueWindow;if(!_scene.prepareDialogue(window,error)||!window.acknowledge(error))return false;return publishDialogue(window,error);
}
bool SnatcherEngine::publishDialogue(DialogueWindow& window,std::string& error){
 SceneRuntime scene=_scene;for(DialogueViewAction action=window.takeViewAction();action!=kDialogueNoViewAction;action=window.takeViewAction()){
  if(action==kDialogueCloseView){if(!scene.requestVisualCommand(0xfe,error))return false;}
  else if(action==kDialogueDefaultView){if(!scene.postDialogueView(window.defaultViewCommand(),error))return false;}
  else if(action==kDialogueSpecialView){if(!scene.postDialogueView(0x15,error))return false;}
  else if(action==kDialogueMenuView){if(!scene.requestMenuView(error))return false;}
  else if(!scene.requestPalettePublication(255,error)||!scene.postDialogueView(uint8_t(window.speakerWord())==1?0x15:window.defaultViewCommand(),error))return false;
 }
 if(!scene.stageDialogue(window,error))return false;
 _scene=scene;_dialogueWindow=window;_dialogueReady=!window.menuTaskOwned()&&window.waiting();
 if(window.finished()&&_events.awaiting(_dialogueToken))return _events.complete(_dialogueToken,false,error);return true;
}
void SnatcherEngine::updateNative(){
 std::string cdError;if(!_cd.update(cdError)&&!cdError.empty())_blocker=cdError;CdAudioCompletion cd;while(_cd.take(cd))_trace.record(Cursor(cd.owner.resource,cd.owner.node),cd.result==kCdAudioFinished?"cd_finished":cd.result==kCdAudioCancelled?"cd_cancelled":"cd_failed",uint32_t(cd.token.id),uint32_t(cd.token.id>>32));
 if(!_runtime)return;_audio.poll();AudioCompletion audio;while(_audio.take(audio)){if(_effectOwner.consume(audio))_trace.record(Cursor(audio.owner.resource,audio.owner.node),audio.result==kAudioFinished?"effect_finished":"effect_cancelled",uint32_t(audio.token.id));if(_streamOwner.consume(audio)){if(audio.result==kAudioFinished){std::string stagingError;if(!finishPcmStreamStaging(stagingError)){_audioError=_blocker=stagingError;return;}}else{if(_residentStaging&&_residentStreamPending){std::string ignored;_residentStaging->invalidateStream(_residentStreamPending,ignored);}_residentStreamPending=0;}_trace.record(Cursor(audio.owner.resource,audio.owner.node),audio.result==kAudioFinished?"stream_finished":"stream_cancelled",uint32_t(audio.token.id));}_trace.record(Cursor(audio.owner.resource,audio.owner.node),audio.result==kAudioFinished?"audio_completed":"audio_cancelled",uint32_t(audio.token.id));}
 std::string feedError=_audio.feedError();if(!feedError.empty()){_audioError=_blocker=feedError;_activity.forgetFlag(3);return;}
 uint32_t now=_system->getMillis(),elapsed=now-_lastTickMillis;_lastTickMillis=now;
 uint64_t suspended=0;std::string error;
 bool advanced=_clock.advanceHost(elapsed,4,[&](uint16_t counter){return stepNative(counter);},_hostUpdatesLast,suspended,error);
 _hostSuspendedUpdates=suspended>UINT64_MAX-_hostSuspendedUpdates?UINT64_MAX:_hostSuspendedUpdates+suspended;
 if(!advanced&&!error.empty())_blocker=error;
}
bool SnatcherEngine::stepNative(uint16_t counter){
 if(!_runtime||_runtime->status()!=kRuntimeChoices||!_pointerInteraction.active()||!_runtime->control().selectedPath().empty()||_coordinatorMode!=2)_pointerRepeat.reset();
 _textPromptUpdated=false;
 enterVisualObserver(counter);
 if(_returnTitleReady){std::string error;uint32_t previous=_events.generation();if(!startStartup(error,true)){_blocker=error;return false;}_trace.record(Cursor(),"application_teardown_acknowledged",previous,_events.generation());return true;}
 std::string completionError;if(!consumeFmCompletions(completionError)){_blocker=completionError;return false;}
 _entryFramePressed=_entryGlobalPressed;_entryGlobalPressed=0;_textEntry.advanceHeldRepeat(_entryFramePressed,_shootingHeld);
 _fmControlTicked=false;_workerStartPressed=_startPressed;_startPressed=false;
 std::string refillError;if(!_streamOwner.serviceRefills(refillError)){_activity.forgetFlag(1);_audioError=_blocker=refillError;return false;}if(!_effectOwner.serviceRefills(refillError)){_activity.forgetFlag(3);_audioError=_blocker=refillError;return false;}
 unsigned cdTrack;uint64_t cdFrames;uint32_t cdPacked=0;std::string cdClockError;bool cdClockKnown=_cd.presentationPosition(cdTrack,cdFrames);if(cdClockKnown&&!encodeCdRelativeClock(cdFrames,cdPacked,cdClockError)){_blocker=cdClockError;return false;}_scene.setCdClock(cdClockKnown,cdClockKnown?cdPacked:0);
 uint16_t background;std::string randomError;if(!_nativeRandom.next(background,randomError)){_blocker=randomError;return false;}
 if(_startupActive)return stepStartup(counter);
 if(_pendingEffects.size()>64){_blocker="pending scene effect capacity exhausted";return false;}
 std::string sceneError;if(!_scene.beginFrame(false,sceneError,counter)){_blocker=sceneError;return false;}
 if(_pointerInteraction.active()){uint16_t first,second;PointerInteractionPosition position;if(!_nativeRandom.next(first,sceneError)||!_nativeRandom.next(second,sceneError)||!_pointerInteraction.position(first,second,position,sceneError)||!_scene.queueAbsolutePlanePosition(0,position.x,position.y,sceneError)){_blocker=sceneError;return false;}}
 // D70A processes a previously emitted presentation queue before E046's
 // coordinator clock. A replacement issued here can suppress that visit.
 if(_presentationToken.id&&!_scene.graphicsServiceBusy()){
  bool found=false;for(const RuntimeRequest& request:_runtime->requests())if(request.token==_presentationToken){found=true;if(!updateQueuedPresentation(request,sceneError)){_blocker=sceneError;return false;}break;}
  if(!found){_blocker="presentation queue lost its source request";return false;}
  if(!_presentationToken.id&&!_runtime->resumeCompleted(sceneError)&&!sceneError.empty()){_blocker=sceneError;return false;}
 }
 // The bookmark's source queue has the same admission point, including
 // original-slot resume. No coordinator-time fallback advances it twice.
 if(!_scene.graphicsServiceBusy()){
  if(_originalResumePending){if(!updateOriginalGameResume(sceneError)){_blocker=sceneError;return false;}}
  else if(_bookmarkToken.id){
   bool found=false;for(const RuntimeRequest& request:_runtime->requests())if(request.token==_bookmarkToken){found=true;if(!updateBookmark(request,sceneError)){_blocker=sceneError;return false;}break;}
   if(!found){_blocker="bookmark lost its source request";return false;}
   if(!_bookmarkToken.id&&!_runtime->resumeCompleted(sceneError)&&!sceneError.empty()){_blocker=sceneError;return false;}
  }
 }
 // D6F6 processes the prior queue before F7F0 can produce new actor markers.
 // Resource transfers block that coordinator, never an invented scene timeout.
 // Source scenario state0 consumes EC5A's initialization packet before
 // state1/2 dispatch. This is one coordinator transaction, not a wait for
 // all deferred graphics jobs or an additional timeline completion predicate.
 if(_commonGraphicsInitPending){if(!_scene.graphicsServiceBusy()&&!_scene.resourceChangePending()){std::vector<uint8_t> sub;if(!_resources.readFile("SUBCODE.BIN",sub,sceneError)||!_scene.queueScenarioGraphicsInitialization(sub.data(),sub.size(),sceneError)||!_runtime->start(_commonGraphicsEntry,std::vector<uint8_t>(352,0),sceneError)){_blocker=sceneError;return false;}_runtime->establishFreshProgression();_trace.record(_commonGraphicsEntry,"new_game_packed_clear",352,448);_trace.record(_commonGraphicsEntry,"new_game_sound_reset",15,0);_commonGraphicsInitPending=false;}}
 else if(_creditsPhase){if(!updateCredits(sceneError)){_blocker=sceneError;return false;}}
 else if(_originalResumePending||_bookmarkToken.id){/* Prior queue owner is serviced above E046. */}
 else if(!_scene.transferBusy()&&!_scene.resourceChangePending()&&!coordinateNative(counter))return false;
 uint8_t activity;if(_activity.snapshot(activity,sceneError,7))_scene.setSoundContext(_scene.effectsSuppressed(),activity);else _scene.clearSoundContext();
 if(!coordinateText(sceneError)){_blocker=sceneError;return false;}
 if(!_scene.updateObjects(false,sceneError)){_blocker=sceneError;return false;}if(_scene.graphicsServiceBusy()&&!_scene.commitTransfers(sceneError)){_blocker=sceneError;return false;}
 for(const SceneMusicCommand& command:_scene.takeMusicCommands())if(!startFm(command.command,sceneError,command.resetFade)){_blocker=sceneError;return false;}
 std::vector<uint16_t> effects=_scene.takeEffects();for(uint16_t effect:effects){PendingEffect pending={effect,_residentBankId,_events.generation(),_runtime->cursor()};_pendingEffects.push_back(pending);}
 while(!_pendingEffects.empty()){if(!dispatchSceneEffect(_pendingEffects.front(),sceneError)){_activity.forgetFlag(3);_audioError=sceneError;break;}_pendingEffects.erase(_pendingEffects.begin());_audioError.clear();}
 if(_pendingEffects.size()>64){_blocker="pending scene effect capacity exhausted";return false;}if(!advanceText(uint8_t(counter),sceneError)){_blocker=sceneError;return false;}if(!_fmControlTicked&&!updateFm(sceneError)){_audioError=_blocker=sceneError;return false;}if(!_scene.commitEnvironmentFrame(counter,sceneError)){_blocker=sceneError;return false;}// Ordinary command input owns its own edges; a future encounter clears its
 // entry latch. Do not retain obsolete shooting edges from menu confirmations.
 if(!_shooting.active&&_runtime->status()==kRuntimeChoices&&!_entryOpen)_shootingPressed=0;
 if(_creditsPhase==3&&_credits.waitingStart()&&_creditsGateRgb.empty()&&CETestCLI::active()){SceneRuntime snapshot=_scene;SceneImage image;if(!snapshot.render(kSceneRGB888,image,sceneError)){_blocker=sceneError;return false;}_creditsGateRgb.swap(image.rgb);_creditsGateTick=(_clock.snapshot().ticks&~uint64_t(65535))|counter;if(_creditsGateTick>_clock.snapshot().ticks)_creditsGateTick-=65536;}
 finishVisualObserver(counter);return true;
}
bool SnatcherEngine::dispatchSceneEffect(const PendingEffect& pending,std::string& error){
 if(_effectCatalog.empty()){std::vector<uint8_t> index;if(!_resources.readFile("PCMLT_01.BIN",index,error)||!parsePcmEffects(index.data(),index.size(),_effectCatalog,error))return false;}
 PcmEffectControl control=planPcmEffectControl(pending.id,_effectCatalog.size());if(control.kind!=kPcmEffectRecord){
  if(control.kind==kPcmEffectMonoOn){_pcmMono=true;_fm.setPercussionMono(true);}else if(control.kind==kPcmEffectMonoOff){_pcmMono=false;_fm.setPercussionMono(false);}else if(control.kind==kPcmEffectStop){if(!_fm.silencePcm(error))return false;_effectOwner.silence();}
  _trace.record(pending.owner,"pcm_effect_control",pending.id&255,unsigned(control.kind));return true;
 }
 const unsigned id=pending.id&255;const PcmEffect* effect=nullptr;for(const PcmEffect& candidate:_effectCatalog)if(candidate.id==id)effect=&candidate;if(!effect){error="scene PCM effect identity unavailable";return false;}
 bool admitted;if(!_effectOwner.preflight(*effect,admitted,error))return false;if(!admitted){_trace.record(pending.owner,"scene_effect_priority_rejected",id,0);return true;}
 std::vector<uint8_t> stagingBytes;const uint8_t* source=nullptr;size_t size=0;
 if(effect->source==kDrumSource){if(_drumData.empty()&&!_resources.readFile("PCMDRMDT.BIN",_drumData,error))return false;source=_drumData.data();size=_drumData.size();}
 else{if(!_residentStaging||_residentStreamPending||!_residentStaging->copyKnownBytes(stagingBytes,error)){if(error.empty())error="Resident effect requires known completed source staging";return false;}source=stagingBytes.data();size=stagingBytes.size();}
 PreparedPcmEffect prepared;AudioToken token;bool accepted=false;if(!(effect->source==kResidentSource?preparePcmStagingEffect(*effect,source,size,sourcePcmEffectProfile(*effect),_pcmMono,0,prepared,error,_quietPcm):preparePcmEffect(*effect,source,size,sourcePcmEffectProfile(*effect),_pcmMono,0,prepared,error,_quietPcm)))return false;
 if(effect->source==kResidentSource&&prepared.sample.sampleCount>=8192&&!_streamOwner.sharedBus()){std::shared_ptr<PcmResidentFeed> feed(new PcmResidentFeed());ResidentPcmFeedParameters parameters;parameters.payloadOffset=prepared.sample.payloadOffset;parameters.rate=prepared.profile.rate;parameters.level=effect->level;parameters.pan=effect->pan;parameters.mono=_pcmMono;parameters.quiet=_quietPcm;if(!feed->start(_residentStaging,parameters,error)||!_effectOwner.startFeed(prepared,_residentStaging->bank(),feed,_residentStaging,AudioOwner(pending.owner.resource,pending.owner.node),pending.generation,accepted,token,error))return false;}
 else if(!_effectOwner.start(prepared,pending.bank,AudioOwner(pending.owner.resource,pending.owner.node),pending.generation,accepted,token,error))return false;
 _trace.record(pending.owner,accepted?"scene_effect_started":"scene_effect_priority_rejected",id,uint32_t(token.id));return true;
}
bool SnatcherEngine::dispatchNarrativeEffect(const RuntimeRequest& request,std::string& error){
 if(_scene.effectsSuppressed()){_trace.record(request.owner,"effect_suppressed",request.first);return _events.complete(request.token,false,error);}
 uint8_t bits;if(!_activity.snapshot(bits,error,7))return false;if(bits){_trace.record(request.owner,"effect_busy_skipped",request.first,bits);return _events.complete(request.token,false,error);}
 if(_fixture){error="narrative effect suppression context unestablished in direct fixture";return false;}
 PendingEffect effect={request.first,_residentBankId,_events.generation(),request.owner};if(!dispatchSceneEffect(effect,error))return false;
 return _events.complete(request.token,false,error);
}
bool SnatcherEngine::dispatchNativeCommand(const RuntimeRequest& request,std::string& error){
 error.clear();if(request.queue==14&&request.operands.size()==1){AudioToken token;if(!_cd.reset(error)||!_cd.start(request.operands[0],64,AudioOwner(request.owner.resource,request.owner.node),_events.generation(),token,error))return false;_scene.setCdClock(true,0);_trace.record(request.owner,"script_cd_started",request.operands[0]);}
 else if((request.queue==11||request.queue==19||request.queue==25)&&request.operands.empty()){if(!_scriptAudioToken.id){if(!_scriptAudioWait.start(request.queue,error))return false;_scriptAudioToken=request.token;}else if(!(_scriptAudioToken==request.token)){error="script audio wait owner conflict";return false;}uint8_t secondary=0;if(request.queue==19&&!_primaryMusicKnown){error="script primary music cache needs its source owner after legacy load";return false;}if(request.queue==25&&!_scene.lastAnimationMusic(secondary)){error="script secondary music cache has no owner";return false;}ScriptAudioWaitResult result=_scriptAudioWait.tick(_primaryMusicCommand,secondary,_cd.active()?(_cd.paused()?kScriptCdPaused:kScriptCdPlaying):kScriptCdIdle,error);if(result==kScriptAudioWaitError)return false;if(result==kScriptAudioWaitPending)return true;_scriptAudioToken=EventToken();}
 else if(request.queue==4&&request.operands.empty())_scene.setMusicSuppressed(true);
 else if(request.queue==28&&request.operands.size()==1){if(!_scene.requestCommandMusic(uint8_t(request.operands[0]),error))return false;}
 else if(request.queue==29&&request.operands.size()==1)_scene.setEffectsSuppressed(uint8_t(request.operands[0])!=0);
 else if(request.queue==27&&request.operands.size()==3){uint32_t current;if(!_scene.cdClock(current)){error="CD wait requires an owned relative source clock";return false;}uint32_t deadline=uint32_t(request.operands[0]&255)<<24|uint32_t(request.operands[1]&255)<<16|uint32_t(request.operands[2]&255)<<8;if(current<deadline)return true;}
 else if(request.queue==33&&request.operands.empty()){if(!_workerStartPressed)return true;}
 else{error="native command operands or owner unresolved";return false;}
 _trace.record(request.owner,"native_command_completed",request.queue);return _events.complete(request.token,false,error);
}
bool SnatcherEngine::coordinateNative(uint8_t counter){
 std::string clockError;bool admitted=false;if(!_scene.advanceCoordinatorClock(clockError,&admitted)){_blocker=clockError;return false;}if(!admitted)return true;
 // Sub1106E/11DCE closes the source menu and waits for its visual report
 // before a confirmed/cancelled choice can emit another scenario request.
 if(_closingCommand.kind!=kCommandNoResponse){if(_scene.visualBusy())return true;CommandResponse response=_closingCommand;_closingCommand=CommandResponse();std::string error;
  if(_runtime->status()!=kRuntimeChoices){_blocker="menu close lost its choice owner";return false;}
  if(response.kind==kCommandConfirmed){if(!_runtime->chooseReference(response.reference,error)){_blocker=error;return false;}if(_closingCommandRoot)_rootSelection=response.index;_trace.record(_runtime->cursor(),"player_command_confirmed",response.reference);}
  else{if(!_runtime->cancelChoice(error)){_blocker=error;return false;}_trace.record(_runtime->cursor(),"player_command_cancelled");}return true;
 }
 if(_runtime->consumeMenuSelectionReset())_rootSelection=0;
 if(_runtime->status()==kRuntimePassFinished){std::string restartError;if(!_runtime->continueTraversal(restartError)){_blocker=restartError;return false;}}
 if(_runtime->status()==kRuntimeChoices){
  if(_entryOpen||_textEntrySession==2||_runtime->control().availableChoices().size()>=9){std::string entryError;if(!updateTextEntry(counter,entryError)){_blocker=entryError;return false;}return true;}
  if(_coordinatorMode!=2){_mouseCommandPending=false;_commandInput.discardPressed();return true;}
  std::string inputError;const std::vector<uint16_t>& refs=_runtime->control().availableChoices();
  if(!_commandInput.active()){if(!_commandInput.open(refs,!_runtime->control().selectedPath().empty(),_rootSelection,inputError)){_blocker=inputError;return false;}_selection=_commandInput.selectedIndex();if(!showMenu(true,inputError)){_commandInput.close();_blocker=inputError;return false;}}
  if(!nativeMenuInputReady(_scene.visualBusy(),_dialogueWindow.menuPainting(),_scene.fontPublicationPending())){_commandInput.discardPressed();return true;}
  if(_pointerInteraction.active()){
   bool nested=!_runtime->control().selectedPath().empty();
   uint32_t polls=_pointerRepeat.poll(_system->getMillis(),!nested,_shootingHeld,ConfMan.hasKey("kbdmouse_speed")?ConfMan.getInt("kbdmouse_speed"):3);
   PointerInteractionResult result;
   // A visit without admitted movement still owns B-held adjustment/cache
   // retention. Release publishes through the original one-poll source path.
   if(!_pointerInteraction.sample(nested,polls?_shootingHeld:uint8_t(_shootingHeld&16),polls?polls:1,result,inputError)){_blocker=inputError;return false;}
   if(result.adjusting){_pointerAdjusting=true;_scene.deferMenuCursorClear();_mouseCommandPending=false;_commandInput.discardPressed();return true;}
   if(_pointerAdjusting){if(!showMenu(false,inputError)){_blocker=inputError;return false;}_pointerAdjusting=false;}
  }
  if(!consumeMouseCommand(inputError)){_blocker=inputError;return false;}CommandResponse response=_commandInput.update(counter);_selection=_commandInput.selectedIndex();
  // Source1106E/11DCE returns the menu task after this emission. Its next
  // script queue belongs to the next coordinator, before that worker emits.
  if(response.kind==kCommandConfirmed||response.kind==kCommandCancelled){if(response.kind==kCommandConfirmed&&!postUiClick(inputError)){_blocker=inputError;return false;}if(!_scene.requestMenuClose(inputError)){_blocker=inputError;return false;}_closingCommand=response;_closingCommandRoot=_runtime->control().selectedPath().empty();_commandInput.close();_scene.clearMenuCursor();_dialogueWindow.cancel();_dialogueReady=false;return true;}
  else if(response.kind==kCommandSelectionChanged){if(!showMenu(false,inputError)){_blocker=inputError;return false;}_trace.record(_runtime->cursor(),"player_command_selected",response.reference);}
 }else{_commandInput.close();_scene.clearMenuCursor();}
 if(_runtime->status()==kRuntimeRunning&&_coordinatorMode==2){_handled.clear();_runtime->run();if(_runtime->consumeMenuSelectionReset())_rootSelection=0;}
 if(_runtime->status()==kRuntimeChoices&&!_commandInput.active()){
  if(_textEntrySession==2||_runtime->control().availableChoices().size()>=9){std::string entryError;if(!updateTextEntry(counter,entryError)){_blocker=entryError;return false;}return true;}
  std::string inputError;if(!_commandInput.open(_runtime->control().availableChoices(),!_runtime->control().selectedPath().empty(),_rootSelection,inputError)){_blocker=inputError;return false;}_selection=_commandInput.selectedIndex();if(!showMenu(true,inputError)){_commandInput.close();_blocker=inputError;return false;}
 }
 if(_runtime->status()!=kRuntimeWaiting)return true;std::string error;
 // Coordinator mode publication only arms its deferred application owner;
 // starting that owner must wait for preceding queued requests to drain.
 // Native settings are synchronous source producers, preceding the deferred
 // graphics/audio consumers. Apply them in their original producer order.
 for(const RuntimeRequest& r:_runtime->requests())if((r.kind==kRequestSemanticSetting||r.kind==kRequestInteractionMode||r.kind==kRequestCoordinatorControl||r.kind==kRequestPresentationBookmark||r.kind==kRequestGameSave)&&_events.awaiting(r.token)){if(!(r.kind==kRequestGameSave?captureOriginalGameSlot(r,error):applySemanticOwner(r,error))){_blocker=error;return false;}}

 for(const RuntimeRequest& r:_runtime->requests()){
  bool seen=false;for(EventToken token:_handled)if(token==r.token)seen=true;if(seen){if(_events.awaiting(r.token)){
   if(r.kind==kRequestCoordinatorControl&&r.first==1){if(!_creditsPhase||!(_creditsToken==r.token))_blocker="application credits lost its source owner";}
  else if(r.kind==kRequestCoordinatorControl&&r.first==2){if(!updateReturnTitle(r,error))_blocker=error;else _blocker.clear();}
  else if(r.kind==kRequestGameSave){if(!writeOriginalGameSlot(r,error))_blocker=error;else _blocker.clear();}
  else if(r.kind==kRequestPresentationBookmark){if(!_bookmarkToken.id&&!updateBookmark(r,error))_blocker=error;}
   else if(r.kind==kRequestAudioWait){AudioWaitResult result=_audioWait.tick(_activity,error);if(result==kAudioWaitFinished){_events.complete(r.token,false,error);_blocker.clear();}else if(result==kAudioWaitUnknown)_blocker=error;}
   else if(r.kind==kRequestActorAction){ActorActionResult result=_scene.actorAction(r.first,r.second,error);if(result==kActorActionApplied)_events.complete(r.token,false,error);else if(result==kActorActionError)_blocker=error;}
   else if(r.kind==kRequestEffect){if(!dispatchNarrativeEffect(r,error))_blocker=error;}
   else if(r.kind==kRequestPcm){if(!updatePcmRequest(r,error))_blocker=error;else _blocker.clear();}
   if(r.kind==kRequestCommand&&(r.queue==4||r.queue==11||r.queue==14||r.queue==19||r.queue==25||r.queue==27||r.queue==28||r.queue==29||r.queue==33)){if(!dispatchNativeCommand(r,error))_blocker=error;else if(!_events.awaiting(r.token))_blocker.clear();}
   if(r.kind==kRequestCommand&&r.queue==31&&r.operands.empty()){if(!updateCounterSettlement(r,counter,error))_blocker=error;else _blocker.clear();}
   if(r.kind==kRequestCommand&&r.queue==24&&r.operands.size()==1){if(!updateShooting(r,error))_blocker=error;else _blocker.clear();}
   if(r.kind==kRequestCommand&&r.queue==16&&_delayToken==r.token&&_delay.tick(false))_events.complete(r.token,false,error);
   if((_events.awaiting(r.token)&&!(r.kind==kRequestCoordinatorControl&&r.first==2&&!_returnTitlePhase))||_scene.transferBusy())break;
  }continue;}_handled.push_back(r.token);
  if(!_events.awaiting(r.token))continue;
  if(r.kind==kRequestCoordinatorControl&&r.first==1){if(!startCredits(r,error))_blocker=error;else _blocker.clear();}
  else if(r.kind==kRequestCoordinatorControl&&r.first==2){if(!updateReturnTitle(r,error))_blocker=error;else _blocker.clear();}
  else if(r.kind==kRequestGameSave){if(!writeOriginalGameSlot(r,error))_blocker=error;else _blocker.clear();}
  else if(r.kind==kRequestPresentationBookmark){if(!_bookmarkToken.id&&!updateBookmark(r,error))_blocker=error;}
  else if(r.kind==kRequestDialogue){if(!showDialogue(r,error))_blocker=error;}
  else if(r.kind==kRequestEffect){if(!dispatchNarrativeEffect(r,error))_blocker=error;}
  else if(r.kind==kRequestPcm){if(!updatePcmRequest(r,error))_blocker=error;else _blocker.clear();}else if(r.kind==kRequestPresentation){
   if(_presentationToken.id){_blocker="presentation queue owner conflict";break;}
   _presentationToken=r.token;_presentationPreparation=SceneResourcePreparation();
   uint16_t resident;unsigned list;_presentationNeedsLoad=!_scene.resourceIdentity(resident,list)||resident!=(r.first>>8);_presentationLoadPhase=0;_presentationData.clear();
   _trace.record(r.owner,"presentation_queue_emitted",r.first);_blocker.clear();}
  else if(r.kind==kRequestFm){_primaryMusicCommand=uint8_t(r.first)==255?0:uint8_t(r.first);_primaryMusicKnown=true;if(!startFm(r.first,error))_blocker=_audioError=error;
   else{_events.complete(r.token,false,error);_trace.record(r.owner,"fm_native_dispatched",r.first);_blocker.clear();}}
  else if(r.kind==kRequestAudioWait){_audioWait=AudioActivityWait();AudioWaitResult result=_audioWait.tick(_activity,error);if(result==kAudioWaitFinished)_events.complete(r.token,false,error);else if(result==kAudioWaitUnknown)_blocker=error;_trace.record(r.owner,"audio_activity_wait",_activity.knownMask());}
  else if(r.kind==kRequestActorAction){ActorActionResult result=_scene.actorAction(r.first,r.second,error);if(result==kActorActionApplied)_events.complete(r.token,false,error);else if(result==kActorActionError)_blocker=error;_trace.record(r.owner,"actor_action_consumer",r.first,r.second);}
  else if(r.kind==kRequestCommand&&r.queue==31&&r.operands.empty()){if(!updateCounterSettlement(r,counter,error))_blocker=error;else _blocker.clear();}
  else if(r.kind==kRequestCommand&&r.queue==24&&r.operands.size()==1){if(!updateShooting(r,error))_blocker=error;else _blocker.clear();}
  else if(r.kind==kRequestCommand&&r.queue==21&&r.operands.size()==2){std::vector<uint8_t> sub;if(!_resources.readFile("SUBCODE.BIN",sub,error)||!_scene.installSystemPresentation(sub,r.operands[0],r.operands[1],error))_blocker=error;else{_events.complete(r.token,false,error);_trace.record(r.owner,"system_presentation_dispatched",r.operands[0],r.operands[1]);_blocker.clear();}}
  else if(r.kind==kRequestCommand&&r.queue==16&&r.operands.size()==1){_delay.start(r.operands[0]);_delayToken=r.token;if(_delay.tick(false))_events.complete(r.token,false,error);_trace.record(r.owner,"coordinator_delay",r.operands[0]);}
  else if(r.kind==kRequestCommand&&(r.queue==4||r.queue==11||r.queue==14||r.queue==19||r.queue==25||r.queue==27||r.queue==28||r.queue==29||r.queue==33)){if(!dispatchNativeCommand(r,error))_blocker=error;else _blocker.clear();}
  else if(r.kind==kRequestCommand){Common::String message=Common::String::format("resource %u node %u queue %u consumer not implemented",unsigned(r.owner.resource),unsigned(r.owner.node),unsigned(r.queue));_blocker=message.c_str();_trace.record(r.owner,"unsupported_consumer",r.queue,r.second);}
  else _blocker="effect playback calibration unresolved";
  if((_events.awaiting(r.token)&&!(r.kind==kRequestCoordinatorControl&&r.first==2&&!_returnTitlePhase))||_scene.transferBusy())break;
 }
 if(!_runtime->resumeCompleted(error)&&!error.empty())_blocker=error;if(_runtime->consumeMenuSelectionReset())_rootSelection=0;if(_delay.finished()&&!_events.awaiting(_delayToken)){_delay.reset();_delayToken=EventToken();}return true;
}
bool SnatcherEngine::updateQueuedPresentation(const RuntimeRequest& request,std::string& error){
 error.clear();if(!(_presentationToken==request.token)||request.kind!=kRequestPresentation){error="presentation queue owner mismatch";return false;}
 if(!_presentationLoadPhase){
  bool ready=false;if(!_scene.advanceResourcePreparation(_presentationPreparation,ready,error))return false;if(!ready)return true;
  if(_presentationNeedsLoad){
   // Queue11 first requests MSCPAUSEON and returns. It tests readiness on
   // its next visit, including when the transport was already idle.
   _cd.pauseForResourceRead();_presentationLoadPhase=1;_trace.record(request.owner,"presentation_read_pause",request.first>>8);return true;
  }
 }
 if(_presentationLoadPhase==1&&_cd.active()&&!_cd.paused())return true;
 if(_presentationLoadPhase<2){
  // A data read replaces the old CD-DA owner. Keep the consumed history, but
  // do not leave a paused music token active after its drive owner changes.
  if(_presentationNeedsLoad&&!_cd.releaseForResourceRead(error))return false;
  _lastTickMillis=_system->getMillis();if(!_resources.readResource(request.first>>8,_presentationData,error))return false;_lastTickMillis=_system->getMillis();
  if(_presentationNeedsLoad){
   // Native immutable-file completion is owned now; source queue11 observes
   // completion on a subsequent visit before tail-dispatching queue12.
   // No optical-drive duration is inferred from the native host's I/O time.
   _presentationLoadPhase=2;_trace.record(request.owner,"presentation_read_completed",request.first>>8);return true;
  }
 }
 _sceneAssetsReady=false;if(!_scene.install(uint16_t(request.first>>8),_presentationData,request.first&255,error))return false;
 _sceneAssets=_scene.assets();_sceneAssetsReady=true;
 _trace.record(request.owner,"scene_assets_staged",uint32_t(_sceneAssets.transfers.size()),uint32_t(_sceneAssets.actors.size()));
 if(!_events.complete(request.token,false,error))return false;
 _trace.record(request.owner,"scene_dispatch_completed",request.first);_presentationToken=EventToken();_presentationLoadPhase=0;_presentationData.clear();_blocker.clear();return true;
}
void SnatcherEngine::drawNative(){
 ++_renderFrames;
 SceneImage image;std::string imageError;_sceneVisible=false;
 bool mouseAimDisplay=!_startupActive&&_shooting.active&&_shooting.absoluteAim;
 if((_startupActive&&_startup.render(_scene,kSceneRGB888,image,imageError))||(!_startupActive&&_runtime&&_scene.render(kSceneRGB888,image,imageError,mouseAimDisplay?22u:64u))){
  if(!_startupActive&&!_sceneAssetsReady)++_renderPendingSceneFrames;
  // One indexed palette cannot represent the source's ordered lower-row
  // publications. Submit actual software RGB565 pixels in the same declared
  // normal-intensity domain as the original reference, never first-seen CLUT.
  const Graphics::PixelFormat format(2,5,6,5,0,11,5,0,0);
  if(_canvasWidth!=image.width||_canvasHeight!=image.height||_system->getWidth()!=image.width||_system->getHeight()!=image.height||_system->getScreenFormat()!=format){initGraphics(int(image.width),int(image.height),&format);_canvasWidth=unsigned(image.width);_canvasHeight=unsigned(image.height);_framePublication.invalidate();}
  if(_system->getScreenFormat()!=format){_renderError="Native scene output requires an RGB565 backend";return;}
  _framePublication.prepare(unsigned(image.width),unsigned(image.height));
  Graphics::Surface screen;screen.init(int(image.width),int(image.height),int(image.width)*2,_framePublication.pixels(),format);for(size_t y=0;y<image.height;++y)for(size_t x=0;x<image.width;++x){size_t at=(y*image.width+x)*3;screen.setPixel(int(x),int(y),format.RGBToColor(image.rgb[at],image.rgb[at+1],image.rgb[at+2]));}
  drawMouse(screen);if(_framePublication.publish())_system->copyRectToScreen(screen.getPixels(),screen.pitch,0,0,int(image.width),int(image.height));_sceneVisible=true;_renderError.clear();return;
 }
 if(!imageError.empty())_renderError=imageError;
 // Asset installation is not display ownership: Main retains its published
 // frame across Sub module preparation. Never replace it with developer text
 // or change video mode while the normal source renderer is unavailable.
 if(!_fixture){++_renderRetainedFrames;return;}
 ++_renderDiagnosticFrames;
 _framePublication.invalidate();
 if(_canvasWidth!=640||_canvasHeight!=240||_system->getScreenFormat().bytesPerPixel!=1){initGraphics(640,240);_canvasWidth=640;_canvasHeight=240;}const byte colors[]={12,16,24,230,235,240};_system->getPaletteManager()->setPalette(colors,0,2);
 std::vector<uint8_t> pixels=_pixels;
 if(_runtime&&_runtime->status()==kRuntimeChoices){pixels.assign(640*240,0);std::vector<uint8_t> data;std::string error;if(_resources.readResource(_runtime->cursor().resource,data,error)){const std::vector<uint16_t>& choices=_runtime->control().availableChoices();for(size_t i=0;i<choices.size();++i){TextReader reader;if(!reader.start(data.data(),data.size(),size_t(0x3800)+choices[i],error))continue;std::vector<TextToken> tokens;if(!reader.read(100,tokens,error))continue;int x=20+int(i/15)*310,y=38+int(i%15)*10;for(const TextToken& token:tokens){if(token.kind!=kTextGlyph)continue;FontGlyph glyph;if(!_font.glyph(token.value,glyph,error)||x+8>int(i/15+1)*310)break;drawGlyph(_font,token.value,x,y,1,pixels,640,240,error);x+=glyph.advance;}}}}
 Graphics::Surface screen;screen.create(640,240,Graphics::PixelFormat::createFormatCLUT8());memcpy(screen.getPixels(),pixels.data(),pixels.size());const Graphics::Font *font=FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
 font->drawString(&screen,_fixture?"Controlled original-resource fixture; scene fidelity unverified":"Snatcher native runtime; gameplay reconstruction incomplete",8,8,624,1);
 if(_runtime&&_runtime->status()==kRuntimeChoices){Common::String info=Common::String::format("Original menu: %u choices; selected %u. Enter selects, Backspace cancels.",unsigned(_runtime->control().availableChoices().size()),_selection+1);font->drawString(&screen,info,8,200,624,1);}
 else if(_dialogueReady)font->drawString(&screen,"Original glyph mask inspection. Enter acknowledges displayed text.",8,200,624,1);
 if(!_blocker.empty())font->drawString(&screen,_blocker.c_str(),8,220,624,1);else if(_runtime&&!_runtime->diagnostic().empty())font->drawString(&screen,_runtime->diagnostic().c_str(),8,220,624,1);
 _system->copyRectToScreen(screen.getPixels(),screen.pitch,0,0,640,240);screen.free();
}
Common::JSONValue *SnatcherEngine::testCLICommand(Common::JSONValue& request){
 if(!CETestCLI::active()||!_runtime)return CETestCLI::unsupported();Common::String action=CETestCLI::stringField(request,"action");std::string error;
 const Common::String command=CETestCLI::stringField(request,"cmd");
 if(action=="copy_to_free_slot_probe"){if(!getMetaEngine()->copySaveFileToFreeSlot(ConfMan.getActiveDomainName().c_str(),90))return CETestCLI::error("copy_to_free_slot_failed");return CETestCLI::ok();}
 if(action=="autosave_now"){saveAutosaveIfEnabled();return CETestCLI::ok();}
 if(action=="save_policy"){
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);Common::JSONArray slots;
  for(int slot=0;slot<=5;++slot){SaveStateDescriptor desc=getMetaEngine()->querySaveMetaInfos(ConfMan.getActiveDomainName().c_str(),slot);Common::JSONObject item;item["slot"]=new Common::JSONValue((long long)slot);item["manual_allowed"]=new Common::JSONValue(getMetaEngine()->isSaveSlotAllowed(ConfMan.getActiveDomainName().c_str(),slot));item["write_protected"]=new Common::JSONValue(desc.getWriteProtectedFlag());item["label"]=new Common::JSONValue(desc.getDescription());slots.push_back(new Common::JSONValue(item));}
  result["thumbnail_supported"]=new Common::JSONValue(getMetaEngine()->hasFeature(MetaEngine::kSavesSupportThumbnail));
  result["gallery_selected"]=new Common::JSONValue(GUI::getRequestedSaveLoadDialog(getMetaEngine(),ConfMan.getActiveDomainName().c_str())==GUI::kSaveLoadDialogGrid);
  result["slots"]=new Common::JSONValue(slots);return new Common::JSONValue(result);
 }
 if(action=="resource_transport_probe"){
  Common::JSONArray tracks;
  for(unsigned track=2;track<=21;++track){
   uint32_t frames=0;if(!_resources.cdAudioFrameCount(track,frames,error)||frames<10)return CETestCLI::error(error.empty()?"Transport track is too short":error.c_str());
   Common::JSONObject item;item["track"]=new Common::JSONValue((long long)track);item["frames"]=new Common::JSONValue((long long)frames);
   for(unsigned endpoint=0;endpoint<2;++endpoint){std::vector<uint8_t> bytes;std::vector<int16_t> pcm;
    if(!_resources.readCdAudioFrames(track,endpoint?frames-10:0,10,bytes,error)||!decodeCdAudioFrames(bytes.data(),bytes.size(),pcm,error))return CETestCLI::error(error.c_str());
    Common::MemoryReadStream stream(bytes.data(),uint32(bytes.size()));item[endpoint?"last_md5":"first_md5"]=new Common::JSONValue(Common::computeStreamMD5AsString(stream));
    item[endpoint?"last_samples":"first_samples"]=new Common::JSONValue((long long)pcm.size());
   }
   tracks.push_back(new Common::JSONValue(item));
  }
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["chd"]=new Common::JSONValue(_resources.isChd());result["tracks"]=new Common::JSONValue(tracks);return new Common::JSONValue(result);
 }
 if(action=="host_stall_probe"){
  const int milliseconds=CETestCLI::intField(request,"milliseconds",0);
  if(milliseconds!=1000&&milliseconds!=60000)return CETestCLI::error("Host stall probe requires a fixed 1s or 60s interval");
  _lastTickMillis=_system->getMillis()-uint32_t(milliseconds);return CETestCLI::ok();
 }
 if(action=="decensor_lifetime_probe"){
  // Fixed local fixture only; no arbitrary file/resource names or live-engine
  // mode mutation. Normal option changes still require an engine restart.
  Common::File patch;if(!patch.open("snatcher-decensor.dat")||patch.size()<=0||patch.size()>1024*1024)return CETestCLI::error("Missing bounded decensor sidecar");
  std::vector<uint8_t> sidecar(size_t(patch.size()));if(patch.read(sidecar.data(),uint32(sidecar.size()))!=sidecar.size()||patch.err())return CETestCLI::error("Sidecar read failed");
  GameResource owner;if(!owner.open(Common::FSNode(ConfMan.getPath("path")),error))return CETestCLI::error(error.c_str());
  const char* names[]={"DATA_F0.BIN","DATA_G0.BIN","DATA_H09.BIN","DATA_P0.BIN","SP25.BIN"};
  std::vector<std::vector<uint8_t> > originals(5),expected(5),held;
  for(unsigned i=0;i<5;++i)if(!owner.readFile(names[i],originals[i],error))return CETestCLI::error(error.c_str());
  std::vector<uint8_t> unaffected;if(!owner.readFile("DATA_J0.BIN",unaffected,error))return CETestCLI::error(error.c_str());
  if(owner.decensorEnabled()||owner.decensorDiagnostics().bytes)return CETestCLI::error("Fresh resource owner inherited patches");
  for(unsigned cycle=0;cycle<8;++cycle){
   if(!owner.configureDecensor(sidecar,error))return CETestCLI::error(error.c_str());
   for(unsigned i=0;i<5;++i){std::vector<uint8_t> bytes;if(!owner.readFile(names[i],bytes,error))return CETestCLI::error(error.c_str());if(!cycle)expected[i]=bytes;else if(bytes!=expected[i])return CETestCLI::error("Repeated configuration compounded patch");}
   auto before=owner.decensorDiagnostics();
   for(unsigned repeat=0;repeat<64;++repeat)for(unsigned i=0;i<5;++i){std::vector<uint8_t> bytes;if(!owner.readFile(names[i],bytes,error)||bytes!=expected[i])return CETestCLI::error("Cached resource changed");if(!bytes.empty())bytes[0]^=1;}
   auto after=owner.decensorDiagnostics();if(after.applications!=before.applications||after.entries!=5||after.bytes!=before.bytes||after.cacheReads-before.cacheReads!=320)return CETestCLI::error("Resource read reapplied or grew patches");
   std::vector<uint8_t> bytes;if(!owner.readFile("DATA_J0.BIN",bytes,error)||bytes!=unaffected)return CETestCLI::error("Unrelated resource changed");
   auto broken=sidecar;broken[0]^=1;if(owner.configureDecensor(broken,error)||!owner.decensorEnabled())return CETestCLI::error("Invalid reconfiguration was not atomic");
  }
  held=expected;const auto stats=owner.decensorDiagnostics();owner.close();
  if(owner.decensorEnabled()||owner.decensorDiagnostics().bytes||held!=expected)return CETestCLI::error("Closed owner retained or invalidated resources");
  if(!owner.open(Common::FSNode(ConfMan.getPath("path")),error))return CETestCLI::error(error.c_str());
  for(unsigned i=0;i<5;++i){std::vector<uint8_t> bytes;if(!owner.readFile(names[i],bytes,error)||bytes!=originals[i])return CETestCLI::error("Reopened owner retained patches");}
  if(owner.decensorEnabled()||owner.decensorDiagnostics().bytes)return CETestCLI::error("Off mode has patch cache");
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["configurations"]=new Common::JSONValue((long long)stats.configurations);result["applications"]=new Common::JSONValue((long long)stats.applications);result["cache_reads"]=new Common::JSONValue((long long)stats.cacheReads);result["cache_bytes"]=new Common::JSONValue((long long)stats.bytes);return new Common::JSONValue(result);
 }
 if(action=="decensor_resource_inventory"){
  Common::JSONArray files;
  for(const DiscFile& entry:_resources.files()){
   std::vector<uint8_t> bytes;if(!_resources.readFile(entry.name,bytes,error))return CETestCLI::error(error.c_str());
   Common::MemoryReadStream stream(bytes.data(),uint32(bytes.size()));Common::JSONObject file;file["name"]=new Common::JSONValue(entry.name.c_str());file["bytes"]=new Common::JSONValue((long long)bytes.size());file["md5"]=new Common::JSONValue(Common::computeStreamMD5AsString(stream));files.push_back(new Common::JSONValue(file));
  }
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["enabled"]=new Common::JSONValue(ConfMan.hasKey("snatcher_decensor")&&ConfMan.getBool("snatcher_decensor"));result["files"]=new Common::JSONValue(files);return new Common::JSONValue(result);
 }
 if(action=="lifecycle_recreate_engine"){
  // Exercise normal engine destruction/chaining, never replace live state in
  // place. This diagnostic is forbidden in ordinary or tracked play sessions.
  const Common::String target=ConfMan.getActiveDomainName();
  const int expected=CETestCLI::intField(request,"instance_serial",-1);
  if(command!="input"||CETestCLI::stringField(request,"kind")!="logical"||!_ready||isPaused()||_fixture||_presentationFixture||!_testInstanceSerial||expected<1||uint64(expected)!=_testInstanceSerial||_testRecreationQueued||target.empty()||!ConfMan.hasGameDomain(target)||ConfMan.get("engineid")!="snatcher"||ConfMan.get("gameid")!="snatcher"||!ChainedGamesMan.empty())return CETestCLI::error("lifecycle_recreation_not_owned");
  if(ConfMan.getBool("confirm_exit")||!ConfMan.hasKey("ce_playthrough_tracking",target)||ConfMan.getBool("ce_playthrough_tracking",target)||Common::CEAchievements().hasActivePlaythroughInProgress()||(ConfMan.hasKey("save_slot")&&ConfMan.getInt("save_slot")>=0))return CETestCLI::error("lifecycle_recreation_requires_untracked_profile");
  ChainedGamesMan.push(target,-1);_testRecreationQueued=true;
  Common::Event event;event.type=Common::EVENT_RETURN_TO_LAUNCHER;_eventMan->pushEvent(event);
  Common::JSONObject response;response["ok"]=new Common::JSONValue(true);response["from_instance_serial"]=new Common::JSONValue((long long)_testInstanceSerial);response["target"]=new Common::JSONValue(target);response["save_slot"]=new Common::JSONValue((long long)-1);response["normal_return_to_launcher_queued"]=new Common::JSONValue(true);return new Common::JSONValue(response);
 }

 // Capture the same save/load boundary without allowing a worker between the
 // snapshot and RGB read. This test-only export never changes story or time.
 auto exportBoundary=[&](Common::JSONObject& response)->bool{
  int slot=CETestCLI::intField(request,"capture_rgb_slot",-1);if(slot<0)return true;
  if(slot>99){error="invalid_boundary_rgb_slot";return false;}
  SceneImage image;if(!_scene.render(kSceneRGB888,image,error))return false;
  Common::String filename=Common::String::format("%s-snatcher-boundary-%02d.rgb",ConfMan.getActiveDomainName().c_str(),slot);
  Common::OutSaveFile* file=_system->getSavefileManager()->openForSaving(filename,false);if(!file){error="boundary_rgb_export_open";return false;}
  uint32 written=file->write(image.rgb.data(),uint32(image.rgb.size()));file->finalize();bool good=!file->err()&&written==image.rgb.size();delete file;
  if(!good){error="boundary_rgb_export_write";return false;}response["boundary_rgb"]=new Common::JSONValue(filename);return true;
 };
 if(action=="save_owner_contracts")return saveOwnerContracts();
 if(action=="save_owner_contracts")return saveOwnerContracts();
 if(action=="save_anywhere_probe")return saveAnywhereProbe(CETestCLI::intField(request,"slot",80));
 if(command=="save"){int slot=CETestCLI::intField(request,"slot",0);Common::Error result=saveGameState(slot,CETestCLI::stringField(request,"description"));if(result.getCode()!=Common::kNoError)return CETestCLI::error(_saveError.c_str());Common::JSONObject response;response["ok"]=new Common::JSONValue(true);if(!exportBoundary(response))return CETestCLI::error(error.c_str());response["checkpoint"]=testCLIState();return new Common::JSONValue(response);}
 if(command=="load"){int slot=CETestCLI::intField(request,"slot",0);Common::Error result=loadGameState(slot);if(result.getCode()!=Common::kNoError)return CETestCLI::error(_saveError.c_str());Common::JSONObject response;response["ok"]=new Common::JSONValue(true);if(!exportBoundary(response))return CETestCLI::error(error.c_str());response["checkpoint"]=testCLIState();return new Common::JSONValue(response);}
 if(action=="credits_gate_rgb_export"){
  int slot=CETestCLI::intField(request,"slot",81);if(slot<0||slot>99||_creditsGateRgb.empty())return CETestCLI::error("credits_gate_capture_unavailable");Common::String filename=Common::String::format("%s-snatcher-credits-gate-%02d.rgb",ConfMan.getActiveDomainName().c_str(),slot);Common::OutSaveFile* file=_system->getSavefileManager()->openForSaving(filename,false);if(!file)return CETestCLI::error("credits_gate_export_open");uint32 written=file->write(_creditsGateRgb.data(),uint32(_creditsGateRgb.size()));file->finalize();bool good=!file->err()&&written==_creditsGateRgb.size();delete file;if(!good)return CETestCLI::error("credits_gate_export_write");Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["filename"]=new Common::JSONValue(filename);result["worker_tick"]=new Common::JSONValue((long long)_creditsGateTick);return new Common::JSONValue(result);
 }
 if(action=="trace"){_trace.enable(true);return CETestCLI::ok();}
 if(action=="visual_observer_arm"){
  int first=CETestCLI::intField(request,"mode_a",28),second=CETestCLI::intField(request,"mode_b",29),delay=CETestCLI::intField(request,"delay",1),bound=CETestCLI::intField(request,"max_workers",18000);
  if(!_ready||_startupActive||_fixture||_presentationFixture||first<0||first>255||second<0||second>255||first==second||delay!=1||bound<1||bound>18000)return CETestCLI::error("invalid_visual_observer_arm");
  resetVisualObserver();_visualObserver.active=true;_visualObserver.serial=_visualObserver.startSerial=_clock.snapshot().ticks;_visualObserver.maxWorkers=unsigned(bound);_visualObserver.captures[0].targetMode=uint8_t(first);_visualObserver.captures[1].targetMode=uint8_t(second);
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["start_worker_tick"]=new Common::JSONValue((long long)_visualObserver.startSerial);result["delay"]=new Common::JSONValue((long long)1);result["max_workers"]=new Common::JSONValue((long long)bound);return new Common::JSONValue(result);
 }
 if(action=="visual_observer_export"){
  int slot=CETestCLI::intField(request,"slot",81);if(slot<0||slot>98||!_visualObserver.done||!_visualObserver.error.empty())return CETestCLI::error("visual_observer_not_complete");
  Common::JSONArray captures;unsigned ordinal=0;for(const auto& capture:_visualObserver.captures){
   if(!capture.captured||capture.rgb.empty())return CETestCLI::error("visual_observer_capture_missing");
   Common::String filename=Common::String::format("%s-snatcher-observer-%02d.rgb",ConfMan.getActiveDomainName().c_str(),slot+ordinal++);Common::OutSaveFile* file=_system->getSavefileManager()->openForSaving(filename,false);if(!file)return CETestCLI::error("visual_observer_export_open");
   uint32 written=file->write(capture.rgb.data(),uint32(capture.rgb.size()));file->finalize();bool good=!file->err()&&written==capture.rgb.size();delete file;if(!good)return CETestCLI::error("visual_observer_export_write");captures.push_back(visualCaptureMetadata(capture,filename.c_str()));
  }
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["captures"]=new Common::JSONValue(captures);result["start_worker_tick"]=new Common::JSONValue((long long)_visualObserver.startSerial);result["workers"]=new Common::JSONValue((long long)_visualObserver.workers);result["delay"]=new Common::JSONValue((long long)1);result["encoding"]=new Common::JSONValue("RGB888 in declared RGB565 reference domain; buffered actual worker boundary, scene copy render");return new Common::JSONValue(result);
 }
 // Passive test inspection: resolve a copy of the actual shared staging owner.
 // No coordinator, input, publication or runtime state advances here.
 if(action=="entry_transition_inspect"){
  auto hexPacket=[](const std::vector<uint8_t>& bytes){Common::String text;static const char digits[]="0123456789abcdef";for(uint8_t value:bytes){text+=digits[value>>4];text+=digits[value&15];}return text;};
  TextEntry entry=_textEntry;Common::JSONObject result;result["ok"]=new Common::JSONValue(true);
  result["established"]=new Common::JSONValue(entry.graphicsReady());
  result["graphics_busy"]=new Common::JSONValue(entry.graphicsBusy());result["key_wait"]=new Common::JSONValue(entry.awaitingKey());
  result["completion_pending"]=new Common::JSONValue(_entryPending);result["close_view_pending"]=new Common::JSONValue((long long)_entryCloseView);
  result["mouse_pending"]=new Common::JSONValue((long long)_entryMousePressed);result["buffer_hex"]=new Common::JSONValue(hexPacket(std::vector<uint8_t>(entry.buffer().begin(),entry.buffer().end())));
  if(entry.fontCanvas().size()==8192&&entry.fontNames().size()==512){
   if(!_scene.prepareTextEntry(entry,error))return CETestCLI::error(error.c_str());std::vector<uint8_t> patterns,names;initializeOriginalFontCanvas(patterns,names);
   result["canvas_clean"]=new Common::JSONValue(entry.fontCanvas()==patterns);result["names_clean"]=new Common::JSONValue(entry.fontNames()==names);
   result["canvas_hex"]=new Common::JSONValue(hexPacket(entry.fontCanvas()));result["names_hex"]=new Common::JSONValue(hexPacket(entry.fontNames()));
  }
  return new Common::JSONValue(result);
 }
 if(action=="scene_actor_inspect"){
  int index=CETestCLI::intField(request,"index",-1);SceneActorView view;if(index<0||index>=64||!_scene.inspectActor(unsigned(index),view,error))return CETestCLI::error("invalid_native_actor");const auto* control=_scene.actor(unsigned(index));Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["owned"]=new Common::JSONValue(view.owned);result["active"]=new Common::JSONValue(view.active);result["cleared"]=new Common::JSONValue(view.cleared);result["phase_known"]=new Common::JSONValue(view.phaseKnown);result["markers_known"]=new Common::JSONValue(view.markersKnown);
  result["resource"]=new Common::JSONValue((long long)view.resource);result["sequence"]=new Common::JSONValue((long long)view.sequence);result["record"]=new Common::JSONValue((long long)view.record);result["remaining"]=new Common::JSONValue((long long)view.remaining);result["flags"]=new Common::JSONValue((long long)view.flags);result["completion"]=new Common::JSONValue((long long)view.completionFlags);result["phase"]=new Common::JSONValue((long long)view.phase);result["x"]=new Common::JSONValue((long long)view.x);result["y"]=new Common::JSONValue((long long)view.y);result["parent"]=new Common::JSONValue((long long)control->parent);result["head"]=new Common::JSONValue((long long)control->head);result["next"]=new Common::JSONValue((long long)control->next);return new Common::JSONValue(result);
 }
 if(action=="scene_rgb_export"){
  int slot=CETestCLI::intField(request,"slot",0);if(slot<0||slot>99||_startupActive||!_sceneAssetsReady)return CETestCLI::error("native_scene_not_ready");SceneRuntime snapshot=_scene;SceneImage image;if(!snapshot.render(kSceneReferenceRGB565,image,error))return CETestCLI::error(error.c_str());Common::String filename=Common::String::format("%s-snatcher-scene-%02d.rgb",ConfMan.getActiveDomainName().c_str(),slot);Common::OutSaveFile* file=_system->getSavefileManager()->openForSaving(filename,false);if(!file)return CETestCLI::error("native_scene_export_open");file->write(image.rgb.data(),uint32(image.rgb.size()));file->finalize();bool good=!file->err();delete file;if(!good)return CETestCLI::error("native_scene_export_write");Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["filename"]=new Common::JSONValue(filename);result["width"]=new Common::JSONValue((long long)image.width);result["height"]=new Common::JSONValue((long long)image.height);result["encoding"]=new Common::JSONValue("RGB888 in previously declared RGB565 reference domain; actual scene copy, no worker advance or backend overlays");return new Common::JSONValue(result);
 }
 if(action=="fm_allocation_probe"){
  PauseToken diagnosticPause=pauseEngine();struct ClockBoundary{uint32_t& stamp;OSystem* system;~ClockBoundary(){stamp=system->getMillis();}} boundary={_lastTickMillis,_system};
  std::vector<uint8_t> boot,fm,fmAllocation,index,drums,sub;if(!_resources.readBoot(boot,error)||boot.size()<512||!_resources.readFile("FMWR_1.BIN",fm,error)||!_resources.readFileAllocation("FMWR_1.BIN",fmAllocation,error)||!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMDRMDT.BIN",drums,error)||!_resources.readFile("SUBCODE.BIN",sub,error))return CETestCLI::error(error.c_str());
  if(fmAllocation.size()<=fm.size())return CETestCLI::error("allocation_probe_requires_actual_tail");std::vector<uint8_t> main(boot.begin()+512,boot.end());Audio::MixerImpl measurement(44100,true);measurement.setReady(true);CeFmAudioOwner probe(measurement,false);probe.setPaused(true);
  int allocationCommand=CETestCLI::intField(request,"command",122);if(allocationCommand<1||allocationCommand>=241||!probe.load(main,fm,index,drums,error,&sub,&fmAllocation)||!probe.start(uint16_t(allocationCommand),error))return CETestCLI::error(error.empty()?"allocation_probe_command":error.c_str());
  const int historyEvents=CETestCLI::intField(request,"history_events",0);if(historyEvents!=0&&historyEvents!=33000)return CETestCLI::error("allocation_probe_history_domain");for(int i=0;i<historyEvents;++i)probe.setMono((i&1)==0);
  std::vector<int16_t> prefix,expected,actual;if(!probe.renderOffline(735,prefix,error))return CETestCLI::error(error.c_str());std::vector<uint8_t> state;if(!probe.snapshotState(state,error)||(state[4]<4||state[4]>6)||!probe.renderOffline(44101,expected,error)||!probe.restoreState(state,error)||!probe.renderOffline(44101,actual,error)||actual!=expected)return CETestCLI::error(error.empty()?"allocation_probe_continuation":error.c_str());
  auto foreignAllocation=fmAllocation;foreignAllocation.back()^=1;CeFmAudioOwner foreign(measurement,false);foreign.setPaused(true);if(!foreign.load(main,fm,index,drums,error,&sub,&foreignAllocation))return CETestCLI::error(error.c_str());std::vector<uint8_t> before,after;if(!foreign.snapshotState(before,error)||foreign.restoreState(state,error)||!foreign.snapshotState(after,error)||before!=after)return CETestCLI::error("allocation_probe_foreign_tail_mutation");
  if(historyEvents){std::vector<uint8_t> legacy;if(probe.snapshotLegacyState(legacy,error))return CETestCLI::error("allocation_probe_legacy_event_limit");for(unsigned i=0;i<65536;++i)probe.setMono((i&1)==0);std::vector<uint8_t> rejected(1,99);if(probe.canSnapshotState(error)||probe.snapshotState(rejected,error)||rejected!=std::vector<uint8_t>(1,99))return CETestCLI::error("allocation_probe_event_overflow_guard");}
  uint64_t nonzero=0;for(int16_t value:expected)if(value)++nonzero;Common::JSONObject result;result["history_events"]=new Common::JSONValue((long long)historyEvents);result["ok"]=new Common::JSONValue(true);result["command"]=new Common::JSONValue((long long)allocationCommand);result["logical_bytes"]=new Common::JSONValue((long long)fm.size());result["actual_allocation_bytes"]=new Common::JSONValue((long long)fmAllocation.size());result["schema"]=new Common::JSONValue((long long)4);result["continuation_frames"]=new Common::JSONValue((long long)44101);result["nonzero_samples"]=new Common::JSONValue((long long)nonzero);result["continuation_exact"]=new Common::JSONValue(true);result["foreign_tail_rejected_transactionally"]=new Common::JSONValue(true);result["controlled_native_owner_fixture"]=new Common::JSONValue(true);result["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(result);
 }
 if(action=="input_compatibility_probe"){
  Common::String hex=CETestCLI::stringField(request,"input_hex");int version=CETestCLI::intField(request,"version",0),flag=CETestCLI::intField(request,"legacy_flag",0),x=CETestCLI::intField(request,"legacy_x",0),y=CETestCLI::intField(request,"legacy_y",0);
  const int chunkOffset=CETestCLI::intField(request,"chunk_offset",-1);
  if((hex.empty()&&(chunkOffset>=0||_compatibilityProbeInput.empty()))||hex.size()>2*1024*1024||(hex.size()&1)||chunkOffset< -1||version<1||version>11||flag<0||flag>1||x<0||x>255||y<0||y>255)return CETestCLI::error("input_compatibility_domain");
  auto nibble=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};NativeSaveSection source(kSaveInput);source.version=uint16_t(version);source.required=true;
  for(unsigned i=0;i<hex.size();i+=2){int hi=nibble(hex[i]),lo=nibble(hex[i+1]);if(hi<0||lo<0)return CETestCLI::error("input_compatibility_hex");source.bytes.push_back(uint8_t(hi*16+lo));}
  if(chunkOffset>=0){if(!chunkOffset)_compatibilityProbeInput.clear();if(size_t(chunkOffset)!=_compatibilityProbeInput.size()||source.bytes.size()>1024*1024-_compatibilityProbeInput.size())return CETestCLI::error("input_compatibility_chunk");_compatibilityProbeInput.insert(_compatibilityProbeInput.end(),source.bytes.begin(),source.bytes.end());Common::JSONObject received;received["ok"]=new Common::JSONValue(true);received["received_bytes"]=new Common::JSONValue((long long)_compatibilityProbeInput.size());return new Common::JSONValue(received);}
  if(hex.empty())source.bytes=_compatibilityProbeInput;
  LegacyInputHeader header;header.deviceFlag=flag!=0;header.anchorX=uint8_t(x);header.anchorY=uint8_t(y);NativeSaveSection migrated(kSaveInput);if(!nativeInputCompatibilityProbe(source,header,migrated,error))return CETestCLI::error(error.c_str());
  NativeSaveSection sentinel(kSaveInput);sentinel.bytes.assign(1,99);NativeSaveSection bad=source;bad.bytes.pop_back();if(nativeInputCompatibilityProbe(bad,header,sentinel,error)||sentinel.bytes!=std::vector<uint8_t>(1,99))return CETestCLI::error("input_compatibility_transaction");
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["version"]=new Common::JSONValue((long long)migrated.version);result["original_bytes"]=new Common::JSONValue((long long)source.bytes.size());result["migrated_bytes"]=new Common::JSONValue((long long)migrated.bytes.size());result["unrelated_state_exact"]=new Common::JSONValue(true);result["opaque_roundtrip_exact"]=new Common::JSONValue(true);result["truncated_transactional"]=new Common::JSONValue(true);result["engine_state_applied"]=new Common::JSONValue(false);return new Common::JSONValue(result);
 }
 if(action=="save_capacity"){
  auto* manager=_system->getSavefileManager();if(!manager)return CETestCLI::error("save_manager_unavailable");
  const auto before=manager->getError().getCode();const Common::String description=manager->getErrorDesc();
  uint64 unsupported=UINT64_MAX,available=0;
  const bool defaultKnown=manager->Common::SaveFileManager::querySavefileFreeSpace(unsupported);
  const bool known=manager->querySavefileFreeSpace(available);
  Common::JSONObject response;response["ok"]=new Common::JSONValue(true);response["known"]=new Common::JSONValue(known);response["available_bytes"]=new Common::JSONValue(Common::String::format("%llu",static_cast<unsigned long long>(available)));
  response["default_unknown_zero"]=new Common::JSONValue(!defaultKnown&&!unsupported);response["error_unchanged"]=new Common::JSONValue(before==manager->getError().getCode()&&description==manager->getErrorDesc());return new Common::JSONValue(response);
 }
 if(action=="input11_rejection_probe"){
  const int slot=CETestCLI::intField(request,"slot",-1);if(slot<0||slot>99||!_runtime)return CETestCLI::error("input11_rejection_domain");PauseToken diagnosticPause=pauseEngine();struct ClockBoundary {uint32_t& stamp;OSystem* system;~ClockBoundary(){stamp=system->getMillis();}} boundary={_lastTickMillis,_system};
  const auto packed=_runtime->packed();const auto cursor=_runtime->cursor();const auto clock=_clock.snapshot();const auto random=_nativeRandom;const auto activity=_activity;ResidentPcmStagingSnapshot staging;if(_residentStaging&&!_residentStaging->snapshot(staging,error))return CETestCLI::error(error.c_str());std::vector<uint8_t> fm;if(!_fm.snapshotState(fm,error))return CETestCLI::error(error.c_str());
  const Common::Error rejected=loadGameState(slot);std::vector<uint8_t> afterFm;ResidentPcmStagingSnapshot afterStaging;uint8_t beforeActivity=0,afterActivity=0;
  if(rejected.getCode()==Common::kNoError||!_fm.snapshotState(afterFm,error)||fm!=afterFm||_runtime->packed()!=packed||!(_runtime->cursor()==cursor)||_clock.snapshot().ticks!=clock.ticks||_clock.snapshot().fraction!=clock.fraction||_nativeRandom.state.first!=random.state.first||_nativeRandom.state.second!=random.state.second||_nativeRandom.draws!=random.draws||!activity.snapshot(beforeActivity,error)||!_activity.snapshot(afterActivity,error)||beforeActivity!=afterActivity)return CETestCLI::error("input11_rejection_changed_live_owner");
  if(_residentStaging&&(!_residentStaging->snapshot(afterStaging,error)||staging.knownBytes!=afterStaging.knownBytes||staging.bank!=afterStaging.bank||staging.generation!=afterStaging.generation||staging.writerKind!=afterStaging.writerKind||staging.writerId!=afterStaging.writerId))return CETestCLI::error("input11_rejection_changed_staging");
  Common::JSONObject out;out["ok"]=new Common::JSONValue(true);out["rejected"]=new Common::JSONValue(true);out["owners_unchanged"]=new Common::JSONValue(true);out["clock_and_random_unchanged"]=new Common::JSONValue(true);out["fm_packet_unchanged"]=new Common::JSONValue(true);out["within_one_paused_native_boundary"]=new Common::JSONValue(true);out["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(out);
 }
 if(action=="pcm_context_export"||action=="pcm_context_import"){
  PauseToken diagnosticPause=pauseEngine();struct ClockBoundary {uint32_t& stamp;OSystem* system;~ClockBoundary(){stamp=system->getMillis();}} boundary={_lastTickMillis,_system};
  const bool residentContext=CETestCLI::intField(request,"resident_context",0)==1;std::vector<uint8_t> bytes(10240,128);std::fill(bytes.begin(),bytes.begin()+10000,144);std::fill(bytes.begin()+10000,bytes.begin()+10004,255);auto staging=std::make_shared<ResidentPcmStaging>(106496);PcmIdleContext context;std::vector<uint8_t> packet;
  if(action=="pcm_context_export"&&residentContext){
   if(!staging->replacePrefix(30,bytes.data(),bytes.size(),error))return CETestCLI::error(error.c_str());Audio::MixerImpl idleMixer(44100,true);idleMixer.setReady(true);CeAudioBackend idleBackend(idleMixer);AudioController idleController(idleBackend);AudioActivity idleActivity;idleActivity.clearForSoundReset();PcmStreamOwner idleSource(idleController,idleActivity);if(!idleSource.enterResidentMode(error,staging)||!idleSource.captureIdleContext(context,error)||!encodePcmIdleContext(context,packet,error))return CETestCLI::error(error.c_str());
  }else if(action=="pcm_context_export"){
   Audio::MixerImpl sourceMixer(44100,true);sourceMixer.setReady(true);CeAudioBackend backend(sourceMixer);AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner source(controller,activity);auto feed=std::make_shared<PcmStreamFeed>();PcmCatalogEntry entry={};entry.id=36;entry.mode=2;entry.priority=4;entry.level=32;entry.pan=17;entry.length=bytes.size();PreparedPcmStream stream;AudioToken token;
   if(!preparePcmStream(entry,bytes.data(),bytes.size(),sourcePcmStreamProfile(),false,stream,error)||!feed->start(staging,36,bytes.data(),bytes.size(),stream.pcm,10000,16276,error)||!source.start(stream,AudioOwner(),1,token,error,feed))return CETestCLI::error(error.c_str());int16_t block[512];
   for(unsigned visit=0;source.active()&&visit<300;++visit){if(!source.serviceRefills(error))return CETestCLI::error(error.c_str());sourceMixer.mixCallback(reinterpret_cast<byte*>(block),sizeof(block));controller.poll();AudioCompletion done;while(controller.take(done))source.consume(done);}
   if(source.active()||!source.captureIdleContext(context,error)||!context.known||context.mode||context.copyMode!=2||!encodePcmIdleContext(context,packet,error))return CETestCLI::error(error.empty()?"pcm_context_actual_completion":error.c_str());
  }else{Common::String text=CETestCLI::stringField(request,"pcm_context_hex");if(text.size()!=14)return CETestCLI::error("pcm_context_extent");auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};for(unsigned at=0;at<text.size();at+=2){int a=digit(text[at]),b=digit(text[at+1]);if(a<0||b<0)return CETestCLI::error("pcm_context_hex");packet.push_back(uint8_t(a*16+b));}if(!decodePcmIdleContext(packet,context,error))return CETestCLI::error(error.c_str());if(context.mode==1){if(!staging->replacePrefix(30,bytes.data(),bytes.size(),error))return CETestCLI::error(error.c_str());}else if(!staging->publishStreamTransfer(36,0,bytes.data(),bytes.size(),error))return CETestCLI::error(error.c_str());}
  Audio::MixerImpl futureMixer(44100,true);futureMixer.setReady(true);CeAudioBackend backend(futureMixer);AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner source(controller,activity);PcmEffectOwner effects(controller,activity,&source);std::shared_ptr<NativePcmBus> bus;if(!source.prepareIdleContext(context,staging,bus,error))return CETestCLI::error(error.c_str());source.commitIdleContext(context,bus);
  std::vector<uint8_t> drum(37,255);drum[0]=drum[1]=drum[2]=0;drum[3]=21;drum[8]=0x3f;drum[9]=0x94;for(unsigned i=10;i<16;++i)drum[i]=0;std::fill(drum.begin()+16,drum.begin()+33,160);PcmEffect record={};record.id=1;record.channel=1;record.priority=4;record.level=32;record.pan=17;record.step=1024;record.source=kDrumSource;PreparedPcmEffect effect;AudioToken token;bool accepted=false;if(!preparePcmEffect(record,drum.data(),drum.size(),sourcePcmEffectProfile(record),false,0,effect,error)||!effects.start(effect,30,AudioOwner(),2,accepted,token,error)||!accepted)return CETestCLI::error(error.c_str());
  std::vector<int16_t> pcm(4411*2);futureMixer.mixCallback(reinterpret_cast<byte*>(pcm.data()),uint(pcm.size()*2));controller.poll();AudioCompletion done;while(controller.take(done))effects.consume(done);if(pcm[0]!=0||pcm[2]!=24||pcm[4]!=47)return CETestCLI::error("pcm_context_initial_interpolation");if(context.mode==1){if(effects.activeCount()||!std::all_of(pcm.begin()+96,pcm.end(),[](int16_t sample){return sample==0;}))return CETestCLI::error("pcm_context_resident_endpoint_not_retired");}else if(!effects.activeCount()||!std::all_of(pcm.begin()+6,pcm.end(),[](int16_t sample){return sample==64;}))return CETestCLI::error("pcm_context_mode0_endpoint_not_preserved");
  auto hex=[](const std::vector<uint8_t>& values){Common::String out;static const char digits[]="0123456789abcdef";for(uint8_t v:values){out+=digits[v>>4];out+=digits[v&15];}return out;};std::vector<uint8_t> future;for(int16_t sample:pcm){future.push_back(uint8_t(uint16_t(sample)>>8));future.push_back(uint8_t(sample));}Common::JSONObject out;out["ok"]=new Common::JSONValue(true);out["pcm_context_hex"]=new Common::JSONValue(hex(packet));out["future_pcm_hex"]=new Common::JSONValue(hex(future));out["retained_priority"]=new Common::JSONValue((long long)(context.mode==1?0:4));out["actual_ce_mixer"]=new Common::JSONValue(true);out["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(out);
 }
 if(action=="pcm_shared_mixer_probe"){
  PauseToken diagnosticPause=pauseEngine();struct ClockBoundary {uint32_t& stamp;OSystem* system;~ClockBoundary(){stamp=system->getMillis();}} boundary={_lastTickMillis,_system};
  Audio::MixerImpl mixer(44100,true);mixer.setReady(true);CeAudioBackend backend(mixer);AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();MusicControl music;PcmStreamOwner speech(controller,activity,&music);PcmEffectOwner effects(controller,activity,&speech);
  std::vector<uint8_t> bytes(40960,128);std::fill(bytes.begin(),bytes.begin()+40000,144);std::fill(bytes.begin()+40000,bytes.begin()+40004,255);auto staging=std::make_shared<ResidentPcmStaging>(106496);auto feed=std::make_shared<PcmStreamFeed>();PcmCatalogEntry entry={};entry.id=36;entry.mode=2;entry.priority=4;entry.level=32;entry.pan=17;entry.length=bytes.size();PreparedPcmStream stream;AudioToken token;
  if(!preparePcmStream(entry,bytes.data(),bytes.size(),sourcePcmStreamProfile(),false,stream,error)||!feed->start(staging,36,bytes.data(),bytes.size(),stream.pcm,40000,16276,error)||!speech.start(stream,AudioOwner(),1,token,error,feed))return CETestCLI::error(error.c_str());
  int16_t output[202];mixer.mixCallback(reinterpret_cast<byte*>(output),sizeof(output));const size_t expected=1+size_t(uint64_t(100)*390625/(uint64_t(24)*44100));if(feed->snapshot().consumedFrames!=expected||output[201]!=64)return CETestCLI::error("shared_mixer_prefetched_beyond_delivery");
  effects.silence();mixer.mixCallback(reinterpret_cast<byte*>(output),sizeof(output));if(std::any_of(output,output+202,[](int16_t s){return s!=0;})||feed->snapshot().consumedFrames!=expected)return CETestCLI::error("shared_mixer_FF_leaked_old_cached_speech");
  std::vector<uint8_t> effectBytes(2068,255);effectBytes[0]=0;effectBytes[1]=0;effectBytes[2]=8;effectBytes[3]=4;effectBytes[8]=0x3f;effectBytes[9]=0x94;for(unsigned i=10;i<16;++i)effectBytes[i]=0;std::fill(effectBytes.begin()+16,effectBytes.begin()+2064,160);PcmEffect record={};record.id=1;record.priority=4;record.channel=1;record.step=1024;record.level=32;record.pan=17;record.source=kDrumSource;PreparedPcmEffect effect;bool accepted=false;
  if(!preparePcmEffect(record,effectBytes.data(),effectBytes.size(),sourcePcmEffectProfile(record),false,0,effect,error)||!effects.start(effect,30,AudioOwner(),1,accepted,token,error)||!accepted)return CETestCLI::error(error.c_str());mixer.mixCallback(reinterpret_cast<byte*>(output),sizeof(output));if(output[201]!=64)return CETestCLI::error("shared_mixer_selected_pair_did_not_wake");controller.setPaused(true);controller.setPaused(false);mixer.mixCallback(reinterpret_cast<byte*>(output),sizeof(output));if(output[201]!=64)return CETestCLI::error("shared_mixer_pause_resurrected_speech");speech.reset();effects.reset();controller.reset();
  Common::JSONObject out;out["ok"]=new Common::JSONValue(true);out["actual_ce_mixer"]=new Common::JSONValue(true);out["prefix_frames"]=new Common::JSONValue((long long)101);out["consumed_source_frames"]=new Common::JSONValue((long long)expected);out["ff_first_delivered_frame_silent"]=new Common::JSONValue(true);out["selected_pair_only_wakes"]=new Common::JSONValue(true);out["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(out);
 }
 if(action=="fm_backend_probe"||action=="fm_pcm_lane_export"||action=="fm_pcm_lane_import"||action=="fm_pcm_busy_export"||action=="fm_pcm_busy_import"||action=="fm_checkpoint_probe"||action=="fm_publication_probe"||action=="fm_publication_cold_export"||action=="fm_publication_cold_import"){
  const unsigned minutes=unsigned(CETestCLI::intField(request,"minutes",0)),primeCommand=unsigned(CETestCLI::intField(request,"command",56)),natural=unsigned(CETestCLI::intField(request,"natural",0));
  const int prefixFrames=CETestCLI::intField(request,"prefix_frames",-1);const bool exportPackets=CETestCLI::intField(request,"export_packets",0)!=0;Common::String legacyHex=CETestCLI::stringField(request,"legacy_packet_hex");
  if(natural>1||(primeCommand!=56&&primeCommand!=79))return CETestCLI::error("checkpoint_probe_domain");
  if(minutes&&(minutes<21||minutes>59))return CETestCLI::error("checkpoint_probe_domain");
  if(prefixFrames>441000||prefixFrames<-1||(prefixFrames>=0&&minutes)||(exportPackets&&prefixFrames<0)||legacyHex.size()>131072)return CETestCLI::error("checkpoint_probe_packet_domain");
  PauseToken diagnosticPause=pauseEngine();
  struct ClockBoundary {uint32_t& stamp;OSystem* system;~ClockBoundary(){stamp=system->getMillis();}} boundary={_lastTickMillis,_system};
  std::vector<uint8_t> boot,fm,fmAllocation,index,drums,sub;
  if(!_resources.readBoot(boot,error)||boot.size()<512||!_resources.readFile("FMWR_1.BIN",fm,error)||!_resources.readFileAllocation("FMWR_1.BIN",fmAllocation,error)||!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMDRMDT.BIN",drums,error)||!_resources.readFile("SUBCODE.BIN",sub,error))return CETestCLI::error(error.c_str());
  std::vector<uint8_t> main(boot.begin()+512,boot.end());
  auto hexPacket=[](const std::vector<uint8_t>& bytes){Common::String text;static const char digits[]="0123456789abcdef";for(uint8_t value:bytes){text+=digits[value>>4];text+=digits[value&15];}return text;};
  if(action=="fm_pcm_lane_export"||action=="fm_pcm_lane_import"){
   const bool importing=action=="fm_pcm_lane_import",pendingOnly=CETestCLI::intField(request,"pending_only",0)!=0;
   auto decode=[&](const Common::String& text,std::vector<uint8_t>& bytes){if(text.empty()||(text.size()&1))return false;auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};for(size_t i=0;i<text.size();i+=2){int a=digit(text[i]),b=digit(text[i+1]);if(a<0||b<0)return false;bytes.push_back(uint8_t(a*16+b));}return bytes.size()>5;};
   Audio::MixerImpl measurement(44100,true);measurement.setReady(true);CeFmAudioOwner owner(measurement,false);owner.setPaused(true);
   if(!owner.load(main,fm,index,drums,error,&sub,&fmAllocation))return CETestCLI::error(error.c_str());std::vector<uint8_t> before,packet;std::vector<int16_t> samples;
   if(!importing){CeFmSynthRenderer tone(44100,false);FmInstrument instrument={};instrument.source[0]=7;instrument.algorithm=7;for(unsigned op=0;op<4;++op){uint8_t* p=instrument.source+1+op*6;p[0]=1;p[1]=24;p[2]=31;p[5]=15;}const uint8_t level[4]={30,30,30,30};std::vector<int16_t> mixed,fmOnly;
    if(!tone.init(error)||!tone.instrument(0,instrument,error)||!tone.levels(0,level,error)||!tone.pan(0,0xc0,error)||!tone.frequency(0,0x2400,error)||!tone.key(0,true,error)||!tone.renderPcmSeparated(2048,mixed,fmOnly,error)||mixed!=fmOnly||!std::any_of(fmOnly.begin(),fmOnly.end(),[](int16_t v){return v!=0;}))return CETestCLI::error(error.empty()?"FM lane is not the actual full-gain mixer contribution":error.c_str());}
   if(importing){if(!decode(legacyHex,packet)||(packet[4]!=6&&packet[4]!=7)||!owner.restoreState(packet,error))return CETestCLI::error(error.empty()?"PCM lane packet6/7 required":error.c_str());}
   else {
    if(!owner.start(79,error))return CETestCLI::error(error.c_str());bool found=false;
    for(unsigned i=0;i<180;++i){if(!owner.renderOffline(1,samples,error))return CETestCLI::error(error.c_str());auto state=owner.snapshot();if(owner.pcmOutputBusy()&&(pendingOnly||owner.pcmSamplesPending())&&(pendingOnly?state.activePercussion==0:state.activePercussion!=0)){found=true;break;}if(!owner.renderOffline(735,samples,error))return CETestCLI::error(error.c_str());}
    if(!found||!owner.snapshotState(before,error))return CETestCLI::error("PCM lane source predicate not reached");auto state=owner.snapshot();
    if(!owner.silencePcm(error))return CETestCLI::error(error.c_str());owner.setPaused(false);owner.setPaused(true);auto after=owner.snapshot();
    if(after.frames!=state.frames||after.updates!=state.updates||after.semanticDriverDigest!=state.semanticDriverDigest||after.keyOnMask!=state.keyOnMask||!owner.snapshotState(packet,error))return CETestCLI::error("PCM silence changed FM musical state");
   }
   auto corrupt=packet;corrupt.back()^=1;std::vector<uint8_t> unchanged;if(owner.restoreState(corrupt,error)||!owner.snapshotState(unchanged,error)||unchanged!=packet)return CETestCLI::error("PCM lane corrupt import not transactional");
   if(!owner.renderOffline(4411,samples,error))return CETestCLI::error(error.c_str());std::vector<uint8_t> pcm;for(int16_t value:samples){pcm.push_back(uint8_t(uint16_t(value)>>8));pcm.push_back(uint8_t(value));}
   Common::JSONObject out;out["ok"]=new Common::JSONValue(true);out["packet_hex"]=new Common::JSONValue(hexPacket(packet));out["before_packet_hex"]=new Common::JSONValue(hexPacket(before));out["future_pcm_hex"]=new Common::JSONValue(hexPacket(pcm));out["corrupt_rejected"]=new Common::JSONValue(true);out["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(out);
  }
  if(action=="fm_pcm_busy_export"||action=="fm_pcm_busy_import"){
   auto decodePacket=[&](const Common::String& text,std::vector<uint8_t>& packet){if(text.empty()||(text.size()&1))return false;auto digit=[](char c){return c>='0'&&c<='9'?int(c-'0'):c>='a'&&c<='f'?int(c-'a'+10):c>='A'&&c<='F'?int(c-'A'+10):-1;};for(size_t n=0;n<text.size();n+=2){int a=digit(text[n]),b=digit(text[n+1]);if(a<0||b<0)return false;packet.push_back(uint8_t(a*16+b));}return packet.size()>5&&(packet[4]==5||packet[4]==6);};
   Audio::MixerImpl measurement(44100,true);measurement.setReady(true);CeFmAudioOwner owner(measurement,false);owner.setPaused(true);if(!owner.load(main,fm,index,drums,error,&sub,&fmAllocation))return CETestCLI::error(error.c_str());std::vector<uint8_t> packet;std::vector<int16_t> pcm;bool legacyRead=false,sawLive=false;
   if(action=="fm_pcm_busy_export"){
    if(legacyHex.empty())return CETestCLI::error("fm_pcm_busy_requires_retained5");std::vector<uint8_t> old;if(!decodePacket(legacyHex,old))return CETestCLI::error("fm_pcm_busy_old5_header");CeFmAudioOwner historical(measurement,false);historical.setPaused(true);if(!historical.load(main,fm,index,drums,error,&sub,&fmAllocation)||!historical.restoreState(old,error))return CETestCLI::error(error.c_str());std::vector<uint8_t> a,b;if(!historical.snapshotState(a,error))return CETestCLI::error(error.c_str());historical.pcmOutputBusy();if(!historical.snapshotState(b,error)||a!=b||historical.snapshot().frames!=17)return CETestCLI::error("fm_pcm_busy_old5_mutation");legacyRead=true;
    if(owner.pcmOutputBusy()||!owner.start(79,error))return CETestCLI::error("fm_pcm_busy_idle_or_start");bool found=false;for(unsigned n=0;n<180;++n){if(!owner.renderOffline(1,pcm,error))return CETestCLI::error(error.c_str());FmAudioSnapshot state=owner.snapshot();sawLive=sawLive||state.activePercussion!=0;if(!state.activePercussion&&owner.pcmOutputBusy()){found=true;break;}if(!owner.renderOffline(735,pcm,error))return CETestCLI::error(error.c_str());}if(!found||!owner.snapshotState(packet,error))return CETestCLI::error("fm_pcm_busy_no_pending_only_within180_updates");
    Audio::MixerImpl pcmMixer(44100,true);pcmMixer.setReady(true);CeAudioBackend backend(pcmMixer);AudioController controller(backend);AudioActivity activity;activity.clearForSoundReset();PcmStreamOwner stream(controller,activity);PcmEffectOwner effects(controller,activity,&stream);PreparedPcmStream speech;speech.source.id=36;speech.source.mode=2;speech.source.priority=4;speech.sample.sampleCount=4096;speech.playbackFrames=4096;speech.droppedSourceFrames=0;speech.profile=sourcePcmStreamProfile();speech.pcm.assign(8192,1000);auto staging=std::make_shared<ResidentPcmStaging>(106496);auto feed=std::make_shared<PcmStreamFeed>();std::vector<uint8_t> bytes(8192,128);AudioToken first,next;if(!feed->start(staging,36,bytes.data(),bytes.size(),speech.pcm,4096,speech.profile.rate,error)||!stream.start(speech,AudioOwner(1,1),24,first,error,feed))return CETestCLI::error(error.c_str());std::vector<int16_t> output(128);if(pcmMixer.mixCallback(reinterpret_cast<byte*>(output.data()),64*4)<=0)return CETestCLI::error("pcm_silence_no_prefix");size_t consumed=feed->snapshot().consumedFrames;effects.silence();controller.setPaused(true);controller.setPaused(false);std::fill(output.begin(),output.end(),123);pcmMixer.mixCallback(reinterpret_cast<byte*>(output.data()),64*4);controller.poll();uint8_t bits=0;if(!std::all_of(output.begin(),output.end(),[](int16_t v){return v==0;})||feed->snapshot().consumedFrames!=consumed||!stream.active()||controller.completionCount()||!activity.snapshot(bits,error)||!(bits&2)||(bits&8))return CETestCLI::error("pcm_silence_mixer_or_ownership");speech.source.id=37;speech.pcm.assign(8192,3000);auto fresh=std::make_shared<PcmStreamFeed>();auto freshStage=std::make_shared<ResidentPcmStaging>(106496);if(!fresh->start(freshStage,37,bytes.data(),bytes.size(),speech.pcm,4096,speech.profile.rate,error)||!stream.start(speech,AudioOwner(1,2),24,next,error,fresh))return CETestCLI::error(error.c_str());std::fill(output.begin(),output.end(),0);if(pcmMixer.mixCallback(reinterpret_cast<byte*>(output.data()),64*4)<=0||std::all_of(output.begin(),output.end(),[](int16_t v){return v==0;})||!feed->snapshot().cancelled||next.id==first.id)return CETestCLI::error("pcm_silence_new_owner_enable");
   }else if(!decodePacket(legacyHex,packet)||!owner.restoreState(packet,error))return CETestCLI::error(error.c_str());
   FmAudioSnapshot before=owner.snapshot();if(before.activePercussion||!owner.pcmOutputBusy())return CETestCLI::error("fm_pcm_busy_restored_predicate");std::vector<uint8_t> unchanged;if(!owner.snapshotState(unchanged,error)||unchanged!=packet||!owner.renderOffline(4411,pcm,error))return CETestCLI::error("fm_pcm_busy_query_or_future");std::vector<uint8_t> samples;for(int16_t value:pcm){samples.push_back(uint8_t(uint16_t(value)>>8));samples.push_back(uint8_t(value));}Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["frames_before"]=new Common::JSONValue((long long)before.frames);result["active_percussion_before"]=new Common::JSONValue((long long)before.activePercussion);result["pending_pcm_busy"]=new Common::JSONValue(true);result["saw_live_percussion"]=new Common::JSONValue(sawLive);result["future_pcm_hex"]=new Common::JSONValue(hexPacket(samples));result["native_pcm_mixer_silence"]=new Common::JSONValue(action=="fm_pcm_busy_export");result["retained_fmah5_read"]=new Common::JSONValue(legacyRead);result["normal_gameplay_claim"]=new Common::JSONValue(false);if(action=="fm_pcm_busy_export")result["packet_hex"]=new Common::JSONValue(hexPacket(packet));return new Common::JSONValue(result);
  }
  if(action=="fm_backend_probe"){
   Audio::MixerImpl gm(44100,true),nm(44100,true);gm.setReady(true);nm.setReady(true);CeFmAudioOwner gens(gm,false,kFmGens),nuked(nm,false,kFmNuked);gens.setPaused(true);nuked.setPaused(true);
   if(!gens.load(main,fm,index,drums,error,&sub,&fmAllocation)||!nuked.load(main,fm,index,drums,error,&sub,&fmAllocation))return CETestCLI::error(error.c_str());Common::JSONObject result;
   for(unsigned musicId:{79u,56u}){gens.reset();nuked.reset();gens.setGains(255,255);nuked.setGains(255,255);if(!gens.start(musicId,error)||!nuked.start(musicId,error))return CETestCLI::error(error.c_str());unsigned voices=0,keys=0,peak=0;uint64_t nonzero=0;
    for(unsigned tick=0;tick<60;++tick){std::vector<int16_t>a,b;if(!gens.renderOffline(735,a,error)||!nuked.renderOffline(735,b,error))return CETestCLI::error(error.c_str());auto g=gens.snapshot(),n=nuked.snapshot();if(g.semanticDriverDigest!=n.semanticDriverDigest||g.updates!=n.updates||g.activeVoiceMask!=n.activeVoiceMask)return CETestCLI::error("fm_backend_changed_sequence");voices|=g.activeVoiceMask;keys|=g.keyOnMask;for(int16_t v:a){if(v)++nonzero;unsigned magnitude=v<0?unsigned(-int(v)):unsigned(v);peak=std::max(peak,magnitude);}}
    if(!nonzero||!voices||!keys)return CETestCLI::error("fm_backend_missing_output");Common::JSONObject song;song["voice_union"]=new Common::JSONValue((long long)voices);song["key_union"]=new Common::JSONValue((long long)keys);song["nonzero_samples"]=new Common::JSONValue((long long)nonzero);song["peak_s16"]=new Common::JSONValue((long long)peak);result[Common::String::format("fm%u",musicId)]=new Common::JSONValue(song);
   }
   gens.setPaused(true);auto before=gens.snapshot();int16 silence[256]={};gm.mixCallback(reinterpret_cast<byte*>(silence),sizeof(silence));if(gens.snapshot().frames!=before.frames)return CETestCLI::error("fm_backend_pause_advanced");gens.setPaused(false);gm.mixCallback(reinterpret_cast<byte*>(silence),sizeof(silence));if(gens.snapshot().frames<=before.frames)return CETestCLI::error("fm_backend_resume_stalled");gens.setPaused(true);
   if(!gens.start(241,error))return CETestCLI::error(error.c_str());std::vector<int16_t> ignored;if(!gens.renderOffline(1472,ignored,error)||gens.snapshot().activeVoiceMask||gens.snapshot().queuedCommands)return CETestCLI::error("fm_backend_stop");gens.reset();if(gens.snapshot().frames||gens.snapshot().keyOnMask)return CETestCLI::error("fm_backend_reset");
   result["ok"]=new Common::JSONValue(true);result["sequence_digests_equal"]=new Common::JSONValue(true);result["pause_resume_stop_reset"]=new Common::JSONValue(true);result["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(result);
  }
  if(action=="fm_publication_cold_export"||action=="fm_publication_cold_import"){
   FmCore core=kFmNuked;Common::String requested=CETestCLI::stringField(request,"fm_core");if(!requested.empty()&&!parseFmCore(requested.c_str(),core))return CETestCLI::error("fm_core_unknown");
   Audio::MixerImpl measurement(44100,true);measurement.setReady(true);CeFmAudioOwner owner(measurement,false,core);owner.setPaused(true);if(!owner.load(main,fm,index,drums,error,&sub,&fmAllocation))return CETestCLI::error(error.c_str());
   const unsigned prefix=CETestCLI::intField(request,"fm_prefix_frames",17),continuation=CETestCLI::intField(request,"continue_command",0);if((prefix!=17&&prefix!=1473)||(continuation&&continuation!=79))return CETestCLI::error("fm_cold_input_domain");
   std::vector<uint8_t> packet;std::vector<int16_t> pcm;
   if(action=="fm_publication_cold_export"){if(!owner.start(79,error)||!owner.start(56,error)||!owner.renderOffline(prefix,pcm,error)||!owner.snapshotState(packet,error))return CETestCLI::error(error.c_str());}
   else{if(legacyHex.empty()||(legacyHex.size()&1))return CETestCLI::error("fm_cold_packet_extent");auto digit=[](char c){return c>='0'&&c<='9'?int(c-'0'):c>='a'&&c<='f'?int(c-'a'+10):c>='A'&&c<='F'?int(c-'A'+10):-1;};for(size_t at=0;at<legacyHex.size();at+=2){int high=digit(legacyHex[at]),low=digit(legacyHex[at+1]);if(high<0||low<0)return CETestCLI::error("fm_cold_packet_hex");packet.push_back(uint8_t(high*16+low));}if(packet.size()<5||(packet[4]!=5&&packet[4]!=6&&packet[4]!=7&&packet[4]!=8)||!owner.restoreState(packet,error))return CETestCLI::error(error.c_str());}
   std::vector<uint8_t> checkpointAfter;auto corruptPacket=packet;corruptPacket.back()^=1;if(owner.restoreState(corruptPacket,error)||!owner.snapshotState(checkpointAfter,error)||checkpointAfter!=packet)return CETestCLI::error("fm_cold_corruption_mutated_owner");auto futurePacket=packet;futurePacket[4]=9;if(owner.restoreState(futurePacket,error)||!owner.snapshotState(checkpointAfter,error)||checkpointAfter!=packet)return CETestCLI::error("fm_cold_future_policy_mutated_owner");
   auto mislabeled=packet;mislabeled[4]=packet[4]==8?7:8;uint64_t checksum=UINT64_C(14695981039346656037);for(size_t i=0;i<mislabeled.size()-8;++i)checksum=(checksum^mislabeled[i])*UINT64_C(1099511628211);for(unsigned i=0;i<8;++i)mislabeled[mislabeled.size()-8+i]=uint8_t(checksum>>((7-i)*8));if(owner.restoreState(mislabeled,error)||!owner.snapshotState(checkpointAfter,error)||checkpointAfter!=packet)return CETestCLI::error("fm_cross_backend_reinterpretation");
   FmAudioSnapshot before=owner.snapshot();if(before.frames!=prefix||before.updates!=(prefix==17?1u:3u)||before.queuedCommands!=(prefix==17?1u:0u)||(continuation&&!owner.start(continuation,error))||!owner.renderOffline(4411,pcm,error))return CETestCLI::error("fm_cold_queued_owner");std::vector<uint8_t> samples;for(int16_t value:pcm){samples.push_back(uint8_t(uint16_t(value)>>8));samples.push_back(uint8_t(value));}
   Common::JSONObject result;result["fm_core"]=new Common::JSONValue(fmCoreName(owner.snapshot().core));result["ok"]=new Common::JSONValue(true);result["frames_before"]=new Common::JSONValue((long long)before.frames);result["queued_before"]=new Common::JSONValue((long long)before.queuedCommands);result["driver_digest_before"]=new Common::JSONValue(Common::String::format("%016llx",(unsigned long long)before.semanticDriverDigest));result["corrupt_rejected"]=new Common::JSONValue(true);result["future_policy_rejected"]=new Common::JSONValue(true);result["cross_backend_reinterpretation_rejected"]=new Common::JSONValue(true);result["future_pcm_hex"]=new Common::JSONValue(hexPacket(samples));result["normal_gameplay_claim"]=new Common::JSONValue(false);if(action=="fm_publication_cold_export")result["packet_hex"]=new Common::JSONValue(hexPacket(packet));return new Common::JSONValue(result);
  }
  if(action=="fm_publication_probe"){
   FmCore core=kFmNuked;Common::String requested=CETestCLI::stringField(request,"fm_core");if(!requested.empty()&&!parseFmCore(requested.c_str(),core))return CETestCLI::error("fm_core_unknown");
   Audio::MixerImpl measurement(44100,true);measurement.setReady(true);CeFmAudioOwner probe(measurement,false,core),restored(measurement,false,core);probe.setPaused(true);restored.setPaused(true);
   if(!probe.load(main,fm,index,drums,error,&sub,&fmAllocation)||!restored.load(main,fm,index,drums,error,&sub,&fmAllocation))return CETestCLI::error(error.c_str());
   if(!probe.start(79,error)||!probe.start(56,error))return CETestCLI::error(error.c_str());FmAudioSnapshot queued=probe.snapshot();
   if(queued.updates||queued.activeVoiceMask||queued.queuedCommands!=2)return CETestCLI::error("fm_publication_admission_mutated_voices");
   std::vector<uint8_t> state,legacy;if(!probe.snapshotState(state,error)||state[4]!=(core==kFmGens?8:7)||probe.snapshotLegacyState(legacy,error)||!restored.restoreState(state,error))return CETestCLI::error("fm_publication_policy_archive");
   std::vector<int16_t> a,b;if(!probe.renderOffline(1,a,error)||!restored.renderOffline(1,b,error)||a!=b||probe.snapshot().queuedCommands!=1||probe.snapshot().updates!=1)return CETestCLI::error("fm_publication_first_service");
   if(!probe.renderOffline(734,a,error)||!restored.renderOffline(734,b,error)||a!=b||probe.snapshot().queuedCommands!=1||probe.snapshot().updates!=1)return CETestCLI::error("fm_publication_chunk_ownership");
   if(!probe.snapshotState(state,error)||!restored.restoreState(state,error)||!probe.renderOffline(1,a,error)||!restored.renderOffline(1,b,error)||a!=b||probe.snapshot().queuedCommands||probe.snapshot().updates!=2)return CETestCLI::error("fm_publication_second_service");
   probe.setDriverSuspended(true);if(!probe.start(79,error)||!probe.renderOffline(1472,a,error)||probe.snapshot().queuedCommands!=1)return CETestCLI::error("fm_publication_suspended_admission");
   if(!probe.snapshotState(state,error)||!restored.restoreState(state,error))return CETestCLI::error("fm_publication_suspended_archive");
   probe.setDriverSuspended(false);restored.setDriverSuspended(false);if(!probe.renderOffline(4411,a,error)||!restored.renderOffline(4411,b,error)||a!=b||probe.snapshot().semanticDriverDigest!=restored.snapshot().semanticDriverDigest)return CETestCLI::error("fm_publication_restored_continuation");
   if(!probe.start(241,error)||!probe.start(79,error)||!probe.renderOffline(1472,a,error)||probe.snapshot().queuedCommands||probe.snapshot().activeVoiceMask)return CETestCLI::error("fm_publication_reset_retires_pending");
   if(!probe.snapshotState(state,error))return CETestCLI::error(error.c_str());auto corrupt=state;corrupt.back()^=1;if(probe.restoreState(corrupt,error)||!probe.snapshotState(legacy,error)||legacy!=state)return CETestCLI::error("fm_publication_corrupt_archive_mutation");
   Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["queued_before_first_service"]=new Common::JSONValue((long long)queued.queuedCommands);result["first_service_frames"]=new Common::JSONValue((long long)1);result["next_service_frame"]=new Common::JSONValue((long long)736);result["schema"]=new Common::JSONValue((long long)state[4]);result["queued_and_midchunk_restore_exact"]=new Common::JSONValue(true);result["suspended_restore_future_samples_equal"]=new Common::JSONValue(true);result["reset_retires_pending"]=new Common::JSONValue(true);result["legacy_export_guarded"]=new Common::JSONValue(true);result["corruption_transactional"]=new Common::JSONValue(true);result["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(result);
  }
  if(!legacyHex.empty()){
   if(legacyHex.size()&1)return CETestCLI::error("checkpoint_probe_hex_extent");std::vector<uint8_t> packet;auto digit=[](char c){return c>='0'&&c<='9'?int(c-'0'):c>='a'&&c<='f'?int(c-'a'+10):c>='A'&&c<='F'?int(c-'A'+10):-1;};for(size_t at=0;at<legacyHex.size();at+=2){int high=digit(legacyHex[at]),low=digit(legacyHex[at+1]);if(high<0||low<0)return CETestCLI::error("checkpoint_probe_hex_digit");packet.push_back(uint8_t(high*16+low));}
   Audio::MixerImpl measurement(44100,true);measurement.setReady(true);CeFmAudioOwner probe(measurement,false);probe.setPaused(true);if(!probe.load(main,fm,index,drums,error,&sub,&fmAllocation))return CETestCLI::error(error.c_str());std::vector<uint8_t> before,after;if(!probe.snapshotState(before,error))return CETestCLI::error(error.c_str());
   bool accepted=probe.restoreState(packet,error);Common::String diagnostic(error.c_str());Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["legacy_restore_accepted"]=new Common::JSONValue(accepted);result["diagnostic"]=new Common::JSONValue(diagnostic);result["controlled_native_owner_fixture"]=new Common::JSONValue(true);result["normal_gameplay_claim"]=new Common::JSONValue(false);
   if(!accepted){if(!probe.snapshotState(after,error)||before!=after)return CETestCLI::error("checkpoint_probe_legacy_rejection_mutation");result["transactionally_unchanged"]=new Common::JSONValue(true);return new Common::JSONValue(result);}
   FmAudioSnapshot restored=probe.snapshot();std::vector<uint8_t> fast,legacy;if(!probe.snapshotState(fast,error))return CETestCLI::error(error.c_str());bool legacyAvailable=probe.snapshotLegacyState(legacy,error);std::vector<int16_t> future,repeated;if(!probe.renderOffline(44101,future,error)||!probe.restoreState(packet,error)||!probe.renderOffline(44101,repeated,error)||future!=repeated)return CETestCLI::error("checkpoint_probe_import_repeatability");
   result["restored_frames"]=new Common::JSONValue((long long)restored.frames);result["percussion_events"]=new Common::JSONValue((long long)restored.percussionEvents);result["active_percussion"]=new Common::JSONValue((long long)restored.activePercussion);result["repeatable_future_pcm"]=new Common::JSONValue(true);result["legacy_semantic_export_available"]=new Common::JSONValue(legacyAvailable);result["current_audio_schema"]=new Common::JSONValue((long long)fast[4]);
   if(legacyAvailable){CeFmAudioOwner semantic(measurement,false);semantic.setPaused(true);if(!semantic.load(main,fm,index,drums,error,&sub,&fmAllocation)||!semantic.restoreState(legacy,error))return CETestCLI::error(error.c_str());FmAudioSnapshot state=semantic.snapshot();std::vector<int16_t> rebuilt;if(!semantic.renderOffline(44101,rebuilt,error)||rebuilt.size()!=future.size())return CETestCLI::error("checkpoint_probe_semantic_reference");uint64_t different=0,first=UINT64_MAX;for(size_t at=0;at<future.size();++at)if(future[at]!=rebuilt[at]){++different;if(first==UINT64_MAX)first=at;}result["full_semantic_future_equal"]=new Common::JSONValue(different==0);result["full_semantic_differing_samples"]=new Common::JSONValue((long long)different);result["first_differing_sample"]=new Common::JSONValue(first==UINT64_MAX?int64(-1):int64(first));result["semantic_percussion_events_equal"]=new Common::JSONValue(state.percussionEvents==restored.percussionEvents);result["semantic_active_percussion_equal"]=new Common::JSONValue(state.activePercussion==restored.activePercussion);}
   if(exportPackets){result["fast_packet_hex"]=new Common::JSONValue(hexPacket(fast));if(legacyAvailable)result["legacy_packet_hex"]=new Common::JSONValue(hexPacket(legacy));}return new Common::JSONValue(result);
  }
  struct Case {uint64_t frames;uint16_t command;bool suspended;bool resetOrigin=false;};
  std::vector<Case> cases={{0,79,false},{1,79,false},{735,79,false},{736,79,false},{88201,79,false},{735,79,true},{736,79,false,true}};
  if(minutes)cases={{uint64_t(44100)*60*minutes,uint16_t(primeCommand),false}};
  if(prefixFrames>=0)cases={{uint64_t(prefixFrames),uint16_t(primeCommand),false}};
  auto same=[](const FmAudioSnapshot& a,const FmAudioSnapshot& b){
   return a.frames==b.frames&&a.updates==b.updates&&a.nonzeroFrames==b.nonzeroFrames&&a.percussionEvents==b.percussionEvents&&a.activePercussion==b.activePercussion&&a.queuedCommands==b.queuedCommands&&a.primaryCompletionSerial==b.primaryCompletionSerial&&a.secondaryCompletionSerial==b.secondaryCompletionSerial&&a.completionPublicationGeneration==b.completionPublicationGeneration&&a.completionPublication==b.completionPublication&&a.active==b.active&&a.paused==b.paused&&a.driverSuspended==b.driverSuspended&&a.error==b.error&&a.activeVoiceMask==b.activeVoiceMask&&a.keyOnMask==b.keyOnMask&&a.semanticDriverDigest==b.semanticDriverDigest;
  };
  Common::JSONArray results;
  for(const auto& fixture:cases){
   Audio::MixerImpl measurement(44100,true);measurement.setReady(true);
   CeFmAudioOwner probe(measurement,false);probe.setPaused(true);
   if(!probe.load(main,fm,index,drums,error,&sub,&fmAllocation))return CETestCLI::error(error.c_str());
   if(fixture.resetOrigin){std::vector<int16_t> prefix;if(!probe.start(56,error)||!probe.renderOffline(88201,prefix,error))return CETestCLI::error(error.c_str());probe.setGains(192,112);probe.setMono(true);probe.setPercussionMono(true);if(!probe.start(243,error)||!probe.renderOffline(736,prefix,error))return CETestCLI::error(error.c_str());probe.reset();probe.setPaused(true);}
   probe.setDriverSuspended(fixture.suspended);
   if(!probe.start(fixture.command,error)||(fixture.suspended&&!probe.start(56,error)))return CETestCLI::error(error.c_str());
   uint32_t began=_system->getMillis();uint64_t done=0;std::vector<int16_t> discarded;
   while(done<fixture.frames){uint32_t count=uint32_t(std::min(uint64_t(65536),fixture.frames-done));if(!probe.renderOffline(count,discarded,error))return CETestCLI::error(error.c_str());done+=count;}
   uint32_t renderMillis=_system->getMillis()-began;
   FmAudioSnapshot saved=probe.snapshot();std::vector<uint8_t> fast,legacy;
   if(!probe.snapshotState(fast,error)||fast.size()<5||(fast[4]!=4&&fast[4]!=5&&fast[4]!=6&&fast[4]!=7))return CETestCLI::error("checkpoint_probe_snapshot");
   bool legacyAvailable=probe.snapshotLegacyState(legacy,error);
   if((fast[4]<5&&legacyAvailable==fixture.resetOrigin)||(legacyAvailable&&legacy[4]!=2))return CETestCLI::error("checkpoint_probe_legacy_origin");
   auto continuation=[&](std::vector<int16_t>& pcm){if(fixture.suspended)probe.setDriverSuspended(false);return (natural||probe.start(56,error))&&probe.renderOffline(44101,pcm,error);};
   std::vector<int16_t> expected,actual;
   if(!continuation(expected))return CETestCLI::error(error.c_str());
   uint64_t nonzero=0;for(int16_t value:expected)if(value)++nonzero;
   if(!nonzero)return CETestCLI::error("checkpoint_probe_silent");
   began=_system->getMillis();
   if(!probe.restoreState(fast,error,saved.primaryCompletionSerial,saved.secondaryCompletionSerial,saved.completionPublicationGeneration))return CETestCLI::error(error.c_str());
   uint32_t fastMillis=_system->getMillis()-began;
   if(!same(saved,probe.snapshot())||!continuation(actual)||actual!=expected)return CETestCLI::error("checkpoint_probe_fast_continuation");
   if(fast[4]==4){std::vector<uint8_t> v3=fast;if(v3.size()<101)return CETestCLI::error("checkpoint_probe_v4_extent");v3[4]=3;v3.erase(v3.begin()+81,v3.begin()+93);uint64_t hash=UINT64_C(14695981039346656037);for(size_t i=0;i<v3.size()-8;++i)hash=(hash^v3[i])*UINT64_C(1099511628211);for(unsigned i=0;i<8;++i)v3[v3.size()-8+i]=uint8_t(hash>>((7-i)*8));
   if(!probe.restoreState(v3,error,saved.primaryCompletionSerial,saved.secondaryCompletionSerial,saved.completionPublicationGeneration)||!same(saved,probe.snapshot())||!continuation(actual)||actual!=expected)return CETestCLI::error("checkpoint_probe_v3_continuation");}
   uint32_t legacyMillis=0;
   if(legacyAvailable){began=_system->getMillis();
    if(!probe.restoreState(legacy,error,saved.primaryCompletionSerial,saved.secondaryCompletionSerial,saved.completionPublicationGeneration))return CETestCLI::error(error.c_str());
    legacyMillis=_system->getMillis()-began;
    if(!same(saved,probe.snapshot())||!continuation(actual)||actual!=expected)return CETestCLI::error("checkpoint_probe_legacy_continuation");
   }
   std::vector<uint8_t> before,after,broken=fast;
   if(!probe.snapshotState(before,error))return CETestCLI::error(error.c_str());
   broken.back()^=1;
   if(probe.restoreState(broken,error)||!probe.snapshotState(after,error)||before!=after)return CETestCLI::error("checkpoint_probe_corrupt_mutation");
   if(probe.restoreState(fast,error,saved.primaryCompletionSerial,saved.secondaryCompletionSerial,saved.completionPublicationGeneration+1)||!probe.snapshotState(after,error)||before!=after)return CETestCLI::error("checkpoint_probe_future_cursor_mutation");
   for(unsigned schema:{2u,4u}){if(schema==2&&!legacyAvailable)continue;std::vector<uint8_t> over=schema==2?legacy:fast;uint64_t limit=uint64_t(44100)*60*60*(schema==2?1:24)+1;for(unsigned i=0;i<8;++i)over[13+i]=uint8_t(limit>>((7-i)*8));if(probe.restoreState(over,error)||!probe.snapshotState(after,error)||before!=after)return CETestCLI::error("checkpoint_probe_budget_mutation");}
   Common::JSONObject row;row["prefix_frames"]=new Common::JSONValue((long long)fixture.frames);row["source_command"]=new Common::JSONValue((long long)fixture.command);row["natural_continuation_without_new_command"]=new Common::JSONValue(bool(natural));row["suspended_with_queued_commands"]=new Common::JSONValue(fixture.suspended);row["reset_origin"]=new Common::JSONValue(fixture.resetOrigin);row["legacy_reference_available"]=new Common::JSONValue(legacyAvailable);row["optimized_continuation_exact"]=new Common::JSONValue(true);row["derived_v3_continuation_exact"]=new Common::JSONValue(fast[4]==4);row["current_audio_schema"]=new Common::JSONValue((long long)fast[4]);row["render_elapsed_ms"]=new Common::JSONValue((long long)renderMillis);row["fast_restore_elapsed_ms"]=new Common::JSONValue((long long)fastMillis);row["legacy_restore_elapsed_ms"]=new Common::JSONValue((long long)legacyMillis);row["fast_bytes"]=new Common::JSONValue((long long)fast.size());row["legacy_bytes"]=new Common::JSONValue((long long)legacy.size());row["continuation_frames"]=new Common::JSONValue((long long)44101);row["continuation_nonzero_samples"]=new Common::JSONValue((long long)nonzero);row["both_continuations_exact"]=new Common::JSONValue(legacyAvailable);row["source_owner_snapshots_exact"]=new Common::JSONValue(true);row["corrupt_and_future_cursor_transactional"]=new Common::JSONValue(true);row["saved_active_percussion"]=new Common::JSONValue((long long)saved.activePercussion);row["saved_queued_commands"]=new Common::JSONValue((long long)saved.queuedCommands);row["saved_active_voice_mask"]=new Common::JSONValue((long long)saved.activeVoiceMask);row["saved_key_on_mask"]=new Common::JSONValue((long long)saved.keyOnMask);row["saved_driver_digest"]=new Common::JSONValue(Common::String::format("%016llx",(unsigned long long)saved.semanticDriverDigest));if(exportPackets){row["fast_packet_hex"]=new Common::JSONValue(hexPacket(fast));if(legacyAvailable)row["legacy_packet_hex"]=new Common::JSONValue(hexPacket(legacy));}results.push_back(new Common::JSONValue(row));
  }
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["cases"]=new Common::JSONValue(results);result["controlled_native_owner_fixture"]=new Common::JSONValue(true);result["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(result);
 }
 if(action=="fm_long_history_probe"){
  unsigned minutes=unsigned(CETestCLI::intField(request,"minutes",21)),primeCommand=unsigned(CETestCLI::intField(request,"command",56));if(minutes<21||minutes>59||(primeCommand!=56&&primeCommand!=79))return CETestCLI::error("long_history_fixture_domain");
  PauseToken diagnosticPause=pauseEngine();struct ClockBoundary {uint32_t& stamp;OSystem* system;~ClockBoundary(){stamp=system->getMillis();}} boundary={_lastTickMillis,_system};
  std::vector<uint8_t> boot,fm,fmAllocation,index,drums,sub;if(!_resources.readBoot(boot,error)||boot.size()<512||!_resources.readFile("FMWR_1.BIN",fm,error)||!_resources.readFileAllocation("FMWR_1.BIN",fmAllocation,error)||!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMDRMDT.BIN",drums,error)||!_resources.readFile("SUBCODE.BIN",sub,error))return CETestCLI::error(error.c_str());std::vector<uint8_t> main(boot.begin()+512,boot.end());
  Audio::MixerImpl measurement(44100,true);measurement.setReady(true);CeFmAudioOwner probe(measurement,false);probe.setPaused(true);if(!probe.load(main,fm,index,drums,error,&sub,&fmAllocation)||!probe.start(uint16_t(primeCommand),error))return CETestCLI::error(error.c_str());
  const uint64_t frames=uint64_t(44100)*60*minutes;uint32_t began=_system->getMillis();uint64_t done=0;std::vector<int16_t> discard;while(done<frames){uint32_t count=uint32_t(std::min(uint64_t(65536),frames-done));if(!probe.renderOffline(count,discard,error))return CETestCLI::error(error.c_str());done+=count;}uint32_t renderedMillis=_system->getMillis()-began;
  std::vector<uint8_t> history;if(!probe.snapshotState(history,error))return CETestCLI::error(error.c_str());FmAudioSnapshot saved=probe.snapshot();if(saved.frames!=frames)return CETestCLI::error("long_history_extent_mismatch");
  // A new audible command after the saved prefix detects lost oscillator/LFO
  // history even when the prefix used a short effect followed by long silence.
  if(!probe.start(56,error))return CETestCLI::error(error.c_str());std::vector<int16_t> continuation,replayed;if(!probe.renderOffline(44101,continuation,error))return CETestCLI::error(error.c_str());uint64_t nonzero=0;for(int16_t sample:continuation)if(sample)++nonzero;if(!nonzero)return CETestCLI::error("long_history_continuation_is_silent");
  began=_system->getMillis();if(!probe.restoreState(history,error,saved.primaryCompletionSerial,saved.secondaryCompletionSerial,saved.completionPublicationGeneration))return CETestCLI::error(error.c_str());uint32_t restoreMillis=_system->getMillis()-began;if(!probe.start(56,error)||!probe.renderOffline(44101,replayed,error)||continuation!=replayed)return CETestCLI::error("long_history_audible_continuation_mismatch");
  std::vector<uint8_t> before,after;if(!probe.snapshotState(before,error))return CETestCLI::error(error.c_str());std::vector<uint8_t> over=history;uint64_t excessive=UINT64_C(44100)*60*60*(history[4]>=3?24:1)+1;for(unsigned i=0;i<8;++i)over[13+i]=uint8_t(excessive>>((7-i)*8));if(probe.restoreState(over,error)||!probe.snapshotState(after,error)||before!=after)return CETestCLI::error("long_history_over_budget_not_transactional");
  Common::String filename=Common::String::format("%s-snatcher-long-history.s16le",ConfMan.getActiveDomainName().c_str());auto* file=_system->getSavefileManager()->openForSaving(filename,false);if(!file)return CETestCLI::error("long_history_export_open");for(int16_t sample:continuation)file->writeUint16LE(uint16_t(sample));file->finalize();bool good=!file->err();delete file;if(!good)return CETestCLI::error("long_history_export_write");
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["prefix_frames"]=new Common::JSONValue((long long)frames);result["minutes"]=new Common::JSONValue((long long)minutes);result["source_command"]=new Common::JSONValue((long long)primeCommand);result["history_bytes"]=new Common::JSONValue((long long)history.size());result["prefix_updates"]=new Common::JSONValue((long long)saved.updates);result["prefix_active"]=new Common::JSONValue(saved.active);result["render_elapsed_ms"]=new Common::JSONValue((long long)renderedMillis);result["restore_elapsed_ms"]=new Common::JSONValue((long long)restoreMillis);result["continuation_frames"]=new Common::JSONValue((long long)44101);result["continuation_nonzero_samples"]=new Common::JSONValue((long long)nonzero);result["continuation_exact"]=new Common::JSONValue(true);result["over_budget_transactional"]=new Common::JSONValue(true);result["filename"]=new Common::JSONValue(filename);result["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(result);
 }
 if(action=="audio_output_probe"){
  std::vector<uint8_t> boot,fm,fmAllocation,index,drums,sub;if(!_resources.readBoot(boot,error)||boot.size()<512||!_resources.readFile("FMWR_1.BIN",fm,error)||!_resources.readFileAllocation("FMWR_1.BIN",fmAllocation,error)||!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMDRMDT.BIN",drums,error)||!_resources.readFile("SUBCODE.BIN",sub,error))return CETestCLI::error(error.c_str());
  int sourceCommand=CETestCLI::intField(request,"command",79);if(sourceCommand!=79&&sourceCommand!=56)return CETestCLI::error("qualified_audio_probe_commands_are_79_and_56");
  Audio::MixerImpl measurement(44100,true);measurement.setReady(true);CeAudioOutputOwner bus(measurement);CeFmAudioOwner probe(bus.mixer(),false);std::vector<uint8_t> main(boot.begin()+512,boot.end());
  if(!probe.load(main,fm,index,drums,error,&sub,&fmAllocation)||!probe.start(uint8_t(sourceCommand),error))return CETestCLI::error(error.c_str());
  uint32_t frames=sourceCommand==79?441000:88200;std::vector<int16_t> pcm;if(!bus.renderOffline(frames,pcm,error))return CETestCLI::error(error.c_str());
  Common::String filename=Common::String::format("%s-snatcher-master%d.s16le",ConfMan.getActiveDomainName().c_str(),sourceCommand);Common::OutSaveFile* out=_system->getSavefileManager()->openForSaving(filename,false);if(!out)return CETestCLI::error("master_probe_save_open");for(int16_t sample:pcm)out->writeUint16LE(uint16_t(sample));out->finalize();bool written=!out->err();delete out;if(!written)return CETestCLI::error("master_probe_save_write");
  bus.setPaused(true);probe.setPaused(true);NativeAudioOutputState output;std::vector<uint8_t> music;if(!bus.snapshot(output,error)||!probe.snapshotState(music,error))return CETestCLI::error(error.c_str());probe.setPaused(false);std::vector<int16_t> continued,replayed;if(!bus.renderOffline(44101,continued,error))return CETestCLI::error(error.c_str());probe.setPaused(true);if(!probe.restoreState(music,error)||!bus.restore(output,error))return CETestCLI::error(error.c_str());probe.setPaused(false);if(!bus.renderOffline(44101,replayed,error)||continued!=replayed)return CETestCLI::error("master_output_semantic_continuation_mismatch");
  if(!bus.start(error))return CETestCLI::error(error.c_str());NativeAudioOutputState pauseBefore,pauseAfter;if(!bus.snapshot(pauseBefore,error))return CETestCLI::error(error.c_str());for(unsigned i=0;i<8;++i)bus.setPaused(true);bus.setPaused(false);std::vector<int16_t> outerSamples(512);measurement.mixCallback(reinterpret_cast<byte*>(outerSamples.data()),1024);bus.setPaused(true);if(!bus.snapshot(pauseAfter,error)||pauseAfter.frames<=pauseBefore.frames)return CETestCLI::error("master_repeated_pause_did_not_resume");
  if(!bus.renderOffline(17,replayed,error))return CETestCLI::error(error.c_str());bus.setPaused(false);measurement.mixCallback(reinterpret_cast<byte*>(outerSamples.data()),1024);bus.setPaused(true);NativeAudioOutputState balanced;if(!bus.snapshot(balanced,error)||balanced.frames<=pauseAfter.frames+17)return CETestCLI::error("master_offline_paused_pull_unbalanced");
  NativeAudioOutputState overflow=balanced;overflow.frames=UINT64_MAX;if(!bus.restore(overflow,error))return CETestCLI::error(error.c_str());std::vector<int16_t> untouched(2,123);if(bus.renderOffline(1,untouched,error)||bus.error().empty()||untouched!=std::vector<int16_t>(2,123)||bus.snapshot(balanced,error))return CETestCLI::error("master_failure_not_transactional_latched");bus.reset();if(!bus.error().empty())return CETestCLI::error("master_reset_did_not_clear_failure");
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["frames"]=new Common::JSONValue((long long)frames);result["filename"]=new Common::JSONValue(filename);result["native_master_continuation_frames"]=new Common::JSONValue((long long)44101);result["source_command"]=new Common::JSONValue((long long)sourceCommand);result["repeated_pause_resumed"]=new Common::JSONValue(true);result["paused_offline_pull_balanced"]=new Common::JSONValue(true);result["callback_error_latched_and_recoverable"]=new Common::JSONValue(true);result["music_volume"]=new Common::JSONValue((long long)measurement.getVolumeForSoundType(Audio::Mixer::kMusicSoundType));result["effect_volume"]=new Common::JSONValue((long long)measurement.getVolumeForSoundType(Audio::Mixer::kSFXSoundType));result["measurement_domain"]=new Common::JSONValue("isolated native production master,44100Hz,full role gain,unmuted; no story state mutation");return new Common::JSONValue(result);
 }
 if(action=="fm_probe"){
  std::vector<uint8_t> boot,fm,fmAllocation,index,drums;if(!_resources.readBoot(boot,error)||boot.size()<512||!_resources.readFile("FMWR_1.BIN",fm,error)||!_resources.readFileAllocation("FMWR_1.BIN",fmAllocation,error)||!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMDRMDT.BIN",drums,error))return CETestCLI::error(error.c_str());
  std::vector<uint8_t> main(boot.begin()+512,boot.end());CeFmAudioOwner probe(*_mixer);probe.setPaused(true);
  if(!probe.load(main,fm,index,drums,error,nullptr,&fmAllocation)||!probe.start(79,error))return CETestCLI::error(error.c_str());
  std::vector<int16_t> pcm;if(!probe.renderOffline(441000,pcm,error))return CETestCLI::error(error.c_str());const FmAudioSnapshot first=probe.snapshot();
  if(first.frames!=441000||!first.nonzeroFrames||!first.percussionEvents)return CETestCLI::error("native_fm_mix_not_active");
  Common::SaveFileManager* manager=_system->getSavefileManager();Common::String filename=ConfMan.getActiveDomainName()+"-snatcher-fm79.s16le";Common::OutSaveFile* out=manager->openForSaving(filename,false);if(!out)return CETestCLI::error("fm_probe_write_open");
  for(int16_t sample:pcm)out->writeUint16LE(uint16_t(sample));out->finalize();bool written=!out->err();delete out;if(!written){manager->removeSavefile(filename);return CETestCLI::error("fm_probe_write_failed");}
  probe.reset();if(!probe.load(main,fm,index,drums,error,nullptr,&fmAllocation)||!probe.start(79,error))return CETestCLI::error(error.c_str());
  uint32_t compared=0;const uint32_t chunks[]={1,735,17,736,4096};for(unsigned i=0;compared<88200;++i){const uint32_t n=std::min(chunks[i%5],88200-compared);std::vector<int16_t> part;if(!probe.renderOffline(n,part,error))return CETestCLI::error(error.c_str());if(!std::equal(part.begin(),part.end(),pcm.begin()+size_t(compared)*2))return CETestCLI::error("fm_chunk_or_reset_mismatch");compared+=n;}
  probe.setGains(192,112);probe.setPercussionMono(true);probe.setDriverSuspended(true);std::vector<int16_t> suspended;if(!probe.renderOffline(737,suspended,error))return CETestCLI::error(error.c_str());probe.setDriverSuspended(false);
  std::vector<uint8_t> audioState;if(!probe.snapshotState(audioState,error))return CETestCLI::error(error.c_str());std::vector<int16_t> continuation;if(!probe.renderOffline(44101,continuation,error)||!probe.restoreState(audioState,error))return CETestCLI::error(error.c_str());std::vector<int16_t> replayed;if(!probe.renderOffline(44101,replayed,error)||replayed!=continuation)return CETestCLI::error("fm_state_continuation_mismatch");
  std::vector<uint8_t> beforeBad,afterBad;if(!probe.snapshotState(beforeBad,error))return CETestCLI::error(error.c_str());std::vector<uint8_t> corrupt=audioState;corrupt[0]^=1;if(probe.restoreState(corrupt,error)||!probe.snapshotState(afterBad,error)||beforeBad!=afterBad)return CETestCLI::error("fm_corrupt_state_changed_owner");
  FmAudioSnapshot cursors=probe.snapshot();if(probe.restoreState(beforeBad,error,cursors.primaryCompletionSerial+1,cursors.secondaryCompletionSerial)||!probe.snapshotState(afterBad,error)||beforeBad!=afterBad)return CETestCLI::error("fm_future_primary_cursor_changed_owner");if(probe.restoreState(beforeBad,error,cursors.primaryCompletionSerial,cursors.secondaryCompletionSerial+1)||!probe.snapshotState(afterBad,error)||beforeBad!=afterBad)return CETestCLI::error("fm_future_secondary_cursor_changed_owner");if(!probe.restoreState(beforeBad,error,cursors.primaryCompletionSerial,cursors.secondaryCompletionSerial)||!probe.snapshotState(afterBad,error)||beforeBad!=afterBad)return CETestCLI::error("fm_valid_completion_cursors_not_exact");if(probe.restoreState(beforeBad,error,cursors.primaryCompletionSerial,cursors.secondaryCompletionSerial,cursors.completionPublicationGeneration+1)||!probe.snapshotState(afterBad,error)||beforeBad!=afterBad)return CETestCLI::error("fm_future_publication_changed_owner");if(!probe.restoreState(beforeBad,error,cursors.primaryCompletionSerial,cursors.secondaryCompletionSerial,cursors.completionPublicationGeneration))return CETestCLI::error("fm_valid_publication_rejected");
  Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["frames"]=new Common::JSONValue((long long)first.frames);result["updates"]=new Common::JSONValue((long long)first.updates);result["nonzero_frames"]=new Common::JSONValue((long long)first.nonzeroFrames);result["percussion_events"]=new Common::JSONValue((long long)first.percussionEvents);result["state_continuation_compared_frames"]=new Common::JSONValue((long long)44101);result["completion_cursor_transactional"]=new Common::JSONValue(true);result["corrupt_state_transactional"]=new Common::JSONValue(true);result["partition_compared_frames"]=new Common::JSONValue((long long)compared);result["filename"]=new Common::JSONValue(filename);result["normal_gameplay_claim"]=new Common::JSONValue(false);return new Common::JSONValue(result);
 }
 if(action=="fragment_roundtrip"){
  // Fixed target-owned diagnostic, not a partial gameplay-save command.
  Common::SaveFileManager *manager=_system->getSavefileManager();Common::String filename=ConfMan.getActiveDomainName()+"-snatcher-fragment.dat";const std::string disc="69e1a081e2bcc8da5a033433b3e7b91c5fda8d7043300d35550be81a83ec900b";
  Common::OutSaveFile *out=manager->openForSaving(filename);if(!out)return CETestCLI::error("fragment_write_open");bool written=writeStateFragment(*out,_runtime->packed(),disc,error);out->finalize();written=written&&!out->err();delete out;if(!written){manager->removeSavefile(filename);return CETestCLI::error("fragment_write_failed");}
  Common::InSaveFile *in=manager->openForLoading(filename);std::vector<uint8_t> decoded;bool read=in&&readStateFragment(*in,disc,decoded,error);delete in;if(!read||decoded!=_runtime->packed())return CETestCLI::error("fragment_roundtrip_failed");return CETestCLI::ok();
 }
 if(action=="event_roundtrip"){
  // Exercise the event owner payload without restoring partial gameplay state.
  NativeSaveSection section;if(!encodeEventState(_events.snapshot(),section,error))return CETestCLI::error("event_snapshot_invalid");NativeSaveEnvelope save;save.sections.push_back(section);std::vector<NativeSaveSectionId> required(1,kSaveEvents);const std::string disc="69e1a081e2bcc8da5a033433b3e7b91c5fda8d7043300d35550be81a83ec900b";
  Common::SaveFileManager *manager=_system->getSavefileManager();Common::String filename=ConfMan.getActiveDomainName()+"-snatcher-events.dat";Common::OutSaveFile *out=manager->openForSaving(filename);if(!out)return CETestCLI::error("event_write_open");bool written=writeNativeSave(*out,save,disc,required,error);out->finalize();written=written&&!out->err();delete out;if(!written){manager->removeSavefile(filename);return CETestCLI::error("event_write_failed");}
  Common::InSaveFile *in=manager->openForLoading(filename);NativeSaveEnvelope loaded;bool read=in&&readNativeSave(*in,disc,required,loaded,error);delete in;EventSnapshot restored;EventScheduler diagnostic;std::vector<EventRebind> mapping;
  if(!read||loaded.sections.size()!=1||!decodeEventState(loaded.sections[0],restored,error)||!diagnostic.restore(restored,mapping,error)||mapping.size()!=_events.pending())return CETestCLI::error("event_roundtrip_failed");_trace.record(_runtime->cursor(),"event_state_roundtrip",uint32_t(mapping.size()));return CETestCLI::ok();
 }
 if(action=="narrative_roundtrip"){
  const std::string disc="69e1a081e2bcc8da5a033433b3e7b91c5fda8d7043300d35550be81a83ec900b";NarrativeSnapshot snapshot;NativeSaveSection narrative,events;
  if(!_runtime->snapshot(disc,snapshot,error)||!encodeNarrativeState(snapshot,narrative,error)||!encodeEventState(_events.snapshot(),events,error))return CETestCLI::error("narrative_snapshot_invalid");
  NativeSaveEnvelope save;save.sections.push_back(narrative);save.sections.push_back(events);std::vector<NativeSaveSectionId> required;required.push_back(kSaveNarrative);required.push_back(kSaveEvents);
  Common::SaveFileManager *manager=_system->getSavefileManager();Common::String filename=ConfMan.getActiveDomainName()+"-snatcher-narrative.dat";Common::OutSaveFile *out=manager->openForSaving(filename);if(!out)return CETestCLI::error("narrative_write_open");bool written=writeNativeSave(*out,save,disc,required,error);out->finalize();written=written&&!out->err();delete out;if(!written){manager->removeSavefile(filename);return CETestCLI::error("narrative_write_failed");}
  Common::InSaveFile *in=manager->openForLoading(filename);NativeSaveEnvelope loaded;bool read=in&&readNativeSave(*in,disc,required,loaded,error);delete in;const NativeSaveSection *narrativePayload=nullptr,*eventPayload=nullptr;for(const NativeSaveSection& payload:loaded.sections){if(payload.id==kSaveNarrative)narrativePayload=&payload;if(payload.id==kSaveEvents)eventPayload=&payload;}
  NarrativeSnapshot decoded;EventSnapshot decodedEvents;EventScheduler diagnosticEvents;SemanticTrace diagnosticTrace;NarrativeRuntime diagnostic(_scenarios,diagnosticEvents,diagnosticTrace);std::vector<EventRebind> rebind;
  if(!read||loaded.sections.size()!=2||!narrativePayload||!eventPayload||!decodeNarrativeState(*narrativePayload,disc,decoded,error)||!decodeEventState(*eventPayload,decodedEvents,error)||!diagnostic.restore(decoded,disc,decodedEvents,rebind,error)||diagnostic.packed()!=_runtime->packed()||!(diagnostic.cursor()==_runtime->cursor()))return CETestCLI::error("narrative_roundtrip_failed");
  _trace.record(_runtime->cursor(),"narrative_state_roundtrip",uint32_t(rebind.size()));return CETestCLI::ok();
 }
 if(action=="timing_roundtrip"){
  // Semantic owner diagnostic only: never restore partial state into gameplay.
  EventSnapshot events=_events.snapshot();TimingSnapshot timing;timing.clock=_clock.snapshot();
  for(const SemanticEvent& event:events.pending)if(event.token==_delayToken){timing.delay=_delay.snapshot();timing.delayToken=_delayToken;}
  NativeSaveSection eventSection,timingSection;if(!encodeEventState(events,eventSection,error)||!encodeTimingState(timing,timingSection,error))return CETestCLI::error("timing_snapshot_invalid");
  NativeSaveEnvelope save;save.sections.push_back(eventSection);save.sections.push_back(timingSection);std::vector<NativeSaveSectionId> required;required.push_back(kSaveEvents);required.push_back(kSaveTimers);
  const std::string disc="69e1a081e2bcc8da5a033433b3e7b91c5fda8d7043300d35550be81a83ec900b";Common::SaveFileManager *manager=_system->getSavefileManager();Common::String filename=ConfMan.getActiveDomainName()+"-snatcher-timing.dat";
  Common::OutSaveFile *out=manager->openForSaving(filename);if(!out)return CETestCLI::error("timing_write_open");bool written=writeNativeSave(*out,save,disc,required,error);out->finalize();written=written&&!out->err();delete out;if(!written){manager->removeSavefile(filename);return CETestCLI::error("timing_write_failed");}
  Common::InSaveFile *in=manager->openForLoading(filename);NativeSaveEnvelope loaded;bool read=in&&readNativeSave(*in,disc,required,loaded,error);delete in;EventSnapshot decodedEvents;TimingSnapshot decodedTiming;EventScheduler diagnostic;GameClock clock;NarrativeDelay delay;EventToken token;std::vector<EventRebind> mapping;
  const NativeSaveSection *eventPayload=nullptr,*timingPayload=nullptr;for(const NativeSaveSection& payload:loaded.sections){if(payload.id==kSaveEvents)eventPayload=&payload;if(payload.id==kSaveTimers)timingPayload=&payload;}
  if(!read||loaded.sections.size()!=2||!eventPayload||!timingPayload||!decodeEventState(*eventPayload,decodedEvents,error)||!decodeTimingState(*timingPayload,decodedTiming,error)||!diagnostic.restore(decodedEvents,mapping,error)||!restoreTimingState(decodedTiming,decodedEvents,mapping,clock,delay,token,error))return CETestCLI::error("timing_roundtrip_failed");
  _trace.record(_runtime->cursor(),"timing_state_roundtrip",uint32_t(clock.snapshot().ticks),delay.elapsed());return CETestCLI::ok();
 }
 if(action=="scene_layout_fixture"){
  int preset=CETestCLI::intField(request,"preset");if(preset<0||preset>2)return CETestCLI::error("invalid_layout_preset");std::vector<uint8_t> boot;SceneLayout layout;
  if(!_resources.readBoot(boot,error)||boot.size()<512||!readSceneLayoutPreset(boot.data()+512,boot.size()-512,SceneLayoutPreset(preset),layout,error)||!_scene.configure(layout,error))return CETestCLI::error("layout_fixture_failed");_presentationFixture=true;return CETestCLI::ok();
 }
 if(action=="new_game"){if(!startStartup(error))return CETestCLI::error("new_game_failed");return CETestCLI::ok();}
 if(action=="post_intro_fixture"){if(!startNative(Cursor(0x3f,0),false,error))return CETestCLI::error("post_intro_fixture_failed");return CETestCLI::ok();}
 if(action=="startup_input"){int mask=CETestCLI::intField(request,"pressed");if(!_startupActive||mask<0||mask>255)return CETestCLI::error("startup_input_invalid");_startupPressed=uint8_t(_startupPressed|mask);return CETestCLI::ok();}
 if(action=="start_cd_track"){int track=CETestCLI::intField(request,"track");AudioToken token;if(track<1||!_cd.start(unsigned(track),64,AudioOwner(0,0),_events.generation(),token,error))return CETestCLI::error("cd_track_start_failed");return CETestCLI::ok();}
 if(action=="start_fixture"){int resource=CETestCLI::intField(request,"resource"),node=CETestCLI::intField(request,"node");if(resource<0x39||resource>0x5f||node<0||node>65535||!startNative(Cursor(uint16_t(resource),uint32_t(node)),true,error))return CETestCLI::error("invalid_fixture_entry");return CETestCLI::ok();}
 if(action=="ack_dialogue"){if(!acknowledgeText(error))return CETestCLI::error("dialogue_not_ready");return CETestCLI::ok();}
 if(action=="choose"){int ordinal=CETestCLI::intField(request,"ordinal");if(ordinal<1||!_runtime->choose(unsigned(ordinal),error))return CETestCLI::error("invalid_choice");return CETestCLI::ok();}
 if(action=="cancel_choice"){if(!_runtime->cancelChoice(error))return CETestCLI::error("invalid_cancellation");return CETestCLI::ok();}
 if(action=="cd_audio_probe"){int track=CETestCLI::intField(request,"track"),frames=CETestCLI::intField(request,"frames",10);std::vector<uint8_t> data;std::vector<int16_t> pcm;AudioToken token;if(track<1||frames<1||frames>75||!_resources.readCdAudioFrames(unsigned(track),0,unsigned(frames),data,error)||!decodeCdAudioFrames(data.data(),data.size(),pcm,error)||!_audio.start(pcm,cdAudioProfile(64),kAudioMusic,AudioOwner(0,0),_events.generation(),token,error))return CETestCLI::error("cd_audio_probe_failed");return CETestCLI::ok();}
 return CETestCLI::unsupported();
}
}
