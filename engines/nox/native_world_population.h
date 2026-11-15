/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_POPULATION_H
#define NOX_NATIVE_WORLD_POPULATION_H
#include "nox/native_world_story.h"
#include "nox/native_random.h"
namespace Nox { namespace Native {
// Last world action is semantic: four directions, another command, or the
// legacy initial no-action value. Modal keys never replace it.
struct WorldPopulationState {
    bool pending=false;
    byte action=4,cursor=100,stormAttempts=0,monsterChoice=0;
    bool valid() const {return action<=5 && cursor>=100 && !(cursor&3) && monsterChoice<4;}
    bool extended() const {return pending || action!=4 || cursor!=100 || stormAttempts || monsterChoice;}
    void arm(byte direction=255) {pending=true;if (direction<4) action=direction;}
};
// Post-command population is a data/RNG transaction, separate from painting
// and battle admission. A newly allocated record joins the next actor pass.
class WorldPopulation {
public:
    WorldPopulation() : _ready(false),_type(255) {}
    bool load(HdvArchive &archive,byte type) {
        Common::Array<byte> game,footer;MapAssetRange range;
        if (!archive.read("NA/GAME",game) || game.size()<0xe60 || !resolveMapAssets(game,type,range) ||
                !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        memcpy(_windowOffsets,game.data()+0xdcb,32);memcpy(_worldOffsets,game.data()+0xdeb,32);
        memcpy(_exclusions,game.data()+0xe0a,72);memcpy(_seaExclusions,game.data()+0xe55,6);
        memcpy(_directionStarts,game.data()+0xe5b,4);memcpy(_footer,footer.data(),72);
        _type=type;_ready=true;return true;
    }
    bool run(World &world,const WorldStory &window,const byte *visible,uint count,byte lightMode,bool night,byte leaderLevel,
            WorldPopulationState &state,GameplayRandom &random,bool &spawned) const {
        spawned=false;
        if (!_ready || !world.ready() || world.mapType()!=_type || !state.valid() || lightMode>3 || !visible || count!=187) return false;
        if (!state.pending || _type>=4) return true;
        Common::Array<byte> catalog;if (!world.populationCatalog(catalog)) return false;
        WorldPopulationState next=state;GameplayRandom after=random;next.pending=false;
        const auto finish=[&]() {state=next;random=after;return true;};
        if (catalog.empty()) return finish();
        if (catalog.size()<11 || catalog[catalog.size()-1]!=255 || (catalog.size()-11)%5) return false;
        byte localX,localY;if (!window.localCoordinates(world,localX,localY)) return false;
        const StoryMapState *cache=window.state();if (!cache) return false;
        // The original loader substitutes these Surface defaults from the
        // first member's level. Native party order/level is authoritative.
        if (!_type && leaderLevel>=4) {
            catalog[6]=catalog[7]=leaderLevel>=6?149:152;catalog[8]=leaderLevel>=6?150:152;
        }
        byte chance;if (!after.next(1,255,chance)) return false;
        byte frequency=catalog[lightMode>=3?1:0];
        if (!_type && (world.field(64)&4)) frequency=byte((frequency>>1)-1);
        if (chance>frequency) return finish();
        for (uint i=0;i<72;i+=2) if (_exclusions[i]==world.coordinateSector() && _exclusions[i+1]==world.mapId()) return finish();
        byte choiceRoll,placementRoll;
        if (!after.next(1,255,choiceRoll) || !after.next(1,255,placementRoll)) return false;
        const bool randomPlacement=choiceRoll<21 || next.action==5;
        uint pair;
        if (randomPlacement) pair=(placementRoll>>3)&30U;
        else {next.monsterChoice=placementRoll>>6;const uint dir=next.action<3?next.action:3;pair=next.monsterChoice*2+_directionStarts[dir];}
        const int origin=int(localY)*48+localX;
        byte terrain=0;bool water=false,found=false;
        for (uint checked=0;checked<16;++checked,pair=(pair+2)%32) {
            const byte high=_windowOffsets[pair+1];
            if (high&64) {if (localY<11 || localY>=37) return finish();}
            else if ((high&32) && localX>=39) return finish();
            const int distance=_windowOffsets[pair]+int(high&31)*256;
            int at=(high&128)?origin+distance:origin-distance;
            if (at<0) at=origin+distance;
            if (at>=2304) continue;
            terrain=cache->terrain[at];water=terrain>=229 && terrain<233;
            if (water) {if (_type==2 && chance!=1) return finish();found=true;break;}
            if (in(terrain,6) || in(terrain,8) || in(terrain,10)) continue;
            found=true;break;
        }
        if (!found) return finish();
        const byte x=offset(world.x(),_worldOffsets[pair]),y=offset(world.y(),_worldOffsets[pair+1]);
        byte tile=0,flags=1;
        if (!_type && randomPlacement && next.stormAttempts<=15) {++next.stormAttempts;tile=225;flags=41;}
        if (!tile && !_type && choiceRoll<32 && !water) {
            bool excluded=false;for (uint i=0;i<6;++i) if (_seaExclusions[i]==world.coordinateSector()) excluded=true;
            if (!excluded) tile=172;
        }
        if (!tile && !_type && terrain==232) {tile=placementRoll<64?_footer[62]:148;flags=placementRoll<64?112:113;}
        if (!tile) {
            uint row=water?6:2;
            const uint start=_type==3 && night?55:10;if (start>=catalog.size()) return false;
            for (uint at=start;at<catalog.size() && catalog[at]!=255;at+=5) {
                if (at+4>=catalog.size()) return false;
                if (catalog[at]==world.coordinateSector()) {row=at+1;break;}
            }
            bool selected=false;
            for (uint pass=0;pass<2 && !selected;++pass) {
                for (uint i=0;i<4;++i) {
                    const byte candidate=catalog[row+((next.monsterChoice+i)&3)];
                    const bool sea=candidate>=_footer[60] && candidate<_footer[61];
                    if (sea!=water) continue;
                    if (sea) {byte roll;if (!after.next(1,255,roll)) return false;if (roll>=192) return finish();}
                    tile=candidate;selected=true;break;
                }
                row=water?6:2;
            }
            if (!selected) return finish(); // bounded malformed/no-category catalog
        }
        const byte *records=world.currentTables()->data()+768;uint target=256;
        for (uint slot=100;slot<=252;slot+=4) {
            const byte *record=records+slot;
            if (record[0]==x && record[1]==y && record[3]!=255) return finish();
            if (record[3]==255 || !(record[0]|record[1]|record[2]|record[3])) {target=slot;break;}
        }
        if (target==256) {
            for (uint checked=0;checked<39;++checked) {
                next.cursor=next.cursor==252?100:byte(next.cursor+4);bool shown=false;
                for (uint i=0;i<187;++i) if (visible[i]==next.cursor) {shown=true;break;}
                if (!shown) {target=next.cursor;break;}
            }
            // CE bounds the original's infinite loop when every slot is visible.
            if (target==256) return finish();
        }
        byte *record=world._states[world._current].tables.data()+768+target;
        record[0]=x;record[1]=y;record[2]=tile;record[3]=flags;
        spawned=true;return finish();
    }
private:
    static byte offset(byte origin,byte encoded) {return byte(origin+((encoded&128)?int(encoded&127):-int(encoded)));}
    bool in(byte terrain,uint index) const {return terrain>=_footer[index] && terrain<_footer[index+1];}
    bool _ready;byte _type,_windowOffsets[32],_worldOffsets[32],_exclusions[72],_seaExclusions[6],_directionStarts[4],_footer[72];
};
} }
#endif
