/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_NAVIGATION_H
#define NOX_NATIVE_WORLD_NAVIGATION_H
#include "nox/resources.h"
namespace Nox { namespace Native {
struct NavigationPoint {
    byte type,map,x,y;
    NavigationPoint(byte t=0,byte m=0,byte a=62,byte b=207) : type(t),map(m),x(a),y(b) {}
};
enum NavigationResult {kNavigationSelected,kNavigationUnbound,kNavigationRefused,kNavigationUnsupported};
// Source map-context and9 crystal-orb locations. These are gameplay records,
// not an emulated register bank or an original-address dispatcher.
class WorldNavigation {
public:
    WorldNavigation() : _ready(false),_used(false),_surfaceX(62),_surfaceY(207) {memset(_orbs,0,36);}
    bool initialize(HdvArchive &archive) {
        if (_ready) return false;
        Common::Array<byte> data;
        if (!archive.readRange("NA/DATA.SAVE.GAMEN",0x1e78,36,data) || data.size()!=36) return false;
        memcpy(_orbs,data.data(),36);_ready=true;return true;
    }
    bool ready() const {return _ready;}
    bool used() const {return _used;}
    const NavigationPoint &parent() const {return _parent;}
    NavigationPoint surfaceReturn() const {return NavigationPoint(0,0,_surfaceX,_surfaceY);}
    const byte *orbs() const {return _orbs;}
    bool enter(const NavigationPoint &origin,byte destinationType) {
        if (!_ready || !worldType(origin.type) || !worldType(destinationType)) return false;
        // LOADER.P267E's parent and Surface-return gameplay fields.
        // A local-to-local transition preserves the previous outer parent.
        if (origin.type<3 || destinationType<3) _parent=origin;
        if (!origin.type) {_surfaceX=origin.x;_surfaceY=origin.y;}
        _used=true;return true;
    }
    NavigationResult bind(byte orb,const NavigationPoint &point) {
        if (!_ready || orb>=9) return kNavigationUnsupported;
        if (point.type!=0 && point.type!=2) return kNavigationRefused;
        byte *record=_orbs+orb*4;record[0]=point.x;record[1]=point.y;record[2]=point.map;record[3]=point.type;_used=true;
        return kNavigationSelected;
    }
    NavigationResult choose(byte orb,NavigationPoint &point) const {
        if (!_ready || orb>=9) return kNavigationUnsupported;
        uint index=orb*4,remaining=4;
        // The original scan retains its incremented index when X is zero.
        // Preserve that defined read order; bound the original cross-array read.
        while (remaining && !_orbs[index]) {++index;--remaining;}
        if (!remaining) return kNavigationUnbound;
        if (index+3>=36) return kNavigationUnsupported;
        point=NavigationPoint(_orbs[index+3],_orbs[index+2],_orbs[index],_orbs[index+1]);return kNavigationSelected;
    }
    bool save(Common::WriteStream &out) const {
        if (!_ready) return false;
        out.writeUint32BE(MKTAG('N','N','A','V'));out.writeUint16LE(1);out.writeByte(_used);
        out.writeByte(_parent.type);out.writeByte(_parent.map);out.writeByte(_parent.x);out.writeByte(_parent.y);
        out.writeByte(_surfaceX);out.writeByte(_surfaceY);out.write(_orbs,36);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=49 || in.readUint32BE()!=MKTAG('N','N','A','V') || in.readUint16LE()!=1) return false;
        WorldNavigation next;const byte used=in.readByte();next._parent.type=in.readByte();next._parent.map=in.readByte();
        next._parent.x=in.readByte();next._parent.y=in.readByte();next._surfaceX=in.readByte();next._surfaceY=in.readByte();
        if (used>1 || !worldType(next._parent.type) || in.read(next._orbs,36)!=36 || in.err()) return false;
        next._used=used!=0;next._ready=true;*this=next;return true;
    }
private:
    static bool worldType(byte type) {return type==0 || (type>=2 && type<=8);}
    bool _ready,_used;NavigationPoint _parent;byte _surfaceX,_surfaceY,_orbs[36];
};
} }
#endif
