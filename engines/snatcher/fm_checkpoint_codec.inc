// Explicit music checkpoint. Immutable sequence bytes are rebound to FM assets.
struct FmCheckpointCodec {
 static bool player(NativeArchive& a,FmPlayer& p,std::string& error){
  a.flag(p._loaded);a.flag(p._muted);a.flag(p._startupHold);a.flag(p._forceMono);a.flag(p._sourceResetPublication);
  a.field(p._musicGain);a.field(p._effectGain);a.field(p._warmup);a.field(p._updates);a.field(p._primaryCompletionSerial);a.field(p._secondaryCompletionSerial);a.field(p._completionPublicationGeneration);a.field(p._completionPublication);
  if(p._loaded&&!p._assets){error="Music checkpoint lacks source assets";return false;}
  for(auto& t:p._tracks){
 a.flag(t.pendingReset);
 a.flag(t.active);
 a.flag(t.percussion);
 a.flag(t.effect);
 a.flag(t.dirtyLevel);
 a.flag(t.shortGate);
 a.flag(t.slideLegato);
 a.field(t.command);
 a.field(t.priority);
 a.field(t.instrument);
 a.field(t.tempo);
 a.field(t.tempoFraction);
 a.field(t.remaining);
 a.field(t.duration);
 a.field(t.gate);
 a.field(t.gateRemaining);
 a.field(t.previousGate);
 a.field(t.noteLevel);
 a.field(t.transpose);
 a.field(t.pan);
 a.field(t.panPeriod);
 a.field(t.panCountdown);
 a.field(t.panWave);
 a.field(t.panIndex);
 a.field(t.pmsDelay);
 a.field(t.pmsCountdown);
 a.field(t.pms);
 a.field(t.amsDelay);
 a.field(t.amsCountdown);
 a.field(t.ams);
 a.field(t.vibratoDelay);
 a.field(t.vibratoWait);
 a.field(t.vibratoRate);
 a.field(t.vibratoPhase);
 a.field(t.vibratoDepth);
 a.field(t.vibratoRamp);
 a.field(t.vibratoRampLeft);
 a.field(t.noiseRate);
 a.field(t.noiseFraction);
 a.field(t.noiseMask);
 a.field(t.noiseIndex);
 a.field(t.vibratoDepth16);
 a.field(t.vibratoStep);
 a.field(t.pitch);
 a.field(t.frequencyPitch);
 a.field(t.slideTarget);
 a.field(t.slideOffset);
 a.field(t.slideIncrement);
 a.field(t.slideTicks);
 a.field(t.slideTicksReset);
 a.field(t.slideDelay);
 a.field(t.slideDelayReset);
 a.field(t.finePitch);
 a.field(t.volume);
 a.field(t.release);
 a.field(t.releaseStep);
 a.field(t.volumeStep);
 a.field(t.volumeRamp);
 a.field(t.volumeTarget);
 a.field(t.cachedAlgorithm);
 for(auto& v:t.cachedLevels)a.field(v);
   auto& r=t.reader;bool present=bool(r._bytes);a.flag(present);uint32_t offset=0,length=0;
   if(!a.reading&&present){const auto& source=p._assets->fm;auto found=std::search(source.begin(),source.end(),r._bytes->begin(),r._bytes->end());if(found==source.end()){error="Music sequence has no immutable source";return false;}offset=uint32_t(found-source.begin());length=uint32_t(r._bytes->size());}
   if(present){a.field(offset);a.field(length);if(a.reading){if(!p._assets||!length||offset>p._assets->fm.size()||length>p._assets->fm.size()-offset){error="Music sequence reference exceeds original source";return false;}r._bytes=std::make_shared<const std::vector<uint8_t>>(p._assets->fm.begin()+offset,p._assets->fm.begin()+offset+length);}}
   else if(a.reading)r._bytes.reset();
   a.number(r._cursor);a.number(r._return);a.number(r._phrase);a.number(r._phraseReturn);a.field(r._duration);a.field(r._gate);a.field(r._level);a.field(r._phraseState);a.flag(r._percussion);a.flag(r._ended);a.flag(r._haveReturn);a.flag(r._havePhrase);
   for(unsigned i=0;i<2;++i){a.number(r._loop[i]);a.field(r._repeat[i]);a.field(r._loopPitch[i]);a.field(r._loopVolume[i]);a.flag(r._haveLoop[i]);}
   if((present&&(r._cursor>length||(r._haveReturn&&r._return>length)||(r._havePhrase&&(r._phrase>length||r._phraseReturn>length))||(r._haveLoop[0]&&r._loop[0]>length)||(r._haveLoop[1]&&r._loop[1]>length)))||(t.active&&!present)||t.cachedAlgorithm>7){error="Invalid music track continuation";return false;}
  }return a.ok;
 }
 static bool publication(NativeArchive& a,FmPublicationQueue& q){
  a.field(q._priority);a.objects(q._requests,31,[&](FmPublicationQueue::Request& r){a.field(r.command);a.flag(r.priorityOwner);});return a.ok;
 }
 template<class I> static bool state(NativeArchive& a,I& i,std::string& error){
  a.flag(i.loaded);a.flag(i.paused);a.flag(i.mono);a.flag(i.fmMono);a.flag(i.driverSuspended);a.flag(i.pendingPercussion);a.field(i.publicationVersion);a.field(i.musicGain);a.field(i.effectGain);a.field(i.baseMusic);a.field(i.baseEffect);a.flag(i.baseMono);a.flag(i.baseFmMono);a.flag(i.baseDriver);
  a.field(i.frames);a.field(i.nonzero);a.field(i.drumEvents);a.field(i.remainder);a.field(i.updatesBase);a.field(i.generation);a.number(i.offset);a.vector(i.pending,1472);a.vector(i.pendingFm,1472);a.queue(i.commands,31);
  if(!publication(a,i.publication)||!player(a,i.player,error))return false;
  std::vector<uint8_t> generator;if(!a.reading&&!i.synth.snapshotGeneratorState(generator,error))return false;a.vector(generator,65536);
  if(a.reading&&!i.synth.restoreGeneratorState(generator,error,true))return false;
  // Private percussion voices retain exact memory-feed/resampler positions.
  auto& c=i.controller;auto& backend=static_cast<CeAudioBackend&>(c._backend);a.field(c._next);a.flag(c._paused);uint32_t count=uint32_t(c._active.size());a.field(count);if(count>4){error="Music percussion checkpoint capacity";return false;}
  if(a.reading)c._active.clear();auto it=c._active.begin();
  for(unsigned n=0;n<count;++n){AudioController::Playing job{};std::vector<uint8_t> playback;if(!a.reading){job=(it++)->second;if(job.feed||job.logical||!backend.snapshotPlayback(job.handle,playback,error))return false;}
   a.field(job.token.id);a.field(job.token.generation);a.field(job.owner.resource);a.field(job.owner.node);a.flag(job.silenced);a.vector(playback,1024*1024);
   if(!job.token.id||job.token.id>=c._next){error="Invalid music percussion token";return false;}
   if(a.reading){job.role=kAudioMusic;if(!backend.restorePlayback({},playback,kAudioMusic,job.handle,error))return false;c._active.emplace(job.token.id,job);}
  }
  a.objects(c._complete,64,[&](AudioCompletion& v){a.field(v.token.id);a.field(v.token.generation);a.field(v.owner.resource);a.field(v.owner.node);a.field(v.result);if(v.result>kAudioCancelled)a.ok=false;});
  for(auto& s:i.percussion._slots){a.field(s.token.id);a.field(s.token.generation);a.field(s.priority);if(s.token.id&&c._active.find(s.token.id)==c._active.end()){error="Music percussion slot has no playback owner";return false;}}
  if(i.loaded!=i.player._loaded||i.publicationVersion<4||i.publicationVersion>8||i.remainder>=599227434||i.offset>i.pending.size()||(i.offset&1)||i.pendingFm.size()!=i.pending.size()||i.nonzero>i.frames){error="Invalid music output continuation";return false;}
  return a.ok;
 }
};
