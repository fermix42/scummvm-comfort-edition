/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PRAEVOC_VALIDATION_H
#define NOX_NATIVE_PRAEVOC_VALIDATION_H
#include "nox/native_night_actors_validation.h"
#include "nox/native_cwspeech.h"
namespace Nox { namespace Native {
inline bool praevocInitialFields505(NightActorValidation504 &v,bool gate) {
    Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);if (!v.game.world.save(out)) return false;
    Common::Array<byte> bytes;bytes.resize(out.size());memcpy(bytes.data(),out.getData(),out.size());
    bytes[11+12]&=239;bytes[11+50]=byte((bytes[11+50]&254)|(gate?1:0));
    Common::MemoryReadStream input(bytes.data(),bytes.size());if (!v.game.world.load(input)) return false;
    v.window=WorldStory();byte previous[187]={};
    return v.window.initialize(v.game.world,v.game.world.currentTables()->data()+1536,256) &&
        WorldViewport::refreshAtWorld(v.game.world,v.window,v.story,v.actors,v.lighting,v.occlusion,v.painter,v.game.appearance,v.game.clock.lightActive,0,
            previous,v.turn.presentation,v.light,v.visible,v.frame,&v.scene) && v.text.importPages(v.frame.data(),v.frame.data(),8192);
}
inline bool praevocTurnCodec505(WorldActorTurnState &turn) {
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES),again(DisposeAfterUse::YES);if (!turn.save(saved)) return false;
    WorldActorTurnState next;Common::MemoryReadStream bytes(saved.getData(),saved.size());
    if (!next.load(bytes) || !next.save(again) || saved.size()!=again.size() || memcmp(saved.getData(),again.getData(),saved.size()) ||
            next.praevocCursor!=turn.praevocCursor || next.praevocUsed!=turn.praevocUsed) return false;
    turn=next;return true;
}
inline bool praevocCoreCodecs505(NightActorValidation504 &v) {
    Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),win(DisposeAfterUse::YES),command(DisposeAfterUse::YES);
    if (!v.game.save(core) || !v.window.save(win) || !v.input.save(command)) return false;
    Common::ScopedPtr<NewGame> next(new NewGame(v.archive));
    Common::MemoryReadStream a(core.getData(),core.size()),b(win.getData(),win.size()),c(command.getData(),command.size());
    WorldStory nextWindow;Common::ScopedPtr<WorldInput> nextInput(new WorldInput);
    if (!next->load(a) || !nextWindow.load(b,next->world) || !nextInput->load(c,v.archive,8) ||
            memcmp(next->world.currentTables()->data(),v.game.world.currentTables()->data(),1792)) return false;
    Common::MemoryReadStream restored(core.getData(),core.size());
    if (!v.game.load(restored)) return false;
    v.window=nextWindow;v.input=*nextInput;return v.input.atWorldCommand();
}
inline bool validateNativePraevoc505(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report,Common::Array<byte> &commands) {
    if (fixture.readUint32BE()!=MKTAG('N','P','S','5') || fixture.readUint16LE()!=1 || fixture.readUint16LE()!=9) return false;
    uint pulses=0,pcm=0;
    for (uint row=0;row<9;++row) {
        const byte command=fixture.readByte();const uint64 expectedEnd=fixture.readUint64LE();const uint count=fixture.readUint32LE();
        report=Common::String::format("praevoc505 waveform%u command%u",row,uint(command));
        Common::Array<uint64> edges;uint64 end;if (!CwSpeechEffect::schedule(archive,command,edges,end) || end!=expectedEnd || edges.size()!=count) return false;
        for (uint i=0;i<count;++i) if (edges[i]!=fixture.readUint64LE()) return false;
        Common::Array<int16> samples;uint toggles;if (!CwSpeechEffect::render(archive,command,samples,toggles) || toggles!=count) return false;
        bool nonzero=false;for (uint i=0;i<samples.size();++i) nonzero|=samples[i]!=0;if (!nonzero) return false;
        commands.push_back(byte(kCwSpeechFirst|command));pulses+=count;pcm+=samples.size();
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    report="praevoc505 old/new turn state codecs";
    for (uint variant=0;variant<4;++variant) {
        WorldActorTurnState turn;turn.effects.originKnown=bool(variant&1);turn.praevocUsed=variant>=2;turn.praevocCursor=variant>=2?2:5;
        if (!praevocTurnCodec505(turn)) return false;
        if (variant<2 && (turn.praevocUsed || turn.praevocCursor!=5)) return false;
    }
    {
        WorldActorTurnState old;Common::MemoryWriteStreamDynamic current(DisposeAfterUse::YES);if (!old.save(current) || current.size()!=119) return false;
        Common::Array<byte> bytes;bytes.resize(117);memcpy(bytes.data(),current.getData(),117);WRITE_LE_UINT16(bytes.data()+4,1);
        WorldActorTurnState restored;Common::MemoryReadStream input(bytes.data(),bytes.size());if (!restored.load(input) || restored.praevocUsed || restored.praevocCursor!=5) return false;
    }
    Common::Array<byte> rates;if (!archive.readRange("NA/BS.ROUTINES.BK2",0x2cbd,6,rates)) return false;
    uint triggers=0,muted=0,continuations=0;
    for (uint mode=0;mode<4;++mode) {
        report=Common::String::format("praevoc505 source checkpoint mode%u",mode);
        Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;
        if (!v.setup(69,15,42,0x12,0,1,nullptr) || !praevocInitialFields505(v,false) || v.scene.objects.lookup[110]!=20) return false;
        const bool enabled=mode==0 || mode==2;SoundEvents events,controlEvents;byte cursor=5;
        Common::ScopedPtr<NightActorValidation504> controlOwner(new NightActorValidation504(archive));NightActorValidation504 &control=*controlOwner;
        if (!control.setup(69,15,42,0x12,0,1,nullptr) || !praevocInitialFields505(control,true)) return false;
        auto sameRandom=[&]() {
            Common::MemoryWriteStreamDynamic a(DisposeAfterUse::YES),b(DisposeAfterUse::YES);
            return v.random.save(a) && control.random.save(b) && a.size()==b.size() && !memcmp(a.getData(),b.getData(),a.size());
        };
        auto sameBaseCues=[&]() {
            SoundEvents base;for (uint i=0;i<events.size();++i) if (byte(events[i])<128) base.push_back(events[i]);
            return base==controlEvents;
        };
        for (uint step=0;step<6;++step) {
            report=Common::String::format("praevoc505 actual trigger mode%u step%u",mode,step);
            v.input.setSoundEvents(&events,enabled);const byte beforePhase=v.game.world.field(12);const TurnClock before=v.game.clock;ClockAdvance advance;
            if (!advanceTurnClock(before,advance)) return false;
            control.input.setSoundEvents(&controlEvents,enabled);
            const WorldInputResult result=v.send(Common::KEYCODE_DOWN),baseline=control.send(Common::KEYCODE_DOWN);
            const bool rngEqual=sameRandom();
            if (result!=kWorldInputHandled || baseline!=kWorldInputHandled || !v.input.atWorldCommand() || v.game.world.x()!=15 || v.game.world.y()!=43 ||
                    v.game.world.field(12)!=byte(beforePhase^16) || memcmp(&v.game.clock,&advance.after,sizeof(TurnClock)) || !rngEqual || !sameBaseCues()) {
                report=Common::String::format("praevoc505 mode%u step%u result%u baseline%u idle%u xy%u,%u flag%u expected%u clock%u/%u:%u expected%u/%u:%u rngEqual%u cues%u baselineCues%u",mode,step,uint(result),uint(baseline),uint(v.input.atWorldCommand()),uint(v.game.world.x()),uint(v.game.world.y()),uint(v.game.world.field(12)),uint(byte(beforePhase^16)),uint(v.game.clock.phase),uint(v.game.clock.hour),uint(v.game.clock.minute),uint(advance.after.phase),uint(advance.after.hour),uint(advance.after.minute),uint(rngEqual),uint(events.size()),uint(controlEvents.size()));return false;
            }
            SoundEvents speech;for (uint i=0;i<events.size();++i) if (byte(events[i])>=128) speech.push_back(events[i]);
            if (enabled) {
                const byte base=rates[cursor];const bool paired=beforePhase&16;
                if (speech.size()!=(paired?2U:1U) || byte(speech[0])!=byte(kCwSpeechFirst|(paired?base|1:base)) ||
                        (paired && byte(speech[1])!=byte(kCwSpeechFirst|byte((base|1)-16)))) return false;
                cursor=cursor?byte(cursor-1):5;
                if (!v.turn.praevocUsed || v.turn.praevocCursor!=cursor) return false;
            } else {if (!speech.empty() || v.turn.praevocCursor!=5 || v.turn.praevocUsed) return false;++muted;}
            ++triggers;events.clear();controlEvents.clear(); // Consume observed presentation requests, never gameplay outcomes.
            if (step==2) {
                report=Common::String::format("praevoc505 midpoint codecs mode%u",mode);
                if (!praevocTurnCodec505(v.turn) || !praevocCoreCodecs505(v)) return false;++continuations;
                v.input.setSoundEvents(&events,enabled); // Same transient rebind as the ordinary host after load.
            }
            if (step<5) {
                control.input.setSoundEvents(&controlEvents,enabled);
                if (v.send(Common::KEYCODE_UP)!=kWorldInputHandled || control.send(Common::KEYCODE_UP)!=kWorldInputHandled || !sameRandom() || !sameBaseCues() ||
                        v.game.world.x()!=15 || v.game.world.y()!=42) return false;
                for (uint i=0;i<events.size();++i) if (byte(events[i])>=128) return false;
                events.clear();controlEvents.clear();
            }
        }
        if (v.turn.praevocCursor!=5 || (v.game.world.field(12)&16)) return false;
    }
    report="praevoc505 source field50 gate suppression";
    Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));NightActorValidation504 &v=*owner;SoundEvents events;
    if (!v.setup(69,15,42,0x12,0,1,nullptr) || !praevocInitialFields505(v,true)) return false;
    v.input.setSoundEvents(&events,true);
    if (v.send(Common::KEYCODE_DOWN)!=kWorldInputHandled || !v.input.atWorldCommand() || v.game.world.y()!=43 ||
            v.turn.praevocUsed || v.turn.praevocCursor!=5 || (v.game.world.field(12)&16) || !(v.game.world.field(50)&1)) return false;
    for (uint i=0;i<events.size();++i) if (byte(events[i])>=128) return false;
    report=Common::String::format("praevoc_speech\twaveforms=9\texact_source_edges=%u\tpcm_samples=%u\tactual_trigger_moves=%u\tmuted_moves=%u\tgate_suppression=1\tcursor_wrap_modes=4\tmidpoint_core_window_input_turn_codecs=%u\told_turn_formats=3\tnew_turn_forms=2\tno_extra_RNG_vs_gated_control=1\tcampaign=0\taudio473_suite=0\n",pulses,pcm,triggers,muted,continuations);
    return true;
}
} }
#endif
