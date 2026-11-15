/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TERRAIN_ANIMATION_VALIDATION_H
#define NOX_NATIVE_TERRAIN_ANIMATION_VALIDATION_H
#include "nox/native_praevoc_validation.h"
#include "nox/native_world_animation.h"
namespace Nox { namespace Native {
inline bool terrainTurnCodec511(WorldActorTurnState &turn) {
    Common::MemoryWriteStreamDynamic a(DisposeAfterUse::YES),b(DisposeAfterUse::YES);
    if (!turn.save(a)) return false;WorldActorTurnState next;Common::MemoryReadStream in(a.getData(),a.size());
    if (!next.load(in) || !next.save(b) || a.size()!=b.size() || memcmp(a.getData(),b.getData(),a.size()) ||
            next.terrainRecovery!=turn.terrainRecovery || next.praevocUsed!=turn.praevocUsed || next.praevocCursor!=turn.praevocCursor) return false;
    turn=next;return true;
}
inline bool terrainAnimationCodec511(WorldAnimationState &state) {
    Common::MemoryWriteStreamDynamic a(DisposeAfterUse::YES),b(DisposeAfterUse::YES);if (!state.save(a)) return false;
    Common::ScopedPtr<WorldAnimationState> next(new WorldAnimationState);Common::MemoryReadStream in(a.getData(),a.size());
    if (!next->load(in) || !next->save(b) || a.size()!=b.size() || memcmp(a.getData(),b.getData(),a.size())) return false;state=*next;return true;
}
inline bool terrainViewport511(NightActorValidation504 &v) {
    v.window=WorldStory();byte previous[187]={};
    return v.window.initialize(v.game.world,v.game.world.currentTables()->data()+1536,256) &&
        WorldViewport::refreshAtWorld(v.game.world,v.window,v.story,v.actors,v.lighting,v.occlusion,v.painter,v.game.appearance,v.game.clock.lightActive,0,
            previous,v.turn.presentation,v.light,v.visible,v.frame,&v.scene) && v.status.paint(v.game,v.scene,v.light,false,v.frame) &&
        v.text.importPages(v.frame.data(),v.frame.data(),8192);
}
inline bool terrainSurfaceSetup511(NightActorValidation504 &v,byte x,byte y) {
    WorldTravelPlan plan;plan.type=0;plan.map=0;plan.x=x;plan.y=y;
    if (!v.game.create("TERRAIN",0,0) || !WorldTravel::commit(v.game.world,plan)) return false;
    v.game.clock.hour=0x12;v.game.clock.minute=0;byte routes[3584]={};NpcSchedulePolicy policy={false,false};
    if (NpcScheduler::apply(v.game.world,0x12,0,policy,v.game.schedule)==kNpcScheduleUnsupported ||
            !v.status.load(v.archive,0) || !v.status.initializeFrame(v.frame) || !v.story.load(v.archive) || !v.actors.load(v.archive,0) ||
            !v.lighting.load(v.archive,0) || !v.occlusion.load(v.archive,0) || !v.painter.load(v.archive,0) || !v.visible.loadRules(v.archive) ||
            !v.routes.importPages(routes,3584) || !v.random.initializeEntropy(119) || !v.messages.load(v.archive) || !v.text.initialize(v.archive,1) ||
            !v.input.loadRules(v.archive,0) || !terrainViewport511(v)) return false;
    return WorldCheckpointFixture::standing(v.archive,v.game,v.scene);
}
inline bool terrainMountSetup511(NightActorValidation504 &v,byte picture) {
    if (!v.setup(69,15,42,0x12,0,1,nullptr)) return false;
    const byte *objects=v.game.world.currentTables()->data()+1024;uint slot=192;
    for (;slot<256;slot+=4) if (!objects[slot] && !objects[slot+1] && !objects[slot+2] && !objects[slot+3]) break;
    if (slot>=256) return false;
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!v.game.world.save(saved)) return false;
    Common::Array<byte> bytes;bytes.resize(saved.size());memcpy(bytes.data(),saved.getData(),saved.size());
    const uint at=271+1024+slot;if (at+4>bytes.size()) return false;
    bytes[at]=15;bytes[at+1]=42;bytes[at+2]=picture;bytes[at+3]=0; // Declared source-compatible transport record before any input.
    Common::MemoryReadStream in(bytes.data(),bytes.size());return v.game.world.load(in) && terrainViewport511(v) && v.scene.objects.lookup[93]==slot;
}
inline bool validateNativeTerrainAnimation511(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    report="terrain511 source fixture header";
    if (fixture.readUint32BE()!=MKTAG('N','S','R','5') || fixture.readUint16LE()!=1 || fixture.readUint16LE()!=16) return false;
    Common::ScopedPtr<WorldAnimationState> state(new WorldAnimationState);state->phase=fixture.readByte();state->pattern=fixture.readByte();
    if (fixture.read(state->groups,5)!=5) return false;state->target=fixture.readByte();WorldActorTurnState turn;turn.terrainRecovery=fixture.readByte();
    Common::Array<byte> image;image.resize(8192);
    for (uint page=0;page<2;++page) if (fixture.read(image.data(),8192)!=8192 || !state->pages[page].importImage(image.data(),8192)) return false;
    byte fields[256]={},visibility[187]={};World world(archive);WorldScene scene;memset(scene.actors.terrain,75,187);
    memset(scene.actors.lookup,255,187);memset(scene.actors.kinds,255,187);memset(scene.objects.lookup,255,187);
    WorldAnimation animation;WorldAnimationContext context;LightState light(1,0,0);
    if (!world.initialize(0,0,128,128,fields,256) || !animation.load(archive,0)) return false;
    Common::MemoryWriteStreamDynamic worldBefore(DisposeAfterUse::YES);if (!world.save(worldBefore)) return false;
    for (uint step=0;step<16;++step) {
        report=Common::String::format("terrain511 exact source raster step%u",step);
        const byte expectedRecovery=fixture.readByte();byte expectedPhases[7];if (fixture.read(expectedPhases,7)!=7) return false;
        const byte expectedTarget=fixture.readByte();Common::Array<WorldAnimationDraw> draws;
        if (!animation.run(world,scene,visibility,187,light,context,*state,draws,nullptr,&turn.terrainRecovery) || !draws.empty() ||
                turn.terrainRecovery!=expectedRecovery || state->phase!=expectedPhases[0] || state->pattern!=expectedPhases[1] ||
                memcmp(state->groups,expectedPhases+2,5) || state->target!=expectedTarget) return false;
        for (uint page=0;page<2;++page) if (fixture.read(image.data(),8192)!=8192 || memcmp(image.data(),state->pages[page].data(),8192)) return false;
        if (step==7 && (!terrainTurnCodec511(turn) || !terrainAnimationCodec511(*state))) return false;
    }
    Common::MemoryWriteStreamDynamic worldAfter(DisposeAfterUse::YES);if (!world.save(worldAfter) || worldBefore.size()!=worldAfter.size() || memcmp(worldBefore.getData(),worldAfter.getData(),worldBefore.size())) return false;
    const uint pointCount=fixture.readUint16LE();if (!pointCount || pointCount>128) return false;Common::Array<byte> points;points.resize(pointCount*3);
    if (fixture.read(points.data(),points.size())!=points.size() || fixture.err() || fixture.pos()!=fixture.size()) return false;
    uint codecs=1;
    for (uint phase=0;phase<17;++phase) for (uint shape=0;shape<4;++shape) {
        report=Common::String::format("terrain511 turn codec phase%u shape%u",phase,shape);
        WorldActorTurnState next;next.terrainRecovery=byte(phase);next.praevocUsed=(shape&1)!=0;next.praevocCursor=next.praevocUsed?2:5;next.effects.originKnown=(shape&2)!=0;
        if (!terrainTurnCodec511(next)) return false;++codecs;
    }
    // Old v2/v3/v4 writers remain byte-identical at default recovery16;
    // explicitly load retained v1's shorter presentation tail as well.
    {WorldActorTurnState old;Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);if (!old.save(out) || out.size()!=119) return false;
     Common::Array<byte> bytes;bytes.resize(117);memcpy(bytes.data(),out.getData(),117);WRITE_LE_UINT16(bytes.data()+4,1);Common::MemoryReadStream in(bytes.data(),117);
     if (!old.load(in) || old.terrainRecovery!=16) return false;}
    WorldTransportMotion motion;if (!motion.load(archive,0)) return false;uint gates=0;
    const byte appearances[]={246,127,128,238},phases[]={0,4,12,13,14,15,16};
    for (uint a=0;a<4;++a) for (uint p=0;p<7;++p) for (uint fast=0;fast<2;++fast) {
        report=Common::String::format("terrain511 source gate appearance%u phase%u speed%u",uint(appearances[a]),uint(phases[p]),fast*4);
        TransportMotionContext c;c.currentTerrain=75;c.picture=appearances[a];c.phase=phases[p];c.speed=byte(fast*4);
        GameplayRandom random,expected;if (!random.initializeEntropy(119)) return false;expected=random;bool stopped=false;
        if (!fast && c.picture!=128) {if (c.phase>14) stopped=true;else if (c.phase<13) {byte roll;if (!expected.next(1,255,roll)) return false;stopped=roll>=64;}}
        TransportMotionOutcome out;if (!motion.currentTerrain(c,random,out) || out.interrupted!=stopped || out.message!=(stopped?26:255) || random.seed()!=expected.seed() || random.tail()!=expected.tail()) return false;++gates;
    }
    const byte forms[]={119,123,124,127,128,238};uint boardings=0,idle=0;
    for (uint f=0;f<6;++f) {
        report=Common::String::format("terrain511 actual B form%u",uint(forms[f]));
        Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;
        if (!terrainMountSetup511(v,forms[f]) || v.send(Common::KEYCODE_b,'B')!=kWorldInputHandled || v.game.world.onFoot() ||
                v.game.appearance!=forms[f] || !v.input.atWorldCommand() || !v.status.paint(v.game,v.scene,v.light,false,v.frame)) return false;
        WorldAnimation mounted;Common::ScopedPtr<WorldAnimationState> frames(new WorldAnimationState);WorldAnimationContext c;c.appearance=v.game.appearance;c.partyObject=v.turn.presentation.partyObject;c.speed=v.input.transportSpeed();
        Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),rng(DisposeAfterUse::YES);if (!mounted.load(archive,8) || !frames->synchronize(v.frame) || !v.game.save(core) || !v.random.save(rng)) return false;
        uint partyDraws=0;
        for (uint step=0;step<32;++step) {
            report=Common::String::format("terrain511 mounted idle form%u step%u",uint(forms[f]),step);Common::Array<WorldAnimationDraw> draws;
            if (!mounted.run(v.game.world,v.scene,v.visible.cells(),187,v.light,c,*frames,draws,nullptr,&v.turn.terrainRecovery)) return false;
            for (uint d=0;d<draws.size();++d) if (draws[d].cell==93) {if (draws[d].tile!=forms[f]) return false;++partyDraws;}
            ++idle;
            if (step==15 && (!terrainTurnCodec511(v.turn) || !terrainAnimationCodec511(*frames) || !praevocCoreCodecs505(v))) return false;
        }
        if ((forms[f]>=128)!=(partyDraws!=0)) return false;
        Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES),rngAfter(DisposeAfterUse::YES);
        if (!v.game.save(after) || !v.random.save(rngAfter) || core.size()!=after.size() || memcmp(core.getData(),after.getData(),core.size()) ||
                rng.size()!=rngAfter.size() || memcmp(rng.getData(),rngAfter.getData(),rng.size())) return false;
        WorldAnimationContext bad=c;bad.appearance=129;Common::Array<WorldAnimationDraw> draws;
        if (mounted.run(v.game.world,v.scene,v.visible.cells(),187,v.light,bad,*frames,draws,nullptr,&v.turn.terrainRecovery)) return false;
        v.frame=frames->pages[frames->target^1];
        report=Common::String::format("terrain511 actual X/G form%u",uint(forms[f]));
        if (v.send(Common::KEYCODE_x,'X')!=kWorldInputHandled || !v.game.world.onFoot() || !v.input.atWorldCommand() || v.send(Common::KEYCODE_g,'G')!=kWorldInputHandled || !v.input.atWorldCommand()) return false;
        ++boardings;codecs+=2;
    }
    report="terrain511 actual Surface75 pre-input checkpoint";
    Common::ScopedPtr<NightActorValidation504> owner;uint point=pointCount;const uint near[]={76,110,94,92};
    for (uint i=0;i<pointCount;++i) {
        Common::ScopedPtr<NightActorValidation504> next(new NightActorValidation504(archive));const byte direction=points[i*3+2];
        if (direction>3) return false;
        if (terrainSurfaceSetup511(*next,points[i*3],points[i*3+1]) && next->scene.actors.terrain[93]==75 && next->scene.actors.lookup[93]==255 &&
                next->scene.actors.lookup[near[direction]]==255 && next->scene.objects.lookup[near[direction]]==255) {owner.reset(next.release());point=i;break;}
    }
    if (point==pointCount) return false;NightActorValidation504 &v=*owner;const byte x=v.game.world.x(),y=v.game.world.y(),direction=points[point*3+2];
    const Common::KeyCode arrows[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
    report="terrain511 on-foot recovery16 interrupts actual movement";
    if (v.send(arrows[direction])!=kWorldInputHandled || v.game.world.x()!=x || v.game.world.y()!=y || v.turn.terrainRecovery!=16 || !v.input.atWorldCommand()) return false;
    WorldAnimation surface;Common::ScopedPtr<WorldAnimationState> frames(new WorldAnimationState);WorldAnimationContext c;c.appearance=v.game.appearance;
    if (!v.status.paint(v.game,v.scene,v.light,false,v.frame) || !surface.load(archive,0) || !frames->synchronize(v.frame)) return false;
    Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),rng(DisposeAfterUse::YES);if (!v.game.save(core) || !v.random.save(rng)) return false;
    for (uint step=0;step<2;++step) {Common::Array<WorldAnimationDraw> draws;if (!surface.run(v.game.world,v.scene,v.visible.cells(),187,v.light,c,*frames,draws,nullptr,&v.turn.terrainRecovery)) return false;}
    Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES),rngAfter(DisposeAfterUse::YES);
    if (v.turn.terrainRecovery!=14 || !v.game.save(after) || !v.random.save(rngAfter) || core.size()!=after.size() || memcmp(core.getData(),after.getData(),core.size()) || rng.size()!=rngAfter.size() || memcmp(rng.getData(),rngAfter.getData(),rng.size())) return false;
    v.frame=frames->pages[frames->target^1];HgrFrame before=v.frame;
    report="terrain511 free G retains recovery raster and counter";
    if (v.send(Common::KEYCODE_g,'G')!=kWorldInputHandled || !surface.preserveRecoveryRaster(v.game.world,v.scene,v.game.appearance,0,v.turn.terrainRecovery) ||
            !v.status.paint(v.game,v.scene,v.light,false,v.frame,false) || v.turn.terrainRecovery!=14) return false;
    for (uint row=88;row<104;++row) if (memcmp(before.data()+HgrFrame::rowOffset(row)+16,v.frame.data()+HgrFrame::rowOffset(row)+16,2)) return false;
    report="terrain511 recovered actual walking and reset";
    if (v.send(arrows[direction])!=kWorldInputHandled || (v.game.world.x()==x && v.game.world.y()==y) || v.turn.terrainRecovery!=16 || !v.input.atWorldCommand()) return false;
    report=Common::String::format("terrain_animation\texact_source_frames=16\texact_whole_images=32\tturn_codec_forms=%u\told_formats=4\tsource_gate_cases=%u\tactual_B_idle_X_forms=%u\tmounted_idle_frames=%u\tcontext_refusals=6\tidle_core_clock_RNG_unchanged=1\tactual_walking_recovery16_to14_and_reset=1\tfree_raster_preserved=1\tcampaign=0\told_animation_audio_suite=0\n",codecs,gates,boardings,idle);
    return true;
}
} }
#endif
