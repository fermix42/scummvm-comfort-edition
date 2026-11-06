/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_STATE_H
#define NOX_NATIVE_ENCOUNTER_STATE_H
#include "nox/native_encounter_preparation.h"
#include "nox/native_encounter_formation.h"
#include "nox/native_battle_round.h"
#include "nox/native_battle_loot.h"
#include "nox/native_battle_advancement_state.h"
#include "nox/native_battle_attack.h"
#include "nox/native_battle_cast_state.h"
#include "nox/native_battle_exit_state.h"
#include "nox/native_battle_cannon.h"
#include "nox/native_quick_battle.h"
#include "nox/native_combat_mode.h"
namespace Nox { namespace Native {
// Owned construction/presentation phases. Tactical command/turn execution and
// return are subsequent operations, not inferred from a placed or painted roster.
class EncounterState {
public:
    // Temporary development presentation initialization, before any gameplay.
    // Source-equivalent dynamic cadence remains a separate acceptance item.
    EncounterState() : formed(false),presented(false),oriented(false),phase(1),command(0),target(255),resultPhase(0) { memset(armorBackup,0,6); }
    static bool rebuildLookup(BattleRecords &state,uint party=1,uint enemies=1,uint pool=0) {
        return BattleRoster::rebuild(state,party,enemies,pool);
    }
    bool orientInitial(byte facing) {
        if (!formed || oriented || facing>3) return false;
        // Original initial tactical presentation performs two ordered actor
        // passes. Each faces the party, then the enemy, advancing the cursor.
        // Reuse the owned world facing representation, rather than assigning
        // an observed enemy flag as a fixture result.
        for (uint pass=0;pass<2;++pass) for (uint slot=0;slot<=252;slot+=4) {
            if (slot<uint(round.partyCount)*4 && records.compact[slot+2] && records.compact[slot+3]!=255) {
                records.compact[slot+3]=byte((records.compact[slot+3]&63)|(facing<<6));facing=byte((facing+1)&3);
            }
            if (!(slot&7) && slot<uint(round.enemyCount)*8 && records.extended[slot+2] && records.extended[slot+3]!=255) {
                records.extended[slot+3]=byte((records.extended[slot+3]&63)|(facing<<6));facing=byte((facing+1)&3);
            }
        }
        oriented=true;return rebuildLookup(records,round.partyCount,round.enemyCount,round.allyCount);
    }
    bool begin(HdvArchive &archive,const NewGame &game,const PreparedEncounter &prepared,GameplayRandom &random) {
        if (formed || !game.ready() || !game.party.memberCount() || game.party.memberCount()>6 ||
            prepared.terrain.size()!=187) return false;
        BattleRecords next;
        for (uint i=0;i<6;++i) { const CharacterRecord *member=game.party.member(i);if (!member) return false;memcpy(next.stats+i*128,member->bytes,128); }
        GameplayRandom nextRandom=random;
        byte count=0,groupFlags=0;
        if (!EncounterFormation::group(archive,prepared.admission.group,prepared.admission.monster?prepared.admission.monster:255,
            game.party.memberCount(),nextRandom,next,count,groupFlags,prepared.contextFlags,prepared.admission.sprite)) return false;
        records=next;random=nextRandom;round.partyCount=byte(game.party.memberCount());round.enemyCount=count;round.category=game.world.mapType();round.mapId=game.world.mapId();round.enemySpell=prepared.admission.spell;round.specialSpell=(game.world.field(58)&128)!=0;round.protectedTargets=prepared.protectedTargets;
        // On-foot category8 always forbids fleeing. Other parent families and
        // transport/Quick preparation remain separate admission callers.
        exit.control=EncounterAdmissionRules::initialNoFlee(round.category,prepared.contextFlags);BattleRound::initializeTurn(records,round);formed=true;return true;
    }
    bool save(Common::WriteStream &out) const {
        if ((presented && !formed) || presented!=oriented || phase>3) return false;
        if (mode.selection && !formed) return false;
        const uint16 baseVersion=mode.selection?13:quick.partyCount?12:11;
        if (!cannon.valid() || (command==3)!=(cannon.phase!=0) || (cannon.used() && (!formed || quick.partyCount))) return false;
        const bool ending=resultPhase==4;
        const bool revelation=resultPhase==5;
        if (revelation && (!formed || !presented || round.phase!=kBattleVictory || command || loot.started || advancement.started || round.mapId!=98)) return false;
        if (ending && (!formed || !presented || round.phase!=kBattleVictory || command || loot.started || advancement.started || round.mapId!=8)) return false;
        out.writeUint32BE(MKTAG('N','B','T','S'));out.writeUint16LE(revelation?16:ending?15:cannon.used()?14:baseVersion);out.writeByte((formed?1:0)|(presented?2:0)|(oriented?4:0)|(!formed && attack.partyTargeting?8:0));out.writeByte(phase);
        if (ending || revelation) out.writeByte(cannon.used()?14:baseVersion);
        if (cannon.used()) out.writeByte(byte(baseVersion));
        if (formed) { out.write(records.stats,2048);out.write(records.compact,256);out.write(records.extended,256); }
        if (formed) {
            if (command>3 || (target!=255 && target>=187) || round.phase>kBattleAutomaticTurn) return false;
            out.writeUint16LE(records.credit);out.writeByte(command);out.writeByte(target);
            out.writeByte(round.partyCount);out.writeByte(round.enemyCount);out.writeByte(round.allyCount);out.writeByte(round.activeParty);out.writeByte(round.category);
            out.writeByte(round.protectedTargets?1:0);out.writeByte(byte(round.phase));out.writeUint32LE(round.rounds);out.writeUint32LE(round.actions);
            if (resultPhase>5) return false;
            out.writeByte(resultPhase);out.write(armorBackup,6);if (!loot.save(out) || !advancement.save(out) || !attack.save(out)) return false;
            out.write(round.summonHealth,32);out.writeByte(round.mapId);out.writeByte(round.enemySpell);out.writeByte(round.specialSpell?1:0);if (!cast.save(out)) return false;
            if (!exit.valid() || (exit.phase && (command || (resultPhase && exit.phase!=3)))) return false;
            out.writeByte(exit.phase);out.writeByte(exit.control);
            if (mode.selection && (!mode.save(out) || (mode.selection==3)!=(quick.partyCount!=0))) return false;
            if (mode.selection) out.writeByte(quick.partyCount?1:0);
            if (quick.partyCount && (!quick.valid() || quick.partyCount!=round.partyCount || quick.enemyCount!=round.enemyCount || command || !quick.save(out))) return false;
        }
        if (cannon.used() && !cannon.save(out)) return false;
        if (presented) out.write(frame.data(),8192);
        return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.readUint32BE()!=MKTAG('N','B','T','S')) return false;
        uint16 version=in.readUint16LE();if (version<1 || version>16) return false;
        const bool ending=version==15,revelation=version==16;bool hasCannon=version==14;
        const byte state=in.readByte();EncounterState next;
        const byte base=state&7;
        if ((state&byte(version>=10?240:248)) || (base!=0 && base!=1 && !((version==2 && base==3) || (version>=3 && base==7))) ||
            ((state&8) && base)) return false;
        next.attack.partyTargeting=(state&8)!=0;
        next.formed=(state&1)!=0;next.presented=(state&2)!=0;
        next.oriented=(state&4)!=0;
        if (version>=2) next.phase=in.readByte();if (next.phase>3) return false;
        if (ending || revelation) {version=in.readByte();if (version<11 || version>14) return false;hasCannon=version==14;}
        if (hasCannon) {version=in.readByte();if (version<11 || version>13) return false;}
        if (next.formed && (in.read(next.records.stats,2048)!=2048 || in.read(next.records.compact,256)!=256 || in.read(next.records.extended,256)!=256 ||
            next.records.compact[0]>=17 || next.records.compact[1]>=11 || next.records.extended[0]>=17 || next.records.extended[1]>=11)) return false;
        if (next.formed && version>=4) {
            next.records.credit=in.readUint16LE();next.command=in.readByte();next.target=in.readByte();
            next.round.partyCount=in.readByte();next.round.enemyCount=in.readByte();next.round.allyCount=in.readByte();next.round.activeParty=in.readByte();next.round.category=in.readByte();
            const byte protectedTargets=in.readByte(),turnPhase=in.readByte();next.round.protectedTargets=protectedTargets!=0;next.round.phase=BattleRoundPhase(turnPhase);
            next.round.rounds=in.readUint32LE();next.round.actions=in.readUint32LE();
            if (next.command>(hasCannon?3:2) || (next.target!=255 && next.target>=187) || protectedTargets>1 || turnPhase>kBattleAutomaticTurn ||
                !next.round.partyCount || next.round.partyCount>6 || next.round.enemyCount>16 || next.round.allyCount>16 || next.round.activeParty>=next.round.partyCount) return false;
        } else if (next.formed) next.round.category=8; // Earlier private checkpoints admitted only this Keep profile.
        if (next.formed && version>=5) {
            next.resultPhase=in.readByte();if (next.resultPhase>(revelation?5:ending?4:version>=6?3:2) || (ending!=(next.resultPhase==4)) || (revelation!=(next.resultPhase==5)) || in.read(next.armorBackup,6)!=6) return false;
            byte bytes[20];if (in.read(bytes,20)!=20) return false;Common::MemoryReadStream part(bytes,20);
            if (!next.loot.load(part) || ((next.resultPhase==1 || (next.resultPhase==2 && version<11)) && !next.loot.started) ||
                ((!next.resultPhase || next.resultPhase==3 || ending || revelation) && next.loot.started)) return false;
            if (version>=6) {
                byte proposal[144];if (in.read(proposal,144)!=144) return false;Common::MemoryReadStream pending(proposal,144);
                if (!next.advancement.load(pending) || (next.resultPhase==3 && (!next.advancement.started || next.advancement.finished || !next.advancement.phase)) ||
                    next.advancement.cursor>next.round.partyCount) return false;
            }
        }
        if (next.formed && version>=7) {
            byte attackBytes[24];if (in.read(attackBytes,24)!=24) return false;Common::MemoryReadStream action(attackBytes,24);
            if (!next.attack.load(action) || (next.command==1 && next.attack.phase!=1) || (next.command!=1 && next.attack.phase)) return false;
        } else if (next.formed && next.command) {
            // Earlier private pending Attack checkpoints only admitted one
            // available main hand and no resolved hand. Preserve that lineage.
            next.attack.phase=1;next.attack.hands[1]=2;
        }
        if (next.formed && version>=8 && in.read(next.round.summonHealth,32)!=32) return false;
        if (next.formed && version>=9) {
            next.round.mapId=in.readByte();next.round.enemySpell=in.readByte();const byte special=in.readByte();
            if (special>1) return false;next.round.specialSpell=special!=0;
            byte bytes[13];if (in.read(bytes,13)!=13) return false;Common::MemoryReadStream casting(bytes,13);
            if (!next.cast.load(casting) || (next.command==2)!=(next.cast.phase!=0)) return false;
        }
        if (next.formed) {
            if (version>=11) { next.exit.phase=in.readByte();next.exit.control=in.readByte(); }
            else next.exit.control=next.round.category>=4?128:0;
            if (!next.exit.valid() || (next.exit.phase && (next.command || (next.resultPhase && next.exit.phase!=3))) ||
                (next.resultPhase==2 && !next.loot.started && next.exit.phase!=3)) return false;
            bool hasQuick=version==12;
            if (version>=13) {
                if (!next.mode.load(in)) return false;const byte quickPresent=in.readByte();if (quickPresent>1 || (next.mode.selection==3)!=(quickPresent!=0)) return false;hasQuick=quickPresent!=0;
                if (next.mode.selection==1 && (next.round.actions || next.command || next.resultPhase)) return false;
            }
            if (hasQuick && (!next.quick.load(in) || next.quick.partyCount!=next.round.partyCount || next.quick.enemyCount!=next.round.enemyCount || next.command)) return false;
        }
        if (version>=12 && !next.formed) return false;
        if (hasCannon) {
            byte bytes[23];if (in.read(bytes,23)!=23) return false;Common::MemoryReadStream part(bytes,23);
            if (!next.formed || next.quick.partyCount || !next.cannon.load(part) || !next.cannon.used() || (next.command==3)!=(next.cannon.phase!=0)) return false;
        }
        if (ending && (!next.formed || !next.presented || next.round.phase!=kBattleVictory || next.command || next.advancement.started || next.round.mapId!=8)) return false;
        if (revelation && (!next.formed || !next.presented || next.round.phase!=kBattleVictory || next.command || next.advancement.started || next.round.mapId!=98)) return false;
        if (next.presented) { byte image[8192];if (in.read(image,8192)!=8192 || !next.frame.importImage(image,8192)) return false; }
        if ((next.presented || (version>=8 && next.formed)) && !rebuildLookup(next.records,next.round.partyCount,next.round.enemyCount,next.round.allyCount)) return false;
        if (in.err() || in.pos()!=in.size()) return false;
        *this=next;return true;
    }
    bool formed,presented,oriented;byte phase,command,target,resultPhase,armorBackup[6];BattleRoundState round;BattleLootState loot;BattleAdvancementState advancement;BattleAttackState attack;BattleCastState cast;BattleExitState exit;BattleCannonState cannon;QuickBattleState quick;CombatModeState mode;BattleRecords records;HgrFrame frame;
};
} }
#endif
