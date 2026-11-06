/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_HAZARDS_H
#define NOX_NATIVE_WORLD_HAZARDS_H
#include "nox/native_world_presentation.h"
#include "nox/native_party.h"
#include "nox/native_message_strip.h"
namespace Nox { namespace Native {
// Ordered post-position party effects; transport exempts swimming and river
// damage while retaining unrelated terrain and actor effects.
class WorldHazards {
public:
    WorldHazards() : _ready(false),_type(255) {}
    bool load(HdvArchive &archive,byte type) {
        Common::Array<byte> game,footer;MapAssetRange range;
        if (!archive.read("NA/GAME",game) || game.size()<0x15d || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset+range.size,72,footer) || footer.size()!=72) return false;
        memcpy(_damage,game.data()+0x155,8);memcpy(_footer,footer.data(),72);_type=type;_ready=true;return true;
    }
    bool apply(const World &world,const WorldScene &scene,Party &party,const FixedMessageCatalog &catalog,
            TextGlyphSurface &text,HgrFrame &frame,bool &defeated,bool projectileHit=false) const {
        if (!_ready || !world.ready() || world.mapType()!=_type || !party.ready()) return false;
        Party next=party;TextGlyphSurface surface=text;HgrFrame image=frame;bool dead=false;
        const byte ground=scene.actors.terrain[93],slot=scene.objects.lookup[93];
        const bool safeObject=slot!=255 && in((*world.currentTables())[1024+slot+2],52);
        const auto effect=[&](byte kind,byte message,byte mask)->bool {
            const bool protectedParty=(world.field(57)&mask)!=0;
            if (!show(catalog,protectedParty?32:message,surface,image)) return false;
            if (!protectedParty) for (uint i=0;i<next.memberCount();++i) if (!next.damage(i,_damage[kind])) return false;
            dead=true;for (uint i=0;i<next.memberCount();++i) if (next.member(i)->bytes[0]!=255) dead=false;
            return true;
        };
        // Main-bank2 F777 precedes F6D4 terrain/overlap effects. A hostile
        // projectile hit uses damage kind3 and protection field57 bit128.
        if (projectileHit && !effect(3,22,128)) return false;
        if (!dead && world.onFoot() && !safeObject) {
            if (ground>=229 && ground<232 && !effect(byte(ground-229),21,4)) return false;
            if (!dead && !_type && ground>=205 && ground<211 && !effect(4,21,4)) return false;
        }
        const byte actor=scene.actors.lookup[93];
        if (!dead && actor!=255 && scene.actors.kinds[93]==0 && (*world.currentTables())[768+actor+2]==225 && !effect(0,33,2)) return false;
        if (!dead && (in(ground,46) || in(ground,48)) && !effect(1,34,1)) return false;
        party=next;text=surface;frame=image;defeated=dead;return true;
    }
private:
    bool in(byte value,uint at) const {return value>=_footer[at] && value<_footer[at+1];}
    static bool show(const FixedMessageCatalog &catalog,byte id,TextGlyphSurface &surface,HgrFrame &frame) {
        if (!surface.importPages(frame.data(),frame.data(),8192) || !showFixedMessageStrip(catalog,id,surface)) return false;
        for (uint y=184;y<192;++y) for (uint x=7;x<32;++x) if (!frame.putByte(x,y,surface.page(0)[HgrFrame::rowOffset(y)+x])) return false;return true;
    }
    bool _ready;byte _type,_footer[72],_damage[8];
};
} }
#endif
