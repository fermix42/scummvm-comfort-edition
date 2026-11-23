/* SPDX-License-Identifier: GPL-3.0-or-later */
static void continuation(NativeArchive& a,Continuation& s,bool compact=false){
#define SAVE_FIELD(type, member) if(a.ok){if(compact&&std::string(#member)=="_startup"){bool needed=s._startupActive;a.flag(needed);s.startupStored=needed;if(needed)owner(a,s.member);}else if(compact&&std::string(#member)=="_creditsData"){if(!a.reading&&(!s._creditsData.empty()||!s.creditsSourceBytes)){s.creditsSourceBytes=uint32_t(s._creditsData.size());s.creditsSourceHash=sourceHash(s._creditsData);}a.field(s.creditsSourceBytes);a.field(s.creditsSourceHash);if(s.creditsSourceBytes>16*1024*1024)a.ok=false;}else owner(a,s.member);if(!a.ok)s.invalidField=#member;}
#include "engine_continuation_fields.inc"
#undef SAVE_FIELD
 if(compact&&s.startupStored!=s._startupActive){a.ok=false;s.invalidField="startup ownership";}
}
static void voice(NativeArchive& a,PcmEffectVoiceSnapshot& v){a.flag(v.metadataKnown);a.flag(v.prefetchOwned);a.flag(v.terminalSeen);a.field(v.sourceCursor);a.field(v.queuedFrames);a.field(v.renderedFrames);a.field(v.droppedUnits);a.field(v.channel);a.field(v.effect);a.field(v.bank);a.field(v.source);a.number(v.payloadOffset);a.number(v.sampleCount);owner(a,v.token);if(v.channel>1||v.source>kResidentSource)a.ok=false;}
static void archiveAudio(NativeArchive& a,ActiveAudio& s){
 a.field(s.next);owner(a,s.streamToken);owner(a,s.busToken);a.field(s.catalog);a.field(s.mode);a.field(s.priority);a.field(s.context.version);a.flag(s.context.known);a.field(s.context.mode);a.field(s.context.copyMode);a.vector(s.bus,32*1024*1024);a.vector(s.stream,32*1024*1024);
 a.objects(s.jobs,8,[&](SavedAudioJob& v){owner(a,v.job.token);owner(a,v.job.owner);a.field(v.job.role);a.flag(v.job.silenced);a.flag(v.job.logical);a.field(v.source);a.field(v.slot);a.field(v.serial);a.vector(v.playback,32*1024*1024);a.vector(v.feed,32*1024*1024);if(v.source>4||v.job.role>kAudioSharedPcm||(v.source==3&&v.slot>1)||(v.source==4&&v.slot>2))a.ok=false;});
 a.objects(s.completions,64,[&](AudioCompletion& v){owner(a,v.token);owner(a,v.owner);a.field(v.result);if(v.result>kAudioCancelled)a.ok=false;});
 for(unsigned i=0;i<2;++i){auto& v=s.effects[i];owner(a,v.token);a.field(v.priority);a.field(v.bank);voice(a,v.voice);a.vector(s.effectFeeds[i],1024*1024);}
 auto& c=s.cd;auto& i=c.idle;a.field(i.version);a.field(i.track);a.field(i.total);a.field(i.nextFrame);a.field(i.lastConsumed);a.field(i.nextToken);a.flag(i.positionKnown);a.flag(i.inputFinished);a.flag(i.paused);a.flag(c.active);a.flag(c.resourcePaused);a.flag(c.completionPending);a.field(c.volume);owner(a,c.token);owner(a,c.owner);owner(a,c.completion.token);owner(a,c.completion.owner);a.field(c.completion.result);a.text(c.completion.error);a.field(c.samples);
 if(!s.next||i.version!=1||!i.nextToken||i.nextFrame>i.total||c.samples>uint64_t(i.nextFrame)*588||c.completion.result>kCdAudioFailed)a.ok=false;
}
static bool captureAudio(SnatcherEngine& e,ActiveAudio& s,std::string& error){
 s.next=e._audio._next;s.streamToken=e._streamOwner._token;s.busToken=e._streamOwner._busToken;s.catalog=e._streamOwner._catalog;s.mode=e._streamOwner._mode;s.priority=e._streamOwner._priority;s.context=e._streamOwner._idleContext;
 auto bus=e._streamOwner._bus;
 if(bus){NativeArchive a(64*1024*1024);if(!bus->archive(a,error)||!a.finished())return false;s.bus.swap(a.bytes);}
 if(e._streamOwner._feed&&!bus){NativeArchive a(64*1024*1024);if(!e._streamOwner._feed->archive(a,error))return false;s.stream.swap(a.bytes);}
 for(unsigned i=0;i<2;++i){s.effects[i]=e._effectOwner._slots[i];if(s.effects[i].feed){NativeArchive a;if(!s.effects[i].feed->archive(a,error))return false;s.effectFeeds[i].swap(a.bytes);}}
 for(const auto& item:e._audio._active){SavedAudioJob v;v.job=item.second;auto f=v.job.feed;
  if(bus&&f==bus)v.source=1;
  else if(f&&f==e._streamOwner._feed)v.source=2;
  else if(f&&bus&&bus->identifyJob(f,v.slot,v.serial))v.source=4;
  else if(f){bool found=false;for(unsigned i=0;i<2;++i)if(f==s.effects[i].feed){v.source=3;v.slot=i;found=true;}if(!found){error="Active audio source has no persistence owner";return false;}}
  if(!v.job.logical&&!e._audioBackend.snapshotPlayback(v.job.handle,v.playback,error))return false;
  s.jobs.push_back(v);
 }
 s.completions.assign(e._audio._complete.begin(),e._audio._complete.end());return e._cd.capturePlayback(s.cd,error);
}
static bool associationsAnywhere(const State& s,const NarrativeSnapshot& n,const EventSnapshot& e,std::string& error){
 const auto& x=s.extra;
 // Requests and their exact source operands are independently checked by
 // NarrativeRuntime::restore. Here bind every active coordinator owner.
 const EventToken tokens[]={s.token,x._creditsToken,x._bookmarkToken,x._presentationToken,x._pcmRequestToken,x._scriptAudioToken,x._shootingToken,x._counterToken,x._returnTitleToken};
 for(EventToken token:tokens)if(token.id){unsigned found=0;for(const auto& event:e.pending)if(event.token==token)++found;if(found!=1){error="Active continuation has no unique scheduler event";return false;}}
 for(EventToken token:x._handled){bool found=false;for(const auto& event:e.pending)if(event.token==token)found=true;
  // Already delivered events can remain in the handled set until the next
  // traversal. They must still belong to this generation and identity range.
  if(!found&&(token.generation!=e.generation||token.id>=e.nextIdentity)){error="Handled event lies outside the saved scheduler";return false;}}
 if(x._shootingToken.id!=0&&!s.shooting.active&&!s.shooting.completed){error="Shooting continuation lacks its coordinator";return false;}
 if(x._startupActive&&n.status!=kRuntimeIdle&&!x._returnTitleToken.id){error="Startup and story ownership conflict";return false;}
 if(s.input._active&&(s.input._references.empty()||s.input._selected>=s.input._references.size())){error="Active command selection is outside its references";return false;}
 return true;
}
static bool prepareActive(SnatcherEngine& e,State& s,const std::shared_ptr<ResidentPcmStaging>& staging,const std::vector<uint8_t>& sub,std::string& error){
 auto& a=s.activeAudio;
 const auto& startup=s.extra._startup;
 if(!startup._main.empty()){
  std::vector<uint8_t> boot,bytes;if(!e._resources.readBoot(boot,error)||boot.size()<512||std::vector<uint8_t>(boot.begin()+512,boot.end())!=startup._main||startup._license._main!=startup._main){error="Startup save source differs from original Main data";return false;}
  const std::vector<uint8_t>* expected[]={&startup._title,&startup._first,&startup._second};const unsigned ids[]={4,3,2};for(unsigned i=0;i<3;++i)if(!e._resources.readResource(ids[i],bytes,error)||bytes!=*expected[i]){error="Startup save source differs from original presentation data";return false;}if(startup._subcode!=sub){error="Startup save SUBCODE source differs";return false;}
 }
 if(!e._cd.preparePlayback(a.cd,error))return false;
 if(!a.bus.empty()){PcmIdleContext context;context.known=true;context.mode=context.copyMode=1;a.restoredBus=std::make_shared<NativePcmBus>(e._audioBackend,staging,context);NativeArchive ar(a.bus,64*1024*1024);if(!a.restoredBus->archive(ar,error)||!ar.finished())return false;}
 if(!a.stream.empty()){a.restoredStream=std::make_shared<PcmStreamFeed>();NativeArchive ar(a.stream,64*1024*1024);if(!a.restoredStream->archive(ar,error,staging)||!ar.finished())return false;}
 for(unsigned i=0;i<2;++i){auto& v=a.effects[i];v.feed.reset();v.bus.reset();if(!a.effectFeeds[i].empty()){v.feed=std::make_shared<PcmResidentFeed>();NativeArchive ar(a.effectFeeds[i]);if(!v.feed->archive(ar,error,staging)||!ar.finished())return false;}else if(v.token.id&&a.restoredBus)v.bus=a.restoredBus;}
 uint64_t last=0;
 for(auto& v:a.jobs){if(!v.job.token.id||v.job.token.id<=last||v.job.token.id>=a.next){error="Invalid ordered PCM token ownership";return false;}last=v.job.token.id;
  v.job.feed.reset();switch(v.source){case 1:v.job.feed=a.restoredBus;break;case 2:v.job.feed=a.restoredStream;break;case 3:v.job.feed=a.effects[v.slot].feed;break;case 4:if(a.restoredBus)v.job.feed=a.restoredBus->restoreJob(v.slot,v.serial);break;}
  if((v.source&&!v.job.feed)||(v.job.logical&&v.source!=4)){error="PCM graph reference has no restored source";return false;}
  uint64_t handle=0;if(!v.job.logical&&!e._audioBackend.restorePlayback(v.job.feed,v.playback,v.job.role,handle,error,true))return false;
 }
 // Reload immutable original shooting tables; only the mutable coordinator is
 // serialized. No previously running encounter is recreated from its beginning.
 if(s.shooting.active&&(!readShootingEncounterData(sub.data(),sub.size(),a.shootingData,error)||!readShootingAimLayout(sub.data(),sub.size(),a.shootingAim,error)||!readShootingFireLayout(sub.data(),sub.size(),a.shootingFire,error)))return false;
 return true;
}
static bool commitActive(SnatcherEngine& e,State& s,const std::vector<EventRebind>& bindings,const std::shared_ptr<ResidentPcmStaging>& staging,std::string& error){
 const uint32_t generation=e._events.generation();
 auto rebind=[&](EventToken& token){if(!token.id)return;for(const auto& b:bindings)if(token==b.previous){token=b.current;return;}token.generation=generation;};
 auto& x=s.extra;
#define SAVE_FIELD(type, field) e.field=x.field;
#include "engine_continuation_fields.inc"
#undef SAVE_FIELD
 EventToken* tokens[]={&e._creditsToken,&e._bookmarkToken,&e._presentationToken,&e._pcmRequestToken,&e._scriptAudioToken,&e._shootingToken,&e._counterToken,&e._returnTitleToken};for(auto t:tokens)rebind(*t);
 for(auto& t:e._handled)rebind(t);for(auto& p:e._pendingEffects)p.generation=generation;for(auto& c:e._gameSaveCaptures)rebind(c.token);rebind(e._originalResumeJob.token);
 e._dialogueToken=s.token;rebind(e._dialogueToken);
 // The GMM opens on a host input edge. Keep accepted game intents and close
 // jobs and accepted one-shot edges. Release physical holds so a load cannot
 // invent continued movement, firing, typing or a new click.
 e._mouseInput.reset();e._commandInput=s.input;e._commandInput._held=0;e._confirmHeld=false;e._pauseKeyHeld=false;e._startupHeld=e._shootingHeld=0;
 std::string mouseError;auto context=e.mouseContext(mouseError);if(mouseError.empty()){e._mouseContextIdentity=context.generation;if(e._mouseCommandPending)e._mouseCommandContext=context.generation;if(e._mouseDialoguePending)e._mouseDialogueContext=context.generation;if(e._mouseStartupPending)e._mouseStartupContext=context.generation;}
 auto& a=s.activeAudio;e._audio._next=a.next;e._audio._paused=true;e._audio._active.clear();e._audio._complete.assign(a.completions.begin(),a.completions.end());
 for(auto& c:e._audio._complete)c.token.generation=generation;
 for(auto& v:a.jobs){auto job=v.job;job.token.generation=generation;job.handle=0;if(!job.logical){if(!e._audioBackend.restorePlayback(job.feed,v.playback,job.role,job.handle,error))return false;e._audioBackend.pause(job.handle,true);}e._audio._active[job.token.id]=job;}
 e._streamOwner._token=a.streamToken;e._streamOwner._token.generation=a.streamToken.id?generation:0;e._streamOwner._busToken=a.busToken;e._streamOwner._busToken.generation=a.busToken.id?generation:0;e._streamOwner._catalog=a.catalog;e._streamOwner._mode=a.mode;e._streamOwner._priority=a.priority;e._streamOwner._idleContext=a.context;e._streamOwner._bus=a.restoredBus;e._streamOwner._feed=a.restoredStream;
 for(unsigned i=0;i<2;++i){e._effectOwner._slots[i]=a.effects[i];auto& v=e._effectOwner._slots[i];if(v.token.id)v.token.generation=generation;if(v.voice.token.id)v.voice.token.generation=generation;}
 if(a.cd.token.id)a.cd.token.generation=generation;if(a.cd.completion.token.id)a.cd.completion.token.generation=generation;if(!e._cd.commitPlayback(a.cd,error))return false;
 e._activity=s.activity;e._music=s.music;e._shooting=s.shooting;if(s.shooting.active){e._shootingData=a.shootingData;e._shootingAim=a.shootingAim;e._shootingFire=a.shootingFire;}e._counterSettlement=x._counterSettlement;
 e._audio.setPaused(e.isPaused());e._cd.setPaused(e.isPaused());e._fm.setPaused(e.isPaused());e._output.setPaused(e.isPaused());
 // The renderer is resized by the next normal draw, without advancing source
 // clocks, running coordinator work, or showing a diagnostic transition frame.
 e._lastTickMillis=e._system->getMillis();return true;
}

