/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_CWSPEECH_H
#define NOX_NATIVE_CWSPEECH_H
#include "nox/resources.h"
#include "nox/speaker_audio.h"
namespace Nox { namespace Native {
// Two authored period waveforms. Read data only, never the preceding program.
class CwSpeechEffect {
public:
    static bool schedule(HdvArchive &archive,byte command,Common::Array<uint64> &edges,uint64 &end) {
        const byte word=command&15,rate=command>>4;
        if ((word==0?rate!=3 && rate!=5 && rate!=7:word!=1 || rate<2 || rate>7)) return false;
        Common::Array<byte> data;if (!archive.readRange("NA/CWSPEAK",0x76,0x695,data)) return false;
        uint headers[2],following[2],cursor=0;
        for (uint i=0;i<2;++i) {
            if (cursor+3>=data.size() || !data[cursor]) return false;
            headers[i]=cursor;following[i]=cursor+READ_LE_UINT16(data.data()+cursor+1);
            if (following[i]<=cursor+2 || following[i]>=data.size() || data[following[i]-1]!=255) return false;
            cursor=following[i];
        }
        if (cursor!=data.size()-1 || data[cursor]) return false;
        Common::Array<uint64> pulses;uint64 time=126+51*word;cursor=headers[word]+2;
        const uint coefficient=5*rate+12;
        while (cursor<following[word] && data[cursor]!=255) {
            const byte period=data[cursor];time+=uint64(coefficient)*(period?period:256);
            if (period!=254) pulses.push_back(time);
            time+=period==254?37:56;++cursor;
            // Fixed cadence at the source waveform's data-block boundaries.
            if (!((cursor+0x75)&255)) time+=4;
        }
        if (cursor!=following[word]-1 || pulses.empty() || time+17>uint64(NOX_CPU_HZ)*2) return false;
        edges=pulses;end=time+17;return true;
    }
    static bool render(HdvArchive &archive,byte command,Common::Array<int16> &samples,uint &toggles) {
        Common::Array<uint64> edges;uint64 end;if (!schedule(archive,command,edges,end)) return false;
        Common::Array<int16> result;SpeakerAudio speaker;speaker_init(&speaker,collect,&result);
        for (uint i=0;i<edges.size();++i) speaker_toggle(&speaker,edges[i]);
        speaker_advance(&speaker,end+NOX_CPU_HZ/100);
        if (result.empty() || result.size()>NOX_AUDIO_HZ*2 || speaker.toggles!=edges.size()) return false;
        samples=result;toggles=uint(speaker.toggles);return true;
    }
private:
    static void collect(int16 sample,void *context) {static_cast<Common::Array<int16> *>(context)->push_back(sample);}
};
} }
#endif
