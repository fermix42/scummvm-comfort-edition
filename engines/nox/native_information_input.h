/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INFORMATION_INPUT_H
#define NOX_NATIVE_INFORMATION_INPUT_H
#include "nox/native_information_catalog.h"
#include "nox/native_text_window.h"
#include "nox/native_opening_backdrop.h"
namespace Nox { namespace Native {
enum InformationCommand {kInformationLocation,kInformationLook,kInformationHelp,kInformationQuests};
enum InformationResult {kInformationPending,kInformationClosed,kInformationUnsupported};
class InformationInput {
public:
    InformationInput() : _loaded(false),_phase(0),_type(0),_map(0) {}
    bool active() const {return _phase!=0;}
    bool matchesWorld(const World &world) const {return !active() || (world.ready() && world.mapType()==_type && world.mapId()==_map);}
    bool begin(InformationCommand command,HdvArchive &archive,const World &world,HgrFrame &frame) {
        if (active() || !world.ready()) return false;
        if (!_loaded) {if (!_catalog.load(archive)) return false;_loaded=true;}
        Common::String body;
        if (command==kInformationLocation) return _catalog.location(world,body) && strip(archive,body,frame);
        _underlay=frame;_type=world.mapType();_map=world.mapId();
        if (command==kInformationLook) {_phase=1;return true;}
        if (command==kInformationHelp) {if (!_catalog.help(body)) return false;}
        else if (command==kInformationQuests) {if (!_catalog.quests(world.questFields(),256,body)) return false;}
        else return false;
        return window(archive,body,frame);
    }
    InformationResult handle(const Common::Event &event,HdvArchive &archive,const World &world,const WorldScene &scene,byte appearance,HgrFrame &frame) {
        if (!active() || !matchesWorld(world)) return kInformationUnsupported;
        if (event.type!=Common::EVENT_KEYDOWN) return kInformationPending;
        if (_phase==1) {
            if (event.kbd.keycode==Common::KEYCODE_ESCAPE) {close(frame);return kInformationClosed;}
            byte direction;
            switch (event.kbd.keycode) {
            case Common::KEYCODE_UP:direction=0;break;case Common::KEYCODE_DOWN:direction=1;break;
            case Common::KEYCODE_RIGHT:direction=2;break;case Common::KEYCODE_LEFT:direction=3;break;
            default:return kInformationPending;
            }
            InformationText result;if (!_catalog.look(world,scene,appearance,direction,result)) return kInformationUnsupported;
            if (result.window) return window(archive,result.text,frame)?kInformationPending:kInformationUnsupported;
            HgrFrame image=_underlay;if (!strip(archive,result.text,image)) return kInformationUnsupported;
            _phase=0;frame=image;return kInformationClosed;
        }
        if (_window.waiting()) return _window.handle(event) && _window.paint(frame)?kInformationPending:kInformationUnsupported;
        if (!_window.ended()) return kInformationUnsupported;
        close(frame);return kInformationClosed;
    }
private:
    bool window(HdvArchive &archive,const Common::String &body,HgrFrame &frame) {
        Common::Array<byte> encoded;for (uint i=0;i<body.size();++i) encoded.push_back(byte(body[i])|128);
        HgrFrame image=_underlay;TextWindow next;
        if (!OpeningBackdrop::renderInformationFrame(image) || !next.begin(archive,encoded.data(),encoded.size(),5,30,4,19,13,&image) || !next.paint(image)) return false;
        _window=next;_phase=2;frame=image;return true;
    }
    void close(HgrFrame &frame) {frame=_underlay;_phase=0;_window=TextWindow();}
    static bool strip(HdvArchive &archive,const Common::String &body,HgrFrame &frame) {
        if (body.size()>25) return false;GlyphFont font;HgrFrame image=frame;
        if (!font.load(archive,1) || !image.clearTextSpan(7,23,25)) return false;
        const uint left=7+(25-body.size())/2;
        for (uint i=0;i<body.size();++i) if (byte(body[i])<32 || !image.drawGlyph(font,byte(body[i]),left+i,23,0)) return false;
        frame=image;return true;
    }
    bool _loaded;byte _phase,_type,_map;InformationCatalog _catalog;TextWindow _window;HgrFrame _underlay;
};
} }
#endif
