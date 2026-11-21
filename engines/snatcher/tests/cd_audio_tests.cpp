/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../cd_audio.h"
#include <cstdio>
#include <deque>
namespace {
int checks=0,failures=0;void check(bool ok,const char *s){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL CD-DA pump: %s\n",s);}}
struct Source:Snatcher::CdFrameSource {
 uint32_t frames;mutable unsigned reads;mutable std::vector<std::pair<uint32_t,uint32_t> > ranges;
 int64_t failAt;bool shortRead;mutable Snatcher::CdAudioPump *reenter;mutable bool reentryRejected;
 Source(uint32_t count):frames(count),reads(0),failAt(-1),shortRead(false),reenter(nullptr),reentryRejected(false){}
 bool frameCount(unsigned track,uint32_t& count,std::string& e)const override{count=0;if(track!=2){e="fixture has only audio track2";return false;}count=frames;return true;}
 bool readFrames(unsigned track,uint32_t first,uint32_t count,std::vector<uint8_t>& pcm,std::string& e)const override{
  ++reads;ranges.push_back(std::make_pair(first,count));if(reenter){std::string nested;reentryRejected=!reenter->update(nested)&&!nested.empty();reenter=nullptr;}
  if(track!=2||first>frames||count>frames-first||int64_t(first)==failAt){pcm.clear();e="fixture source failure";return false;}
  pcm.resize(size_t(count)*2352);for(uint32_t f=0;f<count;++f)for(unsigned b=0;b<2352;++b)pcm[size_t(f)*2352+b]=uint8_t(first+f+b);if(shortRead)pcm.pop_back();return true;
 }
};
bool sameIdle(const Snatcher::CdIdleSnapshot& a,const Snatcher::CdIdleSnapshot& b){return a.version==b.version&&a.track==b.track&&a.total==b.total&&a.nextFrame==b.nextFrame&&a.lastConsumed==b.lastConsumed&&a.nextToken==b.nextToken&&a.positionKnown==b.positionKnown&&a.inputFinished==b.inputFinished&&a.paused==b.paused;}
struct Backend:Snatcher::CdQueueBackend {
 std::deque<std::vector<uint8_t> > chunks;std::vector<uint8_t> played;
 bool qualified=false;
 bool consumedFrames(uint64_t& frames)const override{frames=played.size()/2352;return qualified;}
 bool consumedSamples(uint64_t& samples)const override{samples=played.size()/4;return qualified;}
 bool running,finished,paused,failBegin,failQueue,immediate;unsigned begins,stops,pauseCalls,finishes;uint8_t volume;
 Backend():running(false),finished(false),paused(false),failBegin(false),failQueue(false),immediate(false),begins(0),stops(0),pauseCalls(0),finishes(0),volume(0){}
 bool begin(uint8_t v,std::string& e)override{++begins;volume=v;finished=false;paused=false;if(failBegin){e="fixture mixer unavailable";return false;}running=true;return true;}
 bool queue(const std::vector<uint8_t>& pcm,std::string& e)override{if(failQueue){e="fixture queue failure";return false;}chunks.push_back(pcm);if(immediate)drain();return true;}
 size_t queuedChunks()const override{return chunks.size();}
 void finishInput()override{finished=true;++finishes;}
 bool active()const override{return running&&(!finished||!chunks.empty());}
 void stop()override{++stops;running=false;chunks.clear();}
 void pause(bool value)override{paused=value;++pauseCalls;}
 void drain(){if(paused||chunks.empty())return;played.insert(played.end(),chunks.front().begin(),chunks.front().end());chunks.pop_front();}
};
}
int runCdAudioTests(){
 using namespace Snatcher;checks=failures=0;std::string e;
 {Source source(300);Backend backend;backend.qualified=true;CdAudioPump original(source,backend,75,2);AudioToken token;check(original.start(2,64,AudioOwner(3,4),9,token,e),"active CD save fixture starts");
  // Consume exactly 601 stereo samples, crossing a sector at a fractional age.
  backend.played.assign(backend.chunks.front().begin(),backend.chunks.front().begin()+601*4);backend.chunks.front().erase(backend.chunks.front().begin(),backend.chunks.front().begin()+601*4);
  CdPlaybackState saved;check(original.capturePlayback(saved,e)&&saved.samples==601,"active CD saves exact delivered sample age");Backend restoredBackend;restoredBackend.qualified=true;CdAudioPump restored(source,restoredBackend,75,2);check(restored.preparePlayback(saved,e)&&restored.commitPlayback(saved,e),"source-validated CD restoration");check(restoredBackend.chunks==backend.chunks,"restored CD preserves partial chunk and producer queue boundaries");CdPlaybackState again;check(restored.capturePlayback(again,e)&&again.samples==601,"repeated CD save retains absolute sample origin");
  restoredBackend.played.assign(restoredBackend.chunks.front().begin(),restoredBackend.chunks.front().begin()+580*4);restoredBackend.chunks.front().erase(restoredBackend.chunks.front().begin(),restoredBackend.chunks.front().begin()+580*4);unsigned track=0;uint64_t frames=0;check(restored.consumedPosition(track,frames)&&frames==2,"sector carry crosses restored fractional sample origin");check(restored.capturePlayback(again,e)&&again.samples==1181,"second snapshot retains combined source and consumer age");
 }
Source source(201);Backend backend;CdAudioPump pump(source,backend,75,2);AudioToken token,other;AudioOwner owner(0x3a,88);CdAudioCompletion completion;
 {
  Source input(300);Backend output;output.qualified=true;CdAudioPump reading(input,output,75,2);AudioToken playing;unsigned track=0;uint64_t frames=0;
  reading.pauseForResourceRead();check(!reading.paused(),"idle presentation read does not invent an audio owner");
  check(reading.start(2,64,owner,1,playing,e),"resource-pause fixture starts real queued audio");output.drain();
  check(reading.consumedPosition(track,frames)&&frames==75,"positive source position precedes resource suspension");
  reading.pauseForResourceRead();unsigned reads=input.reads,pauses=output.pauseCalls;
  check(reading.active()&&reading.paused()&&output.paused,"presentation read pauses the existing transport without replacing its owner");
  reading.setPaused(false);reading.setPaused(true);reading.setPaused(false);reading.pauseForResourceRead();output.drain();
  check(output.paused&&output.pauseCalls==pauses,"host pause updates and repeated read intent cannot resume source suspension");
  check(reading.update(e)&&input.reads==reads&&reading.consumedPosition(track,frames)&&frames==75,"read suspension preserves actual consumed position and performs no refill");
  check(reading.presentationPosition(track,frames)&&track==0&&frames==0,"source-paused presentation clock is known zero");
  CdIdleSnapshot idle;check(!reading.captureIdle(idle,e),"active resource suspension does not expand stable idle persistence");
  check(reading.cancel(playing,e)&&reading.take(completion)&&completion.result==kCdAudioCancelled&&!reading.paused(),"normal cancellation releases read suspension with its transport");
  check(reading.start(2,64,owner,1,playing,e)&&!reading.paused()&&!output.paused,"next explicit transport starts independently of the completed read suspension");
  check(!reading.releaseForResourceRead(e)&&reading.active(),"data read cannot skip its actual preceding pause owner");
  reading.pauseForResourceRead();check(reading.releaseForResourceRead(e)&&!reading.active()&&!reading.paused()&&reading.take(completion)&&completion.result==kCdAudioCancelled,"data-read admission replaces the paused music token through normal completion");
  check(reading.captureIdle(idle,e)&&reading.releaseForResourceRead(e),"settled native read handoff retains supported idle persistence and is idempotent");
  check(reading.start(2,64,owner,1,playing,e),"post-read fixture owns a new transport");
  reading.pauseForResourceRead();check(reading.reset(e)&&!reading.paused()&&!reading.active()&&reading.captureIdle(idle,e),"application reset removes transient read intent without changing idle schema");
 }
 check(pump.start(2,219,owner,7,token,e)&&token.id==1&&token.generation==7,"full track starts with explicit token owner");
 unsigned observedTrack=0;uint64_t observedFrames=0;uint32_t packedClock=0;
 check(!pump.consumedPosition(observedTrack,observedFrames),"unqualified backend cannot invent consumed clock");
 check(!pump.presentationPosition(observedTrack,observedFrames),"playing visual clock remains unknown without consumed position");backend.qualified=true;
 check(pump.consumedPosition(observedTrack,observedFrames)&&observedTrack==2&&observedFrames==0,"queued audio is not consumed audio");
 check(encodeCdRelativeClock(75*60+75*23+45,packedClock,e)&&packedClock==0x01234500,"BCD source relative clock");
 check(encodeCdRelativeClock(0,packedClock,e)&&!packedClock&&!encodeCdRelativeClock(100*60*75,packedClock,e),"clock bounds no rollover");
 check(source.reads==2&&pump.inputFramesQueued()==150&&pump.totalFrames()==201&&backend.queuedChunks()==2&&backend.volume==219,"bounded initial buffering");
 check(!pump.start(2,219,owner,7,other,e)&&other.id==0,"active track replacement is explicit");check(pump.update(e)&&source.reads==2&&!pump.take(completion),"full queue reads nothing");
 backend.drain();check(pump.update(e)&&source.reads==3&&pump.inputFramesQueued()==201&&backend.finishes==1&&pump.active(),"last partial chunk marks input finished but not playback");
 check(pump.presentationPosition(observedTrack,observedFrames)&&observedTrack==2&&observedFrames==75,"playing visual clock uses consumed frames");
 pump.setPaused(true);check(pump.presentationPosition(observedTrack,observedFrames)&&observedTrack==0&&observedFrames==0,"paused visual clock clears to source zero");
 check(pump.consumedPosition(observedTrack,observedFrames)&&observedTrack==2&&observedFrames==75,"visual pause does not erase transport history");pump.setPaused(false);
 check(pump.presentationPosition(observedTrack,observedFrames)&&observedTrack==2&&observedFrames==75,"resumed visual clock returns to actual consumption");
 check(source.ranges[0]==std::make_pair(uint32_t(0),uint32_t(75))&&source.ranges[1]==std::make_pair(uint32_t(75),uint32_t(75))&&source.ranges[2]==std::make_pair(uint32_t(150),uint32_t(51)),"contiguous full track input ranges");
 backend.drain();check(pump.update(e)&&pump.active()&&!pump.take(completion),"completion waits for last queued chunk");backend.drain();check(pump.update(e)&&!pump.active()&&pump.take(completion)&&completion.result==kCdAudioFinished&&completion.token.id==token.id&&completion.owner.resource==0x3a&&completion.owner.node==88,"completion after drain preserves owner");
 check(pump.consumedPosition(observedTrack,observedFrames)&&observedTrack==2&&observedFrames==201,"completed position retained from actual consumed source");
 check(pump.presentationPosition(observedTrack,observedFrames)&&observedTrack==0&&observedFrames==0,"completed visual clock does not reuse retained transport position");
 check(backend.played.size()==size_t(201)*2352,"entire track delivered");bool exact=true;for(unsigned f=0;f<201;++f)for(unsigned b=0;b<2352;++b)if(backend.played[size_t(f)*2352+b]!=uint8_t(f+b))exact=false;check(exact,"full track sample bytes ordered exactly");
 check(pump.update(e)&&!pump.take(completion)&&backend.finishes==1,"no duplicate completion or input finish");check(!pump.cancel(token,e),"completed token stale");
 check(pump.start(2,0,owner,8,token,e)&&token.id==2,"mute volume does not remove logical playback");pump.setPaused(true);unsigned oldReads=source.reads;backend.drain();check(pump.update(e)&&source.reads==oldReads&&backend.paused,"pause preserves buffers and input position");unsigned pauses=backend.pauseCalls;pump.setPaused(true);check(backend.pauseCalls==pauses,"repeated pause is idempotent");pump.setPaused(false);check(!backend.paused,"resume unpauses mixer");
 check(!pump.cancel(AudioToken(token.id,9),e)&&pump.active(),"stale generation cannot cancel");pump.cancelGeneration(9);check(pump.active(),"other generation unaffected");pump.cancelGeneration(8);check(!pump.active()&&pump.take(completion)&&completion.result==kCdAudioCancelled&&completion.token.generation==8,"matching generation cancellation once");pump.cancelGeneration(8);check(!pump.take(completion),"repeated generation cancellation no completion");
 check(pump.start(2,255,owner,9,token,e)&&pump.cancel(token,e),"explicit token cancellation");check(!pump.start(2,255,owner,10,other,e),"pending completion cannot be overwritten");check(pump.take(completion)&&pump.start(2,255,owner,10,other,e),"drain completion permits next generation");check(pump.cancel(other,e)&&pump.take(completion),"new track token independent");
 Source empty(0);Backend emptyBackend;CdAudioPump emptyPump(empty,emptyBackend);check(!emptyPump.start(2,255,owner,1,token,e)&&emptyBackend.begins==0,"empty track rejects before mixer");check(!emptyPump.start(1,255,owner,1,token,e),"data or missing track rejects");
 Source small(5);Backend bad;bad.failBegin=true;CdAudioPump beginFailure(small,bad,2,1);check(!beginFailure.start(2,255,owner,1,token,e)&&!token.id&&!beginFailure.take(completion)&&bad.stops==1,"mixer failure no token and cleaned up");
 Backend queueBad;queueBad.failQueue=true;CdAudioPump queueFailure(small,queueBad,2,1);check(!queueFailure.start(2,255,owner,1,token,e)&&!queueFailure.take(completion)&&queueBad.stops==1,"initial queue failure owns no completion");
 Source shortSource(5);shortSource.shortRead=true;Backend shortBackend;CdAudioPump shortPump(shortSource,shortBackend,2,1);check(!shortPump.start(2,255,owner,1,token,e)&&shortBackend.queuedChunks()==0,"short source range rejects");
 Source failing(5);failing.failAt=2;Backend refill;CdAudioPump refillPump(failing,refill,2,1);check(refillPump.start(2,255,owner,55,token,e),"refill failure fixture starts");refill.drain();check(!refillPump.update(e)&&!e.empty()&&refillPump.take(completion)&&completion.result==kCdAudioFailed&&completion.token.generation==55&&!completion.error.empty(),"mid-track read failure emits owned failure once");check(refillPump.update(e)&&!refillPump.take(completion),"failed pump does not continue");
 Source lostSource(5);Backend lost;CdAudioPump lostPump(lostSource,lost,2,1);check(lostPump.start(2,255,owner,5,token,e),"lost mixer fixture starts");lost.running=false;check(!lostPump.update(e)&&lostPump.take(completion)&&completion.result==kCdAudioFailed,"unexpected mixer stop is not normal completion");
 Source fastSource(100);Backend fast;fast.immediate=true;CdAudioPump fastPump(fastSource,fast,3,2);check(fastPump.start(2,255,owner,1,token,e)&&fastSource.reads==2&&fastPump.inputFramesQueued()==6,"concurrently draining backend cannot make unbounded start work");check(fastPump.update(e)&&fastSource.reads==4&&fastPump.inputFramesQueued()==12,"bounded work per refill");fastPump.cancelGeneration(1);check(fastPump.take(completion),"fast fixture cancels");
 Source recursive(1);Backend recursiveBackend;CdAudioPump recursivePump(recursive,recursiveBackend,1,1);recursive.reenter=&recursivePump;check(recursivePump.start(2,255,owner,1,token,e)&&recursive.reentryRejected,"reentrant source update rejects");recursivePump.cancelGeneration(1);recursivePump.take(completion);
 Source pausedSource(1);Backend pausedBackend;CdAudioPump pausedPump(pausedSource,pausedBackend,1,1);pausedPump.setPaused(true);check(pausedPump.start(2,255,owner,1,token,e)&&pausedBackend.paused&&pausedBackend.pauseCalls==1,"paused startup pauses before queued playback");pausedPump.cancelGeneration(1);pausedPump.take(completion);
 Backend limitsBackend;CdAudioPump invalidLimits(small,limitsBackend,0,1);check(!invalidLimits.start(2,255,owner,1,token,e)&&limitsBackend.begins==0,"invalid queue limits fail before mixer");
 Source resetSource(5);Backend resetBackend;CdAudioPump resetPump(resetSource,resetBackend,2,1);AudioToken retired,fresh;check(resetPump.start(2,255,owner,1,retired,e)&&resetPump.reset(e)&&!resetPump.active()&&!resetPump.take(completion)&&!resetPump.cancel(retired,e),"new session resets active music independent of current narrative generation");check(resetPump.start(2,255,owner,99,fresh,e)&&fresh.id>retired.id&&resetPump.reset(e),"music reset retains monotonic token identity");
 struct ResetBackend:Backend {CdAudioPump* owner=nullptr;bool rejected=false;void stop()override{Backend::stop();if(owner){std::string nested;rejected=!owner->update(nested)&&!nested.empty();}}};Source guardedSource(5);ResetBackend guardedBackend;CdAudioPump guardedPump(guardedSource,guardedBackend,2,1);guardedBackend.owner=&guardedPump;check(guardedPump.start(2,255,owner,5,token,e)&&guardedPump.reset(e)&&guardedBackend.rejected&&!guardedPump.active(),"reset protects backend-stop reentrancy and clears the old session");
 // Idle transport ownership is distinct from the scene's retained published
 // clock. No PCM, completion callback or converter state is restored here.
 Source idleSource(7);Backend idleBackend;idleBackend.qualified=true;CdAudioPump idle(idleSource,idleBackend,3,1);CdIdleSnapshot freshIdle,savedIdle,afterIdle;
 check(idle.captureIdle(freshIdle,e)&&validateCdIdleSnapshot(freshIdle,e)&&!freshIdle.positionKnown&&!freshIdle.track&&!freshIdle.total&&!freshIdle.nextFrame&&!freshIdle.lastConsumed&&!freshIdle.inputFinished&&freshIdle.nextToken==1,"fresh idle snapshot has explicit unknown-zero position");
 check(idle.start(2,64,owner,77,token,e),"idle archive source starts");CdIdleSnapshot sentinel;sentinel.track=999;
 unsigned untouchedStops=idleBackend.stops;check(!idle.captureIdle(sentinel,e)&&sentinel.track==999&&!idle.restoreIdle(freshIdle,e)&&idle.active()&&idleBackend.stops==untouchedStops,"active owner cannot be captured or replaced and failed output remains unchanged");
 check(idle.validateIdleSource(freshIdle,e)&&idle.active(),"source-only validation can precede replacing live owners");
 idleBackend.drain();check(idle.update(e),"idle archive refill1");idleBackend.drain();check(idle.update(e),"idle archive refill2");idleBackend.drain();check(idle.update(e)&&!idle.active(),"idle archive playback completes naturally");
 check(!idle.captureIdle(sentinel,e)&&sentinel.track==999&&!idle.restoreIdle(freshIdle,e),"undelivered completion cannot be lost through idle capture or restore");
 check(idle.take(completion)&&!idle.take(completion)&&idle.captureIdle(savedIdle,e)&&savedIdle.positionKnown&&savedIdle.track==2&&savedIdle.total==7&&savedIdle.nextFrame==7&&savedIdle.lastConsumed==7&&savedIdle.inputFinished&&savedIdle.nextToken==2,"delivered completion retains exact bounded source clock and next-token owner");
 Source coldSource(7);Backend coldBackend;coldBackend.qualified=true;CdAudioPump cold(coldSource,coldBackend,3,1);
 check(cold.validateIdleSource(savedIdle,e)&&cold.restoreIdle(savedIdle,e)&&cold.captureIdle(afterIdle,e)&&sameIdle(savedIdle,afterIdle)&&!cold.active()&&!cold.take(completion)&&coldBackend.chunks.empty()&&coldSource.reads==0,"idle cold restore queues no PCM and emits no duplicate completion");
 check(cold.consumedPosition(observedTrack,observedFrames)&&observedTrack==2&&observedFrames==7&&cold.nextTokenCursor()==2,"restored inactive consumed-position query is source-identical");
 check(cold.presentationPosition(observedTrack,observedFrames)&&observedTrack==0&&observedFrames==0,"restored retained position does not become an active visual clock");
 for(unsigned change=0;change<7;++change){auto malformed=savedIdle;if(change==0)malformed.version=2;if(change==1)malformed.nextToken=0;if(change==2)malformed.track=0;if(change==3)malformed.total=0;if(change==4)malformed.nextFrame=8;if(change==5)malformed.lastConsumed=8;if(change==6)malformed.inputFinished=false;unsigned stops=coldBackend.stops;check(!validateCdIdleSnapshot(malformed,e)&&!cold.restoreIdle(malformed,e)&&cold.captureIdle(afterIdle,e)&&sameIdle(savedIdle,afterIdle)&&coldBackend.stops==stops,"malformed idle metadata rejects transactionally before backend stop");}
 auto unavailable=savedIdle;unavailable.track=3;unsigned oldStops=coldBackend.stops;check(!cold.validateIdleSource(unavailable,e)&&!cold.restoreIdle(unavailable,e)&&cold.captureIdle(afterIdle,e)&&sameIdle(savedIdle,afterIdle)&&coldBackend.stops==oldStops,"unavailable original track rejects without owner mutation");
 Source foreignSource(8);Backend foreignBackend;CdAudioPump foreign(foreignSource,foreignBackend);check(!foreign.restoreIdle(savedIdle,e)&&foreign.captureIdle(afterIdle,e)&&sameIdle(freshIdle,afterIdle)&&!foreignBackend.stops,"foreign track length rejects before backend publication");
 auto invented=freshIdle;invented.track=2;check(!validateCdIdleSnapshot(invented,e)&&!cold.restoreIdle(invented,e),"unknown legacy position cannot invent a source track");
 idle.setPaused(true);check(idle.captureIdle(savedIdle,e)&&savedIdle.paused&&cold.restoreIdle(savedIdle,e)&&cold.captureIdle(afterIdle,e)&&sameIdle(savedIdle,afterIdle),"idle pause control preserves explicit ownership");
 check(cold.start(2,64,owner,88,token,e)&&token.id==2&&coldBackend.paused,"future source playback uses preserved next token and paused state");check(cold.reset(e)&&cold.captureIdle(afterIdle,e)&&!afterIdle.positionKnown&&!afterIdle.track&&!afterIdle.total&&!afterIdle.nextFrame&&!afterIdle.lastConsumed&&!afterIdle.inputFinished&&afterIdle.nextToken==3&&afterIdle.paused,"reset clears only position owners while retaining cursor and pause");
 Backend cancelBackend;cancelBackend.qualified=true;CdAudioPump cancelIdle(idleSource,cancelBackend,3,1);check(cancelIdle.start(2,64,owner,9,token,e),"cancelled idle fixture starts");cancelBackend.drain();check(cancelIdle.update(e)&&cancelIdle.cancel(token,e)&&cancelIdle.take(completion)&&cancelIdle.captureIdle(savedIdle,e)&&savedIdle.lastConsumed==3&&savedIdle.nextFrame==6&&!savedIdle.inputFinished&&cold.restoreIdle(savedIdle,e)&&cold.consumedPosition(observedTrack,observedFrames)&&observedFrames==3,"cancelled delivered owner retains actual partial position without claiming track completion");
 struct MetadataSource:Source {mutable CdAudioPump* checking=nullptr;mutable bool rejected=false;MetadataSource():Source(7){}bool frameCount(unsigned track,uint32_t& count,std::string& error)const override{if(checking){CdIdleSnapshot snapshot;std::string nested;rejected=!checking->captureIdle(snapshot,nested)&&!checking->restoreIdle(snapshot,nested);checking=nullptr;}return Source::frameCount(track,count,error);}} metadataSource;
 Backend metadataBackend;CdAudioPump metadataPump(metadataSource,metadataBackend);metadataSource.checking=&metadataPump;check(metadataPump.validateIdleSource(savedIdle,e)&&metadataSource.rejected&&metadataPump.restoreIdle(savedIdle,e),"source metadata validation protects against reentrant capture and restore");
 guardedBackend.rejected=false;check(guardedPump.restoreIdle(freshIdle,e)&&guardedBackend.rejected,"idle restore protects backend-stop reentrancy");
 auto exhausted=savedIdle;exhausted.nextToken=UINT64_MAX;check(cold.restoreIdle(exhausted,e)&&!cold.start(2,64,owner,9,token,e)&&!token.id&&cold.captureIdle(afterIdle,e)&&afterIdle.nextToken==UINT64_MAX,"exhausted token cursor survives restoration without wrap or reset");
 std::printf("CD-DA pump: %d checks, %d failures\n",checks,failures);return failures;
}
#ifdef SNATCHER_CD_AUDIO_TEST_MAIN
int main(){return runCdAudioTests()?1:0;}
#endif
