/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <functional>
#include "engine.h"
#include "engine_save.h"
#include "original_input.h"
#include "scene_state.h"
#include "native_archive.h"
#include "game_service_backend.h"
#include "pcm_cache.h"
#include "pcm_shared.h"
#include "narrative_state.h"
#include "event_state.h"
#include "timing_state.h"
#include "common/config-manager.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/md5.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/endian.h"
#include "graphics/thumbnail.h"
#include "graphics/surface.h"
#include "common/fs.h"
#include "common/formats/json.h"
#include "engines/ce_test_cli.h"
#include "engines/ce_achievements.h"
namespace Snatcher {namespace {
const char* const discIdentity="69e1a081e2bcc8da5a033433b3e7b91c5fda8d7043300d35550be81a83ec900b";
std::vector<NativeSaveSectionId> required(){return {kSaveNarrative,kSaveScene,kSaveTimers,kSaveAudio,kSaveInput,kSaveEvents};}
const NativeSaveSection* section(const NativeSaveEnvelope& save,NativeSaveSectionId id){for(const auto& s:save.sections)if(s.id==id)return &s;return nullptr;}
struct ResourceSource:SceneStateSource {GameResource& source;explicit ResourceSource(GameResource& r):source(r){}bool loadSystemResource(std::vector<uint8_t>& bytes,std::string& error)override{return source.readFile("SUBCODE.BIN",bytes,error);}bool loadSceneResource(uint16_t id,std::vector<uint8_t>& bytes,std::string& error)override{return source.readResource(id,bytes,error);}};
Common::Error failure(std::string& target,const std::string& reason,Common::ErrorCode code){target=reason;return Common::Error(code,Common::String(reason.c_str()));}
Common::String name(int slot){return Common::String::format("%s.%03d",ConfMan.getActiveDomainName().c_str(),slot);}
}
class EngineStateCodec {
public:
 struct Continuation {bool startupStored=false;std::string invalidField;uint32_t creditsSourceBytes=0;uint64_t creditsSourceHash=0;
#define SAVE_FIELD(type, field) type field{};
#include "engine_continuation_fields.inc"
#undef SAVE_FIELD
 };
#include "engine_continuation_codec.inc"
 struct SavedAudioJob {AudioController::Playing job;uint8_t source=0;unsigned slot=0;uint64_t serial=0;std::vector<uint8_t> playback,feed;};
 struct ActiveAudio {uint64_t next=1;AudioToken streamToken,busToken;uint16_t catalog=0;uint8_t mode=0,priority=0;PcmIdleContext context;std::vector<uint8_t> bus,stream;std::vector<SavedAudioJob> jobs;std::vector<AudioCompletion> completions;PcmEffectOwner::Slot effects[2];std::vector<uint8_t> effectFeeds[2];CdPlaybackState cd;std::shared_ptr<NativePcmBus> restoredBus;std::shared_ptr<PcmStreamFeed> restoredStream;ShootingEncounterData shootingData;ShootingAimLayout shootingAim{};ShootingFireLayout shootingFire{};};
 struct State {bool anywhere=false;Continuation extra;ActiveAudio activeAudio;CommandInput input;MusicControl music;AudioActivity activity;unsigned selection=0,rootSelection=0;uint16_t resident=0xffff;bool mono=false,startupComplete=false,dialogueReady=false;std::vector<uint8_t> dialogue;uint16_t randomFirst=0,randomSecond=0;uint64_t randomDraws=0;std::string description;bool dialogueMode=false,legacy=false;EventToken token;NativeAudioOutputState output;ShootingCoordinatorState shooting;uint16_t coordinator=2,interaction=0,textSession=0,textLimit=0,phonePolicy=0,ambient=0,pointerX=112,pointerY=56;bool cacheSchema=false,publicationSchema=false,residentCacheSchema=false;bool quiet=false,primaryKnown=false,pcmKnown=false;uint8_t primary=0;uint16_t lastPcm=0xffff;uint64_t primaryObserved=0,secondaryObserved=0,publicationObserved=0;uint8_t serialObserved=0;NativePresentationBookmark bookmark;uint64_t elapsedOrigin=0;PointerInteractionSnapshot pointer;bool pointerSchema=false;ResidentPcmStagingSnapshot staging;bool stagingSchema=false,stagingPresent=false;bool cdIdleSchema=false;CdIdleSnapshot cdIdle;LegacyInputHeader compatibility;PcmIdleContext pcmContext;bool pcmContextSchema=false;};
 static bool archive(NativeArchive& a,State& s,unsigned version){uint32_t magic=0x534e494e;a.field(magic);if(magic!=0x534e494e)a.ok=false;std::vector<uint8_t> text(s.description.begin(),s.description.end());a.vector(text,255);if(a.reading)s.description.assign(text.begin(),text.end());
  auto& c=s.input;a.vector(c._references,31);a.flag(c._active);a.flag(c._canCancel);a.number(c._selected);a.field(c._held);a.field(c._pressed);a.field(c._repeatPhase);a.field(c._repeatAnchor);if((version<12&&(c._held||c._pressed))||c._repeatPhase>2)a.ok=false;
  auto& m=s.music;std::vector<uint8_t> commands(m._commands.begin(),m._commands.end());a.vector(commands,127);if(a.reading)m._commands.assign(commands.begin(),commands.end());a.field(m._gain);a.field(m._effectGain);a.field(m._sentGain);a.field(m._sentEffect);a.field(m._counter);a.flag(m._fade);uint8_t envelope=uint8_t(m._envelope);a.field(envelope);if(envelope>2||m._counter>3)a.ok=false;m._envelope=MusicEnvelope(envelope);
  a.field(s.activity._known);a.field(s.activity._active);if((s.activity._known&~15)||(s.activity._active&~s.activity._known)||(version<12&&(s.activity._known!=15||s.activity._active)))a.ok=false;a.number(s.selection);a.number(s.rootSelection);a.field(s.resident);a.flag(s.mono);a.flag(s.startupComplete);a.flag(s.dialogueReady);a.vector(s.dialogue,131072);a.field(s.randomFirst);a.field(s.randomSecond);a.field(s.randomDraws);if(version>=2){a.flag(s.dialogueMode);a.field(s.token.id);a.field(s.token.generation);a.field(s.output.version);a.field(s.output.rate);a.field(s.output.frames);for(unsigned j=0;j<2;++j){a.field(s.output.inputQ16[j]);a.field(s.output.outputQ16[j]);}a.field(s.shooting.shots);a.field(s.shooting.misses);a.field(s.shooting.encounter.typeClearHits);a.field(s.shooting.encounter.typeSetHits);a.field(s.shooting.encounter.damage);a.field(s.shooting.encounter.timing);a.field(s.shooting.encounter.mode);a.flag(s.shooting.encounter.alternateInput);a.flag(s.shooting.allSpecialHit);a.field(s.shooting.aimToggle);a.field(s.coordinator);a.field(s.interaction);a.field(s.textSession);a.field(s.textLimit);a.field(s.phonePolicy);a.field(s.ambient);a.field(s.pointerX);a.field(s.pointerY);a.flag(s.quiet);if(s.shooting.encounter.mode>=15||(s.coordinator!=2&&s.coordinator!=3&&s.coordinator!=5)||s.interaction>3||s.pointerX>255||s.pointerY>255||(version<12&&s.ambient))a.ok=false;NativeAudioOutput validate;std::string detail;if(!validate.restore(s.output,detail))a.ok=false;}
  s.cacheSchema=version>=3;if(version>=3){a.field(s.primary);a.flag(s.primaryKnown);a.field(s.lastPcm);a.flag(s.pcmKnown);a.field(s.primaryObserved);a.field(s.secondaryObserved);auto& b=s.bookmark;a.flag(b.valid);if(b.valid){a.field(b.presentation);a.field(b.stream);a.field(b.music);a.field(b.ambient);a.field(b.interaction);a.flag(b.quiet);a.flag(b.suppressed);for(auto& value:b.actors){a.field(value);if((!(value&128)&&value)||((value&64)&&value!=192))a.ok=false;}if((b.presentation>>8)>=97||(version<12&&b.ambient)||b.interaction>3)a.ok=false;}}
  s.publicationSchema=version>=4;if(version>=4){a.field(s.publicationObserved);a.field(s.serialObserved);if(s.serialObserved>15||(s.publicationObserved&15)!=s.serialObserved)a.ok=false;}
  if(version>=5){a.field(s.elapsedOrigin);if(s.elapsedOrigin>=UINT64_C(100)*60*60*60)a.ok=false;}s.residentCacheSchema=version>=6;if(version>=6){a.flag(s.bookmark.pcmKnown);if(!s.bookmark.valid&&s.bookmark.pcmKnown)a.ok=false;}
  s.pointerSchema=version>=7;if(s.pointerSchema){auto& p=s.pointer;a.flag(p.active);a.flag(p.selectedKnown);a.field(p.x16);a.field(p.y16);a.field(p.selectedRegion);a.field(p.mapStamp);if(!p.mapStamp||p.active!=(s.interaction==1)||(!p.selectedKnown&&p.selectedRegion))a.ok=false;}
  s.stagingSchema=version>=8;if(s.stagingSchema){a.flag(s.stagingPresent);if(s.stagingPresent){auto& stage=s.staging;a.number(stage.capacity);a.vector(stage.knownBytes,106496);a.field(stage.bank);a.field(stage.generation);uint8_t writer=uint8_t(stage.writerKind);a.field(writer);stage.writerKind=ResidentPcmWriterKind(writer);a.field(stage.writerId);if(!stage.capacity||stage.capacity>106496||writer>2)a.ok=false;}}
  s.cdIdleSchema=version>=9;if(s.cdIdleSchema){auto& cd=s.cdIdle;a.field(cd.version);a.field(cd.track);a.field(cd.total);a.field(cd.nextFrame);a.field(cd.lastConsumed);a.field(cd.nextToken);a.flag(cd.positionKnown);a.flag(cd.inputFinished);a.flag(cd.paused);std::string detail;if(!validateCdIdleSnapshot(cd,detail))a.ok=false;}
  if(!archiveLegacyInputHeader(a,s.compatibility,std::min(version,10u)))return false;
  s.pcmContextSchema=version>=11;if(s.pcmContextSchema){std::vector<uint8_t> context;std::string detail;if(!a.reading&&!encodePcmIdleContext(s.pcmContext,context,detail))a.ok=false;a.vector(context,7);if(a.reading&&!decodePcmIdleContext(context,s.pcmContext,detail))a.ok=false;}
  if(s.rootSelection>=31||(version<12&&!s.startupComplete))a.ok=false;
  if(version<12&&s.dialogueMode){if(c._active||!c._references.empty()||!s.token.id||!s.token.generation)a.ok=false;}
  else if(version<12&&(s.selection!=c._selected||c._selected>=c._references.size()||!c._active||s.token.id||s.token.generation))a.ok=false;
  if(version>=12){s.anywhere=true;continuation(a,s.extra,version>=13);owner(a,s.shooting);archiveAudio(a,s.activeAudio);}
  return a.finished();
 }
 static uint64_t sourceHash(const std::vector<uint8_t>& bytes){uint64_t h=UINT64_C(14695981039346656037);for(uint8_t b:bytes){h^=b;h*=UINT64_C(1099511628211);}return h;}
 static bool hydrate(State& s,GameResource& resources,std::string& error){
  if(!s.extra.creditsSourceBytes)return true;
  if(!resources.readResource(52,s.extra._creditsData,error)||s.extra._creditsData.size()!=s.extra.creditsSourceBytes||sourceHash(s.extra._creditsData)!=s.extra.creditsSourceHash){error="Credits checkpoint source identity mismatch";return false;}return true;
 }
 static bool inputQuiescent(const CommandInput& input){return !input._held&&!input._pressed;}
 static bool cancellationMatches(const State& s,const ControlState& control){return s.input._canCancel==!control.selectedPath().empty();}
 static bool associations(const State& s,const NarrativeSnapshot& n,const EventSnapshot& e,DialogueWindow& w,GameResource& resources,std::string& error){
  if(s.anywhere)return associationsAnywhere(s,n,e,error);
  if(s.cacheSchema&&(s.bookmark.valid!=n.bookmarkKnown||(s.bookmark.valid&&s.bookmark.presentation!=n.bookmarkedPresentation))){error="bookmark engine/interpreter owner mismatch";return false;}
  if(n.menuSelectionResetPending){error="Unconsumed menu reset cannot match an active UI snapshot";return false;}
  if(!s.dialogueMode)return SceneStateCodec::bindMenuCount(w,unsigned(s.input.references().size()),error)&&n.status==kRuntimeChoices&&e.pending.empty()&&s.input.references()==n.control.available&&s.input._canCancel==!n.control.selected.empty()&&SceneStateCodec::menuAssociation(w,s.selection,s.dialogueReady);
  if(n.status!=kRuntimeWaiting||e.pending.empty()||!e.ready.empty())return false;bool found=false;for(const SemanticEvent& event:e.pending){if(event.completed)return false;if(event.token==s.token){if(event.kind!=kDialogueEvent||found)return false;found=true;}}if(!found)return false;
  const RuntimeRequest* owned=nullptr;for(const RuntimeRequest& r:n.requests)if(r.token==s.token){if(owned)return false;owned=&r;}
  if(!owned||owned->kind!=kRequestDialogue)return false;std::vector<uint8_t> source;if(!resources.readResource(owned->owner.resource,source,error))return false;
  return SceneStateCodec::dialogueAssociation(w,source,size_t(0x3800)+owned->second,owned->first,s.dialogueReady);
 }
 static bool capture(SnatcherEngine& engine,const Common::String& description,NativeSaveSection& out,std::string& error){State s;s.compatibility=engine._originalInputSettings.compatibility;s.input=engine._commandInput;s.music=engine._music;s.activity=engine._activity;s.selection=engine._selection;s.rootSelection=engine._rootSelection;s.resident=engine._residentBankId;s.mono=engine._pcmMono;s.startupComplete=engine._startupCompleted;s.dialogueReady=engine._dialogueReady;s.description=description.c_str();s.shooting=engine._shooting;s.coordinator=engine._coordinatorMode;s.interaction=engine._interactionMode;s.textSession=engine._textEntrySession;s.textLimit=engine._textEntryLimit;s.phonePolicy=engine._phoneEntryPolicy;s.ambient=engine._ambientMode;s.pointerX=engine._pointerX;s.pointerY=engine._pointerY;s.quiet=engine._quietPcm;s.primary=engine._primaryMusicCommand;s.primaryKnown=engine._primaryMusicKnown;s.lastPcm=engine._lastPcmCommand;s.pcmKnown=engine._lastPcmKnown;s.primaryObserved=engine._primaryCompletionObserved;s.secondaryObserved=engine._secondaryCompletionObserved;s.bookmark=engine._bookmark;s.publicationObserved=engine._completionPublicationObserved;s.serialObserved=engine._completionSerialObserved;s.elapsedOrigin=engine._gameElapsedOrigin;s.dialogueMode=engine._runtime->status()==kRuntimeWaiting;if(s.dialogueMode){for(const auto& event:engine._events.snapshot().pending)if(event.token==engine._dialogueToken)s.token=engine._dialogueToken;}if(!engine._output.snapshot(s.output,error))return false;s.randomFirst=engine._nativeRandom.state.first;s.randomSecond=engine._nativeRandom.state.second;s.randomDraws=engine._nativeRandom.draws;if(!SceneStateCodec::encodeDialogue(engine._dialogueWindow,s.dialogue,error))return false;if(!engine._pointerInteraction.snapshot(s.pointer,error))return false;s.stagingPresent=bool(engine._residentStaging);if(s.stagingPresent&&!engine._residentStaging->snapshot(s.staging,error))return false;s.anywhere=true;
#define SAVE_FIELD(type, field) s.extra.field=engine.field;
#include "engine_continuation_fields.inc"
#undef SAVE_FIELD
 if(!s.extra._presentationLoadPhase)s.extra._presentationData.clear();
 if(!s.extra._bookmarkPhase)s.extra._bookmarkData.clear();
 if(!s.extra._creditsPhase)s.extra._creditsData.clear();
 if(!captureAudio(engine,s.activeAudio,error))return false;
 NativeArchive a(64*1024*1024);if(!archive(a,s,13)){error="Invalid save owner: "+(s.extra.invalidField.empty()?std::string("base/audio at byte ")+std::to_string(a.bytes.size()):s.extra.invalidField);return false;}NativeSaveSection value(kSaveInput);value.required=true;value.version=13;value.bytes.swap(a.bytes);out=value;return true;}
 static bool decode(const NativeSaveSection& value,State& s,std::string& error){if(value.id!=kSaveInput||(value.version!=1&&value.version!=2&&value.version!=3&&value.version!=4&&value.version!=5&&value.version!=6&&value.version!=7&&value.version!=8&&value.version!=9&&value.version!=10&&value.version!=11&&value.version!=12&&value.version!=13)){error="invalid engine owner section";return false;}NativeArchive a(value.bytes,64*1024*1024);State staged;staged.legacy=value.version==1;if(!archive(a,staged,value.version)){error="invalid native input/music owner payload";return false;}s=staged;return true;}
#include "engine_active_audio_codec.inc"
};
bool nativeInputCompatibilityProbe(const NativeSaveSection& source,const LegacyInputHeader& header,NativeSaveSection& migrated,std::string& error){
 EngineStateCodec::State state;if(!EngineStateCodec::decode(source,state,error))return false;LegacyInputHeader previous=state.compatibility;state.compatibility=header;unsigned targetVersion=std::max(10u,unsigned(source.version));NativeArchive encoded;if(!EngineStateCodec::archive(encoded,state,targetVersion)){error="opaque compatibility encoding failed";return false;}
 NativeSaveSection next(kSaveInput);next.required=true;next.version=uint16_t(targetVersion);next.bytes=encoded.bytes;EngineStateCodec::State restored;if(!EngineStateCodec::decode(next,restored,error))return false;
 if(restored.compatibility.deviceFlag!=header.deviceFlag||restored.compatibility.anchorX!=header.anchorX||restored.compatibility.anchorY!=header.anchorY){error="opaque compatibility owner mismatch";return false;}
 restored.compatibility=previous;NativeArchive original;if(!EngineStateCodec::archive(original,restored,source.version)||original.bytes!=source.bytes){error="unrelated input state changed during compatibility migration";return false;}
 migrated=next;return true;
}
bool alternateSaveScenario(GameResource& resources,Scenario& scenario,std::string& error){
 std::vector<uint8_t> bytes;
 if(resources.decensorEnabled())return resources.readOriginalResource(82,bytes,error)&&parseScenario(bytes.data(),bytes.size(),scenario,error);
 // Validate the same approved sidecar against the physical disc without
 // enabling it on the live resource owner. Only SP25's verified counterpart
 // is used to rebind saved instruction identities in the reverse direction.
 Common::File patch;if(!patch.open("snatcher-decensor.dat")||patch.size()<=0||patch.size()>1024*1024){error="Loading a decensor save with the option disabled requires snatcher-decensor.dat";return false;}
 std::vector<uint8_t> data(size_t(patch.size()));if(patch.read(data.data(),uint32(data.size()))!=data.size()||patch.err()){error="Cannot read decensor compatibility data";return false;}
 GameResource alternate;
 return openGame(Common::FSNode(ConfMan.getPath("path")),alternate,error)&&alternate.configureDecensor(data,error)&&alternate.readResource(82,bytes,error)&&parseScenario(bytes.data(),bytes.size(),scenario,error);
}
bool captureSaveThumbnail(std::vector<uint8_t>& bytes){
 bytes.clear();Graphics::Surface thumbnail;
 if(!Graphics::createThumbnail(thumbnail))return false;
 Common::MemoryWriteStreamDynamic stream(DisposeAfterUse::YES);
 bool good=Graphics::saveThumbnail(stream,thumbnail)&&!stream.err()&&stream.size()<=48*1024;
 thumbnail.free();if(good)bytes.assign(stream.getData(),stream.getData()+stream.size());return good;
}
Graphics::Surface* decodeSaveThumbnail(const std::vector<uint8_t>& bytes){
 // Bound dimensions and require our standard RGB565 header before allocation.
 if(bytes.size()<22||bytes.size()>48*1024||memcmp(bytes.data(),"THMB",4)||READ_BE_UINT32(bytes.data()+4)!=bytes.size()||bytes[8]!=2)return nullptr;
 unsigned w=READ_BE_UINT16(bytes.data()+9),h=READ_BE_UINT16(bytes.data()+11);
 const byte format[]={2,3,2,3,8,11,5,0,0};
 if(!w||w>160||!h||h>120||bytes.size()!=22+w*h*2||memcmp(bytes.data()+13,format,sizeof(format)))return nullptr;
 Common::MemoryReadStream stream(bytes.data(),uint32(bytes.size()));Graphics::Surface* surface=nullptr;
 if(!Graphics::loadThumbnail(stream,surface)||stream.err()){if(surface){surface->free();delete surface;}return nullptr;}
 return surface;
}
bool nativeSaveDescription(Common::SeekableReadStream& in,std::string& description,Graphics::Surface** thumbnail){
 if(thumbnail)*thumbnail=nullptr;
 byte magic[6];if(in.read(magic,6)!=6||!in.seek(0))return false;
 if(!memcmp(magic,"SNSV",4)&&magic[4]==0&&magic[5]==2){std::vector<uint8_t> preview;std::string error;if(!readNativeSaveMetadata(in,discIdentity,description,thumbnail?&preview:nullptr,error))return false;if(thumbnail)*thumbnail=decodeSaveThumbnail(preview);return true;}
 if(!memcmp(magic,"SGR2",4)){if(in.size()>65545)return false;std::vector<uint8_t> bytes(size_t(in.size()));if(in.read(bytes.data(),uint32(bytes.size()))!=bytes.size())return false;GameSaveRecord record;std::string error;if(!decodeGameSaveMetadata(bytes,record,error))return false;description="Internal Save";if(thumbnail)*thumbnail=decodeSaveThumbnail(record.thumbnail);return true;}
 NativeSaveEnvelope save;std::string error;if(!readNativeSave(in,discIdentity,required(),save,error))return false;const auto* input=section(save,kSaveInput);EngineStateCodec::State state;if(!input||!EngineStateCodec::decode(*input,state,error))return false;description=state.description;if(thumbnail){const auto* preview=section(save,kSaveThumbnail);if(preview&&preview->version==1)*thumbnail=decodeSaveThumbnail(preview->bytes);}return true;
}
bool nativeAutosave(Common::SaveFileManager& manager,const Common::String& file){
 Common::InSaveFile* marker=manager.openForLoading(file+".autosave");if(!marker)return false;
 Common::String expected=marker->readString(0,32);bool valid=marker->size()==32;delete marker;
 Common::InSaveFile* in=manager.openForLoading(file);if(!in)return false;
 valid=valid&&expected==Common::computeStreamMD5AsString(*in);delete in;return valid;
}
static bool markNativeAutosave(Common::SaveFileManager& manager,const Common::String& file){
 Common::InSaveFile* in=manager.openForLoading(file);if(!in)return false;Common::String hash=Common::computeStreamMD5AsString(*in);delete in;
 Common::OutSaveFile* out=manager.openForSaving(file+".autosave",false);if(!out)return false;out->writeString(hash);out->finalize();bool valid=!out->err();delete out;return valid;
}
bool migrateReservedNativeSaves(Common::SaveFileManager& manager,const Common::String& target,std::string& error){
 error.clear();const char* suffixes[]={"",".bak",".ce-achievements.json",".bak.ce-achievements.json",".autosave",".new"};
 for(int slot=0;slot<5;++slot){const Common::String source=Common::String::format("%s.%03d",target.c_str(),slot);
  if(!manager.exists(source)&&!manager.exists(source+".bak"))continue;
  if(slot==0&&nativeAutosave(manager,source))continue;
  Common::InSaveFile* in=manager.openForLoading(source);if(!in)in=manager.openForLoading(source+".bak");byte magic[4]={};if(in){in->read(magic,4);delete in;}
  if(slot>0){bool internal=!memcmp(magic,"SGR2",4);if(!internal){Common::InSaveFile* backup=manager.openForLoading(source+".bak");if(backup){backup->read(magic,4);delete backup;internal=!memcmp(magic,"SGR2",4);}}if(internal)continue;}
  Common::String destination;int destinationSlot=-1;
  for(int freeSlot=5;freeSlot<=99;++freeSlot){Common::String next=Common::String::format("%s.%03d",target.c_str(),freeSlot);bool free=true;for(const char* suffix:suffixes)free=free&&!manager.exists(next+suffix);if(free){destination=next;destinationSlot=freeSlot;break;}}
  if(destination.empty()){error="No free manual slot (5-99) to preserve an existing reserved-slot save";return false;}
  std::vector<Common::String> moved;
  for(const char* suffix:suffixes)if(manager.exists(source+suffix)){
   if(!manager.renameSavefile(source+suffix,destination+suffix,false)){for(auto i=moved.rbegin();i!=moved.rend();++i)manager.renameSavefile(destination+*i,source+*i,false);error="Could not preserve existing reserved-slot save and session metadata";return false;}
   moved.push_back(suffix);
  }
  if(ConfMan.getActiveDomainName()==target&&ConfMan.hasKey("save_slot")&&ConfMan.getInt("save_slot")==slot){
   if(ConfMan.hasKey("save_slot",target)&&ConfMan.getInt("save_slot",target)==slot){ConfMan.setInt("save_slot",destinationSlot,target);ConfMan.flushToDisk();}
   ConfMan.setInt("save_slot",destinationSlot,Common::ConfigManager::kTransientDomain);
  }
 }
 return true;
}
void recordNativeSaveCreated(Common::SaveFileManager& manager,const Common::String& file,int slot,const Common::String& description){
 if(Common::CEAchievements().getCurrentPlaythroughForTarget(ConfMan.getActiveDomainName()).empty()){manager.removeSavefile(file+".ce-achievements.json");return;}
 Common::CEAchievements().recordSaveCreated(file,slot,description);
}
bool SnatcherEngine::saveBoundary(std::string& error)const{
 error.clear();if(!_ready||!_runtime||_fixture||_presentationFixture){error="Native game is not initialized";return false;}
 if(!_audioError.empty()||!_renderError.empty()||!_blocker.empty()){error="Native game has an unresolved subsystem error";return false;}
 // Disk writes execute synchronously on this same engine thread. A save request
 // can never interrupt the actual write; captured logical work remains resumable.
 if(_gameServices.pending||!_gameSaveCaptures.empty()){error="Original save transaction is committing";return false;}
 error=_output.error();return error.empty()&&_fm.canSnapshotState(error);
}
bool SnatcherEngine::canSaveGameStateCurrently(Common::U32String* message){std::string error;bool allowed=saveBoundary(error);if(!allowed&&message)*message=Common::U32String(error.c_str());return allowed;}
bool SnatcherEngine::canLoadGameStateCurrently(Common::U32String*){return _ready&&_runtime;}
bool SnatcherEngine::loadFmAssets(std::string& error){if(_fmLoaded)return true;std::vector<uint8_t> boot,fm,fmAllocation,index,drums,sub;if(!_resources.readBoot(boot,error)||boot.size()<512||!_resources.readFile("FMWR_1.BIN",fm,error)||!_resources.readFileAllocation("FMWR_1.BIN",fmAllocation,error)||!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMDRMDT.BIN",drums,error))return false;std::vector<uint8_t> main(boot.begin()+512,boot.end());if(!_resources.readFile("SUBCODE.BIN",sub,error)||!_fm.load(main,fm,index,drums,error,&sub,&fmAllocation))return false;_fmLoaded=true;_fm.setMono(_pcmMono);_lastTickMillis=_system->getMillis();return true;}
Common::Error SnatcherEngine::saveGameState(int slot,const Common::String& description,bool autosave){
 std::string error;if(slot<0||slot>99||(autosave?slot!=0:slot<5)||description.size()>255||!saveBoundary(error))return failure(_saveError,error.empty()?"Slots 0-4 are reserved; manual saves use slots 5-99":error,Common::kWritingFailed);
 if(!autosave){const Common::String previousOwner=Common::CEAchievements().getSavePlaythroughForFile(name(slot));const Common::String active=Common::CEAchievements().getCurrentPlaythroughForTarget(ConfMan.getActiveDomainName());if(!previousOwner.empty()&&previousOwner!=active)return failure(_saveError,"This manual slot belongs to another Play Session; choose a free slot",Common::kWritingFailed);}
 if(!ensureGameStorage(error))return failure(_saveError,error,Common::kWritingFailed);
 const Common::String target=name(slot);
 if(!writeNativeSnapshot(target,description,error))return failure(_saveError,error,Common::kWritingFailed);
 auto* manager=_system->getSavefileManager();
 if(autosave&&!markNativeAutosave(*manager,target))return failure(_saveError,"Could not record autosave identity",Common::kWritingFailed);
 recordNativeSaveCreated(*manager,target,slot,description);if(autosave)++_testAutosaves;
 _saveError.clear();_lastTickMillis=_system->getMillis();_trace.record(_runtime->cursor(),"native_game_saved",unsigned(slot));return Common::kNoError;
}
bool SnatcherEngine::writeNativeSnapshot(const Common::String& target,const Common::String& description,std::string& error){
 if(!saveBoundary(error))return false;
 _output.setPaused(true);_fm.setPaused(true);NativeSaveEnvelope save;save.format=2;NarrativeSnapshot narrative;NativeSaveSection n,events,scene,timing,audio(kSaveAudio),input,services(kSaveGameServices);TimingSnapshot time;time.clock=_clock.snapshot();time.delay=_delay.snapshot();time.delayToken=_delayToken;
 bool valid=_runtime->snapshot(discIdentity,narrative,error)&&encodeNarrativeState(narrative,n,error)&&encodeEventState(_events.snapshot(),events,error)&&SceneStateCodec::encode(_scene,scene,error,true,true)&&encodeTimingState(time,timing,error)&&_fm.snapshotState(audio.bytes,error)&&EngineStateCodec::capture(*this,description,input,error)&&encodeGameServiceState(_gameServices,services.bytes,error);_fm.setPaused(isPaused());_output.setPaused(isPaused());if(!valid)return false;audio.required=services.required=true;save.sections={n,events,scene,timing,audio,input,services};
 NativeSaveSection metadata(kSaveMetadata);metadata.bytes.assign(description.begin(),description.end());save.sections.push_back(metadata);
 NativeSaveSection preview(kSaveThumbnail);if(captureSaveThumbnail(preview.bytes))save.sections.push_back(preview);
 auto* manager=_system->getSavefileManager();const Common::String pending=target+".new",backup=target+".bak";Common::OutSaveFile* out=manager->openForSaving(pending,false);if(!out){error="Cannot stage native save";return false;}bool written=writeNativeSave(*out,save,discIdentity,required(),error);out->finalize();written=written&&!out->err();delete out;if(!written){manager->removeSavefile(pending);if(error.empty())error="Native save finalization failed";return false;}

 bool old=false;Common::InSaveFile* existing=manager->openForLoading(target);if(existing){old=true;delete existing;manager->removeSavefile(backup);if(!manager->renameSavefile(target,backup,false)){manager->removeSavefile(pending);error="Cannot retain previous native save generation";return false;}}
 if(!manager->renameSavefile(pending,target,false)){if(old)manager->renameSavefile(backup,target,false);error="Cannot publish finalized native save";return false;}
 return true;
}
Common::Error SnatcherEngine::loadGameState(int slot){
 if(slot<0||slot>99||!_ready)return failure(_saveError,"Invalid native load request",Common::kReadingFailed);
 if(slot>=1&&slot<=4){
  GameSaveRecord record;std::string detail;Common::String restoredName;if(readCompatibleOriginalSlot(unsigned(slot),record,detail,&restoredName)){
   if(!_initialSaveLoad)saveAutosaveIfEnabled();
   if(!beginOriginalGameResume(record,originalInputSettings(record),detail))return failure(_saveError,detail,Common::kReadingFailed);
   Common::CEAchievements().recordSaveLoaded(restoredName,slot,Common::String::format("Internal Save Slot %d",slot));return Common::kNoError;
  }
 }
std::string error;auto* manager=_system->getSavefileManager();NativeSaveEnvelope save;bool read=false;Common::String loadedName;for(unsigned generation=0;generation<2&&!read;++generation){const Common::String file=name(slot)+(generation?".bak":"");Common::InSaveFile* in=manager->openForLoading(file);if(in){read=readNativeSave(*in,discIdentity,required(),save,error);delete in;if(read)loadedName=file;}}if(!read)return failure(_saveError,error.empty()?"Native save not found":error,Common::kReadingFailed);
 const auto* n=section(save,kSaveNarrative);const auto* e=section(save,kSaveEvents);const auto* sc=section(save,kSaveScene);const auto* t=section(save,kSaveTimers);const auto* au=section(save,kSaveAudio);const auto* i=section(save,kSaveInput);if(!n||!e||!sc||!t||!au||!i)return failure(_saveError,"Native save missing required owner",Common::kReadingFailed);
 NarrativeSnapshot narrative;EventSnapshot events;TimingSnapshot timing;EngineStateCodec::State state;SceneRuntime scene;ResourceSource source(_resources);DialogueWindow dialogue;std::vector<uint8_t> subcode;EventScheduler stagedEvents=_events;SemanticTrace stagedTrace;NarrativeRuntime stagedRuntime(_scenarios,stagedEvents,stagedTrace);std::vector<EventRebind> bindings;GameClock clock;NarrativeDelay delay;EventToken delayToken;
 if(!decodeNarrativeState(*n,discIdentity,narrative,error))return failure(_saveError,error,Common::kReadingFailed);
 // Only the verified decensor SP25 pair is eligible. All other scenario stamps
 // and every active instruction/return/request still undergo normal checks.
 if(narrative.scenarioStamps[25]!=narrativeScenarioStamp(_scenarios[25])){
  Scenario previous;
  if(!alternateSaveScenario(_resources,previous,error)||!rebindNarrativeScenario(narrative,25,previous,_scenarios[25],error))return failure(_saveError,error,Common::kReadingFailed);
 }
 if(!decodeEventState(*e,events,error)||!decodeTimingState(*t,timing,error)||!EngineStateCodec::decode(*i,state,error)||!EngineStateCodec::hydrate(state,_resources,error)||!SceneStateCodec::decode(*sc,source,scene,error)||!_resources.readFile("SUBCODE.BIN",subcode,error)||!SceneStateCodec::decodeDialogue(state.dialogue,subcode,dialogue,error)||!stagedRuntime.restore(narrative,discIdentity,events,bindings,error)||!restoreTimingState(timing,events,bindings,clock,delay,delayToken,error)||!EngineStateCodec::associations(state,narrative,events,dialogue,_resources,error))return failure(_saveError,error.empty()?"Native save owner association mismatch":error,Common::kReadingFailed);
 bool hospitalMapRecovered=false;if(state.shooting.active&&state.shooting.encounter.mode==5&&!scene.recoverHospitalCombatMap(state.shooting.healthRemaining,subcode,hospitalMapRecovered,error))return failure(_saveError,error,Common::kReadingFailed);
 bool creditsPaletteRecovered=false;if(state.coordinator==5&&!scene.recoverCreditsPalette(creditsPaletteRecovered,error))return failure(_saveError,error,Common::kReadingFailed);
 GameServiceState services;const auto* serviceSection=section(save,kSaveGameServices);
 if(serviceSection){if(!decodeGameServiceState(serviceSection->bytes,services,error))return failure(_saveError,error,Common::kReadingFailed);}
 else{
  // Legacy native starts did not mark DE86's cleared progression owner known.
  // A later source assignment would already have set progressionKnown itself.
  if(!narrative.progressionKnown){if(narrative.progressionSection)return failure(_saveError,"Legacy unknown progression carries a nonzero value",Common::kReadingFailed);narrative.progressionKnown=true;}
  // All older native1 consumers were unsupported. No original slot write could
  // advance their source counter/result; ordinary CE slot numbers are unrelated.
  services.counterKnown=services.resultKnown=true;
 }
 if(!state.anywhere&&!scanGameStorage(*manager,ConfMan.getActiveDomainName(),services,error))return failure(_saveError,error,Common::kReadingFailed);
 // The catalog and PCM bank belong to this open disc and are immutable.
 // Reuse them on warm loads; stage a new copy only before the first PCM use.
 std::vector<PcmCatalogEntry> restoredCatalog=_catalog;std::vector<uint8_t> loadedPcm;const bool loadPcm=restoredCatalog.empty();const std::vector<uint8_t>& restoredPcm=loadPcm?loadedPcm:_pcmData;
 {if(loadPcm){std::vector<uint8_t> index;if(!_resources.readFile("PCMLT_01.BIN",index,error)||!_resources.readFile("PCMLD_01.BIN",loadedPcm,error)||!parsePcmCatalog(index.data(),index.size(),loadedPcm.size(),restoredCatalog,error))return failure(_saveError,error,Common::kReadingFailed);}const PcmCatalogEntry* bank=state.resident==0xffff?nullptr:findPcmEntry(restoredCatalog,state.resident);if(state.resident!=0xffff&&(!bank||bank->mode!=1))return failure(_saveError,"Native resident PCM source identity mismatch",Common::kReadingFailed);}
 PcmCacheFact currentCache,bookmarkCache;
 if(!state.residentCacheSchema){if(!migrateLegacyPcmCache(state.cacheSchema&&state.pcmKnown,state.lastPcm,state.resident,state.bookmark.valid,state.bookmark.stream,restoredCatalog,currentCache,bookmarkCache,error))return failure(_saveError,error,Common::kReadingFailed);state.pcmKnown=currentCache.known;state.lastPcm=currentCache.id;state.bookmark.pcmKnown=bookmarkCache.known;state.bookmark.stream=bookmarkCache.id;}
 else{currentCache.known=state.pcmKnown;currentCache.id=state.lastPcm;bookmarkCache.known=state.bookmark.valid&&state.bookmark.pcmKnown;bookmarkCache.id=state.bookmark.stream;if(!validateResidentCache(currentCache,restoredCatalog,error)||!validateResidentCache(bookmarkCache,restoredCatalog,error))return failure(_saveError,error,Common::kReadingFailed);}
 std::vector<uint8_t> boot;if(!_resources.readBoot(boot,error)||boot.size()<512||!scene.loadGameLayoutPresets(boot.data()+512,boot.size()-512,error)||!scene.loadEnvironmentTables(subcode.data(),subcode.size(),error))return failure(_saveError,error,Common::kReadingFailed);
 std::shared_ptr<ResidentPcmStaging> residentStaging;size_t capacity=65536;for(const auto& entry:restoredCatalog)if(entry.mode==1)capacity=std::max(capacity,entry.length);
 if(state.stagingSchema){if(state.stagingPresent){const auto& stage=state.staging;if(stage.capacity!=capacity||(stage.bank!=0xffff&&stage.bank!=state.resident))return failure(_saveError,"Source allocation capacity or last-resident association mismatch",Common::kReadingFailed);const auto* writer=findPcmEntry(restoredCatalog,stage.writerId);if(stage.writerKind!=kPcmWriterNone){if(!writer||(stage.writerKind==kPcmWriterResident?writer->mode!=1:(writer->mode!=2&&writer->mode!=0x82))||!state.pcmKnown||state.lastPcm!=(stage.writerKind==kPcmWriterResident?stage.writerId:0xffff))return failure(_saveError,"Source allocation writer/cache identity mismatch",Common::kReadingFailed);if(!state.anywhere&&!stage.knownBytes.empty()){if(writer->offset>restoredPcm.size()||writer->length>restoredPcm.size()-writer->offset)return failure(_saveError,"Source allocation writer extent differs from original data",Common::kReadingFailed);std::vector<uint8_t> prefix;if(stage.writerKind==kPcmWriterStream){if(!completedPcmStreamPrefix(restoredPcm.data()+writer->offset,writer->length,prefix,error))return failure(_saveError,error,Common::kReadingFailed);}else prefix.assign(restoredPcm.begin()+writer->offset,restoredPcm.begin()+writer->offset+writer->length);if(stage.knownBytes.size()<prefix.size()||!std::equal(prefix.begin(),prefix.end(),stage.knownBytes.begin()))return failure(_saveError,"Source allocation latest completed prefix differs from original data",Common::kReadingFailed);}}residentStaging.reset(new ResidentPcmStaging(capacity));if(!residentStaging->restore(stage,error))return failure(_saveError,error,Common::kReadingFailed);}}
 else if(state.pcmKnown&&state.lastPcm!=0xffff&&state.lastPcm==state.resident){const auto* bank=findPcmEntry(restoredCatalog,state.resident);if(!bank||bank->mode!=1||bank->offset>restoredPcm.size()||bank->length>restoredPcm.size()-bank->offset)return failure(_saveError,"Restored resident staging source is invalid",Common::kReadingFailed);residentStaging.reset(new ResidentPcmStaging(capacity));if(!residentStaging->replacePrefix(state.resident,restoredPcm.data()+bank->offset,bank->length,error))return failure(_saveError,error,Common::kReadingFailed);}
 if(state.pcmContext.known&&state.pcmContext.copyMode){if(!residentStaging||!state.pcmKnown||(state.pcmContext.mode==1?(residentStaging->bank()!=state.lastPcm||state.staging.writerKind!=kPcmWriterResident):(state.lastPcm!=0xffff||state.staging.writerKind!=kPcmWriterStream)))return failure(_saveError,"PCM copier context differs from its source allocation/cache owner",Common::kReadingFailed);}
 std::shared_ptr<NativePcmBus> restoredPcmBus;if(!_streamOwner.prepareIdleContext(state.pcmContext,residentStaging,restoredPcmBus,error))return failure(_saveError,error,Common::kReadingFailed);

 PointerInteraction pointer;std::vector<uint8_t> hitMap;if(!readInteractionHitMap(subcode.data(),subcode.size(),hitMap,error)||!pointer.loadMap(hitMap,error)||!(state.pointerSchema?pointer.restore(state.pointer,error):pointer.migrateLegacy(state.interaction,error)))return failure(_saveError,error,Common::kReadingFailed);
 if(!state.cdIdleSchema){state.cdIdle=CdIdleSnapshot();state.cdIdle.nextToken=_cd.nextTokenCursor();}
 if(!(state.anywhere?EngineStateCodec::prepareActive(*this,state,residentStaging,subcode,error):_cd.validateIdleSource(state.cdIdle,error)))return failure(_saveError,error,Common::kReadingFailed);
 if(slot!=0&&!_initialSaveLoad)saveAutosaveIfEnabled();
 // Every fallible nonaudio owner and source reload is staged before the audio
 // owner atomically replaces its stream. No callback sees partial story state.
 _output.setPaused(true);
 if(!loadFmAssets(error)||!_fm.restoreState(au->bytes,error,state.cacheSchema?state.primaryObserved:0,state.cacheSchema?state.secondaryObserved:0,state.publicationSchema?state.publicationObserved:0)){_output.setPaused(isPaused());return failure(_saveError,error,Common::kReadingFailed);}
 if(!_output.restore(state.output,error)){_output.setPaused(isPaused());return failure(_saveError,error,Common::kReadingFailed);}
 std::vector<EventRebind> liveBindings;if(!_runtime->restore(narrative,discIdentity,events,liveBindings,error)){_output.setPaused(isPaused());return failure(_saveError,error,Common::kReadingFailed);}
 resetCredits();_cd.reset(error);_cd.commitValidatedIdle(state.cdIdle);_streamOwner.reset();_effectOwner.reset();_audio.reset();_nativeRandom.state=RandomWords(state.randomFirst,state.randomSecond);_nativeRandom.draws=state.randomDraws;_runtime->setRandomSource(&_nativeRandom);_runtime->setQuerySource(this);_pointerInteraction=pointer;_presentationToken=EventToken();_presentationPreparation=SceneResourcePreparation();_presentationLoadPhase=0;_presentationNeedsLoad=false;_presentationData.clear();_bookmark=state.bookmark;_bookmarkToken=EventToken();_bookmarkPhase=0;_bookmarkPreparation=SceneResourcePreparation();_bookmarkNeedsLoad=false;_bookmarkData.clear();_lastPcmCommand=state.lastPcm;_lastPcmKnown=state.pcmKnown;_scriptAudioWait.reset();_scriptAudioToken=EventToken();_primaryMusicCommand=state.primary;_primaryMusicKnown=state.primaryKnown;_primaryCompletionObserved=state.cacheSchema?state.primaryObserved:_fm.snapshot().primaryCompletionSerial;_secondaryCompletionObserved=state.cacheSchema?state.secondaryObserved:_fm.snapshot().secondaryCompletionSerial;_completionPublicationObserved=state.publicationSchema?state.publicationObserved:_fm.snapshot().completionPublicationGeneration;_completionSerialObserved=state.publicationSchema?state.serialObserved:uint8_t(_fm.snapshot().completionPublication&15);_entryCloseView=-1;_textEntry=TextEntry();_entryOpen=_entryPending=_entryKeySaved=false;_shooting=state.shooting;_shootingToken=EventToken();_counterSettlement=ShootingCounterSettlement();_counterToken=EventToken();_shootingHeld=_shootingPressed=_entryGlobalPressed=_entryFramePressed=0;_commonGraphicsInitPending=_textPromptUpdated=false;_commonGraphicsEntry=Cursor();_coordinatorMode=state.coordinator;_interactionMode=state.interaction;_textEntrySession=state.textSession;_textEntryLimit=state.textLimit;_phoneEntryPolicy=state.phonePolicy;_ambientMode=state.ambient;_pointerX=state.pointerX;_pointerY=state.pointerY;_quietPcm=state.quiet;_returnTitleToken=EventToken();_returnTitlePhase=_returnTitleRemaining=0;_returnTitleReady=false;_gameElapsedOrigin=state.elapsedOrigin;_originalInputSettings=StartupSettings();_originalInputSettings.compatibility=state.compatibility;_originalResumePending=false;_pcmRequestToken=EventToken();_pcmRequestPhase=0;_startupRecordReady=false;_gameSaveCaptures.clear();_gameServices=services;_gameStorageScanned=false;_scene=scene;_scene.setRandomSource(&_nativeRandom);_scene.setTrace(&_trace);_sceneAssets=_scene.assets();_sceneAssetsReady=true;_clock=clock;_delay=delay;_delayToken=delayToken;_dialogueToken=EventToken();_commandInput=state.input;_closingCommand=CommandResponse();_music=state.music;_activity=state.activity;_selection=state.selection;_rootSelection=state.rootSelection;_residentBankId=state.resident;_residentStaging=residentStaging;_streamOwner.commitIdleContext(state.pcmContext,restoredPcmBus);_residentStreamPending=0;_pointerAdjusting=false;_catalog.swap(restoredCatalog);if(loadPcm)_pcmData.swap(loadedPcm);_pcmMono=state.mono;_dialogueWindow=dialogue;_dialogueReady=state.dialogueReady;_startupActive=false;_startupCompleted=state.startupComplete;_fixture=_presentationFixture=false;_startupHeld=_startupPressed=0;_startPressed=_workerStartPressed=false;_confirmHeld=false;_pendingEffects.clear();_handled.clear();if(state.dialogueMode){for(const EventRebind& binding:liveBindings)if(binding.previous==state.token){_dialogueToken=binding.current;_handled.push_back(binding.current);}}_audioError.clear();_renderError.clear();_blocker.clear();_startupError.clear();_saveError.clear();if(!state.anywhere){_fm.setPaused(isPaused());_output.setPaused(isPaused());}if(state.legacy)_trace.record(_runtime->cursor(),"legacy_audio_output_migrated",1,2);_lastTickMillis=_system->getMillis();_trace.record(_runtime->cursor(),"native_game_loaded",unsigned(slot));if(hospitalMapRecovered)_trace.record(_runtime->cursor(),"hospital_map_recovered",1);if(creditsPaletteRecovered)_trace.record(_runtime->cursor(),"credits_palette_recovered",1);resetVisualObserver();_mousePositionKnown=false;_mouseAimSelected=false;_entryMousePending=false;_mouseContextIdentity=0;releaseInput();
 if(state.anywhere&&!EngineStateCodec::commitActive(*this,state,liveBindings,residentStaging,error))return failure(_saveError,error,Common::kReadingFailed);
 // Track the generation actually restored; an untagged backup stays untracked.
 Common::CEAchievements().recordSaveLoaded(loadedName,slot);
 return Common::kNoError;
}
Common::JSONValue* SnatcherEngine::saveAnywhereProbe(int slot){
 PauseToken guard=pauseEngine();std::string error;
 _output.setPaused(true);_fm.setPaused(true);_audio.setPaused(true);_cd.setPaused(true);
 auto finish=[&](){_lastTickMillis=_system->getMillis();};
 NativeSaveSection before,after;if(!SceneStateCodec::encode(_scene,before,error,true,true)){finish();return CETestCLI::error(error.c_str());}
 auto story=_runtime->packed();auto clock=_clock.snapshot();auto random=_nativeRandom;const auto sourcePhase=_startup.phase();
 Common::Error saved=saveGameState(slot,"Save anywhere deterministic continuation");if(saved.getCode()!=Common::kNoError){finish();return CETestCLI::error(_saveError.c_str());}
 auto pull=[&](std::vector<int16_t>& out){_fm.setPaused(false);_audio.setPaused(false);_cd.setPaused(false);bool ok=_output.renderOffline(1473,out,error);_fm.setPaused(true);_audio.setPaused(true);_cd.setPaused(true);return ok;};
 std::vector<int16_t> expected,actual;if(!pull(expected)){finish();return CETestCLI::error(error.c_str());}
 Common::Error loaded=loadGameState(slot);if(loaded.getCode()!=Common::kNoError){finish();return CETestCLI::error(_saveError.c_str());}
 if(!SceneStateCodec::encode(_scene,after,error,true,true)||before.bytes!=after.bytes||story!=_runtime->packed()||clock.ticks!=_clock.snapshot().ticks||clock.fraction!=_clock.snapshot().fraction||random.draws!=_nativeRandom.draws||random.state.first!=_nativeRandom.state.first||random.state.second!=_nativeRandom.state.second||sourcePhase!=_startup.phase()){finish();return CETestCLI::error("Save anywhere changed scene, story, clock, RNG or startup phase");}
 if(!pull(actual)||actual!=expected){finish();return CETestCLI::error(error.empty()?"Save anywhere audio continuation differs":error.c_str());}
 loaded=loadGameState(slot);if(loaded.getCode()!=Common::kNoError){finish();return CETestCLI::error(_saveError.c_str());}
 Common::JSONObject result;result["ok"]=new Common::JSONValue(true);result["scene_bytes_equal"]=new Common::JSONValue(true);result["story_clock_rng_equal"]=new Common::JSONValue(true);result["audio_samples_equal"]=new Common::JSONValue(true);result["continuation_frames"]=new Common::JSONValue((long long)1473);long long nonzero=0;for(auto v:expected)if(v)++nonzero;result["nonzero_audio_samples"]=new Common::JSONValue(nonzero);result["test_only"]=new Common::JSONValue(true);finish();return new Common::JSONValue(result);
}

Common::JSONValue* SnatcherEngine::saveOwnerContracts(){std::string error;if(!EngineStateCodec::ownerContracts(*this,error))return CETestCLI::error(error.empty()?"Active owner persistence contract failed":error.c_str());return CETestCLI::ok();}

}
