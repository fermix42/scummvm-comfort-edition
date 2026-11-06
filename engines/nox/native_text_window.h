/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TEXT_WINDOW_H
#define NOX_NATIVE_TEXT_WINDOW_H
#include "nox/native_text_glyph.h"
#include "common/events.h"
namespace Nox { namespace Native {
// Plain authored text window: whole-word placement, explicit CR, scrolling and
// keyboard paging. Formatting/expansion commands require a separate text owner.
class TextWindow {
public:
    TextWindow() : _ready(false),_left(0),_width(0),_top(0),_bottom(0),_paging(0),_lines(0),_position(0),_waiting(false),_ended(false),_resume(kReadWord),_end(kSpace) {}
    bool begin(HdvArchive &archive,const byte *text,uint size,byte left,byte width,byte top,byte bottom,byte paging,const HgrFrame *background=nullptr) {
        if (_ready || (!text && size) || size>65535 || !width || left+width>40 || top>=bottom || bottom>=24 || !paging) return false;
        TextWindow next; if (!next.loadPrompt(archive) || next._prompt.size()>width || !next._surface.initialize(archive,1)) return false;
        if (background && !next._surface.importPages(background->data(),background->data(),8192)) return false;
        for (uint i=0;i<size;++i) if ((text[i]&127)<32 && text[i]!=0x8d) return false;
        next._text.resize(size); if (size) memcpy(next._text.data(),text,size);
        next._left=left;next._width=width;next._top=top;next._bottom=bottom;next._paging=paging;next._ready=true;
        if (!next._surface.setCursor(left,top) || !next.pump()) return false;
        *this=next; return true;
    }
    bool handle(const Common::Event &event) {
        if (!_ready) return false;
        if (!_waiting || event.type!=Common::EVENT_KEYDOWN) return true;
        const Common::KeyCode key=event.kbd.keycode;
        if (key==Common::KEYCODE_UP || key==Common::KEYCODE_DOWN || key==Common::KEYCODE_LEFT || key==Common::KEYCODE_RIGHT || key==Common::KEYCODE_BACKSPACE) return true;
        TextWindow next=*this;next._waiting=false;
        if (!next._surface.clearTextSpan(_left,_bottom,_width) || !next.scroll() || !next.pump()) return false;
        *this=next;return true;
    }
    bool waiting() const { return _waiting; } bool ended() const { return _ended; }
    bool append(const byte *text,uint size) {
        if (!_ready || !_ended || _waiting || (!text && size) || _text.size()+size>65535) return false;
        if (!size) return true;
        for (uint i=0;i<size;++i) if ((text[i]&127)<32 && text[i]!=0x8d) return false;
        TextWindow next=*this;
        for (uint i=0;i<size;++i) next._text.push_back(text[i]);
        next._ended=false;next._resume=kReadWord;
        if (!next.pump()) return false;*this=next;return true;
    }
    uint position() const { return _position; } byte column() const { return _surface.column(); } byte row() const { return _surface.row(); } byte lines() const { return _lines; }
    const byte *page(uint index) const { return _surface.page(index); }
    bool paint(HgrFrame &canvas) const {
        if (!_ready) return false;
        for (uint y=uint(_top)*8;y<(uint(_bottom)+1)*8;++y) for (uint x=_left;x<uint(_left)+_width;++x)
            if (!canvas.putByte(x,y,_surface.page(0)[HgrFrame::rowOffset(y)+x])) return false;
        return true;
    }
    bool save(Common::WriteStream &out) const {
        if (!_ready) return false;
        out.writeUint32BE(MKTAG('N','T','W','N'));out.writeUint16LE(1);
        out.writeByte(_left);out.writeByte(_width);out.writeByte(_top);out.writeByte(_bottom);out.writeByte(_paging);out.writeByte(_lines);
        out.writeUint16LE(_position);out.writeByte(_waiting);out.writeByte(_ended);out.writeByte(_resume);out.writeByte(_end);
        out.writeUint16LE(_word.size());if (!_word.empty()) out.write(_word.data(),_word.size());
        out.writeUint16LE(_text.size());if (!_text.empty()) out.write(_text.data(),_text.size());
        return _surface.save(out)&&!out.err();
    }
    bool load(Common::SeekableReadStream &in,HdvArchive &archive) {
        if (in.size()-in.pos()<16417 || in.readUint32BE()!=MKTAG('N','T','W','N') || in.readUint16LE()!=1) return false;
        TextWindow next;if (!next.loadPrompt(archive)) return false;
        next._left=in.readByte();next._width=in.readByte();next._top=in.readByte();next._bottom=in.readByte();next._paging=in.readByte();next._lines=in.readByte();next._position=in.readUint16LE();
        const byte waiting=in.readByte(),ended=in.readByte(),resume=in.readByte(),end=in.readByte();
        if (!next._width || next._left+next._width>40 || next._top>=next._bottom || next._bottom>=24 || !next._paging || next._prompt.size()>next._width || waiting>1 || ended>1 || resume>kConsumeCR || end>kEnd) return false;
        next._waiting=waiting!=0;next._ended=ended!=0;next._resume=Resume(resume);next._end=WordEnd(end);
        uint size=in.readUint16LE();if (size>next._width || in.size()-in.pos()<int64(size)+2) return false;
        next._word.resize(size);if (size && in.read(next._word.data(),size)!=size) return false;
        size=in.readUint16LE();if (in.size()-in.pos()!=int64(size)+16395 || next._position>size) return false;
        next._text.resize(size);if (size && in.read(next._text.data(),size)!=size) return false;
        for (uint i=0;i<size;++i) if ((next._text[i]&127)<32 && next._text[i]!=0x8d) return false;
        if (!next._surface.load(in,archive) || in.err() || (next._waiting && (next._ended || next._resume==kReadWord || next.row()!=next._bottom)) ||
            (next._ended && (next._waiting || next._position!=next._text.size() || !next._word.empty()))) return false;
        next._ready=true;*this=next;return true;
    }
private:
    enum Resume { kReadWord,kFlushWord,kConsumeCR };
    enum WordEnd { kSpace,kWidth,kCR,kEnd };
    bool loadPrompt(HdvArchive &archive) {
        Common::Array<byte> game;if (!archive.read("NA/GAME",game) || game.size()!=13824) return false;
        uint end=0x152d;while (end<game.size() && game[end]) ++end;
        if (end==game.size() || end-0x152d>40) return false;
        _prompt.resize(end-0x152d);if (!_prompt.empty()) memcpy(_prompt.data(),game.data()+0x152d,_prompt.size());return true;
    }
    bool scroll() { return _surface.scrollTextRows(_left,_top,_bottom,_width) && _surface.setCursor(_left,_bottom-1); }
    bool newline() {
        ++_lines;const byte target=byte(_surface.row()+1);
        if (!_surface.setCursor(_left,target)) return false;
        if (target<_bottom) return true;
        if (_lines>=_paging) {
            _lines=0;_waiting=true;
            for (uint i=0;i<_prompt.size();++i) if (!_surface.print(_prompt[i])) return false;
            return true;
        }
        return scroll();
    }
    bool pump() {
        while (!_waiting && !_ended) {
            if (_resume==kConsumeCR) { ++_position;_resume=kReadWord;continue; }
            if (_resume==kReadWord) {
                _word.clear();_end=kWidth;
                while (_word.size()<_width) {
                    if (_position==_text.size()) { _end=kEnd;break; }
                    const byte ch=_text[_position];if (ch==0x8d) { _end=kCR;break; }
                    _word.push_back(ch);++_position;if ((ch&127)==32) { _end=kSpace;break; }
                }
                _resume=kFlushWord;
                if (_surface.column()+_word.size()>uint(_left)+_width) { if (!newline()) return false;if (_waiting) break; }
            }
            if (_resume==kFlushWord) {
                for (uint i=0;i<_word.size();++i) if (!_surface.print(_word[i])) return false;
                _word.clear();
                if (_end==kEnd) { _ended=true;_resume=kReadWord;break; }
                if (_end==kCR) { _resume=kConsumeCR;if (!newline()) return false;if (_waiting) break; }
                else _resume=kReadWord;
            }
        }
        return true;
    }
    bool _ready;byte _left,_width,_top,_bottom,_paging,_lines;uint16 _position;
    bool _waiting,_ended;Resume _resume;WordEnd _end;Common::Array<byte> _word,_text,_prompt;TextGlyphSurface _surface;
};
} }
#endif
