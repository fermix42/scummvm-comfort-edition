/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_CLOCK_VALIDATION_H
#define NOX_NATIVE_DIALOGUE_CLOCK_VALIDATION_H
#include "nox/native_world_tail_validation.h"
namespace Nox { namespace Native {
inline bool validateNativeDialogueClock515(HdvArchive &archive,Common::String &report) {
    report="dialogueclock515 authored UM97 physical NPC8";
    Common::ScopedPtr<NewGame> probe(new NewGame(archive));if (!probe->create("CLOCK",0,0)) return false;
    World initial(archive);if (!initial.initialize(2,97,142,169,probe->world.questFields(),256)) return false;
    const byte *actor=initial.currentTables()->data()+1280+8;const byte ax=actor[0],ay=actor[1];if (!actor[2] || actor[3]==255 || ax!=142 || ay!=169) return false;
    const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};
    const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};uint cases=0,codecs=0;
    for (uint boundary=0;boundary<4;++boundary) {
        report=Common::String::format("dialogueclock515 native physical-adjacent setup%u NPC%u,%u",boundary,uint(ax),uint(ay));
        Common::ScopedPtr<NightActorValidation504> owner;byte direction=255;
        for (uint i=0;i<4 && !owner;++i) {
            Common::ScopedPtr<NightActorValidation504> candidate(new NightActorValidation504(archive));
            if (tailSetup514(*candidate,2,97,byte(ax+dx[i]),byte(ay+dy[i]),0,0,255,255,false,false,8) && candidate->scene.actors.lookup[near[i]]==8 && (candidate->scene.actors.kinds[near[i]]&135)==4) {direction=byte(i);owner.reset(candidate.release());}
        }
        if (!owner) return false;NightActorValidation504 &v=*owner;
        // Original authored physical NPC retained. Only phase/minute starting
        // prerequisites differ;no fight flag or resulting event is installed.
        v.game.clock.phase=byte(boundary&1);v.game.clock.minute=byte(boundary>=2?0x59:0x34);const TurnClock before=v.game.clock;
        report=Common::String::format("dialogueclock515 actual T/direction opening%u",boundary);
        if (v.send(Common::KEYCODE_t,'T')!=kWorldInputHandled || v.send(keys[direction])!=kWorldInputHandled || !v.input.noticeActive() || v.input.encounter().battle || !sameClock513(before,v.game.clock) || !tailCodecs514(v)) return false;++codecs;
        report=Common::String::format("dialogueclock515 actual authored request closure%u",boundary);WorldInputResult closed=kWorldInputHandled;uint acknowledgments=0;
        while (v.input.noticeActive() && acknowledgments++<16) {closed=v.send(Common::KEYCODE_g,'G');if (closed!=kWorldInputHandled && closed!=kWorldInputEncounter) return false;}
        if (closed!=kWorldInputEncounter || v.input.noticeActive() || !v.input.encounter().battle || v.input.encounter().clockPending || v.input.encounter().bank!=1 || v.input.encounter().slot!=8 || !(v.game.world.field(17)&32) || !sameClock513(before,v.game.clock) || !tailCodecs514(v) || !v.input.completeEncounterClock(v.game.world,v.game.clock) || !sameClock513(before,v.game.clock)) {
            report=Common::String::format("dialogueclock515 closure%u result%u stage%u event%u pending%u field17%u phase%u->%u",boundary,uint(closed),uint(v.input.conversationStage()),uint(v.input.encounter().battle),uint(v.input.encounter().clockPending),uint(v.game.world.field(17)),uint(before.phase),uint(v.game.clock.phase));return false;
        }
        ++cases;++codecs;
    }
    report=Common::String::format("dialogue_clock\tactual_authored_opening_fight_cases=%u\tcomponent_codec_sets=%u\tflag17_not_injected=1\thost_clock_policy_noop=4\tbattle_execution=0\tcampaign=0\told_audio_animation_suite=0\n",cases,codecs);return true;
}
} }
#endif
