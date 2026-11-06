/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_MOVEMENT_EFFECTS_VALIDATION_H
#define NOX_NATIVE_ACTOR_MOVEMENT_EFFECTS_VALIDATION_H
#include "nox/native_actor_movement_effects.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeActorMovementEffects(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','A','M','F') || input.readUint32LE()!=1 || input.readUint32LE()!=124096) return false;
    uint doors=0,effects=0; byte wrapper[2][5]={}; bool found[2]={false,false};
    for (uint i=0;i<124096;++i) {
        Native::ActorMovementEffectContext context; context.mapType=input.readByte(); context.tile=input.readByte(); context.capability=input.readByte(); context.cell=input.readByte(); context.direction=input.readByte();
        const byte outside=input.readByte(); context.outside=outside!=0; Native::ActorMovementEffectState state; state.active=input.readByte(); state.queued=input.readByte(); state.code=input.readByte();
        const byte object=input.readByte(),objectState=input.readByte(); byte expected[5]; input.read(expected,5);
        if (outside>1 || object>1) return false; Native::ActorMovementEffectPlan result;
        if (!Native::ActorMovementEffects::plan(context,object!=0,objectState,state,result) || result.direction!=expected[0] || result.effects.queued!=expected[1] ||
            result.effects.code!=expected[2] || result.objectState!=expected[3] || result.originalDirection!=expected[4] || result.effects.active!=state.active || result.objectChanged!=(objectState!=expected[3])) {
            error=Common::String::format("Native actor movement effects differ case%u type%u tile%u cell%u direction%u",i,uint(context.mapType),uint(context.tile),uint(context.cell),uint(context.direction)); return false;
        }
        doors+=result.objectChanged; effects+=result.effects.code!=state.code;
        if (context.cell==92 && !context.direction && !outside && !state.active && !state.queued && state.code==7 && object && objectState==16 && !context.capability) {
            if (context.mapType==8 && context.tile==137) { memcpy(wrapper[0],expected,5); found[0]=true; }
            if (!context.mapType && context.tile==154) { memcpy(wrapper[1],expected,5); found[1]=true; }
        }
        if (!(i&1023)) {
            Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES); if (!result.effects.save(saved)) return false;
            Common::MemoryReadStream restored(saved.getData(),saved.size()); Native::ActorMovementEffectState copy;
            if (!copy.load(restored) || copy.active!=result.effects.active || copy.queued!=result.effects.queued || copy.code!=result.effects.code) return false;
        }
    }
    if (doors!=44224 || effects!=1920 || !found[0] || !found[1] || input.err() || input.pos()!=input.size()) return false;
    for (uint i=0;i<2;++i) {
        const byte type=i?0:8,map=i?0:7,position=i?136:24; byte fields[256]={}; Native::World world(archive);
        if (!world.initialize(type,map,position,position,fields,256)) return false;
        Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES); if (!world.save(encoded)) return false;
        Common::Array<byte> data; data.resize(encoded.size()); memcpy(data.data(),encoded.getData(),encoded.size()); memset(data.data()+271+1024,0,256);
        const byte original[4]={byte(position-1),byte(position-1),64,16}; memcpy(data.data()+271+1024+32,original,4);
        Common::MemoryReadStream initial(data.data(),data.size()); if (!world.load(initial)) return false;
        Native::WorldScene scene; scene.objects.lookup[75]=32; Native::ActorMovementEffectContext context;
        context.mapType=type; context.tile=i?154:137; context.capability=0; context.cell=92; context.direction=0; context.outside=false;
        Native::ActorMovementEffectState state; state.code=7; byte direction=0;
        if (!Native::ActorMovementEffects::apply(world,scene,context,state,direction) || direction!=wrapper[i][0] || state.queued!=wrapper[i][1] || state.code!=wrapper[i][2] ||
            (*world.currentTables())[1024+35]!=wrapper[i][3] || memcmp(world.questFields(),fields,256)) return false;
        Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES); if (!world.save(w) || !state.save(s)) return false;
        Common::MemoryReadStream ws(w.getData(),w.size()),ss(s.getData(),s.size()); if (!world.load(ws) || !state.load(ss) || (*world.currentTables())[1024+35]!=wrapper[i][3] || state.code!=wrapper[i][2]) return false;
    }
    report+="native-actor-movement-effects\tcases=124096\tdoor_transitions=44224\tranged_requests=1920\towned_world_commits=2\tstate_roundtrip=1\tprojectile_damage_presentation=0\tfull_actor_turn=0\tcpu_execution=0\n"; return true;
}
}
#endif
