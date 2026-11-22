/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include "engine.h"
#include "pcm_visual.h"
namespace Snatcher {
bool SnatcherEngine::finishPcmStreamStaging(std::string& error){
 error.clear();if(!_residentStreamPending)return true;const auto* stream=findPcmEntry(_catalog,_residentStreamPending);if(!stream||(stream->mode!=2&&stream->mode!=0x82)||stream->offset>_pcmData.size()||stream->length>_pcmData.size()-stream->offset){error="Completed stream staging has no original source extent";return false;}
 if(!_residentStaging){size_t capacity=65536;for(const auto& entry:_catalog)if(entry.mode==1)capacity=std::max(capacity,entry.length);_residentStaging.reset(new ResidentPcmStaging(capacity));}
 bool ok=_residentStaging->completeStream(stream->id,_pcmData.data()+stream->offset,stream->length,error);if(ok)_residentStreamPending=0;return ok;
}
bool SnatcherEngine::loadResidentBank(uint16_t id,std::string& error){
 error.clear();if(_catalog.empty()){std::vector<uint8_t> index;if(!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMLD_01.BIN",_pcmData,error)||!parsePcmCatalog(index.data(),index.size(),_pcmData.size(),_catalog,error))return false;}
 PcmRequest request;if(!planPcmRequest(_catalog,id,request,error)||request.kind!=kLoadResidentBank){error="Resident cache requires an original mode1 bank";return false;}
 if((_streamOwner.active()&&!_streamOwner.sharedBus())||!_effectOwner.residentReplacementSafe(id,request.entry.length,_residentStaging)){error="Resident bank overlaps an unqualified active stream or refill owner";return false;}
 if(!_pendingEffects.empty()){error="Resident bank replacement has unconsumed effect-source requests";return false;}
 std::vector<PcmSample> samples;size_t tail;if(!parsePcmSamples(_pcmData.data()+request.entry.offset,request.entry.length,true,samples,tail,error))return false;
 // Source mode1 replaces only the host staging extent. Already prefetched or
 // disjoint voices retain their actual token, PCM, priority and playback cursor.
 if(!_residentStaging){size_t capacity=0;for(const auto& entry:_catalog)if(entry.mode==1)capacity=std::max(capacity,entry.length);if(!capacity){error="Resident source allocation has no verified catalog extent";return false;}_residentStaging.reset(new ResidentPcmStaging(capacity));}
 if(!_streamOwner.enterResidentMode(error,_residentStaging)||!_residentStaging->replacePrefix(id,_pcmData.data()+request.entry.offset,request.entry.length,error))return false;
 _activity.beginResidentTransfer();_residentBankId=id;_lastPcmCommand=id;_lastPcmKnown=true;_activity.completeResidentTransfer();return true;
}
bool SnatcherEngine::updatePcmRequest(const RuntimeRequest& command,std::string& error){
 error.clear();if(_pcmRequestToken.id&&!(_pcmRequestToken==command.token)){error="PCM request owner conflict";return false;}
 if(!_pcmRequestToken.id){_pcmRequestToken=command.token;_pcmRequestPhase=0;_pcmRequestId=command.first;}
 if(!_pcmRequestPhase){
  if(_catalog.empty()){std::vector<uint8_t> index;if(!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMLD_01.BIN",_pcmData,error)||!parsePcmCatalog(index.data(),index.size(),_pcmData.size(),_catalog,error))return false;}
  if(_pcmVisualTable.empty()){std::vector<uint8_t> sub;if(!_resources.readFile("SUBCODE.BIN",sub,error)||!readPcmVisualTable(sub.data(),sub.size(),_catalog.size(),_pcmVisualTable,error))return false;}
  PcmRequest request;if(!planPcmRequest(_catalog,command.first,request,error))return false;
  if(request.kind==kLoadResidentBank){if(!loadResidentBank(command.first,error))return false;_trace.record(command.owner,"resident_bank_loaded",command.first,uint32_t(request.entry.length));}
  else _activity.setKnownFlag(0,true);
  _pcmVisualStartActors.clear();
  _pcmRequestPhase=1;return true;
 }
 if(_pcmRequestPhase==1){_pcmRequestPhase=2;return true;}
 bool mapped;uint8_t list;if(!lookupPcmVisual(_pcmVisualTable,_pcmRequestId,mapped,list,error))return false;
 if(_pcmRequestPhase==2&&(mapped||_scene.palettePublicationPending())){if(mapped&&!_scene.requestPalettePublication(1,error))return false;_pcmRequestPhase=3;_trace.record(command.owner,"pcm_visual_prepared",command.first,list);return true;}
 if(_pcmRequestPhase==3&&mapped){uint16_t resource;unsigned primary;if(!_scene.resourceIdentity(resource,primary)){error="PCM-linked display has no primary resource owner";return false;}std::vector<uint8_t> bytes;if(!_resources.readResource(resource,bytes,error))return false;SceneAssets overlay;if(parseSceneAssets(bytes.data(),bytes.size(),list,16*1024*1024,overlay,error)!=kSceneAssetsReady||!_scene.installOverlay(resource,bytes,list,error))return false;_pcmVisualStartActors=overlay.actors;_trace.record(command.owner,"pcm_display_published",command.first,list);}
 // The linked actor can have an authored timed pose before its A9 sound-start
 // wait (Napoleon's sneeze is one). Starting the mixer before that pose gives
 // audio a head start: EOF releases caption cleanup before the actor restores
 // its hidden portrait. Admit playback at that actual visual boundary instead.
 if(_pcmRequestPhase<4){_pcmRequestPhase=4;return true;}
 if(!_scene.pcmVisualStartReady(_pcmVisualStartActors))return true;
 PcmRequest request;if(!planPcmRequest(_catalog,command.first,request,error))return false;
 if(request.kind!=kLoadResidentBank){PreparedPcmStream stream;AudioToken token;
   if(!_residentStaging){size_t capacity=65536;for(const auto& entry:_catalog)if(entry.mode==1)capacity=std::max(capacity,entry.length);_residentStaging.reset(new ResidentPcmStaging(capacity));}
   std::shared_ptr<PcmStreamFeed> feed(new PcmStreamFeed);_activity.setKnownFlag(0,true);
   if(!preparePcmStream(request.entry,_pcmData.data(),_pcmData.size(),sourcePcmStreamProfile(),_pcmMono,stream,error,_quietPcm)||!feed->start(_residentStaging,command.first,_pcmData.data()+request.entry.offset,request.entry.length,stream.pcm,stream.sample.sampleCount,stream.profile.rate,error)||!_streamOwner.start(stream,AudioOwner(command.owner.resource,command.owner.node),_events.generation(),token,error,feed)){_activity.forgetFlag(0);_scene.clearSoundContext();_audioError=error;return false;}
   // The tracked writer publishes actual completed resource services. Audible
   // EOF must not reconstruct transfers that were never requested/published.
   _residentStreamPending=0;_lastPcmCommand=0xffff;_lastPcmKnown=true;_effectOwner.reset();for(const auto& effect:_pendingEffects)_trace.record(effect.owner,"scene_effect_cancelled_by_stream",effect.id,effect.bank);_pendingEffects.clear();_audioError.clear();_trace.record(command.owner,"stream_started",command.first,uint32_t(token.id));}
 if(!_events.complete(command.token,false,error))return false;_pcmRequestToken=EventToken();_pcmRequestPhase=0;_pcmRequestId=0;return true;
}
}
