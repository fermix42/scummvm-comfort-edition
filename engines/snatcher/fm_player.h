/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_FM_PLAYER_H
#define SNATCHER_FM_PLAYER_H
#include "fm_synth.h"
namespace Snatcher {
struct FmDrumEvent {uint8_t track,id,level5;FmDrumEvent(uint8_t t=0,uint8_t i=0,uint8_t v=0):track(t),id(i),level5(v){}};
struct FmPerformanceData {
 uint16_t frequency[256];uint8_t frequencyStep[256],vibrato[64],panWave[64],pan[4],noise[513];
};
bool readFmPerformanceData(const uint8_t* mainIp,size_t size,FmPerformanceData& data,std::string& error);
bool fmPitchFrequency(const FmPerformanceData& data,uint16_t pitch,uint16_t& frequency,std::string& error);
// Original nonlinear source channel gain before native synthesis carrier TLs.
uint8_t fmChannelLevel(uint8_t noteLevel,uint16_t release,uint16_t volume,int16_t loopVolume,uint8_t musicGain,uint8_t effectGain,bool effect);
struct FmReplayOrigin {
 uint32_t version=1;uint64_t updates=0,primary=0,secondary=0,publicationGeneration=0;
 uint8_t publication=0,musicGain=255,effectGain=255,warmup=0;
 uint16_t dirtyLevelMask=0;bool loaded=false,muted=false,startupHold=false,forceMono=false,valid=true;
};
struct FmTrackState {
 bool pendingReset,active,percussion,effect,dirtyLevel,shortGate,slideLegato;uint8_t command,priority,instrument,tempo,tempoFraction,remaining,duration,gate,gateRemaining,previousGate,noteLevel;
 uint8_t transpose,pan,panPeriod,panCountdown,panWave,panIndex,pmsDelay,pmsCountdown,pms,amsDelay,amsCountdown,ams;
 uint8_t vibratoDelay,vibratoWait,vibratoRate,vibratoPhase,vibratoDepth,vibratoRamp,vibratoRampLeft;
 uint8_t noiseRate,noiseFraction;uint16_t noiseMask,noiseIndex,vibratoDepth16,vibratoStep;
 uint16_t pitch,frequencyPitch,slideTarget,slideOffset;int16_t slideIncrement;uint8_t slideTicks,slideTicksReset,slideDelay,slideDelayReset;
 uint16_t finePitch,volume,release,releaseStep;int16_t volumeStep;uint8_t volumeRamp,volumeTarget;
 uint8_t cachedAlgorithm,cachedLevels[4]; // source25..29 survive command assignment
 FmSequenceReader reader;
 FmTrackState();
};
// Native source musical scheduler. One tick is one caller-owned original
// sound update; this object reads no wall clock or device registers. Six FM
// voices plus two percussion tracks, with native FM-effect overlays4/5.
class FmPlayer {
public:
 FmPlayer();
 bool load(const uint8_t* mainIp,size_t mainSize,const uint8_t* fm,size_t fmSize,std::string& error);
 bool start(uint16_t command,FmSynthSink& synth,std::string& error);
 bool start(uint16_t command,FmSynthSink& synth,bool& accepted,std::string& error);
 bool tick(FmSynthSink& synth,std::vector<FmDrumEvent>& drums,std::string& error);
 bool stop(FmSynthSink& synth,std::string& error);
 void setGains(uint8_t music,uint8_t effects);
 // Archive-selected event publication; old histories retain their exact policy.
 void setSourceResetPublication(bool enabled){_sourceResetPublication=enabled;}
 bool active()const;bool musicActive()const;
 uint16_t activeVoiceMask()const;
 uint64_t semanticDigest()const;
 FmReplayOrigin replayOrigin()const;
 bool restoreReplayOrigin(const FmReplayOrigin& origin,std::string& error);
 uint64_t updates()const{return _updates;}
 uint64_t primaryCompletionSerial()const{return _primaryCompletionSerial;}
 uint64_t secondaryCompletionSerial()const{return _secondaryCompletionSerial;}
 uint64_t completionPublicationGeneration()const{return _completionPublicationGeneration;}
 uint8_t completionPublication()const{return _completionPublication;}
 const FmTrackState* track(unsigned index)const{return index<10?&_tracks[index]:nullptr;}
private:
 friend struct FmCheckpointCodec;
 struct Assets {std::vector<uint8_t> main,fm;std::vector<FmInstrument> instruments;};
 // Tick/start stage mutable musical state, never copies of loaded resources.
 std::shared_ptr<const Assets> _assets;FmPerformanceData _data;
 FmTrackState _tracks[10];bool _loaded,_muted,_startupHold,_forceMono,_sourceResetPublication;uint8_t _musicGain,_effectGain,_warmup;uint64_t _updates,_primaryCompletionSerial,_secondaryCompletionSerial,_completionPublicationGeneration;uint8_t _completionPublication;
 bool visible(unsigned track)const;unsigned physical(unsigned track)const;
 bool step(FmSynthSink& synth,std::vector<FmDrumEvent>& drums,std::string& error);
 bool tickTrack(unsigned index,bool carried,FmSynthSink& synth,std::vector<FmDrumEvent>& drums,std::string& error);
 bool event(unsigned index,const FmSequenceEvent& event,FmSynthSink& synth,std::vector<FmDrumEvent>& drums,bool& timed,std::string& error);
 bool pitch(unsigned index,int32_t modulation,FmSynthSink& synth,std::string& error);
 bool level(unsigned index,FmSynthSink& synth,std::string& error);
 bool pan(unsigned index,FmSynthSink& synth,std::string& error);
 bool resetVoice(unsigned index,FmSynthSink& synth,std::string& error);
 bool muteMusic(FmSynthSink& synth,std::string& error);
 bool key(unsigned index,bool on,FmSynthSink& synth,std::string& error);
};
}
#endif
