/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TACTICAL_CONTROLS_VALIDATION_H
#define NOX_NATIVE_TACTICAL_CONTROLS_VALIDATION_H
#include "nox/native_battle_party_control.h"
#include "nox/native_battle_presentation.h"
namespace Nox {
inline bool validateNativeTacticalControls481(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    using namespace Native;
    report="tactical lock source selection";
    if (input.readUint32BE()!=MKTAG('N','T','4','8') || input.readUint32LE()!=1 || input.readUint32LE()!=3024) return false;
    BattleRound rules;byte terrain[187];memset(terrain,1,187);
    for (uint i=0;i<3024;++i) {
        byte row[9];if (input.read(row,9)!=9) return false;
        BattleRecords records;BattleRoundState state;GameplayRandom random;
        if (!random.initializeEntropy(119)) return false;
        state.partyCount=row[0];state.activeParty=row[1];state.lockedParty=row[4];state.enemyCount=0;
        for (uint p=0;p<state.partyCount;++p) {records.stats[p*128]=0;WRITE_LE_UINT16(records.stats+p*128+2,100);records.compact[p*4+3]=row[2]==p+1?row[3]:0;}
        const uint32 seed=random.seed();bool accepted=false;
        if (!rules.lockParty(row[2],terrain,187,state,random,records,accepted) || accepted!=(row[5]!=0) || state.lockedParty!=row[6] || state.rounds!=row[7] || state.actions || random.seed()!=seed) {
            report=Common::String::format("tactical source case%u selection/lock/round",i);return false;
        }
        // Original slice stops before the next D85D input admission. Native
        // finish also performs that admission after its (empty here) NPC phase.
        const byte selected=accepted && row[7] && row[6]?byte(row[6]-1):row[8];
        if (state.activeParty!=selected) {report=Common::String::format("tactical source case%u active",i);return false;}
    }
    report="tactical retained source admission";
    if (input.readUint32LE()!=189) return false;
    for (uint i=0;i<189;++i) {
        byte row[5];if (input.read(row,5)!=5) return false;
        BattleRecords records;BattleRoundState state;state.partyCount=row[0];state.lockedParty=row[1];
        WRITE_LE_UINT16(records.stats+(row[1]-1)*128+2,100);records.compact[(row[1]-1)*4+3]=row[2];
        const byte selected=BattleRound::retainedParty(records,state);
        if (state.lockedParty!=row[3] || (row[3] && selected!=row[4]) || (!row[3] && selected!=255)) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="tactical shifted input decoding";
    const char *keys=")!@#$%^";
    for (byte i=0;i<7;++i) {
        byte choice=255;Common::KeyState punctuation(Common::KeyCode(keys[i]),keys[i]);
        if (!BattlePartyControl::requested(punctuation,choice) || choice!=i) return false;
        Common::KeyState shifted(Common::KeyCode(Common::KEYCODE_0+i),0,Common::KBD_SHIFT);
        if (!BattlePartyControl::requested(shifted,choice) || choice!=i) return false;
        Common::KeyState plain(Common::KeyCode(Common::KEYCODE_0+i),'0'+i);
        if (BattlePartyControl::requested(plain,choice)) return false;
    }
    // Controlled two-member formation uses actual NewGame member data and the
    // authored group124/template0. Duplicated member and flat field are declared
    // setup before tested scheduling, not recruitment or earned combat evidence.
    report="tactical controlled formation";
    Common::ScopedPtr<NewGame> game(new NewGame(archive));if (!game->create("LOCK",0,0)) return false;
    Common::ScopedPtr<EncounterState> battle(new EncounterState);
    memcpy(battle->records.stats,game->party.member(0)->bytes,128);memcpy(battle->records.stats+128,game->party.member(0)->bytes,128);
    GameplayRandom random;if (!random.initializeEntropy(119) || !rules.load(archive)) return false;
    byte enemies=0,flags=0;
    if (!EncounterFormation::group(archive,124,0,2,random,battle->records,enemies,flags) || enemies!=1 || flags) return false;
    battle->formed=true;battle->round.partyCount=2;battle->round.enemyCount=enemies;battle->round.category=8;
    if (!BattleRoster::rebuild(battle->records,2,enemies,0)) return false;
    bool accepted=false;const uint32 firstActions=battle->round.actions;
    report="tactical lock actual NPC cycle";
    if (!rules.lockParty(1,terrain,187,battle->round,random,battle->records,accepted) || !accepted || battle->round.lockedParty!=1 || battle->round.activeParty || battle->round.rounds!=1 || battle->round.actions<=firstActions) return false;
    PreparedEncounter field;field.terrain.resize(187);memcpy(field.terrain.data(),terrain,187);
    BattlePresentation presentation;HgrFrame locked,unlocked;
    report="tactical retained roster rendering";
    if (!presentation.load(archive,8) || !presentation.paint(field,*battle,0,locked)) return false;
    Common::ScopedPtr<EncounterState> plain(new EncounterState(*battle));plain->round.lockedParty=0;
    if (!presentation.paint(field,*plain,0,unlocked) || !memcmp(locked.data(),unlocked.data(),8192)) return false;
    report="tactical NBTS17 continuation";
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!battle->save(saved) || READ_LE_UINT16(saved.getData()+4)!=17) return false;
    Common::ScopedPtr<EncounterState> restored(new EncounterState);Common::MemoryReadStream bytes(saved.getData(),saved.size());
    if (!restored->load(bytes) || restored->round.lockedParty!=1 || restored->round.activeParty) return false;
    if (!rules.pass(terrain,187,restored->round,random,restored->records) || restored->round.lockedParty!=1 || restored->round.activeParty || restored->round.rounds!=2) return false;
    report="tactical unlock normal party continuation";
    if (!rules.lockParty(0,terrain,187,restored->round,random,restored->records,accepted) || !accepted || restored->round.lockedParty || restored->round.activeParty!=1 || restored->round.rounds!=2) return false;
    Common::MemoryWriteStreamDynamic legacy(DisposeAfterUse::YES);if (!restored->save(legacy) || READ_LE_UINT16(legacy.getData()+4)!=11) return false;
    Common::MemoryReadStream oldBytes(legacy.getData(),legacy.size());if (!plain->load(oldBytes) || plain->round.lockedParty) return false;
    report="tactical dead retained member release";
    restored->round.lockedParty=1;WRITE_LE_UINT16(restored->records.stats+2,0);
    if (BattleRound::retainedParty(restored->records,restored->round)!=255 || restored->round.lockedParty) return false;
    report="tactical-controls\tsource_selections=3024\tretained_admissions=189\tshifted_aliases=14\tactual_npc_cycles=2\tlock_unlock_codecs=2\troster_indicator=1\tcampaign=0\taudio_suite=0\n";return true;
}
}
#endif
