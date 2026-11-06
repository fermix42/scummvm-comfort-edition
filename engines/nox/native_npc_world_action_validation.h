/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_WORLD_ACTION_VALIDATION_H
#define NOX_NATIVE_NPC_WORLD_ACTION_VALIDATION_H
#include "nox/native_npc_world_action.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeNpcWorldAction(HdvArchive &archive,Common::SeekableReadStream &input,Common::SeekableReadStream &continuation,Common::SaveFileManager &files,bool cold,Common::String &report,Common::String &error,
        const char *reportKey="native-npc-world-action",const char *checkpointFamily="nox-native-world-action") {
    if (input.readUint32BE()!=MKTAG('N','N','W','C')) return false;
    const uint version=input.readUint32LE(); if ((version!=1 && version!=2 && version!=3) || input.readUint32LE()!=2) return false;
    const bool withEffects=version==3; const uint outcomeSize=withEffects?606:347;
    uint mode=0,expectedGroups=1440,expectedOperations=22742,expectedBattles=24,expectedWaits=13739;
    if (version>=2) { mode=input.readUint32LE(); expectedGroups=input.readUint32LE(); expectedOperations=input.readUint32LE(); expectedBattles=input.readUint32LE(); expectedWaits=input.readUint32LE(); }
    if (mode>2 || (mode==2 && !withEffects) || !expectedGroups || expectedGroups>4000 || expectedOperations>64000) return false;
    if (continuation.readUint32BE()!=MKTAG('N','W','A','C') || continuation.readUint32LE()!=1) return false;
    const uint checkpointGroup=continuation.readUint32LE(),checkpointOperation=continuation.readUint32LE();
    if (checkpointGroup>=2000 || checkpointOperation>=16 || continuation.err() || continuation.pos()!=continuation.size()) return false;
    const uint owners=withEffects?7:6;
    const char *suffixes[7]={"world","schedule","store","state","rng","text","effects"}; Common::String names[7];
    for (uint i=0;i<owners;++i) names[i]=Common::String::format("%s-%s.bin",checkpointFamily,suffixes[i]);
    Native::FixedMessageCatalog catalog; Native::NpcWorldAction rules; if (!catalog.load(archive) || !rules.load(archive,8)) return false;
    uint total=0,battles=0,waits=0,groupTotal=0;
    for (uint profile=0;profile<2;++profile) {
        const uint32 length=input.readUint32LE(); if (length>12000000 || length<16) return false; Common::Array<byte> data; data.resize(length);
        if (input.read(data.data(),length)!=length) return false; if (cold && !profile) continue;
        Common::MemoryReadStream source(data.data(),length);
        if (source.readUint32BE()!=MKTAG('N','N','W','A') || source.readUint32LE()!=1) return false;
        const uint groups=source.readUint32LE(),expectedTotal=source.readUint32LE();
        if (!groups || groups>2000 || expectedTotal>groups*16 || (cold && checkpointGroup>=groups) || (version==1 && (groups!=720 || expectedTotal!=(profile?11222U:11520U)))) return false;
        groupTotal+=groups;
        for (uint group=0;group<(cold?checkpointGroup+1:groups);++group) {
            error=Common::String::format("Native NPC world setup rejected profile%u group%u cold%u",profile,group,uint(cold));
            byte context[6],party[2],actor[8],objects[256],payload[256],fields[256]={};
            source.read(context,6); source.read(party,2); const byte terrain=source.readByte(),obstacleAndEffect=source.readByte(),freeze=source.readByte();
            const byte obstacle=mode==2?(obstacleAndEffect&127):obstacleAndEffect;
            const byte initialEffect=mode==2?(obstacleAndEffect>>7):0;
            const byte worldType=mode?context[0]:8,worldMap=mode?context[1]:7,actorSlot=mode==1?context[4]:0;
            if ((actorSlot&7) || !rules.load(archive,worldType)) return false;
            source.read(actor,8); source.read(objects,256); source.read(payload,256);
            const uint32 seed=source.readUint32LE(); const byte tail=source.readByte(),last=source.readByte(); const uint16 attempts=source.readUint16LE(),count=source.readUint16LE();
            if (!count || count>16 || obstacle>2) return false;
            if (cold && group<checkpointGroup) { if (!source.seek(count*outcomeSize,SEEK_CUR)) return false; continue; }
            Native::World world(archive); Native::NpcScheduleState schedule; Native::NpcRouteStore store; Native::NpcWorldActionState state;
            Native::GameplayRandom random; Native::TextGlyphSurface surface; Native::ActorMovementEffectState effects; effects.code=7; effects.active=initialEffect;
            if (cold) {
                Common::InSaveFile *in[7]={}; bool ok=true;
                for (uint i=0;i<owners;++i) { in[i]=files.openForLoading(names[i]); ok=ok && in[i]!=nullptr; }
                ok=ok && world.load(*in[0]) && schedule.load(*in[1]) && store.load(*in[2]) && state.load(*in[3]) && random.load(*in[4]) && surface.load(*in[5],archive);
                if (ok && withEffects) ok=effects.load(*in[6]);
                for (uint i=0;i<owners;++i) delete in[i]; if (!ok) return false;
            } else {
                byte pages[3584]={}; memcpy(pages,payload,256); state.freezeTimer=freeze; state.enabled=byte(profile); state.contact.slot=248; state.contact.cell=186;
                if (!world.initialize(worldType,worldMap,party[0],party[1],fields,256) || !schedule.initialize(archive) || !store.importPages(pages,3584) || !random.importState(seed,tail,last,attempts) || !surface.initialize(archive,1)) return false;
                Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES); if (!world.save(w) || !schedule.save(s)) return false;
                Common::Array<byte> imported; imported.resize(w.size()); memcpy(imported.data(),w.getData(),w.size());
                memset(imported.data()+271+768,0,256); memset(imported.data()+271+1280,0,256);
                memcpy(imported.data()+271+1024,objects,256); memcpy(imported.data()+271+1280+actorSlot,actor,8);
                if (obstacle==1) { const byte other[8]={byte(party[0]+1),party[1],137,0,0,0,0,0}; memcpy(imported.data()+271+1288,other,8); }
                Common::MemoryReadStream ws(imported.data(),imported.size()); if (!world.load(ws)) return false;
                byte npc[572]; memcpy(npc,s.getData(),572); memset(npc+454,255,56); if (mode!=1) { npc[454]=77; npc[455]=0; npc[456]=1; npc[457]=0; } npc[570]=0;
                npc[6+77]=context[4]; npc[166+77]=context[5]; Common::MemoryReadStream ss(npc,572); if (!schedule.load(ss)) return false;
            }
            for (uint operation=0;operation<count;++operation) {
                error=Common::String::format("Native NPC world input rejected profile%u group%u operation%u cold%u",profile,group,operation,uint(cold));
                const byte status=source.readByte(),expectedDirection=source.readByte(),hidden=source.readByte(),changed=source.readByte(),timer=source.readByte(),stalled=source.readByte();
                const byte availability=source.readByte(),requestSlot=source.readByte(),requestCell=source.readByte(); byte expectedActor[8],requests[56],route[256],preferences[4];
                source.read(expectedActor,8); source.read(requests,56); source.read(route,256);
                const byte relativeX=source.readByte(),relativeY=source.readByte(),phase=source.readByte(),cell=source.readByte(); source.read(preferences,4);
                const uint32 afterSeed=source.readUint32LE(); const byte afterTail=source.readByte(),afterLast=source.readByte(); const uint16 afterAttempts=source.readUint16LE();
                const byte approaches=source.readByte(),pattern=source.readByte();
                byte expectedObjects[256]={},expectedEffects[3]={};
                if (withEffects && (source.read(expectedObjects,256)!=256 || source.read(expectedEffects,3)!=3)) return false;
                if (cold && operation<checkpointOperation) continue;
                const bool execute=!cold || operation!=checkpointOperation; byte direction=4; bool message=false; Native::NpcWorldActionResult result=Native::kNpcWorldAdvanced;
                if (execute) {
                    Native::WorldScene scene; memset(scene.actors.terrain,terrain,187); const byte *record=world.currentTables()->data()+1280+actorSlot;
                    const byte x=byte(record[0]-world.x()+128),y=byte(record[1]-world.y()+128);
                    if (x<120 || x>=137 || y<123 || y>=134) return false; const uint current=(y-123)*17+x-120;
                    if (record[3]&16) {
                        const uint cells[4]={current,current+1,current+17,current+18};
                        for (uint i=0;i<4;++i) { if (cells[i]>=187) return false; scene.actors.lookup[cells[i]]=actorSlot; }
                    } else if (record[6] || state.action.navigation.motion.hidden || !(record[7]&128)) scene.actors.lookup[current]=actorSlot;
                    if (obstacle==1) scene.actors.lookup[94]=8;
                    if (obstacle==2) scene.objects.lookup[92]=32;
                    result=withEffects?rules.advanceWithEffects(world,schedule,store,actorSlot,scene,4,255,state,random,catalog,surface,effects,direction,message):
                        rules.advance(world,schedule,store,actorSlot,scene,4,255,state,random,catalog,surface,direction,message);
                }
                if (withEffects && (memcmp(world.currentTables()->data()+1024,expectedObjects,256) || effects.active!=expectedEffects[0] || effects.queued!=expectedEffects[1] || effects.code!=expectedEffects[2])) {
                    error=Common::String::format("Containing NPC movement effects differ profile%u group%u operation%u",profile,group,operation); return false;
                }
                if ((execute && (uint(result)!=status || direction!=expectedDirection || message)) || memcmp(world.currentTables()->data()+1280+actorSlot,expectedActor,8) ||
                    memcmp(schedule.requests(),requests,56) || memcmp(store.data(),route,256) || memcmp(state.action.navigation.preferences,preferences,4) ||
                    state.action.navigation.motion.relativeX!=relativeX || state.action.navigation.motion.relativeY!=relativeY || byte(state.action.navigation.motion.movementPhase)!=phase || state.action.navigation.screenCell!=cell ||
                    byte(state.action.navigation.motion.hidden)!=hidden || byte(state.action.navigation.appearanceChanged)!=changed || state.action.pattern!=pattern ||
                    state.freezeTimer!=timer || byte(state.stalled)!=stalled || state.contact.availability!=availability || state.contact.slot!=requestSlot || state.contact.cell!=requestCell ||
                    schedule.approachCount()!=approaches || random.seed()!=afterSeed || random.tail()!=afterTail || random.last()!=afterLast || random.generationAttempts()!=afterAttempts) {
                    error=Common::String::format("Native scene-derived NPC phase differs profile%u group%u operation%u result%u/%u dir%u/%u timer%u/%u stalled%u/%u RNG%08X/%08X",profile,group,operation,uint(result),uint(status),uint(direction),uint(expectedDirection),uint(state.freezeTimer),uint(timer),uint(state.stalled),uint(stalled),random.seed(),afterSeed); return false;
                }
                Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES),r(DisposeAfterUse::YES),n(DisposeAfterUse::YES),g(DisposeAfterUse::YES),t(DisposeAfterUse::YES);
                error=Common::String::format("Native NPC world serialization rejected profile%u group%u operation%u cold%u",profile,group,operation,uint(cold));
                if (!world.save(w) || !schedule.save(s) || !store.save(r) || !state.save(n) || !random.save(g) || !surface.save(t)) return false;
                Common::MemoryReadStream ws(w.getData(),w.size()),ss(s.getData(),s.size()),rs(r.getData(),r.size()),ns(n.getData(),n.size()),gs(g.getData(),g.size()),ts(t.getData(),t.size());
                if (!world.load(ws) || !schedule.load(ss) || !store.load(rs) || !state.load(ns) || !random.load(gs) || !surface.load(ts,archive)) return false;
                if (withEffects) { Common::MemoryWriteStreamDynamic e(DisposeAfterUse::YES); if (!effects.save(e)) return false; Common::MemoryReadStream ei(e.getData(),e.size()); if (!effects.load(ei)) return false; }
                if (!cold && profile==1 && group==checkpointGroup && operation==checkpointOperation) {
                    error=Common::String::format("Native NPC world checkpoint admission rejected direction%u status%u",uint(direction),uint(status));
                    if (status || (mode==2?effects.queued!=1:mode==1?state.contact.availability!=0:direction>=4)) return false;
                    for (uint i=0;i<owners;++i) {
                        Common::OutSaveFile *out=files.openForSaving(names[i],false); if (!out) return false;
                        bool ok=i==0?world.save(*out):i==1?schedule.save(*out):i==2?store.save(*out):i==3?state.save(*out):i==4?random.save(*out):i==5?surface.save(*out):effects.save(*out);
                        out->finalize(); ok=ok&&!out->err(); delete out; if (!ok) return false;
                    }
                }
                if (execute) { ++total; battles+=status==2; waits+=status==1; }
            }
        }
        if (source.err() || (!cold && source.pos()!=source.size())) return false;
    }
    error=Common::String::format("Native NPC world totals rejected operations%u battles%u waits%u cold%u",total,battles,waits,uint(cold));
    if (input.err() || input.pos()!=input.size() || (!cold && (groupTotal!=expectedGroups || total!=expectedOperations || battles!=expectedBattles || waits!=expectedWaits))) return false;
    error.clear();
    if (cold) report+=Common::String::format("%s\tphase=cold\t%s=1\tprepared_preferences_restored=1\tcontinued_operations=%u\tcpu_execution=0\n",reportKey,mode==2?"actual_queued_effect_restored":mode==1?"actual_queued_request_restored":"actual_route_move_restored",total);
    else report+=Common::String::format("%s\tphase=warm\tchains=%u\toperations=%u\tbattle_handoffs=%u\tforced_waits=%u\tscene_derived_collision_preview=1\tinjected_collision_decisions=0\tstationary_large_domain=%u\tcontaining_movement_effects=%u\tranged_request_domain=%u\tfull_actor_turn=0\tcpu_execution=0\n",reportKey,groupTotal,total,battles,waits,uint(mode==1),uint(withEffects),uint(mode==2));
    return true;
}
}
#endif
