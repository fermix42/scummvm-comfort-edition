/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_LOOT_H
#define NOX_NATIVE_BATTLE_LOOT_H
#include "nox/native_vortex_cast.h"
#include "nox/native_inventory.h"
namespace Nox { namespace Native {
// Persist the real decision boundary between enemies. Item choices alter
// inventory capacity before later random generation, as in the original loop.
struct BattleLootState {
    bool started,pending,takeAll,finished,committed,food,scripted;
    byte count,cursor,kind,id,partyCount;
    uint16 amount,generated,startingFood,startingGold;
    BattleLootState() : started(false),pending(false),takeAll(false),finished(false),committed(false),food(false),scripted(false),count(0),cursor(0),kind(0),id(0),partyCount(0),amount(0),generated(0),startingFood(0),startingGold(0) {}
    bool save(Common::WriteStream &out) const {
        if (count>16 || cursor>count || (started && (!partyCount || partyCount>6)) || kind>(scripted?2:1) || (pending && finished) || (scripted && pending)) return false;
        out.writeUint32BE(MKTAG('N','L','T','S'));out.writeUint16LE(scripted?2:1);
        out.writeByte((started?1:0)|(pending?2:0)|(takeAll?4:0)|(finished?8:0)|(committed?16:0)|(food?32:0)|(scripted?64:0));
        out.writeByte(count);out.writeByte(cursor);out.writeByte(kind);out.writeByte(id);out.writeByte(partyCount);
        out.writeUint16LE(amount);out.writeUint16LE(generated);out.writeUint16LE(startingFood);out.writeUint16LE(startingGold);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=20 || in.readUint32BE()!=MKTAG('N','L','T','S')) return false;
        const uint16 version=in.readUint16LE();if (version!=1 && version!=2) return false;
        BattleLootState next;const byte flags=in.readByte();if ((flags&128) || (version==1 && (flags&64))) return false;
        next.started=flags&1;next.pending=flags&2;next.takeAll=flags&4;next.finished=flags&8;next.committed=flags&16;next.food=flags&32;
        next.scripted=flags&64;
        next.count=in.readByte();next.cursor=in.readByte();next.kind=in.readByte();next.id=in.readByte();next.partyCount=in.readByte();
        next.amount=in.readUint16LE();next.generated=in.readUint16LE();next.startingFood=in.readUint16LE();next.startingGold=in.readUint16LE();
        if (in.err() || next.count>16 || next.cursor>next.count || next.kind>(next.scripted?2:1) || (next.scripted && next.pending) || (next.started && (!next.partyCount || next.partyCount>6)) ||
            (next.pending && (!next.started || next.finished)) || (next.committed && !next.finished)) return false;
        *this=next;return true;
    }
};
class BattleLoot {
public:
    BattleLoot() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> scale,ranges;
        if (!_catalog.load(archive) || !archive.readRange("NA/SRTN.INVENTORY",0x7ff9,20,scale) || scale.size()!=20 ||
            !archive.readRange("NA/SRTN.INVENTORY",0x8191,40,ranges) || ranges.size()!=40) return false;
        for (uint i=0;i<10;++i) _scale[i]=READ_LE_UINT16(scale.data()+i*2);
        memcpy(_ranges,ranges.data(),40);_ready=true;return true;
    }
    bool begin(byte enemies,byte party,uint16 food,uint16 gold,BattleLootState &state) const {
        if (!_ready || state.started || !enemies || enemies>16 || !party || party>6) return false;
        BattleLootState next;next.scripted=state.scripted;if (next.scripted) { next.kind=state.kind;next.id=state.id; }
        next.started=true;next.count=enemies;next.partyCount=party;next.startingFood=food;next.startingGold=gold;state=next;return true;
    }
    bool advance(const BattleRecords &battle,Inventory &inventory,GameplayRandom &random,BattleLootState &state,byte *fields=nullptr,uint fieldSize=0) const {
        if (!_ready || !state.started || state.pending || state.finished) return false;
        BattleLootState next=state;Inventory items=inventory;GameplayRandom rng=random;
        if (next.scripted) {
            ItemDefinition definition;if (!_catalog.definition(next.kind,next.id,definition)) return false;
            if (next.kind==2 && (definition.bytes[8]&32)) {
                if (!fields || fieldSize!=256) return false;
                ++fields[definition.bytes[7]];
            }
            // The source increments the authored field before inventory's
            // capacity refusal. Special definitions bypass ordinary capacity.
            if ((definition.bytes[8]&128) || items.ordinaryCapacityRemaining()) {
                if (items.grantOne(_catalog,next.kind,next.id)==kInventoryInvalid) return false;
            }
            next.cursor=next.count;next.generated=1;next.takeAll=true;next.finished=true;
            state=next;inventory=items;return true;
        }
        while (next.cursor<next.count) {
            const byte *enemy=battle.stats+768+uint(next.cursor)*32;
            const byte level=enemy[1]&127,descriptor=enemy[0];byte factor;
            if (!level || level>10 || !decimal(descriptor,factor)) return false;
            next.food=descriptor<33;
            bool currency=true;
            if (!next.food) { byte roll;if (!folded(rng,roll)) return false;currency=bcd(roll)<descriptor; }
            if (currency) {
                byte variation,sign;if (!rng.next(1,20,variation) || !rng.next(1,255,sign)) return false;
                if (variation>=11) variation-=10;
                const byte varied=byte(sign&128?factor-variation:factor+variation);
                const uint32 quotient=uint32(varied)*_scale[level-1]/100;
                uint16 amount=uint16(MIN<uint32>(quotient,65535));
                if (next.food) amount=uint16((amount&0xff00)|byte(amount*2));
                next.amount=uint16(next.amount+amount);
                const uint16 capacity=next.food?uint16(next.partyCount*150):9999;
                const uint16 remaining=uint16(capacity-(next.food?next.startingFood:next.startingGold));
                if (next.amount>remaining) next.amount=remaining;
            }
            ++next.cursor;
            if (!items.ordinaryCapacityRemaining()) continue;
            bool item=descriptor==153;
            if (!item) {
                byte roll;if (!folded(rng,roll)) return false;
                item=bcd(roll)<=bcd(factor/5);
            }
            if (!item) continue;
            bool selected=false;
            for (uint attempt=0;attempt<4096;++attempt) {
                byte choice;if (!rng.next(1,255,choice)) return false;
                if (choice>=85 && choice<170) continue;
                const byte kind=choice<85?0:1;const uint start=kind*20;
                const byte lower=_ranges[start+(level==1?0:uint(level-2)*2)],upper=_ranges[start+uint(level-1)*2+1];
                byte id;if (!rng.next(lower,upper,id)) return false;ItemDefinition definition;
                if (!_catalog.definition(kind,id,definition)) return false;
                if (definition.bytes[8]&128) continue;
                next.kind=kind;next.id=id;next.pending=true;++next.generated;selected=true;break;
            }
            if (!selected) return false;
            if (!next.takeAll) { state=next;inventory=items;random=rng;return true; }
            const InventoryResult granted=items.grantOne(_catalog,next.kind,next.id);
            if (granted==kInventoryInvalid) return false;
            next.pending=false;
        }
        next.finished=true;state=next;inventory=items;random=rng;return true;
    }
    bool choose(bool take,bool all,const BattleRecords &battle,Inventory &inventory,GameplayRandom &random,BattleLootState &state) const {
        if (!_ready || !state.started || !state.pending || state.finished) return false;
        BattleLootState next=state;Inventory items=inventory;GameplayRandom rng=random;
        if (all) { next.takeAll=true;take=true; }
        if (take && items.grantOne(_catalog,next.kind,next.id)==kInventoryInvalid) return false;
        next.pending=false;
        if (!advance(battle,items,rng,next)) return false;
        state=next;inventory=items;random=rng;return true;
    }
    bool commit(uint16 &food,uint16 &gold,BattleLootState &state) const {
        if (!_ready || !state.started || !state.finished || state.committed || state.pending || food!=state.startingFood || gold!=state.startingGold) return false;
        if (state.food) food=uint16(food+state.amount);else gold=uint16(gold+state.amount);
        state.committed=true;return true;
    }
    bool describe(const BattleLootState &state,Common::String &name) const {
        if (!_ready || (!state.pending && !(state.scripted && state.finished))) return false;ItemDefinition definition;
        if (!_catalog.definition(state.kind,state.id,definition)) return false;name.clear();
        for (uint i=13;i<32 && definition.bytes[i];++i) name+=char(definition.bytes[i]&127);return true;
    }
private:
    static bool decimal(byte value,byte &out) { if ((value&15)>9 || (value>>4)>9) return false;out=byte((value>>4)*10+(value&15));return true; }
    static byte bcd(uint value) { return byte((value/10)*16+value%10); }
    static bool folded(GameplayRandom &random,byte &out) { if (!random.next(1,199,out)) return false;if (out>=100) out-=100;return true; }
    bool _ready;uint16 _scale[10];byte _ranges[40];ItemCatalog _catalog;
};
} }
#endif
