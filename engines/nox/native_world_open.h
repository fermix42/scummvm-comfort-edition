/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_OPEN_H
#define NOX_NATIVE_WORLD_OPEN_H
#include "nox/native_world_presentation.h"
namespace Nox { namespace Native {
enum WorldOpenResult { kWorldOpenChanged, kWorldOpenRefused, kWorldOpenLock, kWorldOpenInteraction, kWorldOpenUnsupported };
struct WorldOpenEvent {
    byte message, object, cell;
    bool sound;
    WorldOpenEvent() : message(255),object(255),cell(255),sound(false) {}
};
// Ordinary selected-direction Open admission and direct door/control actions.
// Locked access and external interactions are typed handoffs; they are never
// treated as a successful unlock. The caller owns messages and turn policy.
class WorldOpen {
public:
    WorldOpen() : _ready(false),_type(255),_first(0),_last(0) {}
    bool load(HdvArchive &archive,byte type) {
        Common::Array<byte> game,footer;MapAssetRange assets;
        if (!archive.read("NA/GAME",game) || game.size()!=13824 || !resolveMapAssets(game,type,assets) ||
            !archive.readRange(assets.resource,assets.offset+assets.size,72,footer)) return false;
        _first=footer[26];_last=footer[27];memcpy(_interactions,game.data()+0x8f2,20);
        _type=type;_ready=true;return true;
    }
    WorldOpenResult run(World &world,const WorldScene &scene,Direction direction,WorldOpenEvent &event) const {
        if (!_ready || !world.ready() || world.mapType()!=_type) return kWorldOpenUnsupported;
        byte x=world.x(),y=world.y(),cell;
        switch (direction) {
        case kNorth: --y;cell=76;break;case kSouth: ++y;cell=110;break;
        case kEast: ++x;cell=94;break;case kWest: --x;cell=92;break;
        default:return kWorldOpenUnsupported;
        }
        WorldOpenEvent raised;raised.cell=cell;const byte slot=scene.objects.lookup[cell];raised.object=slot;
        if (slot==255) {
            if (interaction(scene.actors.terrain[cell])) { raised.message=38;event=raised;return kWorldOpenRefused; }
            refuse(world,raised);event=raised;return kWorldOpenRefused;
        }
        if (slot>252 || (slot&3)) return kWorldOpenUnsupported;
        byte *object=world._states[world._current].tables.data()+1024+slot;
        if (object[0]!=x || object[1]!=y || !object[2] || object[2]==255) return kWorldOpenUnsupported;
        if ((object[2]>=_first && object[2]<_last) || interaction(object[2])) { event=raised;return kWorldOpenInteraction; }
        if (_type==3 && (object[2]==91 || object[2]==92)) { raised.message=23;raised.sound=true;event=raised;return kWorldOpenRefused; }
        const byte state=object[3];
        if (state==16) { object[3]=41;raised.message=2;event=raised;return kWorldOpenChanged; }
        if (state>=17 && state<32) { event=raised;return kWorldOpenLock; }
        if (state==48 || state==49) {
            if (slot<4) return kWorldOpenUnsupported;
            if (state==48) { ++object[2];object[3]=49;object[-1]=50; }
            else { --object[2];object[3]=48;object[-1]=254; }
            raised.message=3;event=raised;return kWorldOpenChanged;
        }
        refuse(world,raised);event=raised;return kWorldOpenRefused;
    }
private:
    bool interaction(byte tile) const {
        for (uint i=0;i<20;i+=2) if (_interactions[i]==_type && _interactions[i+1]==tile) return true;
        return false;
    }
    static void refuse(const World &world,WorldOpenEvent &event) {
        event.message=world.mapId()==48 && (world.x()==16 || world.x()==18) && world.y()==35?40:23;event.sound=true;
    }
    bool _ready;byte _type,_first,_last,_interactions[20];
};
} }
#endif
