/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_CAST_H
#define NOX_NATIVE_BATTLE_CAST_H
#include "nox/native_encounter_state.h"
#include "common/events.h"
namespace Nox { namespace Native {
enum BattleCastInput { kBattleCastIgnored,kBattleCastPending,kBattleCastCommitted,kBattleCastCancelled,kBattleCastRefused,kBattleCastUnsupported };
// Ordinary C/slot/target/direction/party continuation. All effects retire through
// the same round owner as Attack. Shared target identity and PvP stay in Attack's
// persistent selection state; casting does not invent a second remembered target.
class BattleCast {
public:
    bool load(HdvArchive &archive) {
        Common::Array<byte> combat,game;
        return _rules.load(archive) && archive.readRange("NA/SRTN.COMBAT.S",0x84e2,40,combat) && archive.readRange("NA/GAME",0x170d,40,game) &&
            text(combat,_notDead) && text(game,_failed);
    }
    static bool begin(EncounterState &battle) {
        if (battle.command || battle.cast.phase || battle.exit.phase || battle.round.phase==kBattleDefeat) return false;
        battle.command=2;battle.target=255;battle.cast=BattleCastState();battle.cast.phase=kBattleCastSlot;return true;
    }
    BattleCastInput handle(const Common::Event &event,const SpellBook &book,const byte *terrain,uint length,const BattleRound &rounds,
            GameplayRandom &random,EncounterState &battle,Common::String &notice) const {
        if (event.type!=Common::EVENT_KEYDOWN) return kBattleCastIgnored;
        if (battle.command!=2 || !battle.cast.phase || !battle.cast.valid() || !terrain || length!=187) return kBattleCastUnsupported;
        EncounterState next=battle;GameplayRandom rng=random;const Common::KeyCode key=event.kbd.keycode;
        const uint member=next.round.activeParty,index=member*4;const byte cell=byte(next.records.compact[index+1]*17+next.records.compact[index]);
        VortexVictim caster;if (!next.records.actor(cell,caster) || caster.kind!=3) return kBattleCastUnsupported;
        bool execute=false;
        if (next.cast.phase==kBattleCastSlot) {
            if (key==Common::KEYCODE_ESCAPE) { clear(next);battle=next;return kBattleCastCancelled; }
            if (key<Common::KEYCODE_0 || key>Common::KEYCODE_9) return kBattleCastIgnored;
            next.cast.slot=byte(key-Common::KEYCODE_0);CharacterRecord character;memcpy(character.bytes,caster.stats,128);
            const CastResult prepared=_rules.prepare(book,character,next.cast.slot,true,next.cast.prepared);
            if (prepared!=kCastPrepared) {
                if (prepared==kCastUnsupported) return kBattleCastUnsupported;
                notice=_rules.refusal(prepared);clear(next);battle=next;return kBattleCastRefused;
            }
            const byte spell=next.cast.prepared.spell;
            if (spell==12) next.cast.phase=kBattleCastDirection;
            else if (spell>=49 && spell<=51) {
                next.cast.phase=kBattleCastParty;next.cast.member=0;
                for (uint i=0;i<next.round.partyCount;++i) if (next.records.stats[i*128]!=255) { next.cast.member=byte(i);break; }
            } else if (spell==14 || spell==16 || spell==17 || spell==18 || spell==19 || spell==20 || spell==21 || spell==23 || spell==25 || spell==32 || spell==48 || spell==52) execute=true;
            else if (BattleDirectedSpell::supports(spell) || (spell>=2 && spell<=4) || (spell>=27 && spell<=29)) {
                if (!BattleTargets::initial(next.records,cell,255,next.attack.partyTargeting,next.attack.rememberedKind[member],next.attack.rememberedIndex[member],next.target)) return kBattleCastUnsupported;
                if (next.target==255) { clear(next);battle=next;return kBattleCastCancelled; }
                next.cast.phase=kBattleCastTarget;
            } else return kBattleCastUnsupported; // No invented test/reserved/world spell.
        } else if (next.cast.phase==kBattleCastTarget) {
            if (key==Common::KEYCODE_ESCAPE || key==Common::KEYCODE_TAB) { clear(next);battle=next;return kBattleCastCancelled; }
            const int direction=arrow(key);
            if (direction>=0) {
                if (!BattleTargets::cycle(next.records,cell,255,next.attack.partyTargeting,byte(direction),next.target)) return kBattleCastUnsupported;
            } else {
                if (key!=Common::KEYCODE_RETURN && key!=Common::KEYCODE_a && key!=Common::KEYCODE_f && key!=Common::KEYCODE_SPACE) return kBattleCastIgnored;
                if (!BattleAttack::remember(next.records,cell,next.target,next.attack)) return kBattleCastUnsupported;execute=true;
            }
        } else if (next.cast.phase==kBattleCastParty) {
            if (key==Common::KEYCODE_ESCAPE) { clear(next);battle=next;return kBattleCastCancelled; }
            if (key==Common::KEYCODE_RETURN) execute=true;
            else if (key==Common::KEYCODE_UP || key==Common::KEYCODE_DOWN || key==Common::KEYCODE_SPACE) {
                const uint step=key==Common::KEYCODE_UP?next.round.partyCount-1:1;
                for (uint n=0;n<next.round.partyCount;++n) {
                    next.cast.member=byte((next.cast.member+step)%next.round.partyCount);
                    if (next.cast.prepared.spell==49 || next.records.stats[uint(next.cast.member)*128]!=255) break;
                }
            } else {
                const char *shortcuts="!@#$%^";int chosen=-1;
                for (uint i=0;i<6;++i) if (event.kbd.ascii==uint16(shortcuts[i])) chosen=int(i);
                // Original shifted shortcuts always filter dead members;
                // resurrection's arrow cycling separately allows them.
                if (chosen<0 || uint(chosen)>=next.round.partyCount || next.records.stats[uint(chosen)*128]==255) return kBattleCastIgnored;
                next.cast.member=byte(chosen);
            }
        } else if (next.cast.phase==kBattleCastDirection) execute=true;
        else return kBattleCastUnsupported;
        if (!execute) { battle=next;return kBattleCastPending; }
        const CastPreparation prepared=next.cast.prepared;const byte spell=prepared.spell;Common::String effectNotice;
        if (spell==14 || spell==16 || spell==17 || spell==18 || spell==19 || spell==20) {
            SummonOutcome outcome;if (!rounds.summon(prepared,terrain,length,next.round,rng,next.records,outcome)) return kBattleCastUnsupported;
        } else if (spell==32 || spell==48 || (spell>=49 && spell<=52)) {
            byte objects[187];memset(objects,255,187);RestorationOutcome outcome;
            if (!rounds.restore(spell,prepared.manaCost,next.cast.member,false,terrain,length,objects,187,255,next.armorBackup,6,next.round,rng,next.records,outcome)) return kBattleCastUnsupported;
            if (outcome.effect==kRestorationNotDead) effectNotice=_notDead;
            else if (outcome.effect==kRestorationOccupied) effectNotice=_failed;
        } else if ((spell>=2 && spell<=4) || (spell>=27 && spell<=29)) {
            AreaSpellOutcome outcome;if (!rounds.area(prepared,next.target,false,terrain,length,next.round,rng,next.records,outcome)) return kBattleCastUnsupported;
        } else {
            DirectedSpellSelection selection;selection.cell=next.target;selection.direction=arrow(key)<0?255:byte(arrow(key));
            selection.cancelled=key==Common::KEYCODE_ESCAPE;DirectedSpellOutcome outcome;
            if (!rounds.directed(prepared,selection,terrain,length,next.round,rng,next.records,outcome)) return kBattleCastUnsupported;
        }
        notice=_rules.name(spell)+effectNotice;clear(next);battle=next;random=rng;return kBattleCastCommitted;
    }
private:
    static int arrow(Common::KeyCode key) {
        return key==Common::KEYCODE_UP?0:key==Common::KEYCODE_DOWN?1:key==Common::KEYCODE_RIGHT?2:key==Common::KEYCODE_LEFT?3:-1;
    }
    static void clear(EncounterState &battle) { battle.command=0;battle.target=255;battle.cast=BattleCastState(); }
    static bool text(const Common::Array<byte> &data,Common::String &out) {
        out.clear();for (uint i=0;i<data.size();++i) { const byte c=data[i]&127;if (!c) return true;out+=char(c); }return false;
    }
    CastRules _rules;Common::String _notDead,_failed;
};
} }
#endif
