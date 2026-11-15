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
    byte appearance,partyObject,left,right,top,bottom,speed;
    bool notice;
    WorldAnimationContext() : appearance(246),partyObject(255),left(12),right(17),top(0),bottom(12),speed(0),notice(false) {}
};
struct WorldAnimationDraw {
    byte cell,tile,phase;
    WorldAnimationDraw(byte c,byte t,byte p) : cell(c),tile(t),phase(p) {}
};
// Visible idle raster using the current map-family atlas. Pattern bits gate
// four small-actor groups and the party group. Owned static boats119/123/124
// retain their painted footprints, as sourceD2C1 skips pictures below128.
// Canonical records, clocks and RNG remain read-only. The explicit caller-owned
// terrain recovery counter advances with the source party-slide raster.
class WorldAnimation {
public:
    WorldAnimation() : _ready(false),_type(255) {}
    byte mapType() const {return _type;}
    bool load(HdvArchive &archive,byte type=8) {
        Common::Array<byte> game,footer;MapAssetRange range;
        if (!_atlas.load(archive,type) || !archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        for (uint i=0;i<5;++i) _patterns[i]=READ_LE_UINT16(game.data()+0x8a7+2*i);
        memcpy(_footer,footer.data(),72);_type=type;_ready=true;return true;
    }
    bool preserveRecoveryRaster(const World &world,const WorldScene &scene,byte appearance,byte speed,byte recovery) const {
        return _ready && world.ready() && world.mapType()==_type && recovery<16 && speed!=4 &&
            appearance>=128 && !(appearance>=225 && appearance<229) && scene.actors.terrain[93]==_footer[67];
    }
    bool run(const World &world,const WorldScene &scene,const byte *visibility,uint count,const LightState &light,
            const WorldAnimationContext &context,WorldAnimationState &state,Common::Array<WorldAnimationDraw> &output,byte *enabledOut=nullptr,byte *terrainRecovery=nullptr) const {
        if (!_ready || !world.ready() || world.mapType()!=_type || !visibility || count!=187 || light.mode>3 ||
            !partyContext(world,context) || !state.valid() || context.left>context.right || context.right>17 || context.top>context.bottom || context.bottom>12) return false;
        byte recovery=terrainRecovery?*terrainRecovery:16;if (recovery>16) return false;
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
                const byte kind=scene.actors.kinds[cell];
                const byte bank=kind&7,quadrant=kind&120;
                if ((bank!=0 && bank!=4) || (kind&128) || (bank==4 && (actor&7))) return false;
                const byte *record=records+(bank==4?1280:768)+actor;
                if (!record[2] || record[3]==255) return false;
                const bool large=(record[3]&(bank==4?16:8))!=0;
                if (large) {
                    const uint part=quadrant==64?0:quadrant==32?1:quadrant==16?2:quadrant==8?3:4;
                    if (part==4 || record[2]>252) return false;
                    tile=byte(record[2]+part);
                } else {
                    if (quadrant) return false;
                    // Unscheduled NPCs retain the source dormant picture.
                    tile=bank==4 && record[7]==255 && !record[6]?127:record[2];small=true;
                }
                actorState=record[3];
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
            // Original scene D301..D33E keeps an NPC's picture on these
            // terrain ranges except role0; underworld ordinary actors use
            // the authored picture threshold before selecting ground+1.
            if ((in(ground,36) || in(ground,38)) && actor!=255) {
                if (ground<128) continue;
                const bool npc=(scene.actors.kinds[cell]&7)==4;
                if ((npc && records[1280+actor+5]<1) ||
                        (!npc && world.mapType()==2 && records[768+actor+2]<185)) tile=byte(ground+1);
            }
            // D379/D6A3 slides the existing party image one pixel row on
            // the current physical page; it does not fetch a fresh tile.
            if (cell==93 && !(tile>=225 && tile<229) &&
                    (context.partyObject==255 || context.appearance>=128) && context.speed!=4 && ground==_footer[67]) {
                if (!terrainRecovery) return false;
                if (recovery>=5) {
                    HgrFrame &page=next.pages[next.target];
                    for (int row=15;row>0;--row) for (uint col=16;col<18;++col)
                        if (!page.putByte(col,88+row,page.data()[HgrFrame::rowOffset(87+row)+col])) return false;
                    if (!page.putByte(16,88,0) || !page.putByte(17,88,0)) return false;
                    --recovery;
                }
                continue;
            }
            // D371..D3BD keeps the dedicated boat pictures225..228 intact;
            // other party pictures select the adjacent water terrain picture.
            if (cell==93 && (in(ground,40) || in(ground,42)) && !(tile>=225 && tile<229)) {
                if (ground<128) continue;
                tile=byte(ground+1);
            }
            byte phase=state.phase;
            if (cell==93 || (small && tile!=_footer[68])) {
                const uint group=cell==93?4:((actorState>>6)&3);
                phase=enabled[group]?state.groups[group]:byte((state.groups[group]+3)&3);
                if (small && (actorState&32)) phase^=2;
            }
            byte picture=tile;if (tile==_footer[62] && ground==232) picture=_footer[63];
            if (!_atlas.animated(picture) || !next.pages[next.target].drawCell(_atlas,picture,phase,cell,16)) { warning("Native animation raster picture %u ground %u cell %u actor %u phase %u",picture,ground,cell,actor,phase);return false; }
            draws.push_back(WorldAnimationDraw(byte(cell),picture,phase));
        }
        for (uint i=0;i<5;++i) if (enabled[i]) next.groups[i]=byte((state.groups[i]+1)&3);
        next.phase=byte((state.phase+1)&3);next.pattern=byte((state.pattern+1)&15);next.target^=1;
        state=next;output=draws;if (enabledOut) memcpy(enabledOut,enabled,5);if (terrainRecovery) *terrainRecovery=recovery;return true;
    }
private:
    static bool partyContext(const World &world,const WorldAnimationContext &context) {
        if (world.onFoot()) return context.partyObject==255 && context.appearance>=246 && context.appearance<254;
        if (context.partyObject==255 || (context.partyObject&3) ||
                (context.appearance!=119 && context.appearance!=123 && context.appearance!=124 && context.appearance!=127 && context.appearance!=128 && context.appearance!=238)) return false;
        const byte *object=world.currentTables()->data()+1024+context.partyObject;
        return object[2]==context.appearance && object[0]==world.x() && object[1]==world.y();
    }
    bool in(byte tile,uint index) const { return tile>=_footer[index] && tile<_footer[index+1]; }
    bool _ready;byte _type;uint16 _patterns[5];byte _footer[72];TileAtlas _atlas;
};
} }
#endif