static bool ownerContracts(SnatcherEngine& e,std::string& error){
 std::vector<uint8_t> sub,creditsSource;if(!e._resources.readFile("SUBCODE.BIN",sub,error)||!e._resources.readResource(52,creditsSource,error))return false;
 // No live encounter is played or modified. This fixture isolates the missing
 // persistence requirement: target timelines, fire continuation and input locks.
 ShootingCoordinatorState shooting;shooting.active=true;shooting.encounter.initialized=true;shooting.encounter.mode=4;shooting.encounter.spawnPhase=3;shooting.encounter.specialTimer=91;shooting.subphase=7;shooting.timer=23;shooting.fire.phase=4;shooting.fire.shots=17;shooting.aimCell=8;shooting.aimX=197;shooting.aimY=97;shooting.capturedScrollX=-13;shooting.capturedScrollY=29;shooting.inputLocked=true;
 for(unsigned i=0;i<9;++i){auto& t=shooting.encounter.targets[i];t.active=t.markersKnown=true;t.phaseAndType=uint8_t(3+i);t.record=uint16_t(19+i);t.remaining=uint16_t(37+i);t.timer=uint16_t(97+i);t.templateIndex=int16_t(i);for(unsigned j=0;j<8;++j)t.markers[j]=uint8_t(i*8+j);}
 NativeArchive saved;owner(saved,shooting);ShootingCoordinatorState restored;NativeArchive read(saved.bytes);owner(read,restored);if(!saved.finished()||!read.finished()||!restored.active||!restored.inputLocked||restored.subphase!=7||restored.timer!=23||restored.encounter.spawnPhase!=3||restored.encounter.specialTimer!=91||restored.fire.phase!=4||restored.fire.shots!=17||restored.aimCell!=8||restored.aimX!=197||restored.aimY!=97||restored.capturedScrollX!=-13||restored.capturedScrollY!=29){error="Shooting continuation field contract";return false;}
 for(unsigned i=0;i<9;++i){const auto& t=restored.encounter.targets[i];if(!t.active||!t.markersKnown||t.record!=19+i||t.remaining!=37+i||t.timer!=97+i||t.templateIndex!=int(i)){error="Shooting target continuation contract";return false;}for(unsigned j=0;j<8;++j)if(t.markers[j]!=i*8+j)return false;}
 for(bool numeric:{false,true}){
  TextEntry original;TextEntrySettings settings;settings.session=1;settings.limit=18;std::vector<TextEntryChoice> choices;for(unsigned i=0;i<9;++i){TextEntryChoice c;c.reference=uint16_t(100+i);c.label={uint8_t((numeric?'0':'A')+i)};choices.push_back(c);}TextEntryEffects effects;
  if(!original.loadAssets(sub.data(),sub.size(),error)||!original.begin(choices,settings,effects,error)||!original.beginGraphics(false,error))return false;
  for(unsigned tick=0;tick<9;++tick)if(!original.consumeGraphics(uint8_t(tick),error))return false;
  NativeArchive packet;owner(packet,original);TextEntry loaded;NativeArchive archive(packet.bytes);owner(archive,loaded);if(!packet.finished()||!archive.finished()){error="Entry continuation encoding contract";return false;}
  for(unsigned tick=9;tick<40;++tick){TextEntryEffects x,y;if(!original.coordinateGraphics(false,x,error)||!loaded.coordinateGraphics(false,y,error)||!original.consumeGraphics(uint8_t(tick),error)||!loaded.consumeGraphics(uint8_t(tick),error))return false;NativeArchive a,b;owner(a,original);owner(b,loaded);if(!a.finished()||!b.finished()||a.bytes!=b.bytes){error="Entry font worker continuation differs";return false;}}
  TextEntryEffects x,y;if(!original.step(32,32,41,x,error)||!loaded.step(32,32,41,y,error)||original.buffer()!=loaded.buffer()||original.cursor()!=loaded.cursor()||original.awaitingKey()!=loaded.awaitingKey()){error="Entry accepted key continuation differs";return false;}
  if(!original.step(128,128,42,x,error)||!loaded.step(128,128,42,y,error))return false;
  NativeArchive key;owner(key,original);TextEntry pending;NativeArchive keyRead(key.bytes);owner(keyRead,pending);if(!keyRead.finished()||!pending.awaitingKey()||!pending.resumeKey(false,y,error)||!pending.awaitingKey()||!original.resumeKey(true,x,error)||!pending.resumeKey(true,y,error)||original.buffer()!=pending.buffer()||original.cursor()!=pending.cursor()){error="Entry animation barrier lost by save";return false;}
 }
 CreditsTimeline credits;if(!credits.start(creditsSource,true,error))return false;CreditsInput input;input.ready=true;input.cdPlaying=true;input.clockKnown=true;std::vector<CreditsEvent> events;if(!credits.tick(input,events,error))return false;NativeArchive packet;owner(packet,credits);CreditsTimeline loaded;NativeArchive archive(packet.bytes);owner(archive,loaded);if(!packet.finished()||!archive.finished()){error="Credits continuation encoding contract";return false;}for(unsigned tick=0;tick<60;++tick){std::vector<CreditsEvent> a,b;if(!credits.tick(input,a,error)||!loaded.tick(input,b,error)||a.size()!=b.size()){error="Credits continuation event count";return false;}for(unsigned i=0;i<a.size();++i)if(a[i].kind!=b[i].kind||a[i].value!=b[i].value)return false;NativeArchive x,y;owner(x,credits);owner(y,loaded);if(x.bytes!=y.bytes){error="Credits continuation differs";return false;}}
 return true;
}
