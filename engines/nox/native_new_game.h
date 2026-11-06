/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NEW_GAME_H
#define NOX_NATIVE_NEW_GAME_H
#include "nox/native_equipment_actions.h"
#include "nox/native_book.h"
#include "nox/native_world.h"
#include "nox/native_clock.h"
#include "nox/native_npc_schedule.h"
#include "nox/native_inn_storage.h"
#include "nox/native_world_navigation.h"
namespace Nox { namespace Native {
// Authored starting data and core data persistence. Menu, world/interaction
// continuations, initial actor turn and first scene are separate owners.
class NewGame {
    friend class PlayerSessionState;
    friend class TempleService;
    friend class InnService;
    friend class InnMembership;
public:
    explicit NewGame(HdvArchive &archive) : world(archive), gold(0),food(0),appearance(0), _archive(archive), _ready(false) {}
    bool ready() const { return _ready; }
    bool create(const Common::String &name, byte gender, byte race) {
        if (_ready || world.ready() || party.ready()) return false;
        Common::Array<byte> data;
        if (!_archive.read("NA/DATA.SAVE.GAMEN",data) || data.size() != 8045) return false;
        ItemCatalog catalog; Party nextParty; Inventory nextInventory; SpellBook nextBook;
        if (!catalog.load(_archive) || !nextParty.create(_archive,name,gender,race) ||
            !EquipmentActions::recalculateAll(catalog,nextParty) ||
            !nextInventory.importRecords(data.data() + 0x1800,1536) ||
            !nextBook.importState(data.data() + 0x1e60,20)) return false;
        byte fields[256]; memset(fields,0,sizeof(fields)); memcpy(fields,data.data() + 0x1700,80);
        fields[10] = 1; fields[55] |= 32;
        if (!fields[77]) { fields[77] = 50; fields[78] = 116; fields[79] = 49; }
        TurnClock nextClock;
        nextClock.hour = data[0x1ef3]; nextClock.minute = data[0x1ef5];
        nextClock.lightActive = data[0x1f0e]; nextClock.lightTicks = data[0x1f0f]; nextClock.hourlyCounter = fields[20];
        ClockAdvance check; if (!advanceTurnClock(nextClock,check)) return false;
        // Shipwreck is the original opening location, not a test checkpoint.
        World nextWorld(_archive); NpcScheduleState nextSchedule; NpcSchedulePolicy policy = {false,false};
        if (!nextWorld.initialize(8,7,24,38,fields,sizeof(fields)) ||
            !nextWorld.initializeNewGameSurface(data.data() + 0x600,1536) || !nextSchedule.initialize(_archive) ||
            NpcScheduler::apply(nextWorld,nextClock.hour,nextClock.minute,policy,nextSchedule) != kNpcScheduleApplied) return false;
        world.adoptState(nextWorld); schedule = nextSchedule;
        party = nextParty; inventory = nextInventory; book = nextBook; clock = nextClock;
        if (!inn.initialize(_archive) || !navigation.initialize(_archive)) return false;
        food=READ_LE_UINT16(data.data()+0x1e74);gold=READ_LE_UINT16(data.data()+0x1e76);appearance=data[0x1ef8];_ready = true; return true;
    }
    World world; Party party; Inventory inventory; SpellBook book; TurnClock clock; NpcScheduleState schedule;
    InnStorage inn;
    WorldNavigation navigation;
    uint16 gold,food;byte appearance;
    bool save(Common::WriteStream &out) const {
        if (!_ready) return false;
        out.writeUint32BE(MKTAG('N','G','S','T'));out.writeUint16LE(navigation.used()?4:3);
        Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),p(DisposeAfterUse::YES),b(DisposeAfterUse::YES),s(DisposeAfterUse::YES),i(DisposeAfterUse::YES);
        byte items[1536];if (!world.save(w) || !party.save(p) || !book.save(b) || !schedule.save(s) || !inn.save(i) || !inventory.exportRecords(items,1536)) return false;
        Common::MemoryWriteStreamDynamic *const parts[]={&w,&p,&b,&s,&i};
        for (uint part=0;part<5;++part) { out.writeUint32LE(parts[part]->size());out.write(parts[part]->getData(),parts[part]->size()); }
        out.write(items,1536);
        if (navigation.used()) {Common::MemoryWriteStreamDynamic part(DisposeAfterUse::YES);if (!navigation.save(part)) return false;out.writeUint32LE(part.size());out.write(part.getData(),part.size());}
        out.writeByte(clock.phase);out.writeByte(clock.hour);out.writeByte(clock.minute);
        out.writeByte(clock.lightTicks);out.writeByte(clock.lightActive);out.writeByte(clock.hourlyCounter);
        out.writeUint16LE(gold);out.writeUint16LE(food);out.writeByte(appearance);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.readUint32BE()!=MKTAG('N','G','S','T')) return false;
        const uint16 version=in.readUint16LE();if (version<2 || version>4) return false;
        NewGame next(_archive);
        for (uint i=0;i<(version>=3?5U:4U);++i) {
            const uint32 length=in.readUint32LE();if (!length || length>1048576 || length>uint32(in.size()-in.pos())) return false;
            Common::Array<byte> bytes;bytes.resize(length);if (in.read(bytes.data(),length)!=length) return false;
            Common::MemoryReadStream part(bytes.data(),length);
            if (!(i==0?next.world.load(part):i==1?next.party.load(part):i==2?next.book.load(part):i==3?next.schedule.load(part):next.inn.load(part))) return false;
        }
        // Earlier native checkpoints could not leave companions at inns.
        // Their absent owner initializes from the immutable original template.
        if (version==2 && !next.inn.initialize(_archive)) return false;
        byte items[1536];if (in.read(items,1536)!=1536 || !next.inventory.importRecords(items,1536)) return false;
        if (version==4) {
            const uint32 size=in.readUint32LE();if (size!=49 || int64(size)>in.size()-in.pos()) return false;
            byte bytes[49];if (in.read(bytes,49)!=49) return false;Common::MemoryReadStream part(bytes,49);
            if (!next.navigation.load(part) || !next.navigation.used()) return false;
        } else if (!next.navigation.initialize(_archive)) return false;
        next.clock.phase=in.readByte();next.clock.hour=in.readByte();next.clock.minute=in.readByte();
        next.clock.lightTicks=in.readByte();next.clock.lightActive=in.readByte();next.clock.hourlyCounter=in.readByte();ClockAdvance check;
        next.gold=in.readUint16LE();next.food=in.readUint16LE();next.appearance=in.readByte();
        if (!advanceTurnClock(next.clock,check) || in.err() || in.pos()!=in.size() || next.clock.hourlyCounter!=next.world.questFields()[20]) return false;
        world.adoptState(next.world);party=next.party;inventory=next.inventory;book=next.book;clock=next.clock;schedule=next.schedule;inn=next.inn;navigation=next.navigation;gold=next.gold;food=next.food;appearance=next.appearance;_ready=true;return true;
    }
private:
    friend class ExplorationSave;
    friend class BattleAftermath;
    friend class WorldConversation;
    void adoptState(const NewGame &source) {
        world.adoptState(source.world);party=source.party;inventory=source.inventory;book=source.book;
        clock=source.clock;schedule=source.schedule;_ready=source._ready;
        inn=source.inn;navigation=source.navigation;
        gold=source.gold;food=source.food;appearance=source.appearance;
    }
    HdvArchive &_archive;
    bool _ready;
};
} }
#endif
