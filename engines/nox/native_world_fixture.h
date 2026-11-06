/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_FIXTURE_H
#define NOX_NATIVE_WORLD_FIXTURE_H
#include "nox/native_new_game.h"
#include "nox/native_world_presentation.h"
#include "nox/native_visibility.h"
namespace Nox { namespace Native {
// Development-only controlled starting location, completed before input.
// No actor contact, battle formation or expected gameplay result is installed.
class WorldCheckpointFixture {
public:
    // Complete source-derived pre-conversation owners, before any world key.
    // The file contains only starting conditions, never an expected reply/item.
    static bool conversation(NewGame &game,LightState &light,GameplayRandom &random,Common::SeekableReadStream &input) {
        if (!game.ready() || input.size()-input.pos()!=2717 || input.readUint32BE()!=MKTAG('N','C','P','F') || input.readUint16LE()!=2) return false;
        const byte type=input.readByte(),map=input.readByte();
        if (type!=game.world.mapType() || map!=game.world.mapId()) return false;
        byte fields[256],partyBytes[870],items[1536],bookBytes[20],rngBytes[14];
        if (input.read(fields,256)!=256 || input.read(partyBytes,870)!=870 || input.read(items,1536)!=1536 || input.read(bookBytes,20)!=20) return false;
        Party party;Inventory inventory;SpellBook book;TurnClock clock;ClockAdvance check;
        Common::MemoryReadStream partyInput(partyBytes,870);
        if (!party.load(partyInput) || !inventory.importRecords(items,1536) || !book.importState(bookBytes,20)) return false;
        const uint16 gold=input.readUint16LE(),food=input.readUint16LE();
        clock.phase=input.readByte();clock.hour=input.readByte();clock.minute=input.readByte();clock.lightTicks=input.readByte();clock.lightActive=input.readByte();clock.hourlyCounter=input.readByte();
        LightState nextLight;nextLight.mode=input.readByte();nextLight.level=input.readByte();nextLight.transitionTicks=input.readByte();
        if (!advanceTurnClock(clock,check) || fields[20]!=clock.hourlyCounter || nextLight.mode>3 || nextLight.level>4 || input.read(rngBytes,14)!=14 || input.err()) return false;
        GameplayRandom nextRandom;Common::MemoryReadStream rngInput(rngBytes,14);if (!nextRandom.load(rngInput)) return false;
        memcpy(game.world._fields,fields,256);game.party=party;game.inventory=inventory;game.book=book;game.gold=gold;game.food=food;game.clock=clock;
        light=nextLight;random=nextRandom;
        // Local NPC positions/availability are authored schedules, not raw
        // zero-coordinate templates. Initialize them at the declared fixture
        // clock before the first input, using the ordinary source owner.
        const NpcSchedulePolicy policy={false,false};
        return NpcScheduler::apply(game.world,clock.hour,clock.minute,policy,game.schedule)!=kNpcScheduleUnsupported;
    }
    static bool standing(HdvArchive &archive,const NewGame &game,const WorldScene &scene) {
        TraversalRules rules;TraversalProbe probe;TraversalDecision result;
        if (!game.ready() || !rules.load(archive,game.world.mapType())) return false;
        probe.terrain=scene.actors.terrain[93];probe.actorPresent=scene.actors.lookup[93]!=255;
        const byte slot=scene.objects.lookup[93];probe.objectPresent=slot!=255;
        if (probe.objectPresent) {
            if (slot&3) return false;
            const byte *object=game.world.currentTables()->data()+1024+slot;
            probe.objectTile=object[2];probe.objectState=object[3];
        }
        return rules.probe(probe,result) && !result.blocked;
    }
    static bool conditions(NewGame &game,const Common::String &conditions,int health) {
        if (!game.ready() || conditions.size()!=game.party.memberCount() || health==0 || health>65535 || health<-1) return false;
        byte records[768];for (uint i=0;i<6;++i) memcpy(records+i*128,game.party.member(i)->bytes,128);
        for (uint i=0;i<conditions.size();++i) {
            byte *member=records+i*128;
            if (conditions[i]<'0' || conditions[i]>'2' || member[0]==255 || !READ_LE_UINT16(member+2)) return false;
            member[0]=byte(conditions[i]-'0');
            if (health>0) { if (health>READ_LE_UINT16(member+72)) return false;WRITE_LE_UINT16(member+2,uint16(health)); }
        }
        return game.party.applyBattleMembers(records,768,game.party.memberCount());
    }
    static bool restoreMana(NewGame &game) {
        if (!game.ready()) return false;
        byte records[768];
        for (uint i=0;i<6;++i) memcpy(records+i*128,game.party.member(i)->bytes,128);
        for (uint i=0;i<game.party.memberCount();++i) if (records[i*128]!=255) records[i*128+4]=records[i*128+18];
        return game.party.applyBattleMembers(records,768,game.party.memberCount());
    }
    static bool actor(NewGame &game,const byte *input,uint length) {
        if (!input || length!=7 || !game.ready() || input[0]>1 || (input[1]&3) ||
            (input[0] && (input[1]&7)) || input[4]>=254) return false;
        byte *record=game.world._states[game.world._current].tables.data()+(input[0]?1280:768)+input[1];
        if (!record[2] || record[3]==255) return false;
        record[0]=input[2];record[1]=input[3];record[3]=input[4];
        if (input[0]) { record[6]=input[5];record[7]=input[6]; }
        return true;
    }
    // Development-only position setup inside the SAME map. Preserve the
    // purchased record,controls,currency and all world progression. Actual
    // tested map transfer must still run through the normal movement owner.
    static bool repositionMounted(NewGame &game,byte slot,byte type,byte map,byte x,byte y) {
        if (!game.ready() || game.world.onFoot() || slot==255 || (slot&3) || game.world.mapType()!=type || game.world.mapId()!=map) return false;
        byte terrain;if (!game.world.terrainAt(x,y,terrain)) return false;
        byte *record=game.world._states[game.world._current].tables.data()+1024+slot;
        if (!record[2] || record[0]!=game.world.x() || record[1]!=game.world.y()) return false;
        game.world._x=x;game.world._y=y;record[0]=x;record[1]=y;return true;
    }
    static bool relocate(HdvArchive &archive,NewGame &game,byte type,byte map,byte x,byte y) {
        if (!game.ready() || !game.world.onFoot()) return false;
        World next(game.world);
        if (type==0) {
            bool found=false;
            for (uint i=0;i<next._states.size();++i) if (!next._states[i].family && !next._states[i].id) found=true;
            if (!found) {
                Common::Array<byte> data;
                if (!archive.read("NA/DATA.SAVE.GAMEN",data) || data.size()!=8045 ||
                    !next.initializeNewGameSurface(data.data()+0x600,1536)) return false;
            }
        }
        if (!next.travel(type,map,x,y)) return false;
        game.world.adoptState(next);return true;
    }
};
} }
#endif
