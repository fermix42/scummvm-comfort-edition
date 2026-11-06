/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_OUTSIDE_H
#define NOX_NATIVE_ACTOR_OUTSIDE_H
#include "nox/native_world_story.h"
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
struct ActorOutsideProbe { byte terrain[4],actorTile; bool unrestricted; };
struct ActorOutsideDecision { byte terrain[4],blocked[4]; };
// Off-screen terrain policy uses the owned map window, not the viewport's
// actor/object lookup or its different object and terrain exceptions.
class ActorOutsideRules {
public:
    ActorOutsideRules() : _ready(false),_type(255) {}
    bool load(HdvArchive &archive,byte type) {
        _ready=false; Common::Array<byte> game,footer; MapAssetRange range;
        if (!archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        memcpy(_footer,footer.data(),72); _type=type; _ready=true; return true;
    }
    bool probe(const ActorOutsideProbe &input,ActorOutsideDecision &output) const {
        if (!_ready) return false;
        ActorOutsideDecision result; memcpy(result.terrain,input.terrain,4);
        const bool water=in(input.actorTile,60);
        for (uint i=0;i<4;++i) {
            const byte terrain=input.terrain[i];
            result.blocked[i]=input.unrestricted?0:byte(water ? terrain<229 || terrain>=233 :
                in(terrain,6) || in(terrain,8) || (_type==7 && terrain>=119 && terrain<121) || terrain==_footer[67]);
        }
        output=result; return true;
    }
    bool fromWindow(const World &world,const WorldStory &window,byte x,byte y,byte actorTile,bool unrestricted,ActorOutsideDecision &output) const {
        if (!_ready || world.mapType()!=_type || !window.matchesWorld(world)) return false;
        ActorOutsideProbe input; input.actorTile=actorTile; input.unrestricted=unrestricted;
        const int dx[4]={0,0,1,-1},dy[4]={-1,1,0,0};
        for (uint i=0;i<4;++i) if (!window.terrainAt(byte(int(x)+dx[i]),byte(int(y)+dy[i]),input.terrain[i])) return false;
        return probe(input,output);
    }
private:
    bool in(byte value,uint offset) const { return value>=_footer[offset] && value<_footer[offset+1]; }
    bool _ready; byte _type,_footer[72];
};
} }
#endif
