/* SPDX-License-Identifier: GPL-3.0-or-later */
// Included inside EngineStateCodec. All fields are semantic owners. No host
// pointers, mixer handles, callback closures or desktop input state are saved.
template<class T> static void owner(NativeArchive& a,T& v){a.field(v);}
static void owner(NativeArchive& a,bool& v){a.flag(v);}
static void owner(NativeArchive& a,std::string& v){a.text(v,1024);}
static void owner(NativeArchive& a,std::vector<uint8_t>& v){a.vector(v,8*1024*1024);}
template<class T>static void owner(NativeArchive& a,std::vector<T>& v){a.objects(v,4096,[&](T& t){owner(a,t);});}
template<class T,size_t N>static void owner(NativeArchive& a,std::array<T,N>& v){for(auto& t:v)owner(a,t);}
static void owner(NativeArchive& a,EventToken& v){a.field(v.id);a.field(v.generation);if(!v.id!=!v.generation)a.ok=false;}
static void owner(NativeArchive& a,AudioToken& v){a.field(v.id);a.field(v.generation);}
static void owner(NativeArchive& a,AudioOwner& v){a.field(v.resource);a.field(v.node);}
static void owner(NativeArchive& a,Cursor& v){a.field(v.resource);a.field(v.node);}
static void owner(NativeArchive& a,SceneResourcePreparation& v){a.field(v.stage);if(v.stage>SceneResourcePreparation::kPrepared)a.ok=false;}
static void owner(NativeArchive& a,LegacyInputHeader& v){a.flag(v.deviceFlag);a.field(v.anchorX);a.field(v.anchorY);}
static void owner(NativeArchive& a,StartupSettings& v){a.field(v.controlMapping);a.flag(v.mono);owner(a,v.compatibility);if(v.controlMapping>=6)a.ok=false;}
static void owner(NativeArchive& a,StartupSavedSlot& v){a.flag(v.known);a.flag(v.present);a.flag(v.metadataKnown);a.field(v.counter);a.field(v.hours);a.field(v.minutes);a.field(v.progression);}
static void owner(NativeArchive& a,StartupStorage& v){a.flag(v.known);a.flag(v.available);owner(a,v.slots);}
static void owner(NativeArchive& a,StartupOptions& v){owner(a,v._settings);a.flag(v._known);a.field(v._row);a.field(v._previous);if(v._row>4)a.ok=false;}
static void owner(NativeArchive& a,StartupSaveSelector& v){owner(a,v._storage);a.flag(v._known);a.field(v._selected);a.field(v._previous);if(v._selected>3)a.ok=false;}
static void owner(NativeArchive& a,OriginalFont& v){a.vector(v._rows,816);a.vector(v._advances,102);a.flag(v._loaded);if(v._loaded&&(v._rows.size()!=816||v._advances.size()!=102))a.ok=false;}
static void owner(NativeArchive& a,StartupCinematicEvent& v){a.field(v.action);a.field(v.value);a.field(v.resource);a.field(v.auxiliary);a.number(v.paletteOffset);a.field(v.id);if(v.action>kStartupCinematicSkip)a.ok=false;}
static void owner(NativeArchive& a,StartupCinematic& v){
 a.objects(v._entries,4096,[&](StartupCinematic::Entry& e){a.field(e.clock);owner(a,e.event);});a.field(v._index);a.field(v._track);a.field(v._nextId);a.flag(v._started);a.flag(v._trackReady);a.flag(v._finished);owner(a,v._pending);if(v._index>v._entries.size())a.ok=false;
}
static void owner(NativeArchive& a,StartupLicense& v){
 owner(a,v._main);std::string error;if(!SceneStateCodec::renderer(a,v._renderer,error))a.ok=false;
 SceneStateCodec::archiveLayout(a,v._layout);
 owner(a,v._colors);owner(a,v._visible);owner(a,v._music);
 a.field(v._prepare);a.field(v._phase);a.field(v._counter);a.field(v._gradient);a.field(v._offset);a.field(v._spriteTile);a.field(v._visibleSpriteTile);a.field(v._scrollX);a.field(v._scrollY);a.field(v._spriteX);a.field(v._visibleScrollX);a.field(v._visibleScrollY);a.field(v._visibleSpriteX);a.flag(v._visibleSprite);a.flag(v._started);a.flag(v._finished);a.flag(v._previouslyPresented);a.flag(v._display);a.flag(v._sprite);
}
static void owner(NativeArchive& a,StartupPresenter& v){
 a.flag(v._mouseSkipRequested);owner(a,v._license);owner(a,v._options);owner(a,v._selector);owner(a,v._storage);owner(a,v._cinematic);owner(a,v._pcOptionsFont);a.field(v._phase);
 owner(a,v._main);owner(a,v._title);owner(a,v._first);owner(a,v._second);owner(a,v._subcode);owner(a,v._tracks);owner(a,v._fm);owner(a,v._pcm);a.field(v._delay);a.field(v._paint);a.flag(v._savedGames);a.flag(v._trackPosted);a.flag(v._soundDriverSuspended);a.flag(v._continueSelected);a.flag(v._cancelLoad);a.field(v._loadedSlot);a.field(v._previousTitle);a.field(v._previousModule);if(v._phase>kStartupNativeLoaded||v._loadedSlot>4)a.ok=false;
}
static void owner(NativeArchive& a,FontProgramState& v){a.number(v.cursor);a.number(v.glyphs);a.field(v.delay);a.field(v.remaining);a.field(v.color);a.field(v.x);a.field(v.y);a.flag(v.fixedWidth);a.flag(v.active);a.flag(v.modeChanged);}
static void owner(NativeArchive& a,TextEntryChoice& v){a.field(v.reference);owner(a,v.label);}
static void owner(NativeArchive& a,TextEntry& v){
 owner(a,v._graphicsSource);owner(a,v._graphicsCanvas);owner(a,v._graphicsNames);owner(a,v._graphicsProgram);
 uint32_t n=uint32_t(v._graphicsQueue.size());a.field(n);if(n>128){a.ok=false;return;}if(a.reading)v._graphicsQueue.clear();for(unsigned i=0;i<n;++i){if(a.reading)v._graphicsQueue.emplace_back(std::vector<uint8_t>());auto& q=v._graphicsQueue[i];owner(a,q.bytes);a.flag(q.blink);}owner(a,v._graphicsFont);
 a.flag(v._graphicsBusy);a.flag(v._graphicsReady);a.flag(v._graphicsInitial);a.flag(v._graphicsBlink);a.flag(v._graphicsUnderline);a.flag(v._graphicsFresh);a.flag(v._graphicsResetCaption);a.flag(v._graphicsPolicy);a.flag(v._graphicsPalette);a.flag(v._graphicsNamesPending);a.flag(v._graphicsClearCanvas);a.flag(v._graphicsClearBuffer);a.field(v._graphicsPhase);a.field(v._graphicsDirtyRows);a.field(v._graphicsView);
 owner(a,v._font);a.flag(v._loaded);a.flag(v._active);a.flag(v._completed);a.field(v._settings.session);a.field(v._settings.limit);a.field(v._settings.phonePolicy);a.field(v._settings.mode);
 owner(a,v._numeric);owner(a,v._alpha);owner(a,v._frames);owner(a,v._phoneLengths);owner(a,v._numericText);owner(a,v._choices);owner(a,v._buffer);owner(a,v._repeat);a.number(v._selected);a.field(v._heldRepeat);a.field(v._pendingKey);a.field(v._cursor);a.field(v._gridIndex);a.field(v._mode);a.field(v._columns);a.field(v._originX);a.field(v._originY);a.field(v._textX);a.field(v._phase);a.field(v._anchor);a.field(v._blinkDeadline);a.flag(v._underline);a.flag(v._blinkStarted);
 if(v._cursor>17||(!v._choices.empty()&&v._selected>=v._choices.size())||v._graphicsFont.cursor>v._graphicsProgram.size()||v._graphicsCanvas.size()>8192||v._graphicsNames.size()>512)a.ok=false;
}
static void owner(NativeArchive& a,CreditsTimeline& v){a.objects(v._records,256,[&](CreditsTimeline::Record& r){a.field(r.handler);a.field(r.deadline);});a.flag(v._active);a.flag(v._finished);a.flag(v._requested);a.flag(v._alternate);a.field(v._index);a.field(v._countdown);a.field(v._sourceStamp);if(v._index < -1||v._index>int(v._records.size()))a.ok=false;}
static void owner(NativeArchive& a,ScriptAudioWait& v){auto s=v.state();a.field(s.version);a.field(s.queue);a.field(s.remaining);a.flag(s.active);if(a.reading){std::string error;if(!v.restore(s,error))a.ok=false;}}
static void owner(NativeArchive& a,AudioActivityWait& v){a.field(v._remaining);a.flag(v._advance);a.flag(v._done);if(v._remaining>3600)a.ok=false;}
static void owner(NativeArchive& a,GameSaveElapsed& v){a.field(v.hours);a.field(v.minutes);a.field(v.seconds);a.field(v.frames);}
static void owner(NativeArchive& a,GameSaveBookmark& v){a.flag(v.known);a.flag(v.quiet);a.flag(v.suppressed);a.field(v.presentation);a.field(v.pcm);a.field(v.music);a.field(v.ambient);a.field(v.interaction);owner(a,v.actors);}
static void owner(NativeArchive& a,GameSaveRecord& v){owner(a,v.disc);owner(a,v.scenarioStamps);owner(a,v.packed);a.flag(v.contextKnown);a.flag(v.sourcePolicy40);a.flag(v.sourcePolicy80);a.field(v.controllerMapping);a.field(v.pointerAnchorX);a.field(v.pointerAnchorY);a.field(v.counter);a.field(v.resource);a.field(v.resumeEntry);a.field(v.progression);owner(a,v.elapsed);owner(a,v.cursorHistory);owner(a,v.bookmark);}
static void owner(NativeArchive& a,GameSaveContext& v){a.flag(v.known);a.flag(v.progressionKnown);a.field(v.entry);a.field(v.progression);owner(a,v.packed);owner(a,v.history);}
static void owner(NativeArchive& a,RuntimeRequest& v){a.field(v.kind);owner(a,v.owner);a.field(v.first);a.field(v.second);owner(a,v.token);a.field(v.queue);owner(a,v.operands);owner(a,v.gameSave);}
static void owner(NativeArchive& a,SnatcherEngine::PendingEffect& v){a.field(v.id);a.field(v.bank);a.field(v.generation);owner(a,v.owner);}
static void bookmark(NativeArchive& a,NativePresentationBookmark& b){a.flag(b.valid);a.field(b.presentation);a.field(b.stream);a.field(b.music);a.field(b.ambient);a.field(b.interaction);a.flag(b.quiet);a.flag(b.suppressed);owner(a,b.actors);a.flag(b.pcmKnown);}
static void owner(NativeArchive& a,SnatcherEngine::GameSaveCapture& v){owner(a,v.token);bookmark(a,v.bookmark);owner(a,v.elapsed);owner(a,v.inputSettings);a.field(v.controllerMapping);a.flag(v.mappingKnown);a.flag(v.sourceContextSupported);a.flag(v.mono);}
static void owner(NativeArchive& a,ShootingTargetState& v){a.flag(v.active);a.flag(v.markersKnown);a.flag(v.cleared);a.field(v.phaseAndType);a.field(v.flags);for(auto& b:v.markers)a.field(b);a.field(v.record);a.field(v.remaining);a.field(v.timer);a.field(v.templateIndex);}
static void owner(NativeArchive& a,ShootingCoordinatorState& v){
 a.flag(v.active);a.flag(v.completed);a.flag(v.aimEnabled);a.flag(v.inputLocked);a.flag(v.allSpecialHit);a.flag(v.absoluteAim);auto& e=v.encounter;
 a.flag(e.initialized);a.flag(e.alternateInput);a.field(e.mode);a.field(e.phase);a.field(e.timing);a.field(e.spawnDelay);a.field(e.spawnPhase);a.field(e.selectedCell);a.field(e.spawned);a.field(e.specialTargets);a.field(e.variant);a.field(e.typeClearHits);a.field(e.typeSetHits);a.field(e.damage);a.field(e.activeTargets);a.field(e.attackPhase);a.field(e.attackCell);a.field(e.cooldown);a.field(e.firePhase);a.field(e.inputGate);a.field(e.inputToggle);a.field(e.specialTimer);a.field(e.modeFourteenLatch);for(auto& t:e.targets)owner(a,t);
 auto& f=v.fire;a.field(f.phase);a.field(f.shots);a.field(f.aimUpdateCounter);a.field(f.modeFourteenLatch);
 a.field(v.subphase);a.field(v.timer);a.field(v.misses);a.field(v.shots);a.field(v.pointerDelay);a.field(v.fireTarget);a.field(v.aimCell);a.field(v.aimX);a.field(v.aimY);a.field(v.aimToggle);a.field(v.healthRemaining);a.field(v.capturedScrollX);a.field(v.capturedScrollY);if(e.mode>=15||v.aimCell>8||e.activeTargets>9||v.healthRemaining>6)a.ok=false;
}
static void owner(NativeArchive& a,ShootingCounterSettlement& v){a.flag(v.active);a.field(v.phase);}
static void owner(NativeArchive& a,CommandResponse& v){a.field(v.kind);a.field(v.reference);a.field(v.index);if(v.kind>kCommandCancelled)a.ok=false;}
