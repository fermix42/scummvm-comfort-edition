/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_COMBAT_FIXTURE_H
#define NOX_NATIVE_COMBAT_FIXTURE_H
#include "nox/native_battle_aftermath.h"
#include "nox/native_battle_collisions.h"
namespace Nox { namespace Native {
// Explicit development checkpoint setup, before the tested input stream. This
// is assisted/controlled state, never an earned character or campaign claim.
inline bool prepareCombatBookFixture(HdvArchive &archive,byte spell,int mana,int health,NewGame &game,EncounterState &battle,Common::String &report) {
    if (!game.ready() || game.party.memberCount()!=1 || !battle.formed || !battle.presented || battle.command ||
        battle.round.partyCount!=1 || battle.round.enemyCount!=1 || battle.round.allyCount || battle.records.stats[1]!=1 ||
        READ_LE_UINT16(battle.records.stats+770)!=12) return false;
    BattleAdvancement levels;ItemCatalog items;if (!levels.load(archive) || !items.load(archive)) return false;
    WRITE_LE_UINT16(battle.records.stats+5,32000);
    for (uint settlement=0;settlement<9;++settlement) {
        BattleAdvancementState proposal;if (!levels.begin(battle.records,1,proposal)) return false;
        for (uint n=0;!proposal.finished && n<4;++n) {
            if (proposal.phase==1) { if (!levels.allocate(proposal.ability==2?proposal.remaining:0,proposal)) return false; }
            else if (proposal.phase==2) { if (!levels.confirm(true,battle.records,1,proposal)) return false; }
            else return false;
        }
        if (!proposal.finished) return false;
    }
    if (battle.records.stats[1]!=10) return false;
    if (mana>=0) { if (uint(mana)>battle.records.stats[18]) return false;battle.records.stats[4]=byte(mana); }
    if (health>=0) { if (!health || uint(health)>READ_LE_UINT16(battle.records.stats+72)) return false;WRITE_LE_UINT16(battle.records.stats+2,uint16(health)); }
    if (!game.party.applyBattleMembers(battle.records.stats,768,1)) return false;
    byte shortcuts[20];memset(shortcuts,255,10);memset(shortcuts+10,0,10);SpellBook book;if (!book.importState(shortcuts,20)) return false;
    byte bookId=255;
    for (uint i=0;i<70;++i) {
        ItemDefinition item;if (!items.definition(2,byte(i),item)) return false;
        if ((item.bytes[12]&31)!=8 || item.bytes[6]!=spell) continue;
        if (game.inventory.grantOne(items,2,byte(i))!=kInventoryApplied || book.select(items,game.inventory,game.party,byte(i),0,false,false)!=kBookReadied) return false;
        bookId=byte(i);break;
    }
    if (bookId==255) return false;game.book=book;
    report=Common::String::format("controlled-combat-book-setup\tspell=%u\tbook=%u\tXP_initialization=32000\tactual_INT_level_settlements=9\tlevel=%u\tINT=%u\tMP=%u\tHP=%u\tshortcut_reset=1\tactual_book_grant_and_readiness=1\tbefore_input_stream=1\tassisted_development_checkpoint=1\n",uint(spell),uint(bookId),uint(battle.records.stats[1]),uint(battle.records.stats[18]),uint(battle.records.stats[4]),uint(READ_LE_UINT16(battle.records.stats+2)));
    return true;
}
// Development input policy only: it emits ordinary keys. It never commits
// movement,damage,death,rewards or world transitions. Bounds prohibit grinding
// or hanging; the same production input handlers execute every selected key.
class CombatFixtureInput {
public:
    CombatFixtureInput() : steps(0),finished(false),_followingWorld(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> stations;
        if (!_collisions.load(archive) || !archive.readRange("NA/GAME",0x8bb,15,stations)) return false;
        memcpy(_stations,stations.data(),15);return true;
    }
    bool next(const EncounterState &battle,const PreparedEncounter &field,Common::Event &event) {
        if (finished || ++steps>256) return false;Common::KeyCode key=Common::KEYCODE_SPACE;
        if (battle.resultPhase==4) {finished=true;return true;}
        if (!battle.formed) {
            if (_followingWorld) { finished=true;return true; }
            _followingWorld=true;
        } else if (battle.resultPhase==3 || battle.round.phase==kBattleDefeat) return false;
        else if (battle.resultPhase==2) key=Common::KEYCODE_RETURN;
        else if (battle.resultPhase==1) key=Common::KEYCODE_l;
        else if (battle.round.phase==kBattleVictory) key=Common::KEYCODE_ESCAPE;
        else if (battle.command==1 || battle.command==3) key=Common::KEYCODE_RETURN;
        else if (battle.command) return false; // The authored spell input must finish first.
        else {
            const uint index=battle.round.activeParty*4;const byte cell=byte(battle.records.compact[index+1]*17+battle.records.compact[index]);
            VortexVictim actor;byte kind,range,target=255;
            if (!battle.records.actor(cell,actor) || !BattleTargets::hand(actor,false,kind,range) ||
                !BattleTargets::nearest(battle.records,cell,range,true,false,target)) return false;
            if (field.contextFlags&64) {
                if (!cannonInput(battle,field,actor,key)) return false;
            } else if (target!=255) key=Common::KEYCODE_a;
            else {
                if (!BattleTargets::nearest(battle.records,cell,255,false,false,target) || target==255 || field.terrain.size()!=187) return false;
                byte blocked[4];if (!_collisions.probe(battle.round.category,actor.actor[0],actor.actor[1],actor.actor[2],false,false,
                    field.terrain.data(),battle.records.indices,nullptr,nullptr,blocked)) return false;
                const int dx[]={0,0,1,-1},dy[]={-1,1,0,0};const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
                uint best=uint(ABS(int(actor.actor[0])-int(target%17))+ABS(int(actor.actor[1])-int(target/17)));
                for (uint d=0;d<4;++d) if (!blocked[d]) {
                    const uint distance=uint(ABS(int(actor.actor[0])+dx[d]-int(target%17))+ABS(int(actor.actor[1])+dy[d]-int(target/17)));
                    if (distance<best) { best=distance;key=keys[d]; }
                }
            }
        }
        event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,key>=Common::KEYCODE_SPACE && key<=Common::KEYCODE_z?uint16(key):0);
        return true;
    }
    uint steps;bool finished;
private:
    bool cannonInput(const EncounterState &battle,const PreparedEncounter &field,const VortexVictim &actor,Common::KeyCode &key) const {
        // Development input policy only. Search the actual collision graph
        // for an authored station, then emit F/Return/reload through normal
        // gameplay. No relocation, damage, RNG or continuation state writes.
        if (field.terrain.size()!=187) return false;
        const uint origin=actor.actor[1]*17+actor.actor[0];if (origin>=187) return false;
        byte first[187],queue[187];bool seen[187]={};uint read=0,write=0;
        first[origin]=255;queue[write++]=byte(origin);seen[origin]=true;
        const int dx[]={0,0,1,-1},dy[]={-1,1,0,0};
        const Common::KeyCode keys[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
        while (read<write) {
            const uint cell=queue[read++];bool station=false;
            for (uint i=0;i<15;++i) if (cell+34==_stations[i]) {station=true;break;}
            if (station) {key=cell==origin?Common::KEYCODE_f:keys[first[cell]];return true;}
            byte blocked[4];if (!_collisions.probe(battle.round.category,byte(cell%17),byte(cell/17),actor.actor[2],false,false,
                field.terrain.data(),battle.records.indices,nullptr,nullptr,blocked)) return false;
            for (uint d=0;d<4;++d) {
                const int x=int(cell%17)+dx[d],y=int(cell/17)+dy[d];
                if (blocked[d] || x<0 || x>=17 || y<0 || y>=11) continue;
                const uint next=uint(y*17+x);if (seen[next]) continue;
                seen[next]=true;first[next]=cell==origin?byte(d):first[cell];queue[write++]=byte(next);
            }
        }
        // Crowded decks may have no free station this turn; a real Pass
        // lets the remaining actors act rather than mutating their occupancy.
        key=Common::KEYCODE_SPACE;return true;
    }
    bool _followingWorld;BattleCollisions _collisions;byte _stations[15];
};
} }
#endif
