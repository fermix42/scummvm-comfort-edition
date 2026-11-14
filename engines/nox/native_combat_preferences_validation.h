/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_COMBAT_PREFERENCES_VALIDATION_H
#define NOX_NATIVE_COMBAT_PREFERENCES_VALIDATION_H
#include "nox/native_combat_mode_admission.h"
namespace Nox {
inline bool validateNativeCombatPreferences485(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    using namespace Native;
    report="combat preference real configuration ownership";
    const Common::String domain="nox-preferences485-fixture";if (ConfMan.hasGameDomain(domain)) return false;ConfMan.addGameDomain(domain);
    if (CombatPreferences::read(ConfMan,domain)!=kPreferTactical) return false;
    ConfMan.setBool("nox_classic_saves",true,domain);ConfMan.setBool("nox_native_combat_quick",true,domain);
    if (CombatPreferences::read(ConfMan,domain)!=kPreferQuick) return false;
    ConfMan.setBool("nox_native_combat_ask",true,domain);if (CombatPreferences::read(ConfMan,domain)!=kPreferAsk) return false;
    for (uint mode=0;mode<3;++mode) {CombatPreferences::write(ConfMan,domain,mode);if (uint(CombatPreferences::read(ConfMan,domain))!=mode || !ConfMan.getBool("nox_classic_saves",domain)) return false;}
    if (CombatPreferences::read(ConfMan,domain,true)!=kPreferAsk) return false;
    ConfMan.set("nox_combat_mode","invalid",domain);if (CombatPreferences::read(ConfMan,domain)!=kPreferTactical) return false;
    CombatPreferences::write(ConfMan,domain,99);if (CombatPreferences::read(ConfMan,domain)!=kPreferTactical) return false;
    ConfMan.removeGameDomain(domain);
    report="combat preference source advice connections";
    if (input.readUint32BE()!=MKTAG('N','P','4','8') || input.readUint32LE()!=1 || input.readUint32LE()!=11) return false;
    uint source=0,codecs=0,restrictions=0;
    for (uint i=0;i<11;++i) {
        byte row[14];if (input.read(row,14)!=14 || !row[0] || row[0]>6) return false;
        Common::ScopedPtr<EncounterState> initial(new EncounterState);initial->formed=initial->presented=initial->oriented=true;
        initial->round.partyCount=row[0];initial->round.enemyCount=row[10];initial->round.category=0;initial->round.mapId=row[8];
        for (uint p=0;p<row[0];++p) {
            byte *stats=initial->records.stats+p*128;stats[1]=row[1+p];WRITE_LE_UINT16(stats+2,64);
            byte *actor=initial->records.compact+p*4;actor[0]=byte(p+1);actor[1]=8;actor[2]=246;actor[3]=0;
        }
        for (uint e=0;e<row[10];++e) {
            byte *stats=initial->records.stats+768+e*32;stats[0]=7;stats[1]=row[7];stats[16]=row[9];WRITE_LE_UINT16(stats+2,64);
            byte *actor=initial->records.extended+e*8;actor[0]=byte(e+1);actor[1]=2;actor[2]=row[9];actor[3]=1;actor[4]=1;actor[5]=3;
        }
        if (!BattleRoster::rebuild(initial->records,row[0],row[10],0)) return false;
        PreparedEncounter field;field.contactSprite=164;field.admission.group=124;
        Common::ScopedPtr<EncounterState> forced(new EncounterState(*initial)),ask(new EncounterState(*initial)),tactical(new EncounterState(*initial));
        report=Common::String::format("combat preference source context%u",i);
        if (!CombatModeAdmission::begin(archive,field,kPreferQuick,*forced) || forced->mode.selection!=3 || forced->mode.average!=row[11] || forced->mode.difference!=row[12] || forced->mode.warning!=(row[13]!=0) || !forced->quick.partyCount) return false;
        if (!CombatModeAdmission::begin(archive,field,kPreferAsk,*ask) || ask->mode.selection!=1 || ask->quick.partyCount) return false;
        Common::MemoryWriteStreamDynamic pending(DisposeAfterUse::YES);if (!ask->save(pending)) return false;
        Common::MemoryReadStream pendingBytes(pending.getData(),pending.size());if (!ask->load(pendingBytes) || !CombatModeAdmission::select(archive,field,true,*ask) || memcmp(&ask->records,&forced->records,sizeof(BattleRecords))) return false;
        ++codecs;
        if (!CombatModeAdmission::begin(archive,field,kPreferTactical,*tactical) || tactical->mode.selection!=2 || tactical->quick.partyCount || !CombatModeAdmission::begin(archive,field,kPreferQuick,*tactical) || tactical->mode.selection!=2 || tactical->quick.partyCount) return false;
        Common::MemoryWriteStreamDynamic prepared(DisposeAfterUse::YES);if (!forced->save(prepared)) return false;Common::MemoryReadStream preparedBytes(prepared.getData(),prepared.size());
        if (!forced->load(preparedBytes) || forced->mode.selection!=3 || forced->mode.difference!=row[12] || !QuickBattle::restore(forced->records,forced->quick)) return false;
        for (uint p=0;p<row[0];++p) if (forced->records.stats[p*128+1]!=row[1+p]) return false;
        for (uint e=0;e<row[10];++e) if (forced->records.stats[768+e*32]!=7 || forced->records.stats[769+e*32]!=row[7]) return false;
        ++codecs;
        field.contextFlags=16;Common::ScopedPtr<EncounterState> restricted(new EncounterState(*initial));
        if (!CombatModeAdmission::begin(archive,field,kPreferQuick,*restricted) || restricted->mode.selection!=2 || restricted->quick.partyCount || restricted->exit.control!=128) return false;
        ++restrictions;++source;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    // New mode2 must still reach the existing actual automatic round owner.
    // Declared sleeping member is a pre-operation predicate, not earned combat.
    report="selected Tactical actual automatic round";
    Common::ScopedPtr<NewGame> game(new NewGame(archive));if (!game->create("MODE",0,0)) return false;
    PreparedEncounter field;field.terrain.resize(187);memset(field.terrain.data(),1,187);field.admission.group=124;
    GameplayRandom random;if (!random.initializeEntropy(119)) return false;
    Common::ScopedPtr<EncounterState> battle(new EncounterState);if (!battle->begin(archive,*game,field,random) || !battle->orientInitial(0)) return false;battle->presented=true;
    battle->records.stats[0]=2;battle->records.compact[3]=byte((battle->records.compact[3]&192)|2);BattleRound::initializeTurn(battle->records,battle->round);
    if (!CombatModeAdmission::begin(archive,field,kPreferTactical,*battle) || battle->mode.selection!=2 || !CombatModeAdmission::automaticTactical(*battle)) return false;
    BattleRound round;if (!round.load(archive) || !round.automatic(field.terrain.data(),187,battle->round,random,battle->records) || battle->round.rounds!=1 || !battle->round.actions) return false;
    report=Common::String::format("combat-preferences\tconfig_cases=9\tsource_advice_contexts=%u\task_forced_quick_equivalence=%u\tmode_quick_codecs=%u\tforced_tactical=%u\tselected_tactical_auto_cycle=1\tgui_layout_qualification=0\tcampaign=0\taudio_suite=0\n",source,source,codecs,restrictions);return true;
}
}
#endif
