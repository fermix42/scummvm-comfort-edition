/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SNATCHER_FM_GENS_H
#define SNATCHER_FM_GENS_H
#include "fm_synth.h"
namespace Snatcher {
// Isolated LGPL Gens2.10/GME synthesis. Same normalized source commands;
// no game decoding, CPU, platform, register-bus timing or media ownership.
class GensFmSynthRenderer:public FmSynthSink {
public:
 explicit GensFmSynthRenderer(uint32_t rate=44100,bool coupled=true);
 ~GensFmSynthRenderer() override;
 bool init(std::string&);bool reset(std::string&) override;
 bool instrument(unsigned,const FmInstrument&,std::string&) override;
 bool frequency(unsigned,uint16_t,std::string&) override;
 bool levels(unsigned,const uint8_t[4],std::string&) override;
 bool carrierLevels(unsigned,const uint8_t[4],uint8_t,std::string&) override;
 bool key(unsigned,bool,std::string&) override;
 bool fastRelease(unsigned,std::string&) override;
 bool pan(unsigned,uint8_t,std::string&) override;
 bool lfo(uint8_t,std::string&) override;
 bool renderPcm(uint32_t,std::vector<int16_t>&,std::string&) override;
 uint32_t sampleRate()const override;uint64_t renderedFrames()const;
 uint8_t keyOnMask()const;void setReplayDiscard(bool);
 bool snapshotState(std::vector<uint8_t>&,std::string&)const;
 bool restoreState(const std::vector<uint8_t>&,std::string&,uint64_t expectedFrames=UINT64_MAX);
private:
 struct Impl;Impl* _impl;
 bool apply(const std::vector<FmSynthWrite>&,std::string&);
 GensFmSynthRenderer(const GensFmSynthRenderer&);GensFmSynthRenderer& operator=(const GensFmSynthRenderer&);
};
}
#endif
