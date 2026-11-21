/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_AFTERMATH_H
#define NOX_NATIVE_BATTLE_AFTERMATH_H
#include "nox/native_battle_advancement.h"
#include "nox/native_encounter_state.h"
#include "nox/native_world_input.h"
#include "nox/native_battle_world_return.h"
#include "nox/native_ending.h"
#include "nox/native_battle_advancement_presentation.h"
namespace Nox { namespace Native {
// Result transaction joins the actual battle output to its parent world/party.
// Pending loot choices and final acknowledgement remain explicit continuations.
class BattleAftermath {
public:
    BattleAftermath() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> text;
        if (!_rewards.load(archive) || !_advancement.load(archive) || !_loot.load(archive) || !_advancementPresentation.load(archive) ||
            !archive.readRange("NA/SRTN.INVENTORY",0x7b6d,0x655,text)) return false;
        if (!readText(text,0xa1b4,_gold) || !readText(text,0xa1c3,_food) || !readText(text,0xa1d2,_xp) ||
            !readText(text,0xa1e0,_found) || !readText(text,0xa1fb,_choices)) return false;
        _ready=true;return true;
    }
    bool begin(const WorldActorTurnEvent &contact,NewGame &game,EncounterState &battle,GameplayRandom &random,bool scripted=false,byte contactSprite=0) const {
        if (!_ready || !contact.battle || battle.round.phase!=kBattleVictory || battle.resultPhase || battle.command ||
            !game.ready() || game.party.memberCount()!=battle.round.partyCount) return false;
        NewGame next(game);EncounterState after=battle;GameplayRandom rng=random;BattleRewardOutcome award;BattleTreasureAward treasure;
        if (!_rewards.distribute(after.records,after.round.partyCount,after.round.category,false,next.gold,after.armorBackup,6,award) ||
            (!scripted && !contactVictory(next.world,contact,treasure)) ||
            !next.party.applyBattleMembers(after.records.stats,768,after.round.partyCount)) return false;
        //R:992A..99D1 enters its nonreturning ending before advancement/loot.
        //A guessed quest flag, last-enemy death or an ordinary victory cannot
        //substitute for this parent-map/contact identity and acknowledgement.
        if (EndingPresentation::selected(after.round.mapId,contactSprite)) {
            after.resultPhase=4;game.adoptState(next);battle=after;random=rng;return true;
        }
        if (treasure.marker) { after.loot.scripted=true;after.loot.kind=treasure.kind;after.loot.id=treasure.id; }
        if (EndingPresentation::revelation(after.round.mapId,next.world.field(56),next.world.field(59))) {
            after.resultPhase=5;game.adoptState(next);battle=after;random=rng;return true;
        }
        if (!_advancement.begin(after.records,after.round.partyCount,after.advancement)) return false;
        if (!after.advancement.finished) {after.resultPhase=3;after.advancement.phase=4;}
        else if (!beginLoot(next,after,rng)) return false;
        game.adoptState(next);battle=after;random=rng;return true;
    }
    bool acknowledgeRevelation(NewGame &game,EncounterState &battle,GameplayRandom &random) const {
        if (!_ready || battle.resultPhase!=5 || battle.round.phase!=kBattleVictory || !EndingPresentation::revelation(battle.round.mapId,game.world.field(56),game.world.field(59))) return false;
        NewGame next(game);EncounterState after=battle;GameplayRandom rng=random;
        if (!_advancement.begin(after.records,after.round.partyCount,after.advancement)) return false;
        if (!after.advancement.finished) {after.resultPhase=3;after.advancement.phase=4;}
        else if (!beginLoot(next,after,rng)) return false;
        game.adoptState(next);battle=after;random=rng;return true;
    }
    bool escape(NewGame &game,EncounterState &battle) const {
        if (!_ready || battle.exit.phase!=3 || battle.exit.control!=3 || battle.command || battle.resultPhase ||
            !game.ready() || game.party.memberCount()!=battle.round.partyCount) return false;
        NewGame next(game);EncounterState after=battle;BattleRewardOutcome award;
        if (!_rewards.distribute(after.records,after.round.partyCount,after.round.category,true,next.gold,after.armorBackup,6,award) ||
            !next.party.applyBattleMembers(after.records.stats,768,after.round.partyCount)) return false;
        // No XP,advancement,treasure or completed-contact effects on this path.
        after.resultPhase=2;game.adoptState(next);battle=after;return true;
    }
    bool abilityInput(Common::KeyCode key,NewGame &game,EncounterState &battle,GameplayRandom &random) const {
        if (!_ready || battle.resultPhase!=3) return false;
        NewGame next(game);EncounterState after=battle;GameplayRandom rng=random;BattleAdvancementState &state=after.advancement;
        if (state.phase==1) {
            if (key>=Common::KEYCODE_0 && key<=Common::KEYCODE_9) {
                if (state.digits<2) { state.value=byte(state.value*10+key-Common::KEYCODE_0);++state.digits; }
            } else if (key==Common::KEYCODE_BACKSPACE || key==Common::KEYCODE_DELETE) { if (state.digits) { --state.digits;state.value/=10; } }
            else if (key==Common::KEYCODE_RETURN) {
                if (!state.digits) return true;
                if (!_advancement.allocate(state.value,state)) return false;
                if (state.phase==1) state.digits=state.value=0;
            }
        } else if (state.phase==2) {
            if (key!=Common::KEYCODE_y && key!=Common::KEYCODE_n) return true;
            if (!_advancement.confirm(key==Common::KEYCODE_y,after.records,after.round.partyCount,state)) return false;
            if (!state.finished) state.phase=4;
        } else if (state.phase==3) {
            if (!_advancement.restart(after.records,after.round.partyCount,state)) return false;
            state.phase=4;
        } else if (state.phase==4) return true;
        else return false;
        if (!next.party.applyBattleMembers(after.records.stats,768,after.round.partyCount)) return false;
        if (state.finished && !beginLoot(next,after,rng)) return false;
        game.adoptState(next);battle=after;random=rng;return true;
    }
    bool advanceIntroduction(EncounterState &battle) const {
        if (!_ready || battle.resultPhase!=3 || battle.advancement.phase!=4) return false;
        BattleAdvancementState &s=battle.advancement;
        const uint position=uint(s.digits)*100+s.value+1;
        if (position>=_advancementPresentation.introduction(s).size()) {s.phase=1;s.digits=s.value=0;}
        else {s.digits=byte(position/100);s.value=byte(position%100);}
        return true;
    }
    bool choose(bool take,bool all,NewGame &game,EncounterState &battle,GameplayRandom &random) const {
        if (!_ready || battle.resultPhase!=1 || !battle.loot.pending) return false;
        NewGame next(game);EncounterState after=battle;GameplayRandom rng=random;
        if (!_loot.choose(take,all,after.records,next.inventory,rng,after.loot) || !settleSummary(next,after)) return false;
        game.adoptState(next);battle=after;random=rng;return true;
    }
    bool returnReady(const EncounterState &battle) const { return _ready && battle.resultPhase==2 &&
        (battle.exit.phase==3 || (battle.loot.finished && battle.loot.committed)); }
    bool retireWorldContact(const WorldActorTurnEvent &contact,NewGame &game,const EncounterState &battle,bool scripted=false) const {
        if (!returnReady(battle) || !contact.battle || !game.world.ready()) return false;
        if (scripted) return true;
        return BattleWorldReturn::retire(game.world._states[game.world._current].tables.data(),1792,game.world._fields,256,contact.bank,contact.slot,battle.exit.phase==3);
    }
    bool paint(const GlyphFont &font,EncounterState &battle) const {
        if (!_ready || !battle.resultPhase) return false;HgrFrame image=battle.frame;
        if (battle.resultPhase==3) {
            const BattleAdvancementState &state=battle.advancement;
            if (state.cursor>=battle.round.partyCount || !_advancementPresentation.paint(font,battle.records.stats+state.cursor*128,state,image)) return false;
            for (uint member=0;member<battle.round.partyCount;++member) {
                if (!image.clearTextSpan(25,1+member*2,14)) return false;
                const byte *record=battle.records.stats+member*128;
                for (uint i=0;i<14 && record[75+i];++i)
                    if (!image.drawGlyph(font,record[75+i],25+i,1+member*2,member==state.cursor?127:0)) return false;
            }
            battle.frame=image;return true;
        }
        for (uint row=14;row<24;++row) if (!image.clearTextSpan(0,row,40)) return false;
        uint row=15;
        if (battle.loot.pending) {
            Common::String item;if (!_loot.describe(battle.loot,item) || !draw(font,image,_found,row) || !draw(font,image,item,row)) return false;
            ++row;if (!draw(font,image,_choices,row)) return false;
        } else {
            if (battle.loot.scripted) {
                Common::String item;if (!_loot.describe(battle.loot,item) || !draw(font,image,item+"\r",row)) return false;
            }
            Common::String amount=Common::String::format("%u",uint(battle.loot.amount));
            if (battle.loot.amount && !draw(font,image,amount+(battle.loot.food?_food:_gold),row)) return false;
            if (!draw(font,image,Common::String::format("%u",uint(battle.records.credit))+_xp,row)) return false;
        }
        battle.frame=image;return true;
    }
private:
    bool beginLoot(NewGame &game,EncounterState &battle,GameplayRandom &random) const {
        if (!_loot.begin(battle.round.enemyCount,battle.round.partyCount,game.food,game.gold,battle.loot) ||
            !_loot.advance(battle.records,game.inventory,random,battle.loot,game.world._fields,256)) return false;
        battle.resultPhase=1;return settleSummary(game,battle);
    }
    bool settleSummary(NewGame &game,EncounterState &battle) const {
        if (!battle.loot.finished) return true;
        if (!_loot.commit(game.food,game.gold,battle.loot)) return false;
        // The original summary enforces this display/pool minimum after the
        // per-member XP transaction; do not apply XP a second time.
        if (!battle.records.credit) battle.records.credit=1;
        battle.resultPhase=2;return true;
    }
    static bool contactVictory(World &world,const WorldActorTurnEvent &contact,BattleTreasureAward &treasure) {
        if (!world.ready()) return false;
        return BattleWorldReturn::effects(world._states[world._current].tables.data(),1792,world._fields,256,contact.bank,contact.slot,false,&treasure);
    }
    static bool readText(const Common::Array<byte> &data,uint address,Common::String &out) {
        const uint start=address-0x9c00;if (start>=data.size()) return false;out.clear();
        for (uint i=start;i<data.size();++i) { const byte c=data[i]&127;if (!c) return true;out+=char(c); }
        return false;
    }
    static bool draw(const GlyphFont &font,HgrFrame &image,const Common::String &text,uint &row) {
        uint column=1;
        for (uint i=0;i<text.size();++i) {
            const byte c=byte(text[i]);if (c==13) { ++row;column=1;continue; }
            if (row>=24) return true;
            if (column>=39) { ++row;column=1;if (row>=24) return true; }
            if (!image.drawGlyph(font,c,column++,row,0)) return false;
        }
        return true;
    }
    bool _ready;BattleRewardRules _rewards;BattleAdvancement _advancement;BattleLoot _loot;BattleAdvancementPresentation _advancementPresentation;
    Common::String _food,_gold,_xp,_found,_choices;
};
} }
#endif
