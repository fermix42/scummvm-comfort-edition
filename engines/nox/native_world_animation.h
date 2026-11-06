/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_ANIMATION_H
#define NOX_NATIVE_WORLD_ANIMATION_H
#include "nox/native_world_presentation.h"
#include "nox/native_lighting.h"
namespace Nox { namespace Native {
struct WorldAnimationState {
    byte phase,pattern,groups[5],target;
    HgrFrame pages[2];
    WorldAnimationState() : phase(0),pattern(0),target(0) { memset(groups,0,5); }
    bool synchronize(const HgrFrame &frame) {
        WorldAnimationState next=*this;next.pages[0]=frame;next.pages[1]=frame;
        if (!next.valid()) return false;*this=next;return true;
    }
    bool valid() const {
        if (phase>3 || pattern>15 || target>1) return false;
        for (uint i=0;i<5;++i) if (groups[i]>3) return false;
        // Semantic images never retain the original machine's unrelated
        // variables/code in the eight unused bytes of each128-byte block.
        for (uint page=0;page<2;++page) for (uint block=0;block<64;++block)
            for (uint i=120;i<128;++i) if (pages[page].data()[block*128+i]) return false;
        return true;
    }
    bool save(Common::WriteStream &out) const {
        if (!valid()) return false;out.writeUint32BE(MKTAG('N','W','A','M'));out.writeUint16LE(1);
        out.writeByte(phase);out.writeByte(pattern);out.write(groups,5);out.writeByte(target);
        out.write(pages[0].data(),8192);out.write(pages[1].data(),8192);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=16398 || in.readUint32BE()!=MKTAG('N','W','A','M') || in.readUint16LE()!=1) return false;
        WorldAnimationState next;next.phase=in.readByte();next.pattern=in.readByte();if (in.read(next.groups,5)!=5) return false;next.target=in.readByte();
        for (uint page=0;page<2;++page) { byte image[8192];if (in.read(image,8192)!=8192 || !next.pages[page].importImage(image,8192)) return false; }
        if (in.err() || !next.valid()) return false;*this=next;return true;
    }
};
struct WorldAnimationContext {
    byte appearance,partyObject,left,right,top,bottom;
    bool notice;
    WorldAnimationContext() : appearance(246),partyObject(255),left(12),right(17),top(0),bottom(12),notice(false) {}
};
struct WorldAnimationDraw {
    byte cell,tile,phase;
    WorldAnimationDraw(byte c,byte t,byte p) : cell(c),tile(t),phase(p) {}
};
// Visible on-foot Keep idle raster. Authored pattern bits independently gate
// four small-actor groups and the party group. Gameplay, clocks, RNG and actor
// records are read-only. Other appearance/terrain/actor cases are boundaries.
class WorldAnimation {
public:
    WorldAnimation() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> game,footer;MapAssetRange range;
        if (!_atlas.load(archive,8) || !archive.read("NA/GAME",game) || !resolveMapAssets(game,8,range) ||
            !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        for (uint i=0;i<5;++i) _patterns[i]=READ_LE_UINT16(game.data()+0x8a7+2*i);
        memcpy(_footer,footer.data(),72);_ready=true;return true;
    }
    bool run(const World &world,const WorldScene &scene,const byte *visibility,uint count,const LightState &light,
            const WorldAnimationContext &context,WorldAnimationState &state,Common::Array<WorldAnimationDraw> &output,byte *enabledOut=nullptr) const {
        if (!_ready || !world.ready() || world.mapType()!=8 || world.mapId()!=7 || !visibility || count!=187 || light.mode!=1 ||
            context.appearance!=246 || context.partyObject!=255 || !state.valid() || context.left>context.right || context.right>17 || context.top>context.bottom || context.bottom>12) return false;
        WorldAnimationState next=state;byte enabled[5];Common::Array<WorldAnimationDraw> draws;
        for (uint i=0;i<5;++i) enabled[i]=(_patterns[i]&(1U<<state.pattern))?byte(1U<<(state.pattern&7)):0;
        const byte *records=world.currentTables()->data();
        for (uint cell=0;cell<187;++cell) {
            if (visibility[cell]>1) return false;
            if (visibility[cell] || (context.notice && cell%17>=context.left && cell%17<context.right && cell/17>=context.top && cell/17<context.bottom)) continue;
            const byte ground=scene.actors.terrain[cell],actor=scene.actors.lookup[cell],object=scene.objects.lookup[cell];
            byte tile=ground,actorState=0;bool small=false;
            if (actor!=255) {
                if (actor>252 || (actor&3)) return false;
                if (scene.actors.kinds[cell]!=0) return false;
                const byte *record=records+768+actor;if (!record[2] || record[3]==255 || (record[3]&8)) return false;
                tile=record[2];actorState=record[3];small=true;
            } else if (cell==93) tile=context.appearance;
            else if (object!=255) {
                if (object>252 || (object&3)) return false;
                const byte *record=records+1024+object;tile=record[2];
                if (record[3]==54) { if (object>248) return false;if (world.field(record[4])&record[7]) ++tile; }
            }
            if (tile<128 || ground==65 || ground==66 || ground==76 || in(ground,32)) continue;
            if (tile>=_footer[50] && tile<_footer[51] && actor==255 && cell!=93) continue;
            // Water/transport substitutions and large/NPC footprints need
            // their own qualification rather than a guessed ordinary raster.
            if (((in(ground,36) || in(ground,38)) && actor!=255) ||
                (cell==93 && (in(ground,40) || in(ground,42)))) return false;
            byte phase=state.phase;
            if (cell==93 || (small && tile!=_footer[68])) {
                const uint group=cell==93?4:((actorState>>6)&3);
                phase=enabled[group]?state.groups[group]:byte((state.groups[group]+3)&3);
                if (small && (actorState&32)) phase^=2;
            }
            byte picture=tile;if (tile==_footer[62] && ground==232) picture=_footer[63];
            if (!_atlas.animated(picture) || !next.pages[next.target].drawCell(_atlas,picture,phase,cell,16)) return false;
            draws.push_back(WorldAnimationDraw(byte(cell),picture,phase));
        }
        for (uint i=0;i<5;++i) if (enabled[i]) next.groups[i]=byte((state.groups[i]+1)&3);
        next.phase=byte((state.phase+1)&3);next.pattern=byte((state.pattern+1)&15);next.target^=1;
        state=next;output=draws;if (enabledOut) memcpy(enabledOut,enabled,5);return true;
    }
private:
    bool in(byte tile,uint index) const { return tile>=_footer[index] && tile<_footer[index+1]; }
    bool _ready;uint16 _patterns[5];byte _footer[72];TileAtlas _atlas;
};
} }
#endif
