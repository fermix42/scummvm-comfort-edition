/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MOVE_COMPLETION_VALIDATION_H
#define NOX_NATIVE_MOVE_COMPLETION_VALIDATION_H
#include "nox/native_terrain_animation_validation.h"
#include "nox/native_party_move_completion.h"
namespace Nox { namespace Native {
inline bool samePartyCell512(const HgrFrame &a,const HgrFrame &b) {
    for (uint row=88;row<104;++row) if (memcmp(a.data()+HgrFrame::rowOffset(row)+16,b.data()+HgrFrame::rowOffset(row)+16,2)) return false;
    return true;
}
inline bool validateNativeMoveCompletion512(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    report="completion512 source fixture header";
    if (fixture.readUint32BE()!=MKTAG('N','M','C','5') || fixture.readUint16LE()!=1 || fixture.readUint16LE()!=16) return false;
    const byte quiet=fixture.readByte();if (quiet!=75) return false;
    for (uint row=0;row<16;++row) {
        const byte arrived=fixture.readByte(),before=fixture.readByte(),pending=fixture.readByte(),expected=fixture.readByte(),preserve=fixture.readByte();
        report=Common::String::format("completion512 exact source case%u terrain%u pending%u",row,uint(arrived),uint(pending));
        if (pending>1 || preserve>1) return false;
        HgrFrame prior,frame;for (uint y=88;y<104;++y) for (uint x=16;x<18;++x) if (!prior.putByte(x,y,byte(y*13+x))) return false;
        if (!frame.putByte(0,0,91)) return false;const HgrFrame untouched=frame;byte remaining=before;bool retained=false;
        if (!PartyMoveCompletion::apply(arrived,quiet,remaining,prior,frame,&retained) || remaining!=expected || retained!=bool(preserve) || frame.data()[0]!=91 ||
                (retained?!samePartyCell512(prior,frame):memcmp(untouched.data(),frame.data(),8192)!=0)) return false;
    }
    const uint pointCount=fixture.readUint16LE();if (!pointCount || pointCount>128) return false;Common::Array<byte> points;points.resize(pointCount*3);
    if (fixture.read(points.data(),points.size())!=points.size() || fixture.err() || fixture.pos()!=fixture.size()) return false;
    Common::ScopedPtr<NightActorValidation504> owner;byte inside=255,exit=255;const int delta[]={-17,17,1,-1};
    const Common::KeyCode arrows[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
    TraversalRules traversal;if (!traversal.load(archive,0)) return false;
    report="completion512 immutable within75 fixture admission";
    for (uint i=0;i<pointCount && !owner;++i) {
        Common::ScopedPtr<NightActorValidation504> candidate(new NightActorValidation504(archive));
        if (!terrainSurfaceSetup511(*candidate,points[i*3],points[i*3+1]) || candidate->scene.actors.terrain[93]!=75 || candidate->scene.actors.lookup[93]!=255) continue;
        for (uint direction=0;direction<4 && !owner;++direction) {
            const uint target=uint(93+delta[direction]);
            if (candidate->scene.actors.terrain[target]!=75 || candidate->scene.actors.lookup[target]!=255 || candidate->scene.objects.lookup[target]!=255) continue;
            for (uint out=0;out<4;++out) {
                const uint end=uint(93+delta[direction]+delta[out]);const byte terrain=candidate->scene.actors.terrain[end];TraversalProbe probe;probe.terrain=terrain;TraversalDecision decision;
                if (terrain==75 || (terrain>=229 && terrain<233) || !traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction ||
                        candidate->scene.actors.lookup[end]!=255 || candidate->scene.objects.lookup[end]!=255) continue;
                inside=byte(direction);exit=byte(out);owner.reset(candidate.release());break;
            }
        }
    }
    if (!owner) return false;NightActorValidation504 &v=*owner;const byte x=v.game.world.x(),y=v.game.world.y();
    WorldAnimation animation;Common::ScopedPtr<WorldAnimationState> frames(new WorldAnimationState);WorldAnimationContext context;context.appearance=v.game.appearance;
    if (!animation.load(archive,0) || !frames->synchronize(v.frame)) return false;
    for (uint i=0;i<2;++i) {Common::Array<WorldAnimationDraw> draws;if (!animation.run(v.game.world,v.scene,v.visible.cells(),187,v.light,context,*frames,draws,nullptr,&v.turn.terrainRecovery)) return false;}
    if (v.turn.terrainRecovery!=14) return false;v.frame=frames->pages[frames->target^1];const HgrFrame prior=v.frame;
    report="completion512 actual within75 counter/raster hold";
    const WorldInputResult within=v.send(arrows[inside]);
    if (within!=kWorldInputHandled || !v.input.atWorldCommand() || (v.game.world.x()==x && v.game.world.y()==y) || v.scene.actors.terrain[93]!=75 ||
            v.turn.terrainRecovery!=14 || !samePartyCell512(prior,v.frame) || !animation.preserveRecoveryRaster(v.game.world,v.scene,v.game.appearance,0,v.turn.terrainRecovery) ||
            !v.status.paint(v.game,v.scene,v.light,false,v.frame,false) || !samePartyCell512(prior,v.frame)) {
        report=Common::String::format("completion512 within result%u xy%u/%u from%u/%u ground%u recovery%u raster%u idle%u",uint(within),uint(v.game.world.x()),uint(v.game.world.y()),uint(x),uint(y),uint(v.scene.actors.terrain[93]),uint(v.turn.terrainRecovery),uint(samePartyCell512(prior,v.frame)),uint(v.input.atWorldCommand()));return false;
    }
    if (!terrainTurnCodec511(v.turn)) return false;
    report="completion512 actual leave75 reset";
    if (v.send(arrows[exit])!=kWorldInputHandled || !v.input.atWorldCommand() || v.scene.actors.terrain[93]==75 || v.turn.terrainRecovery!=16) return false;
    // Independent pre-input source-compatible compact records; no combat is
    // executed and no victory/flee result is installed.
    uint contacts=0;
    for (uint kind=0;kind<3;++kind) {
        report=Common::String::format("completion512 contact checkpoint%u",kind);
        byte compact[24]={};compact[0]=17;compact[1]=byte(kind?43:44);compact[2]=164;compact[3]=1;
        Common::ScopedPtr<NightActorValidation504> checkpoint(new NightActorValidation504(archive));NightActorValidation504 &c=*checkpoint;
        if (!c.setup(69,17,42,0x12,0,1,compact)) return false;c.turn.terrainRecovery=14; // Declared before first tested input.
        if (!kind) {
            const WorldInputResult result=c.send(Common::KEYCODE_DOWN);
            if (result!=kWorldInputEncounter || c.game.world.x()!=17 || c.game.world.y()!=43 || c.turn.terrainRecovery!=16 ||
                    !c.input.encounter().battle || c.input.encounter().bank || c.input.encounter().slot) {
                report=Common::String::format("completion512 queuedmove result%u xy%u/%u recovery%u event%u bank%u slot%u",uint(result),uint(c.game.world.x()),uint(c.game.world.y()),uint(c.turn.terrainRecovery),uint(c.input.encounter().battle),uint(c.input.encounter().bank),uint(c.input.encounter().slot));return false;
            }
        } else if (kind==1) {
            WorldWait wait;WorldActorTurnEvent event;if (!wait.load(archive,8)) return false;
            const WorldWaitResult result=wait.run(c.game.world,c.window,c.scene,c.turn,c.game.clock,c.light,c.visible,c.game.schedule,c.routes,c.random,c.messages,c.text,c.frame,c.game.appearance,0,event,true,4,nullptr,&c.game.party);
            if (result!=kWorldWaitBattle || !event.battle || event.bank || event.slot || c.turn.terrainRecovery!=14 || c.game.world.x()!=17 || c.game.world.y()!=42) {
                report=Common::String::format("completion512 stationary result%u recovery%u event%u bank%u slot%u",uint(result),uint(c.turn.terrainRecovery),uint(event.battle),uint(event.bank),uint(event.slot));return false;
            }
        } else {
            const WorldInputResult ask=c.send(Common::KEYCODE_a,'A'),result=c.send(Common::KEYCODE_DOWN);
            if (ask!=kWorldInputHandled || result!=kWorldInputEncounter || !c.input.encounter().battle || c.turn.terrainRecovery!=14 || c.game.world.x()!=17 || c.game.world.y()!=42) {
                report=Common::String::format("completion512 manual ask%u result%u recovery%u event%u",uint(ask),uint(result),uint(c.turn.terrainRecovery),uint(c.input.encounter().battle));return false;
            }
        }
        if (!terrainTurnCodec511(c.turn)) return false;++contacts;
    }
    report=Common::String::format("move_completion\texact_source_branch_cases=16\tactual_within75_counter_raster_hold=1\tactual_leave75_reset=1\tqueued_movement_stationary_manual_contact_cases=%u\tturn_codecs=4\tbattle_execution=0\tcampaign=0\told_animation_audio_suite=0\n",contacts);
    return true;
}
} }
#endif
