/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_SPATIAL_H
#define NOX_NATIVE_ACTOR_SPATIAL_H
#include "nox/native_actor_step.h"
#include "nox/native_world_presentation.h"
#include "nox/native_npc_contact.h"
#include "common/memstream.h"
namespace Nox { namespace Native {
// Leading-edge geometry is retained between actors in the original rules.
// Eight semantic candidate cells only; this is not a machine-memory context.
struct ActorSpatialState {
    byte cells[8], known;
    ActorSpatialState() : known(0) { memset(cells,255,8); }
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','A','S','S')); out.writeUint16LE(1); out.writeByte(known); out.write(cells,8); return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=15 || in.readUint32BE()!=MKTAG('N','A','S','S') || in.readUint16LE()!=1) return false;
        ActorSpatialState next; next.known=in.readByte(); if (in.read(next.cells,8)!=8 || in.err()) return false;
        // Candidate coordinates are source byte-relative viewport indices.
        // Clipped footprints can retain187..254; probe bounds every use.
        *this=next; return true;
    }
};
struct ActorSpatialContext {
    byte column, row, tile, kind, capability, partyObject, previousTerrain;
    byte actorState, actorFlags, actorSlot;
    bool large, restrictedTerrain, unrestricted, partyContact;
    const byte *visibility;
    ActorSpatialContext() : actorState(0),actorFlags(0),actorSlot(0),visibility(nullptr) {}
};
enum ActorSpatialStatus { kSpatialComplete, kSpatialScriptContact, kSpatialBattleContact, kSpatialUnsupported, kSpatialStationaryConversation };
struct ActorSpatialResult {
    byte blocked[4], effectiveTerrain, contactDirection, contactCell;
    ActorSpatialStatus status;
    bool boundedLookup;
    ActorSpatialResult() : effectiveTerrain(0), contactDirection(255), contactCell(255), status(kSpatialUnsupported),boundedLookup(false) { memset(blocked,0,4); }
};
// Uses the owned viewport's terrain and stamped occupancy. Contact is a typed
// handoff, before running a script or battle. Clipped large footprints and
// off-screen movement require their own spatial owner.
class ActorSpatialRules {
public:
    ActorSpatialRules() : _ready(false), _type(255) {}
    bool load(HdvArchive &archive, byte type) {
        Common::Array<byte> game, footer; MapAssetRange range;
        if (!_small.load(archive,type) || !_contact.load(archive,type) || !archive.read("NA/GAME",game) || !resolveMapAssets(game,type,range) ||
            !archive.readRange(range.resource,range.offset + range.size,72,footer)) return false;
        memcpy(_footer,footer.data(),72); _type=type; _ready=true; return true;
    }
    bool run(const World &world, const WorldScene &scene, const ActorSpatialContext &context, ActorSpatialState &state, ActorSpatialResult &output, NpcContactState *contact=nullptr, bool stationaryConversation=false) const {
        if (!world.ready() || world.mapType()!=_type) return false;
        return probe(scene,world._states[world._current].tables.data()+1024,context,state,output,contact,stationaryConversation);
    }
    bool probe(const WorldScene &scene, const byte *objects, const ActorSpatialContext &c, ActorSpatialState &state, ActorSpatialResult &output, NpcContactState *contact=nullptr, bool stationaryConversation=false) const {
        if (!_ready || !objects || c.column<120 || c.column>136 || c.row<123 || c.row>133 ||
            (c.partyObject!=255 && (c.partyObject>252 || (c.partyObject&3)))) return false;
        ActorSpatialResult result; result.status=kSpatialComplete; result.effectiveTerrain=c.previousTerrain;
        const int cell=(c.row-123)*17+c.column-120;
        ActorSpatialState next=state;
        NpcContactState request; if (contact) { if (!contact->valid()) return false; request=*contact; }
        if (c.large) {
            // North/south edge sentinels occupy the first cell; west occupies
            // the second. Its first cell retains the prior actor's candidate.
            if (c.row==123) { next.cells[0]=255; next.known|=1; }
            else { next.cells[0]=byte(cell-17); next.cells[1]=byte(cell-16); next.known|=3; }
            if (c.row==132) { next.cells[2]=255;next.known|=4; }
            else { next.cells[2]=byte(cell+34);next.cells[3]=byte(cell+35);next.known|=12; }
            if (c.column==136) {
                next.cells[5]=255;next.known|=32;
                if (!(next.known&16)) { next.cells[4]=255;next.known|=16;result.boundedLookup=true; }
            } else { next.cells[4]=byte(cell+2); next.cells[5]=byte(cell+19); next.known|=48; }
            if (c.column==120) {
                next.cells[7]=255; next.known|=128;
                if (!(next.known&64)) { next.cells[6]=255;next.known|=64;result.boundedLookup=true; }
            }
            else { next.cells[6]=byte(cell-1); next.cells[7]=byte(cell+16); next.known|=192; }
        }
        const int single[4]={cell-17,cell+17,cell+1,cell-1};
        for (byte direction=0; direction<4; ++direction) {
            const bool edge=direction==0 ? c.row==123 : direction==1 ? c.row==(c.large?132:133) : direction==2 ? c.column==136 : c.column==120;
            if (edge && !c.large) { result.blocked[direction]=2; continue; }
            for (uint part=0; part<(c.large?2U:1U); ++part) {
                const int at=c.large?next.cells[direction*2+part]:single[direction];
                if (at==255) { result.blocked[direction]=2; break; }
                if (at<0) return false;
                bool party=at==93;
                if (c.partyObject!=255 && objects[c.partyObject+2]==119 && (at==94 || at==110 || at==111)) party=true;
                if (party) {
                    const bool script=c.kind>=4 && (!c.large || at==93);
                    bool battle=c.large ? at!=93 || (contact?request.availability!=0:c.partyContact) : c.kind==0;
                    if (script && contact) {
                        // The original large-actor helper clobbers its subcell
                        // cursor and can read outside the candidate vector.
                        // Moving actors must never receive a guessed vector.
                        // A verified stationary on-foot caller can instead
                        // complete its queued conversation: no navigation or
                        // RNG consumes the remaining collision scratch.
                        const bool clobbersCursor=c.large && (c.actorState&8) && !(request.availability&128) && c.actorFlags!=255;
                        if (clobbersCursor && (!stationaryConversation || c.capability || c.actorFlags || c.partyObject!=255)) return false;
                        WorldActor actor; actor.kind=c.kind; actor.state=c.actorState; actor.flags=c.actorFlags;
                        if (!_contact.consider(actor,c.actorSlot,byte(cell),scene.actors.terrain[cell],request)) return false;
                        if (clobbersCursor && !request.availability) {
                            result.status=kSpatialStationaryConversation; result.contactCell=byte(at); result.contactDirection=direction;
                            state=next; *contact=request; output=result; return true;
                        }
                        battle=c.large ? request.availability!=0 : c.capability==255;
                        if (!c.large && !battle) { result.blocked[direction]=1; break; }
                    }
                    if ((script && !contact) || battle) {
                        result.status=script && !contact?kSpatialScriptContact:kSpatialBattleContact;
                        result.contactCell=byte(at); result.contactDirection=direction; state=next; if (contact) *contact=request; output=result; return true;
                    }
                }
                if (c.large) {
                    if (c.unrestricted) { state=next; if (contact) *contact=request; output=result; return true; }
                    const bool water=in(c.tile,60);
                    // Original overflow terrain bytes alias the owned mask;
                    // actor/object overflow has no such ownership contract.
                    if (at>=187 && (water || !c.visibility)) {
                        result.blocked[direction]=2;result.boundedLookup=true;break;
                    }
                    const byte terrain=at<187?scene.actors.terrain[at]:c.visibility[at-187];
                    const bool terrainBlocked=water?terrain<229 || terrain>=233:
                        in(terrain,6) || in(terrain,8) || in(terrain,10) || in(terrain,26) || terrain==_footer[67];
                    if (at>=187) {
                        if (terrainBlocked) result.blocked[direction]=1;
                        else { result.blocked[direction]=2;result.boundedLookup=true; }
                        break;
                    }
                    const bool blocked=terrainBlocked || scene.actors.lookup[at]!=255 || scene.objects.lookup[at]!=255;
                    if (blocked) { result.blocked[direction]=1; break; }
                } else {
                    ActorStepProbe input; input.terrain=scene.actors.terrain[at]; input.actorTile=c.tile;
                    input.occupied=scene.actors.lookup[at]!=255; const byte slot=scene.objects.lookup[at];
                    input.objectPresent=slot!=255;
                    if (input.objectPresent && (slot>252 || (slot&3))) return false;
                    input.objectTile=input.objectPresent?objects[slot+2]:0; input.objectState=input.objectPresent?objects[slot+3]:0;
                    input.previousTerrain=result.effectiveTerrain; input.large=false; input.restrictedTerrain=c.restrictedTerrain; input.unrestricted=c.unrestricted;
                    ActorStepDecision after; if (!_small.probe(input,after)) return false;
                    result.blocked[direction]=byte(after.blocked); result.effectiveTerrain=after.effectiveTerrain;
                }
            }
        }
        state=next; if (contact) *contact=request; output=result; return true;
    }
private:
    bool in(byte tile,uint offset) const { return tile>=_footer[offset] && tile<_footer[offset+1]; }
    ActorStepRules _small; NpcContactRules _contact; bool _ready; byte _type,_footer[72];
};
} }
#endif
