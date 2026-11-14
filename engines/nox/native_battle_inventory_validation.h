/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_INVENTORY_VALIDATION_H
#define NOX_NATIVE_BATTLE_INVENTORY_VALIDATION_H
#include "nox/native_battle_inventory.h"
#include "nox/native_battle_presentation.h"
namespace Nox {
inline bool validateNativeBattleInventory482(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    using namespace Native;
    report="inventory source combat guard";
    if (input.readUint32BE()!=MKTAG('N','I','4','8') || input.readUint32LE()!=1 || input.readUint32LE()!=144) return false;
    for (uint i=0;i<144;++i) {byte row[5];if (input.read(row,5)!=5 || InventoryActions::readyCombatRefusal(row[0]!=0,row[1],row[2],row[3]==4)!=row[4]) return false;}
    report="inventory source partial/full equipment records";
    if (input.readUint32LE()!=64) return false;ItemCatalog catalog;if (!catalog.load(archive)) return false;
    for (uint i=0;i<64;++i) {
        byte flags[3],expected[128];CharacterRecord member;EquipmentRecord gear;
        if (input.read(flags,3)!=3 || input.read(member.bytes,128)!=128 || input.read(gear.bytes,16)!=16 || input.read(expected,128)!=128 ||
            !recalculateEquipment(catalog,gear,member,flags[2] || flags[0],flags[2] || flags[1],flags[2]!=0) || memcmp(member.bytes,expected,128)) {report=Common::String::format("inventory source derived case%u",i);return false;}
    }
    if (input.err() || input.pos()!=input.size()) return false;
    uint menus=0,refusals=0,transactions=0,codecs=0;
    for (uint profile=0;profile<5;++profile) {
        report=Common::String::format("inventory profile%u setup",profile);
        Common::ScopedPtr<NewGame> game(new NewGame(archive));if (!game->create("ITEMS",0,0)) return false;
        // Source-supported controlled readiness/casting prerequisites, not
        // earned attributes. Starting inventory already occupies both hands.
        byte members[768];for (uint i=0;i<6;++i) memcpy(members+i*128,game->party.member(i)->bytes,128);
        members[16]=50;members[17]=20;members[18]=20;members[4]=20;
        if (!game->party.applyBattleMembers(members,768,1) || !EquipmentActions::recalculateAll(catalog,game->party)) return false;
        if (profile==1) {
            EquipmentOwnership ownership;const EquipmentRecord *gear=game->party.equipment(0);
            if (!ownership.load(archive) || gear->bytes[2]!=1 || EquipmentActions::selectArmor(catalog,ownership,0,gear->bytes[3],true,game->party,game->inventory)!=kArmorUnreadied) return false;
        }
        byte kind=profile==0?0:profile<3?1:2,id=255;CastRules casting;if (!casting.load(archive)) return false;
        const byte *member=game->party.member(0)->bytes;
        for (uint candidate=1;candidate<(kind==0?102U:kind==1?122U:168U);++candidate) {
            ItemDefinition definition;if (!catalog.definition(kind,byte(candidate),definition)) return false;const byte *d=definition.bytes;
            if (profile==0 && ((d[12]&31)!=1 || (d[11]&128) || d[2]>member[16] || d[3]>member[17] || d[4]>member[18] || game->inventory.find(kind,byte(candidate))>=0)) continue;
            if (profile==1 && (d[12]!=1 || d[2]>member[16] || d[3]>member[17] || d[4]>member[18])) continue;
            if (profile==2 && ((d[12]&31)<2 || (d[12]&31)>7)) continue;
            if (profile==3 && ((d[12]&31)!=8 || d[6]>=49 || d[13]==163 || d[4]>member[18])) continue;
            if (profile==4 && (candidate<64 || !(d[8]&64) || (d[8]&128))) continue;
            if (game->inventory.grantOne(catalog,kind,byte(candidate))!=kInventoryApplied) return false;
            if (profile==3) {
                SpellBook book=game->book;CastPreparation prepared;
                if (book.select(catalog,game->inventory,game->party,byte(candidate),2,false,false)!=kBookReadied || casting.prepare(book,*game->party.member(0),2,true,prepared)!=kCastPrepared) continue;
                game->book=book;
            }
            id=byte(candidate);break;
        }
        if (id==255) return false;
        PreparedEncounter field;field.terrain.resize(187);memset(field.terrain.data(),1,187);field.admission.group=124;field.admission.monster=0;
        GameplayRandom random;if (!random.initializeEntropy(119)) return false;
        Common::ScopedPtr<EncounterState> battle(new EncounterState);
        if (!battle->begin(archive,*game,field,random) || !battle->orientInitial(0)) return false;
        battle->presented=true;battle->round.lockedParty=1;
        // Declared pre-operation live battle divergence and temporary armor.
        // No expected outcome or setup mutation is inserted once the menu opens.
        if (READ_LE_UINT16(battle->records.stats+2)<2) return false;
        WRITE_LE_UINT16(battle->records.stats+2,READ_LE_UINT16(battle->records.stats+2)-1);battle->records.stats[7]=73;battle->records.stats[11]=93;
        BattlePresentation presentation;if (!presentation.load(archive,8) || !presentation.paint(field,*battle,0,battle->frame)) return false;
        const BattleRecords before=battle->records;const uint32 seed=random.seed();const TurnClock clock=game->clock;SoundEvents sounds;
        Common::MemoryWriteStreamDynamic gameBefore(DisposeAfterUse::YES);if (!game->save(gameBefore)) return false;
        report=Common::String::format("inventory profile%u browse/cancel",profile);
        if (!BattleInventory::begin(archive,*game,*battle,&sounds) || !battle->inventory.active() || !battle->inventory.inCombat()) return false;
        auto send=[&](Common::KeyCode key,byte ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);return BattleInventory::handle(event,archive,*game,*battle,&sounds);};
        if (send(Common::KEYCODE_v,'V')!=kInventoryWindowHandled || send(Common::KEYCODE_a,'A')!=kInventoryWindowHandled || send(Common::KEYCODE_ESCAPE)!=kInventoryWindowClosed) return false;
        Common::MemoryWriteStreamDynamic gameAfter(DisposeAfterUse::YES);if (!game->save(gameAfter) || gameBefore.size()!=gameAfter.size() || memcmp(gameBefore.getData(),gameAfter.getData(),gameBefore.size()) || memcmp(&before,&battle->records,sizeof(BattleRecords))) return false;
        if (!BattleInventory::begin(archive,*game,*battle,&sounds)) return false;
        const int slot=game->inventory.find(kind,id);if (slot<0) return false;uint ordinal=0;
        for (uint i=0;i<uint(slot);++i) if (!game->inventory.item(i)->empty()) ++ordinal;
        for (uint i=0;i<ordinal;++i) if (send(Common::KEYCODE_DOWN)!=kInventoryWindowHandled) return false;
        report=Common::String::format("inventory profile%u actual transaction",profile);
        const uint soundCount=sounds.size();
        if (send(profile==4?Common::KEYCODE_u:Common::KEYCODE_r,profile==4?'U':'R')!=kInventoryWindowHandled) return false;
        if (profile>=2) {
            if (memcmp(&before,&battle->records,sizeof(BattleRecords)) || sounds.size()!=soundCount+1 || sounds.back()!=kSpeakerRefusal || battle->inventory.phase()!=1) return false;
            if (profile!=4 && (send(Common::KEYCODE_SPACE,' ')!=kInventoryWindowHandled || memcmp(&before,&battle->records,sizeof(BattleRecords)))) return false;
            ++refusals;
        } else {
            const InventoryItem *item=game->inventory.item(slot);EquipmentOwnership ownership;byte mask,clear;
            if (!ownership.load(archive) || !ownership.masks(0,mask,clear) || !((item->bytes[3]|item->bytes[4])&mask) || READ_LE_UINT16(battle->records.stats+2)!=READ_LE_UINT16(before.stats+2)) return false;
            if (!profile && (battle->records.stats[7]!=73 || battle->records.stats[11]!=93)) return false;
            if (profile==1 && (battle->records.stats[9]!=before.stats[9] || battle->records.stats[10]!=before.stats[10])) return false;
            ++transactions;
        }
        report=Common::String::format("inventory profile%u retained modal codec",profile);
        if (profile==4 && (send(Common::KEYCODE_d,'D')!=kInventoryWindowHandled || battle->inventory.phase()!=3)) return false;
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!battle->save(saved) || READ_LE_UINT16(saved.getData()+4)!=18) return false;
        Common::ScopedPtr<EncounterState> restored(new EncounterState);Common::MemoryReadStream bytes(saved.getData(),saved.size());
        if (!restored->load(bytes) || !restored->inventory.active() || !restored->inventory.inCombat() || restored->round.lockedParty!=1) return false;
        *battle=*restored;++codecs;
        if (profile==4 && (send(Common::KEYCODE_n,'N')!=kInventoryWindowHandled || battle->inventory.phase()!=1)) return false;
        if (send(Common::KEYCODE_ESCAPE)!=kInventoryWindowClosed || !presentation.paint(field,*battle,0,battle->frame)) return false;
        if (!profile) {
            const byte cell=byte(battle->records.compact[1]*17+battle->records.compact[0]);
            if (!BattleAttack::begin(battle->records,cell,false,battle->attack,battle->target) || !battle->records.stats[9]) return false;
        }
        if (profile==3) {CharacterRecord caster;memcpy(caster.bytes,battle->records.stats,128);CastPreparation prepared;if (casting.prepare(game->book,caster,2,true,prepared)!=kCastPrepared) return false;}
        if (battle->round.actions || battle->round.rounds || battle->round.lockedParty!=1 || seed!=random.seed() || memcmp(&clock,&game->clock,sizeof(TurnClock)) || memcmp(before.stats+768,battle->records.stats+768,1280) || memcmp(before.extended,battle->records.extended,256) || memcmp(before.compact,battle->records.compact,256)) return false;
        ++menus;
    }
    report=Common::String::format("battle-inventory\tsource_guards=144\tpartial_full_records=64\tactual_menus=%u\trefusal_families=%u\tpermitted_transactions=%u\tmodal_codecs=%u\tfree_round_rng=1\tcampaign=0\taudio_suite=0\n",menus,refusals,transactions,codecs);return true;
}
}
#endif
