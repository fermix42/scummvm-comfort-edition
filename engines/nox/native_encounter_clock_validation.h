/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_CLOCK_VALIDATION_H
#define NOX_NATIVE_ENCOUNTER_CLOCK_VALIDATION_H
#include "nox/native_move_completion_validation.h"
namespace Nox { namespace Native {
inline bool sameClock513(const TurnClock &a,const TurnClock &b) {
    return a.phase==b.phase && a.hour==b.hour && a.minute==b.minute && a.lightTicks==b.lightTicks && a.lightActive==b.lightActive && a.hourlyCounter==b.hourlyCounter;
}
inline bool clockInputCodec513(HdvArchive &archive,WorldInput &input,bool pending) {
    Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);if (!input.save(out)) return false;
    const uint version=READ_LE_UINT16(out.getData()+4);if ((!pending && version!=17) || (pending && version==17)) return false;
    Common::MemoryReadStream stream(out.getData(),out.size());Common::ScopedPtr<WorldInput> loaded(new WorldInput);
    if (!loaded->load(stream,archive,8) || !loaded->encounter().battle || loaded->encounter().clockPending!=pending) return false;
    Common::MemoryWriteStreamDynamic round(DisposeAfterUse::YES);if (!loaded->save(round) || out.size()!=round.size() || memcmp(out.getData(),round.getData(),out.size())) return false;
    if (!pending) {
        // The retained inner historical format keeps the historical pending
        // tick. New wrapper changes only explicit caller ownership.
        const uint size=READ_LE_UINT32(out.getData()+6);Common::MemoryReadStream old(out.getData()+10,size);Common::ScopedPtr<WorldInput> legacy(new WorldInput);
        if (!legacy->load(old,archive,8) || !legacy->encounter().clockPending) return false;
    }
    input=*loaded;return true;
}
inline bool validateNativeEncounterClock513(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    report="clock513 original scalar fixture";
    if (fixture.readUint32BE()!=MKTAG('N','E','C','5') || fixture.readUint16LE()!=1 || fixture.readUint16LE()!=36) return false;
    for (uint i=0;i<36;++i) {
        byte a[6],b[6];if (fixture.read(a,6)!=6 || fixture.read(b,6)!=6) return false;
        TurnClock before;before.phase=a[0];before.hour=a[1];before.minute=a[2];before.lightTicks=a[3];before.lightActive=a[4];before.hourlyCounter=a[5];ClockAdvance next;
        if (!advanceTurnClock(before,next) || next.after.phase!=b[0] || next.after.hour!=b[1] || next.after.minute!=b[2] || next.after.lightTicks!=b[3] || next.after.lightActive!=b[4] || next.after.hourlyCounter!=b[5]) return false;
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    uint contacts=0,codecs=0;
    for (uint kind=0;kind<3;++kind) for (uint boundary=0;boundary<4;++boundary) {
        report=Common::String::format("clock513 actual contact kind%u boundary%u",kind,boundary);
        byte compact[24]={};compact[0]=17;compact[1]=byte(kind?43:44);compact[2]=164;compact[3]=1;
        Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;
        if (!v.setup(69,17,42,0x12,0,1,compact)) return false;
        // All clock/actor prerequisites are declared before first tested key.
        v.game.clock.phase=byte(boundary&1);v.game.clock.minute=byte(boundary>=2?0x59:0x34);v.game.clock.hour=byte(boundary==3?0x23:0x12);v.game.clock.lightTicks=1;v.game.clock.lightActive=1;v.game.clock.hourlyCounter=2;
        const TurnClock before=v.game.clock;ClockAdvance expected;if (!advanceTurnClock(before,expected)) return false;
        if (!kind) {if (v.send(Common::KEYCODE_DOWN)!=kWorldInputEncounter) return false;}
        else if (kind==1) {
            WorldWait wait;WorldActorTurnEvent event;if (!wait.load(archive,8) || wait.run(v.game.world,v.window,v.scene,v.turn,v.game.clock,v.light,v.visible,v.game.schedule,v.routes,v.random,v.messages,v.text,v.frame,v.game.appearance,0,event,true,4,nullptr,&v.game.party)!=kWorldWaitBattle || !v.input.queueEncounter(event)) return false;
        } else if (v.send(Common::KEYCODE_a,'A')!=kWorldInputHandled || v.send(Common::KEYCODE_DOWN)!=kWorldInputEncounter) return false;
        const bool pending=kind!=2;
        if (!sameClock513(before,v.game.clock) || v.input.encounter().clockPending!=pending || !clockInputCodec513(archive,v.input,pending)) return false;
        ++codecs;
        if (!v.input.completeEncounterClock(v.game.world,v.game.clock) || !sameClock513(v.game.clock,pending?expected.after:before) || (pending && v.game.world.field(20)!=expected.after.hourlyCounter)) return false;
        ++contacts;
    }
    // Typed already-completed admission branch only;not an actual Shadow run.
    Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;byte empty[24]={};
    if (!v.setup(69,17,42,0x12,0,1,empty)) return false;
    WorldActorTurnEvent event;event.battle=true;event.clockPending=false;event.bank=0;event.slot=0;event.direction=0;event.cell=93;
    const TurnClock before=v.game.clock;
    if (!v.input.queueEncounter(event) || !clockInputCodec513(archive,v.input,false) || !v.input.completeEncounterClock(v.game.world,v.game.clock) || !sameClock513(before,v.game.clock)) return false;++codecs;
    // A non-encounter inner body cannot claim the new admission wrapper.
    Common::ScopedPtr<WorldInput> idle(new WorldInput);Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES),bad(DisposeAfterUse::YES);if (!idle->save(body)) return false;
    bad.writeUint32BE(MKTAG('N','W','I','N'));bad.writeUint16LE(17);bad.writeUint32LE(body.size());bad.write(body.getData(),body.size());Common::MemoryReadStream invalid(bad.getData(),bad.size());Common::ScopedPtr<WorldInput> unchanged(new WorldInput);
    if (unchanged->load(invalid,archive,8) || !unchanged->atWorldCommand()) return false;
    report=Common::String::format("encounter_clock\toriginal_scalar_cases=36\tactual_contact_clock_cases=%u\tinput_codecs=%u\tlegacy_pending_defaults=5\talready_completed_policy=1\tinvalid_nonencounter_wrapper=1\tbattle_execution=0\tcampaign=0\told_audio_animation_suite=0\n",contacts,codecs);return true;
}
} }
#endif
