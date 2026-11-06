/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SURFACE_OFFSCREEN_VALIDATION_H
#define NOX_NATIVE_SURFACE_OFFSCREEN_VALIDATION_H
#include "nox/native_world_actor_turn.h"
namespace Nox {
inline bool validateNativeSurfaceOffscreen(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    const uint32 tag=in.readUint32BE();const bool large=tag==MKTAG('N','W','L','T');
    if ((!large && tag!=MKTAG('N','S','O','T')) || in.readUint32LE()!=1) return false;
    const uint count=in.readUint32LE();if (large ? count!=27 : (count!=30 && count!=60)) return false;
    Native::WorldActorTurn rules;Native::FixedMessageCatalog catalog;if (!rules.load(archive,0) || !catalog.load(archive)) return false;
    for (uint test=0;test<count;++test) {
        byte before[2996],expected[2996],spatial[8];
        if (large && in.read(spatial,8)!=8) return false;
        if (in.read(before,2996)!=2996 || in.read(expected,2996)!=2996) return false;
        const uint draws=in.readUint16LE();if (draws>1000) return false;Common::Array<byte> pixels;pixels.resize(draws*3);
        if (in.read(pixels.data(),pixels.size())!=pixels.size()) return false;
        Native::World world(archive);Native::WorldStory window;Native::WorldActorTurnState state;Native::WorldScene prior,after;
        Native::NpcScheduleState schedule;Native::NpcRouteStore routes;Native::GameplayRandom random;Native::TextGlyphSurface text;
        if (!world.initialize(0,0,77,170,before+1792,256) || !schedule.initialize(archive) || !text.initialize(archive,1)) return false;
        Common::MemoryWriteStreamDynamic initial(DisposeAfterUse::YES);if (!world.save(initial)) return false;
        Common::Array<byte> owned;owned.resize(initial.size());memcpy(owned.data(),initial.getData(),initial.size());memcpy(owned.data()+271,before,1792);
        Common::MemoryReadStream ws(owned.data(),owned.size());if (!world.load(ws) || !window.initialize(world,before+1536,256)) return false;
        byte pages[3584]={};if (!routes.importPages(pages,3584) || !random.importState(READ_LE_UINT32(before+2983),before[2987],before[2988],READ_LE_UINT16(before+2989))) return false;
        memcpy(prior.actors.terrain,before+2048,187);memcpy(prior.objects.lookup,before+2422,187);memcpy(prior.actors.lookup,before+2609,187);memcpy(prior.actors.kinds,before+2796,187);
        state.presentation.facing=before[2991];state.presentation.hidden=before[2992]!=0;
        if (large) { state.compact.spatial.known=255;memcpy(state.compact.spatial.cells,spatial,8); }
        state.compact.freezeTimer=before[2993];state.compact.enabled=before[2994];state.presentation.retainedCell=before[2995];state.presentation.retainedCellKnown=true;
        Native::WorldActorTurnEvent event;
        if (!rules.run(world,prior,before+2235,187,state,schedule,routes,random,catalog,text,after,event,4,&window)) {
            error=Common::String::format("Surface ordered offscreen turn rejected case%u",test);return false;
        }
        Common::MemoryWriteStreamDynamic result(DisposeAfterUse::YES);
        result.write(world.currentTables()->data(),1792);result.write(world.questFields(),256);result.write(after.actors.terrain,187);
        result.write(before+2235,187);result.write(after.objects.lookup,187);result.write(after.actors.lookup,187);result.write(after.actors.kinds,187);
        result.writeUint32LE(random.seed());result.writeByte(random.tail());result.writeByte(random.last());result.writeUint16LE(random.generationAttempts());
        result.writeByte(state.presentation.facing);result.writeByte(state.presentation.hidden);result.writeByte(state.compact.freezeTimer);result.writeByte(state.compact.enabled);result.writeByte(state.presentation.retainedCell);
        for (uint i=0;i<2996;++i) if (result.getData()[i]!=expected[i]) {
            error=Common::String::format("Surface whole turn differs case%u byte%u native%u original%u",test,i,result.getData()[i],expected[i]);return false;
        }
        if (event.battle || after.draws.size()!=draws) return false;
        for (uint i=0;i<draws;++i) if (after.draws[i].cell!=pixels[i*3] || after.draws[i].tile!=pixels[i*3+1] || after.draws[i].height!=pixels[i*3+2]) return false;
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES),again(DisposeAfterUse::YES);if (!state.save(saved)) return false;
        Native::WorldActorTurnState cold;Common::MemoryReadStream input(saved.getData(),saved.size());
        if (!cold.load(input) || !cold.save(again) || saved.size()!=again.size() || memcmp(saved.getData(),again.getData(),saved.size())) return false;
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report+=Common::String::format("native-surface-offscreen\twhole_turns=%u\tlarge_world=%u\trecords_fields_occupancy_RNG_retained_cell=1\tsemantic_draws=1\tstate_roundtrips=%u\tcpu_execution=0\n",count,uint(large),count);return true;
}
}
#endif
