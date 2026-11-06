/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_AUDIO_H
#define NOX_NATIVE_AUDIO_H
#include "nox/native_music.h"
#include "nox/native_random.h"
#include "nox/mockingboard.h"
#include "nox/speaker_audio.h"
namespace Nox { namespace Native {
// Sample-driven music service. The retained AY master oscillator is synthesis,
// not CPU scheduling: there are no registers/PCs, instruction or IRQ callbacks.
class MusicPlayback {
public:
    MusicPlayback() : _ready(false),_battleCue(false),_cycle(0),_nextTick(9730),_fraction(0),_samples(0),_nonzero(0) {mock_init(&_synth);}
    bool start(HdvArchive &archive,const byte *song,uint size,byte volume=7) {
        MusicPlayback next;if (!next._music.initialize(archive,song,size,volume)) return false;
        next._ready=true;*this=next;return true;
    }
    bool splash(HdvArchive &archive) {Common::Array<byte> song;return archive.read("NA/MUSIC.SPLASH",song)&&start(archive,song.data(),song.size());}
    bool combat(HdvArchive &archive,GameplayRandom &random,byte map,byte sprite) {
        if (_battleCue) return true;
        Common::Array<byte> source,song;GameplayRandom nextRandom=random;
        if ((map==0 || map==8) && sprite==133) {
            // GAME8ACF short-circuits the generic loader for the Shadow.
            if (!archive.read("NA/GAME",source) || source.size()<0x2b55 || !descriptorSong(archive,source.data()+0x2b4c,song)) return false;
        } else {
            Common::Array<byte> loader,combat;
            if (!archive.read("NA/LOADER.P",loader) || loader.size()<10121 || !archive.read("NA/SRTN.COMBAT.S",combat)) return false;
            const byte *entry=loader.data()+9927+137+9*4;
            const uint directory=entry[0]|(uint(entry[1])<<8)|(uint(entry[2])<<16);
            if (directory>combat.size() || combat.size()-directory<15) return false;
            byte draw;if (!nextRandom.next(1,191,draw)) return false;
            const byte *record=combat.data()+directory+uint(draw>>6)*5;
            const uint offset=record[0]|(uint(record[1])<<8)|(uint(record[2])<<16),size=READ_LE_UINT16(record+3);
            if (!size || size>16384 || offset>combat.size() || size>combat.size()-offset) return false;
            song.resize(size);memcpy(song.data(),combat.data()+offset,size);
        }
        MusicPlayback next;if (!next.start(archive,song.data(),song.size())) return false;
        next._battleCue=true;*this=next;random=nextRandom;return true;
    }
    bool battleCue() const {return _battleCue;}
    void leaveCombat() {_battleCue=false;}
    bool revelation(HdvArchive &archive) {
        Common::Array<byte> game,song;
        if (!archive.read("NA/GAME",game) || game.size()<0x2b55 || !descriptorSong(archive,game.data()+0x2b4c,song)) return false;
        MusicPlayback next;if (!next.start(archive,song.data(),song.size())) return false;
        next._battleCue=true;*this=next;return true;
    }
    bool ending(HdvArchive &archive) {
        // R:9938 loads the nine-byte A2AE resource descriptor before starting
        // music. Decode that data reference; no overlay execution is involved.
        Common::Array<byte> combat,song;
        return archive.read("NA/SRTN.COMBAT.S",combat) && combat.size()>=17587 && descriptorSong(archive,combat.data()+17578,song) && start(archive,song.data(),song.size());
    }
    bool ready() const {return _ready;}
    bool playing() const {return _ready&&_music.playing();}
    uint64 samples() const {return _samples;}
    uint64 nonzero() const {return _nonzero;}
    bool render(int16 *samples,uint count) {
        if (!samples || count>48000) return false;
        for (uint i=0;i<count;++i) {
            _fraction+=NOX_CPU_HZ;const uint64 cycle=_cycle+_fraction/NOX_AUDIO_HZ;_fraction%=NOX_AUDIO_HZ;
            while (_ready&&_nextTick<=cycle) {
                const bool update=_music.playing()&&!_music.wait();
                if (!_music.tick()) return false;
                if (update&&_music.playing()) for (uint chip=0;chip<2;++chip) for (uint reg=0;reg<16;++reg) mock_set_ay_register(&_synth,chip,reg,_music.registers()[reg],_nextTick);
                _nextTick+=9730;
            }
            const int sample=_ready?mock_sample(&_synth,cycle):0;
            samples[i]=int16(CLIP<int>(sample,-32768,32767));_cycle=cycle;++_samples;_nonzero+=samples[i]!=0;
        }
        return true;
    }
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','A','U','D'));out.writeUint16LE(2);out.writeByte((_ready?1:0)|(_battleCue?2:0));
        if (!_ready) return !out.err();
        Common::MemoryWriteStreamDynamic song(DisposeAfterUse::YES);if (!_music.save(song)) return false;
        out.writeUint32LE(song.size());out.write(song.getData(),song.size());out.writeUint64LE(_cycle);out.writeUint64LE(_nextTick);out.writeUint32LE(_fraction);out.writeUint64LE(_samples);out.writeUint64LE(_nonzero);
        for (uint chip=0;chip<2;++chip) {
            const NoxAY &a=_synth.ay[chip];out.write(a.reg,16);out.writeByte(a.selected);out.write(a.tone,3);out.writeByte(a.noise);out.writeByte(a.attack);out.writeByte(a.holding);out.writeByte(a.env_step);
            for (uint i=0;i<3;++i) out.writeUint32LE(a.tone_count[i]);out.writeUint32LE(a.noise_count);out.writeUint32LE(a.env_count);out.writeUint32LE(a.lfsr);out.writeUint64LE(a.tick);out.writeUint64LE(a.writes);
        }
        uint64 bits;memcpy(&bits,&_synth.last_input,8);out.writeUint64LE(bits);memcpy(&bits,&_synth.last_output,8);out.writeUint64LE(bits);return !out.err();
    }
    bool load(Common::SeekableReadStream &in,HdvArchive &archive) {
        if (in.readUint32BE()!=MKTAG('N','A','U','D')) return false;
        const uint version=in.readUint16LE();if (version!=1 && version!=2) return false;
        const byte flags=in.readByte();if (flags>(version==1?1:3) || flags==2) return false;const bool ready=(flags&1)!=0;MusicPlayback next;next._battleCue=(flags&2)!=0;
        if (!ready) {if (in.err() || in.pos()!=in.size()) return false;*this=next;return true;}
        const uint size=in.readUint32LE();if (!size || size>16464 || size>in.size()-in.pos()) return false;
        Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size) return false;
        Common::MemoryReadStream song(bytes.data(),size);if (!next._music.load(song,archive)) return false;
        next._cycle=in.readUint64LE();next._nextTick=in.readUint64LE();next._fraction=in.readUint32LE();next._samples=in.readUint64LE();next._nonzero=in.readUint64LE();
        if (next._fraction>=NOX_AUDIO_HZ || next._nextTick<=next._cycle || next._nextTick-next._cycle>9730 || next._nonzero>next._samples || next._cycle>uint64(NOX_CPU_HZ)*86400*365) return false;
        for (uint chip=0;chip<2;++chip) {
            NoxAY &a=next._synth.ay[chip];if (in.read(a.reg,16)!=16) return false;a.selected=in.readByte();if (in.read(a.tone,3)!=3) return false;
            a.noise=in.readByte();a.attack=in.readByte();a.holding=in.readByte();a.env_step=in.readByte();
            for (uint i=0;i<3;++i) a.tone_count[i]=in.readUint32LE();a.noise_count=in.readUint32LE();a.env_count=in.readUint32LE();a.lfsr=in.readUint32LE();a.tick=in.readUint64LE();a.writes=in.readUint64LE();
            if (a.selected>15 || a.noise>1 || (a.attack!=0&&a.attack!=15) || a.holding>1 || a.env_step>15 || a.tick!=next._cycle/8) return false;
            for (uint i=0;i<3;++i) if (a.tone[i]>1 || !a.tone_count[i] || a.tone_count[i]>4095) return false;
            if (!a.noise_count || a.noise_count>62 || !a.env_count || a.env_count>131070 || !a.lfsr || a.lfsr>131071) return false;
        }
        uint64 bits=in.readUint64LE();memcpy(&next._synth.last_input,&bits,8);bits=in.readUint64LE();memcpy(&next._synth.last_output,&bits,8);
        if (in.err() || in.pos()!=in.size() || !(next._synth.last_input>=-32768&&next._synth.last_input<=32768) || !(next._synth.last_output>=-65536&&next._synth.last_output<=65536)) return false;
        next._ready=true;*this=next;return true;
    }
private:
    static bool descriptorSong(HdvArchive &archive,const byte *descriptor,Common::Array<byte> &song) {
        Common::Array<byte> game,resource;if (!archive.read("NA/GAME",game)) return false;
        const uint pointer=READ_LE_UINT16(descriptor);
        if (pointer<0x6000 || pointer-0x6000>=game.size() || READ_LE_UINT16(descriptor+2)!=0x0400) return false;
        const uint at=pointer-0x6000,length=game[at];if (!length || length>31 || at+1+length>game.size()) return false;
        Common::String name="NA/";for (uint i=0;i<length;++i) name+=char(game[at+1+i]);
        const uint offset=descriptor[4]|(uint(descriptor[5])<<8)|(uint(descriptor[6])<<16),size=READ_LE_UINT16(descriptor+7);
        if (!size || size>16384 || !archive.read(name,resource) || offset>resource.size() || size>resource.size()-offset) return false;
        song.resize(size);memcpy(song.data(),resource.data()+offset,size);return true;
    }
    bool _ready,_battleCue;MusicSequencer _music;Mockingboard _synth;uint64 _cycle,_nextTick;uint _fraction;uint64 _samples,_nonzero;
};
} }
#endif
