/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_FM_SYNTH_H
#define SNATCHER_FM_SYNTH_H
#include "audio_fm.h"
namespace Snatcher {
class AudioBackend;
enum FmCore {kFmNuked,kFmGens};
const char* fmCoreName(FmCore core);
bool parseFmCore(const std::string& name,FmCore& core);
// Independent scalar output-domain coupling: reference's mandatory DC removal
// retains511/512 per44100Hz sample. This is separate from the OPN2 generator
// and from optional equalizer/low-pass settings; it models the recorded oracle
// output domain, not a claim about original console analog component values.
class FmOutputCoupling {
public:
 FmOutputCoupling():_input(0),_output(0){}
 void reset(){_input=_output=0;}
 int64_t process(int64_t inputQ16,uint32_t sampleRate);
private:friend class NativeFmSynthRenderer;friend class GensFmSynthRenderer;int64_t _input,_output;
};
// Backend commands for the compatible audio-only six-voice sound generator.
// The independently implemented player owns notes, pitch tables, source timing,
// modulation, levels and priorities. There is no Sega CD platform state here.
struct FmSynthWrite {uint8_t part,address,value;FmSynthWrite(uint8_t p=0,uint8_t a=0,uint8_t v=0):part(p),address(a),value(v){}};
uint8_t fmCarrierMask(uint8_t algorithm);
bool encodeFmInstrument(unsigned channel,const FmInstrument& instrument,std::vector<FmSynthWrite>& writes,std::string& error);
bool encodeFmFrequency(unsigned channel,uint16_t frequency,std::vector<FmSynthWrite>& writes,std::string& error);
bool encodeFmLevels(unsigned channel,const uint8_t levels[4],std::vector<FmSynthWrite>& writes,std::string& error);
bool encodeFmCarrierLevels(unsigned channel,const uint8_t levels[4],uint8_t mask,std::vector<FmSynthWrite>& writes,std::string& error);
bool encodeFmKey(unsigned channel,bool on,std::vector<FmSynthWrite>& writes,std::string& error);
bool encodeFmFastRelease(unsigned channel,std::vector<FmSynthWrite>& writes,std::string& error);
bool encodeFmPan(unsigned channel,uint8_t value,std::vector<FmSynthWrite>& writes,std::string& error);
class FmSynthSink {
public:
 virtual ~FmSynthSink(){}
 virtual bool reset(std::string& error)=0;
 virtual bool instrument(unsigned channel,const FmInstrument& value,std::string& error)=0;
 virtual bool frequency(unsigned channel,uint16_t value,std::string& error)=0;
 virtual bool levels(unsigned channel,const uint8_t values[4],std::string& error)=0;
 virtual bool carrierLevels(unsigned channel,const uint8_t values[4],uint8_t mask,std::string& error)=0;
 virtual bool key(unsigned channel,bool on,std::string& error)=0;
 virtual bool fastRelease(unsigned channel,std::string& error)=0;
 virtual bool pan(unsigned channel,uint8_t value,std::string& error)=0;
 virtual bool lfo(uint8_t value,std::string& error)=0;
 // Pull exact stereo frames. Not pulling preserves all envelope/phase state.
 virtual bool renderPcm(uint32_t frames,std::vector<int16_t>& pcm,std::string& error)=0;
 virtual uint32_t sampleRate()const=0;
};
// Audio-only LGPL Nuked-OPN2 generator. Game data/sequence interpretation stays
// in our player. No CPU, BIOS, disc, graphics or machine callbacks are present.
class NativeFmSynthRenderer:public FmSynthSink {
public:
 explicit NativeFmSynthRenderer(uint32_t sampleRate=44100,bool coupledOutput=true);~NativeFmSynthRenderer() override;
 bool init(std::string& error);
 bool reset(std::string& error) override;
 bool instrument(unsigned,const FmInstrument&,std::string&) override;
 bool frequency(unsigned,uint16_t,std::string&) override;
 bool levels(unsigned,const uint8_t[4],std::string&) override;
 bool carrierLevels(unsigned,const uint8_t[4],uint8_t,std::string&) override;
 bool key(unsigned,bool,std::string&) override;
 bool fastRelease(unsigned,std::string&) override;
 bool pan(unsigned,uint8_t,std::string&) override;
 bool lfo(uint8_t,std::string&) override;
 bool renderPcm(uint32_t,std::vector<int16_t>&,std::string&) override;
 uint32_t sampleRate()const override;
 uint8_t keyOnMask()const;
 uint64_t renderedFrames()const;
 size_t pendingWrites()const;
 bool snapshotState(std::vector<uint8_t>& bytes,std::string& error)const;
 bool restoreState(const std::vector<uint8_t>& bytes,std::string& error,uint64_t expectedFrames=UINT64_MAX);
 void setReplayDiscard(bool discard);
private:
 struct Impl;Impl *_impl;
 bool apply(const std::vector<FmSynthWrite>& writes,std::string& error);
 NativeFmSynthRenderer(const NativeFmSynthRenderer&);NativeFmSynthRenderer& operator=(const NativeFmSynthRenderer&);
};
#ifndef SNATCHER_FM_CORE_ONLY
// Private, device-less CE mixer and audio-only generator above.
// It never attaches its generator to the user's mixer and invokes no game or
// timer callbacks. Caller-owned scheduling pulls PCM for the native music path.
class CeFmSynthRenderer:public FmSynthSink {
public:
 explicit CeFmSynthRenderer(uint32_t sampleRate=44100,bool coupledOutput=true,FmCore core=kFmNuked);~CeFmSynthRenderer() override;
 bool init(std::string& error);
 bool reset(std::string& error) override;
 bool instrument(unsigned,const FmInstrument&,std::string&) override;
 bool frequency(unsigned,uint16_t,std::string&) override;
 bool levels(unsigned,const uint8_t[4],std::string&) override;
 bool carrierLevels(unsigned,const uint8_t[4],uint8_t,std::string&) override;
 bool key(unsigned,bool,std::string&) override;
 bool fastRelease(unsigned,std::string&) override;
 bool pan(unsigned,uint8_t,std::string&) override;
 bool lfo(uint8_t,std::string&) override;
 bool renderPcm(uint32_t,std::vector<int16_t>&,std::string&) override;
 uint32_t sampleRate()const override;
 uint8_t keyOnMask()const;
 // Native percussion is mixed into the same deterministic pull timeline.
 // This is our existing PCM adapter, never the legacy SegaPCM implementation.
 AudioBackend& percussionBackend();
 // Preserve the independently generated FM lane before percussion mixing, so
 // a game PCM-silence event can suppress only undelivered percussion.
 bool renderPcmSeparated(uint32_t frames,std::vector<int16_t>& mixed,std::vector<int16_t>& fm,std::string& error);
 bool snapshotGeneratorState(std::vector<uint8_t>& bytes,std::string& error)const;
 bool restoreGeneratorState(const std::vector<uint8_t>& bytes,std::string& error);
 void setReplayDiscard(bool discard);
private:
 struct Impl;Impl *_impl;
 CeFmSynthRenderer(const CeFmSynthRenderer&);CeFmSynthRenderer& operator=(const CeFmSynthRenderer&);
};
#endif
}
#endif
