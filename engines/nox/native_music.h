/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MUSIC_H
#define NOX_NATIVE_MUSIC_H
#include "nox/resources.h"
namespace Nox { namespace Native {
// Authored note stream, not original instructions. MOCK.DRIVER020C..0302
// schedules three voices; bank switching, IRQ delivery and VIA polling vanish.
class MusicSequencer {
public:
    MusicSequencer() : _cursor(0),_time(0),_wait(0),_volume(7),_playing(false) {memset(_periods,0,12);memset(_deadlines,0,6);memset(_registers,0,16);}
    bool initialize(HdvArchive &archive,const byte *song,uint size,byte volume=7) {
        if (!song || !size || size>16384 || volume>15) return false;
        Common::Array<byte> periods;if (!archive.readRange("NA/MOCK.DRIVER",625,12,periods)) return false;
        MusicSequencer next;memcpy(next._periods,periods.data(),12);next._song.resize(size);memcpy(next._song.data(),song,size);
        next._volume=volume;next._registers[7]=56;next._registers[12]=64;next._playing=true;*this=next;return true;
    }
    bool tick() {
        if (!_playing) return true;
        if (_wait) {--_wait;return true;}
        MusicSequencer next=*this;
        for (uint commands=0;commands<16384;++commands) {
            if (next._cursor>=next._song.size()) return false;
            const byte command=next._song[next._cursor++];
            if (!command) {--next._cursor;next._playing=false;*this=next;return true;}
            //Use the original resident/main-thread timing. The original's
            //bank-restoration carry can perturb deadlines during auxiliary
            //execution; no CPU bank context belongs in a native song clock.
            const uint16 deadline=uint16(next._time+command);
            if (command&1) {
                //0237 decrements Y, which still contains the full command;
                // the preceding LSR tests its low bit without changing Y.
                next._wait=byte(command-1);
                for (uint voice=0;voice<3;++voice) next._registers[8+voice]=next._deadlines[voice]>=next._time?next._volume:0;
                next._time=deadline;*this=next;return true;
            }
            if (next._cursor>=next._song.size()) return false;
            const byte encoded=next._song[next._cursor++];const uint voice=encoded&3;
            // Source uses voices1..3. Zero aliases its timestamp/mixer scratch;
            // it is absent from the supported authored streams, never guessed.
            if (!voice) return false;
            uint note=encoded>>2,octave=0;while (note>=12) {note-=12;++octave;}
            uint16 period=next._periods[note];
            //The source's first INX precedes its octave comparison, so its
            //shift exponent is2-floor(note/12), not3-floor(note/12).
            if (octave<2) period=uint16(period<<(2-octave));else period>>=octave-2;
            next._registers[(voice-1)*2]=byte(period);next._registers[(voice-1)*2+1]=byte(period>>8);next._deadlines[voice-1]=deadline;
        }
        return false;
    }
    bool playing() const {return _playing;}
    uint cursor() const {return _cursor;}
    byte wait() const {return _wait;}
    byte volume() const {return _volume;}
    const byte *registers() const {return _registers;}
    void setVolume(byte volume) {_volume=MIN<byte>(15,volume);}
    void originalFields(byte *fields) const {
        WRITE_LE_UINT16(fields,_time);for (uint i=0;i<3;++i) WRITE_LE_UINT16(fields+2+i*2,_deadlines[i]);memcpy(fields+8,_registers,16);
    }
    bool save(Common::WriteStream &out) const {
        if (_song.empty() || _cursor>=_song.size()) return false;
        out.writeUint32BE(MKTAG('N','M','U','S'));out.writeUint16LE(1);out.writeUint16LE(_song.size());out.write(_song.data(),_song.size());
        out.writeUint16LE(_cursor);out.writeUint16LE(_time);out.writeByte(_wait);out.writeByte(_volume);out.writeByte(_playing);
        for (uint i=0;i<3;++i) out.writeUint16LE(_deadlines[i]);out.write(_registers,16);return !out.err();
    }
    bool load(Common::SeekableReadStream &in,HdvArchive &archive) {
        if (in.readUint32BE()!=MKTAG('N','M','U','S') || in.readUint16LE()!=1) return false;
        const uint size=in.readUint16LE();if (!size || size>16384 || in.size()-in.pos()!=int64(size+29)) return false;
        Common::Array<byte> song;song.resize(size);if (in.read(song.data(),size)!=size) return false;
        MusicSequencer next;if (!next.initialize(archive,song.data(),size)) return false;
        next._cursor=in.readUint16LE();next._time=in.readUint16LE();next._wait=in.readByte();next._volume=in.readByte();const byte playing=in.readByte();
        for (uint i=0;i<3;++i) next._deadlines[i]=in.readUint16LE();
        if (in.read(next._registers,16)!=16 || in.err() || next._cursor>=size || next._volume>15 || playing>1) return false;
        next._playing=playing!=0;*this=next;return true;
    }
private:
    Common::Array<byte> _song;byte _periods[12];uint16 _cursor,_time,_deadlines[3];byte _wait,_volume,_registers[16];bool _playing;
};
} }
#endif
