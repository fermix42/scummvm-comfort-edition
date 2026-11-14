/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SPEAKER_H
#define NOX_NATIVE_SPEAKER_H
#include "nox/speaker_audio.h"
#include "common/array.h"
#include "nox/resources.h"
#include "nox/native_sound_events.h"
namespace Nox { namespace Native {
// Semantic tone schedules, expressed in the authored oscillator timebase.
// No instruction executor, CPU state, disk bootstrap or IRQ delivery.
class SpeakerEffect {
public:
    static bool refusal(Common::Array<int16> &samples,uint &toggles) {
        Common::Array<int16> result;SpeakerAudio speaker;
        speaker_init(&speaker,collect,&result);
        // GAME910E: two calls of the tone generator, with(33,16),(240,16).
        const uint64 firstEnd=tone(speaker,33,16,51);
        const uint64 end=tone(speaker,240,16,firstEnd+48);
        speaker_advance(&speaker,end+NOX_CPU_HZ/100);
        if (speaker.toggles!=143 || result.empty() || result.size()>48000) return false;
        toggles=uint(speaker.toggles);samples=result;return true;
    }
    static bool cast(HdvArchive &archive,Common::Array<int16> &samples,uint &toggles) {
        Common::Array<byte> notes;if (!archive.readRange("NA/GAME",0x3154,18,notes) || notes.size()!=18) return false;
        Common::Array<int16> result;SpeakerAudio speaker;speaker_init(&speaker,collect,&result);
        // Authored spell-selection chime: nine period/duration pairs. The
        // inter-note cadence is the shared sequencer's fixed service cost.
        uint64 end=0,start=87;
        for (uint i=0;i<18;i+=2) {
            if (!notes[i] || !notes[i+1]) return false;
            end=tone(speaker,notes[i],notes[i+1],start);start=end+68;
        }
        speaker_advance(&speaker,end+20+NOX_CPU_HZ/100);
        if (result.empty() || result.size()>48000) return false;
        toggles=uint(speaker.toggles);samples=result;return true;
    }
    static bool render(HdvArchive &archive,SpeakerCue cue,Common::Array<int16> &samples,uint &toggles) {
        if (cue==kSpeakerRefusal) return refusal(samples,toggles);
        if (cue==kSpeakerSelection) return cast(archive,samples,toggles);
        if (cue==kSpeakerMagic) return alternating(archive,samples,toggles);
        if (cue==kSpeakerProjectile) return sequence(archive,"NA/LWR.MAIN.RTNS",0x13a8,14,samples,toggles,88);
        if (cue==kSpeakerActorHit) return sequence(archive,"NA/BS.AUX.RTN.BK2",0x2710,26,samples,toggles,85);
        if (cue==kSpeakerPartyHit) return partyHit(samples,toggles);
        const uint period=cue==kSpeakerImpact?254:cue==kSpeakerLongLow?240:128;
        const uint duration=cue==kSpeakerImpact?16:cue==kSpeakerLongLow?29:2;
        if (cue!=kSpeakerImpact && cue!=kSpeakerLongLow && cue!=kSpeakerNotification) return false;
        Common::Array<int16> result;SpeakerAudio speaker;speaker_init(&speaker,collect,&result);
        const uint64 end=tone(speaker,period,duration,48);
        speaker_advance(&speaker,end+NOX_CPU_HZ/100);toggles=uint(speaker.toggles);samples=result;return !result.empty();
    }
    static bool sequence(HdvArchive &archive,const Common::String &resource,uint offset,uint count,Common::Array<int16> &samples,uint &toggles,uint64 start=66) {
        Common::Array<byte> notes;if (!count || (count&1) || count>128 || !archive.readRange(resource,offset,count,notes) || notes.size()!=count) return false;
        Common::Array<int16> result;SpeakerAudio speaker;speaker_init(&speaker,collect,&result);uint64 end=0;
        for (uint i=0;i<count;i+=2) {if (!notes[i] || !notes[i+1]) return false;end=tone(speaker,notes[i],notes[i+1],start);start=end+68;}
        speaker_advance(&speaker,end+20+NOX_CPU_HZ/100);toggles=uint(speaker.toggles);samples=result;return !result.empty() && result.size()<=240000;
    }
    static bool alternating(HdvArchive &archive,Common::Array<int16> &samples,uint &toggles) {
        Common::Array<byte> notes;if (!archive.readRange("NA/GAME",0x3176,20,notes) || notes.size()!=20) return false;
        Common::Array<int16> result;SpeakerAudio speaker;speaker_init(&speaker,collect,&result);uint64 end=0,start=90;
        for (uint group=0;group<20;group+=5) {
            if (!notes[group] || !notes[group+1] || !notes[group+2] || !notes[group+3] || !notes[group+4]) return false;
            for (uint repeat=0;repeat<notes[group+4];++repeat) {
                end=tone(speaker,notes[group],notes[group+1],start);
                end=tone(speaker,notes[group+2],notes[group+3],end+61);
                start=end+(repeat+1<notes[group+4]?81:83);
            }
        }
        speaker_advance(&speaker,end+29+NOX_CPU_HZ/100);toggles=uint(speaker.toggles);samples=result;return !result.empty() && result.size()<=240000;
    }
    static bool partyHit(Common::Array<int16> &samples,uint &toggles) {
        Common::Array<int16> result;SpeakerAudio speaker;speaker_init(&speaker,collect,&result);
        uint64 end=0,start=64;
        for (uint repeat=0;repeat<13;++repeat) {
            end=tone(speaker,96,2,start);end=tone(speaker,74,1,end+51);start=end+58;
        }
        speaker_advance(&speaker,end+18+NOX_CPU_HZ/100);toggles=uint(speaker.toggles);samples=result;return !result.empty();
    }
private:
    static void collect(int16 sample,void *context) {static_cast<Common::Array<int16> *>(context)->push_back(sample);}
    static uint64 tone(SpeakerAudio &speaker,uint period,uint duration,uint64 start) {
        const uint iterations=duration*256,periods=(iterations-1)/period;
        // The timer wraps once per256 iterations. Closed arithmetic retains
        // its cadence correction and terminal partial period without stepping
        // the original counting instructions.
        for (uint pulse=0;pulse<=periods;++pulse) {
            const uint elapsed=pulse*period;
            speaker_toggle(&speaker,start+uint64(36)*elapsed-pulse-4*(elapsed/256));
        }
        return start+uint64(36)*iterations-periods-4*duration+7;
    }
};
} }
#endif
