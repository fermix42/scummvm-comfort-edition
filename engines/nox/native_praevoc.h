/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PRAEVOC_H
#define NOX_NATIVE_PRAEVOC_H
#include "nox/resources.h"
#include "nox/native_sound_events.h"
namespace Nox { namespace Native {
// Keep69's admitted floor trigger. The caller owns its field50 condition.
class PraevocSpeechRules {
public:
    bool load(HdvArchive &archive) {
        if (!archive.readRange("NA/BS.ROUTINES.BK2",0x2cbd,6,_rates)) return false;
        for (uint i=0;i<6;++i) if (_rates[i]!=0x30 && _rates[i]!=0x50 && _rates[i]!=0x70) return false;
        return true;
    }
    bool baseCue(byte cursor,SoundEvents &events) const {
        if (_rates.size()!=6 || cursor>5) return false;
        events.push_back(SpeakerCue(kCwSpeechFirst|_rates[cursor]));return true;
    }
    bool trigger(byte &phase,byte &cursor,bool &used,bool effectsEnabled,SoundEvents &events) const {
        if (_rates.size()!=6 || cursor>5) return false;
        if (effectsEnabled) {
            byte command=_rates[cursor];
            if (phase&16) {command|=1;events.push_back(SpeakerCue(kCwSpeechFirst|command));command-=16;}
            events.push_back(SpeakerCue(kCwSpeechFirst|command));cursor=cursor?byte(cursor-1):5;used=true;
        }
        phase^=16;return true;
    }
private:
    Common::Array<byte> _rates;
};
} }
#endif
