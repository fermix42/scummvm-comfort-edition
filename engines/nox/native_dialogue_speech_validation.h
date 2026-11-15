/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_SPEECH_VALIDATION_H
#define NOX_NATIVE_DIALOGUE_SPEECH_VALIDATION_H
#include "nox/native_praevoc_validation.h"
namespace Nox { namespace Native {
inline bool validateNativeDialogueSpeech506(HdvArchive &archive,Common::String &report) {
    Common::Array<byte> rates;if (!archive.readRange("NA/BS.ROUTINES.BK2",0x2cbd,6,rates)) return false;
    uint replies=0,codecs=0,requests=0;
    for (uint mode=0;mode<4;++mode) {
        report=Common::String::format("dialogue506 checkpoint mode%u",mode);
        Common::ScopedPtr<NightActorValidation504> owner;
        const byte positions[][2]={{20,42},{20,40},{19,41},{21,41}};
        const byte cells[]={76,110,94,92};
        const Common::KeyCode directions[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
        uint selected=4;
        for (uint at=0;at<4;++at) {
            Common::ScopedPtr<NightActorValidation504> next(new NightActorValidation504(archive));
            if (next->setup(69,positions[at][0],positions[at][1],0x12,0,1,nullptr) &&
                    next->scene.actors.lookup[cells[at]]==24 && (next->scene.actors.kinds[cells[at]]&135)==4) {owner.reset(next.release());selected=at;break;}
        }
        if (selected==4) return false;
        NightActorValidation504 &v=*owner;SoundEvents events;const bool enabled=mode==0 || mode==2;
        v.turn.praevocCursor=byte(mode+1);v.turn.praevocUsed=true; // Declared pre-input nondefault saved cursor.
        const byte cursor=v.turn.praevocCursor,phase=v.game.world.field(12)&16;const TurnClock clock=v.game.clock;
        Common::MemoryWriteStreamDynamic rngBefore(DisposeAfterUse::YES);if (!v.random.save(rngBefore)) return false;
        auto send=[&](Common::KeyCode key,uint16 ascii=0) {v.input.setSoundEvents(&events,enabled);return v.send(key,ascii);};
        report=Common::String::format("dialogue506 actual opening mode%u",mode);
        const WorldInputResult opened=send(Common::KEYCODE_t,'T'),spoken=send(directions[selected]);
        GlyphFont font;HgrFrame glyphCheck=v.frame;const char *authored="Achtung!";bool raster=font.load(archive,1);
        for (uint i=0;authored[i] && raster;++i) raster=glyphCheck.drawGlyph(font,byte(authored[i])|128,25+i,1,0);
        raster=raster && !memcmp(glyphCheck.data(),v.frame.data(),8192);
        if (opened!=kWorldInputHandled || spoken!=kWorldInputHandled || !raster || events.size()!=(enabled?1U:0U) ||
                (enabled && byte(events[0])!=byte(kCwSpeechFirst|rates[cursor])) ||
                v.turn.praevocCursor!=cursor || !v.turn.praevocUsed || (v.game.world.field(12)&16)!=phase) {
            report=Common::String::format("dialogue506 opening mode%u selected%u T%u direction%u stage%u raster%u cues%u first%u expected%u cursor%u/%u flag%u",mode,selected,uint(opened),uint(spoken),uint(v.input.conversationStage()),uint(raster),uint(events.size()),events.empty()?0U:uint(byte(events[0])),uint(byte(kCwSpeechFirst|rates[cursor])),uint(v.turn.praevocCursor),uint(cursor),uint(v.game.world.field(12)));return false;
        }
        requests+=events.size();events.clear();++replies;
        report=Common::String::format("dialogue506 consumed modal codecs mode%u",mode);
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!v.input.save(saved) || !praevocTurnCodec505(v.turn)) return false;
        Common::MemoryReadStream input(saved.getData(),saved.size());Common::ScopedPtr<WorldInput> restored(new WorldInput);
        if (!restored->load(input,archive,8)) return false;v.input=*restored;++codecs;
        for (uint page=0;page<16 && !v.input.atWorldCommand();++page) {
            report=Common::String::format("dialogue506 resumed close mode%u page%u stage%u",mode,page,uint(v.input.conversationStage()));
            if (send(Common::KEYCODE_SPACE,' ')!=kWorldInputHandled || !events.empty()) return false;
        }
        Common::MemoryWriteStreamDynamic rngAfter(DisposeAfterUse::YES);
        if (!v.input.atWorldCommand() || !v.random.save(rngAfter) || rngBefore.size()!=rngAfter.size() ||
                memcmp(rngBefore.getData(),rngAfter.getData(),rngBefore.size()) || memcmp(&clock,&v.game.clock,sizeof(TurnClock)) ||
                (v.game.world.field(12)&32) || (v.game.world.field(12)&16)!=phase || v.turn.praevocCursor!=cursor) return false;
        report=Common::String::format("dialogue506 subsequent information mode%u",mode);
        if (send(Common::KEYCODE_g,'G')!=kWorldInputHandled || !v.input.atWorldCommand() || !events.empty()) return false;
    }
    report=Common::String::format("dialogue_speech\tauthored_Achtung_replies=%u\tsound_modes=4\tnondefault_cursors=4\trequests=%u\tmuted=2\tmodal_turn_codec_continuations=%u\tno_replay_on_resume=1\tcallback_consumed=1\tno_cursor_phase_clock_RNG_change=1\tsubsequent_commands=4\tcampaign=0\told_audio_suite=0\n",replies,requests,codecs);
    return true;
}
} }
#endif
