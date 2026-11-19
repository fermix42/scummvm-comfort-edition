/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TITLE_H
#define NOX_NATIVE_TITLE_H
#include "nox/native_lz.h"
#include "nox/native_graphics.h"
#include "nox/speaker_audio.h"
namespace Nox { namespace Native {
// LOGO1211's reverse aPLib packet grammar. Data lengths and references are
// bounded; neither the original decompressor nor title program is executed.
class TitlePacket {
public:
    static bool decode(const byte *data,uint size,uint count,Common::Array<byte> &out) {
        if (!data || !size || !count || count>32768) return false;
        TitlePacket p(data,size,count);if (!p.emit(p._in.readByte())) return false;
        bool matchBefore=false;uint old=0;
        while (p._in.ok()) {
            if (!p._in.readBit()) {if (!p.emit(p._in.readByte())) return false;matchBefore=false;}
            else if (!p._in.readBit()) {
                uint high;if (!p.gamma(high) || high<(matchBefore?1U:2U)) return false;
                high-=matchBefore?1:2;uint distance=old,length;
                if (!high) {if (!p.gamma(length)) return false;}
                else {
                    distance=((high-1)<<8)|p._in.readByte();if (!p.gamma(length)) return false;
                    if (distance>=32000) ++length;if (distance>=1280) ++length;if (distance<128) length+=2;
                }
                if (!p.copy(distance,length)) return false;old=distance;matchBefore=true;
            } else if (!p._in.readBit()) {
                const byte value=p._in.readByte();const uint distance=value>>1;
                if (!distance) {if (!p._in.ok() || p._in.remaining() || p._destination) return false;out=p._out;return true;}
                if (!p.copy(distance,2+(value&1))) return false;old=distance;matchBefore=true;
            } else {
                uint distance=0;for (uint i=0;i<4;++i) distance=(distance<<1)|uint(p._in.readBit());
                if (!(distance?p.copy(distance,1):p.emit(0))) return false;matchBefore=false;
            }
        }
        return false;
    }
private:
    TitlePacket(const byte *data,uint size,uint count) : _in(data,size),_destination(count) {_out.resize(count);}
    bool gamma(uint &value) {
        value=1;do {value=(value<<1)|uint(_in.readBit());if (!_in.ok() || value>65535) return false;} while (_in.readBit());return _in.ok();
    }
    bool emit(byte value) {if (!_destination || !_in.ok()) return false;_out[--_destination]=value;return true;}
    bool copy(uint distance,uint count) {
        if (!distance || distance>_out.size()-_destination || count>_destination) return false;
        while (count--) {const byte value=_out[_destination+distance-1];if (!emit(value)) return false;}return true;
    }
    ReversePacketReader _in;Common::Array<byte> _out;uint _destination;
};
// Typed ownership of the original title's delta pictures and pulse-width voice.
// LOGO091D..1210 combines immutable image records and 101-cycle audio cells.
// Only image offsets, page selection and audio cells remain; no CPU/ABI/IRQ.
class TitlePresentation {
public:
    TitlePresentation() : _sprite(0),_audio(0),_index(0),_frame(0),_token(45),_page(1),_rewound(false),_finished(false),_auxiliary(true),_cycle(0),_cells(0),_menuStamp(0) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> logo;if (!archive.read("NA/LOGO",logo) || logo.size()!=23757) return false;
        TitlePresentation next;
        if (!TitlePacket::decode(logo.data()+0x37c9,0x2504,18432,next._main) ||
            !TitlePacket::decode(logo.data()+0x2a75,0xd54,9472,next._tables) ||
            !TitlePacket::decode(logo.data()+0x1cfd,0xd78,13824,next._secondVoice) ||
            !TitlePacket::decode(logo.data()+0xdec,0xf11,10744,next._pictures)) return false;
        next._planes.resize(32768);memset(next._planes.data(),0,next._planes.size());
        // LOGO145F/1478: the two authored initial bitmap rectangles, page2.
        if (!next.sprite(21,86,5,29,29,1,1) || !next.sprite(147,169,30,5,6,0,1)) return false;
        for (uint bank=0;bank<2;++bank) memcpy(next._planes.data()+bank*16384,next._planes.data()+bank*16384+8192,8192);
        *this=next;return true;
    }
    bool advance(uint64 cycle,bool &changed) {
        changed=false;if (_planes.size()!=32768) return false;
        while (!_finished && _cycle<cycle) {if (!step(nullptr,nullptr)) return false;changed=true;}return true;
    }
    bool speech(Common::Array<int16> &samples,Common::Array<uint64> *edges=nullptr) const {
        if (_planes.size()!=32768 || _cycle) return false;
        TitlePresentation player=*this;Common::Array<int16> pcm;SpeakerAudio speaker;speaker_init(&speaker,collect,&pcm);
        while (!player._finished) if (!player.step(&speaker,edges)) return false;
        speaker_advance(&speaker,player._cycle+NOX_CPU_HZ/100);
        if (speaker.toggles!=159688 || pcm.size()>NOX_AUDIO_HZ*10) return false;samples=pcm;return true;
    }
    // After the spoken intro, retain the original E0..FF/F0..FF picture
    // deltas as a silent attract loop. No voice stream is restarted.
    bool advanceMenu(uint32 now,bool &changed) {
        changed=false;if (!_finished || _planes.size()!=32768) return false;
        if (!_menuStamp) {_menuStamp=now;return true;}
        const uint frames=MIN<uint>(4,uint32(now-_menuStamp)/26);if (!frames) return true;_menuStamp=now;
        for (uint i=0;i<frames;++i) {
            for (uint cell=0;cell<256;++cell) {_index=byte(cell);if (!delta()) return false;}
            _frame=byte(_frame+1);if (!_frame) _frame=0xf0;
        }
        changed=true;return true;
    }
    bool finished() const {return _finished;}
    uint64 cycle() const {return _cycle;}
    uint cells() const {return _cells;}
    const Common::Array<byte> &planes() const {return _planes;}
    void renderRGB(Common::Array<byte> &rgb) const {
        static const uint32 colors[]={0x000000,0xe31e60,0x604ebd,0xff44fd,0x00a360,0x9c9c9c,0x14cffd,0xd0c3ff,0x607203,0xff6a3c,0x9c9c9c,0xffa0d0,0x14f53c,0xd0dd8d,0x72ffd0,0xffffff};
        rgb.resize(560*384*3);
        for (uint y=0;y<192;++y) {
            byte bits[560];const uint at=_page*8192+HgrFrame::rowOffset(y);
            for (uint col=0;col<40;++col) for (uint k=0;k<7;++k) {bits[col*14+k]=(_planes[16384+at+col]>>k)&1;bits[col*14+7+k]=(_planes[at+col]>>k)&1;}
            for (uint x=0;x<560;++x) {
                const uint p=x&~3U,n=bits[p]|(bits[p+1]<<1)|(bits[p+2]<<2)|(bits[p+3]<<3);const uint32 c=colors[((n<<1)|(n>>3))&15];
                for (uint dy=0;dy<2;++dy) {byte *v=rgb.data()+((y*2+dy)*560+x)*3;v[0]=byte(c>>16);v[1]=byte(c>>8);v[2]=byte(c);}
            }
        }
    }
private:
    static void collect(int16 value,void *context) {static_cast<Common::Array<int16> *>(context)->push_back(value);}
    bool sprite(uint top,uint bottom,uint col,uint widthMain,uint widthAux,uint shift,uint page) {
        for (uint y=top;y<bottom;++y) {
            if (y>=192 || col+widthMain>40 || col+shift+widthAux>40 || _sprite+widthMain+widthAux>_pictures.size()) return false;
            const uint at=page*8192+HgrFrame::rowOffset(y)+col;
            memcpy(_planes.data()+at,_pictures.data()+_sprite,widthMain);_sprite+=widthMain;
            memcpy(_planes.data()+16384+at+shift,_pictures.data()+_sprite,widthAux);_sprite+=widthAux;
        }
        return true;
    }
    bool delta() {
        const uint record=uint(_tables[0x2400+_frame])*256+_index;
        if (record>=0x1200) return false;
        const uint high=_main[0x3600+record],low=_tables[record],value=_tables[0x1200+record];
        const uint destination=(high<<8)|low;
        // The authored records select either image plane; DB is a discarded
        // scratch record. C054/C055 are display-page tokens, never hardware calls.
        if (destination>=0x2000 && destination<0x6000) _planes[(_auxiliary?16384:0)+destination-0x2000]^=byte(value);
        else if (destination==0xda) {if (value!=1) return false;_auxiliary=!_auxiliary;}
        else if (destination==0xc054) _page=0;
        else if (destination==0xc055) _page=1;
        else if (destination!=0xdb) return false;
        return true;
    }
    void pulse(SpeakerAudio *speaker,Common::Array<uint64> *edges,uint width) const {
        const uint offsets[]={4,4+width,54,54+width};
        for (uint i=0;i<4;++i) {const uint64 t=_cycle+offsets[i];if (speaker) speaker_toggle(speaker,t);if (edges) edges->push_back(t);}
    }
    bool nextToken() {
        if (_audio+_index>=_main.size()) return false;_token=_main[_audio+_index];_index=byte(_index+1);return true;
    }
    bool step(SpeakerAudio *speaker,Common::Array<uint64> *edges) {
        if (_finished || ++_cells>90000) return false;
        if (_token==3 || _token==5) {
            const bool tail=_token==5;
            if (!tail) pulse(speaker,edges,22);
            _audio=tail?0x3500:_audio+256;_frame=byte(_frame+1);if (!_frame) _frame=0xf0;
            const bool rewind=tail && _frame==0x70 && !_rewound;
            if (rewind) {_frame=0x10;_rewound=true;}
            if (_frame==0xe0) {_cycle+=52;_finished=true;return true;}
            // Fixed costs of the bounded source page/rectangle services. These
            // retain waveform cadence without bank polling or instruction execution.
            _cycle+=tail?(rewind?72:_frame==0x71?135659:88):101;
            if (tail && _frame==0x71) {
                if (!sprite(51,149,11,17,17,0,0) || !sprite(49,149,11,17,17,0,1)) return false;
                _index=17;_audio=0;_token=45;return true;
            }
            if (tail) {_token=7;if (_frame==0x10) {_audio=0;_token=45;}return true;}
            if (!nextToken()) return false;if (_frame==0xe8 || _frame==0xef) {_audio=0;_token=45;}return true;
        }
        if (_token==0x51) {
            memcpy(_main.data(),_secondVoice.data(),_secondVoice.size());_audio=0x3500;_token=7;_cycle+=235684;return true;
        }
        if (_token!=7 && (_token<9 || _token>79 || !(_token&1))) return false;
        if (!delta()) return false;
        if (_token!=7 && (speaker || edges)) {
            pulse(speaker,edges,(_token-1)/2);
        }
        _cycle+=101;return nextToken();
    }
    Common::Array<byte> _main,_tables,_secondVoice,_pictures,_planes;
    uint _sprite,_audio;byte _index,_frame,_token,_page;bool _rewound,_finished,_auxiliary;uint64 _cycle;uint _cells;uint32 _menuStamp;
};
} }
#endif
