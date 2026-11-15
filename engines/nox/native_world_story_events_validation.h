/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_STORY_EVENTS_VALIDATION_H
#define NOX_NATIVE_WORLD_STORY_EVENTS_VALIDATION_H
#include "nox/native_world_input.h"
namespace Nox { namespace Native {
inline bool validateNativeWorldStory525(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    report="World story525 original predicates";
    if(fixture.readUint32BE()!=MKTAG('N','W','S','5')) return false;
    const uint cases=fixture.readUint16LE();if(!cases || cases>16) return false;
    for(uint i=0;i<cases;++i) {
        byte row[6];if(fixture.read(row,6)!=6) return false;
        const bool expected=row[5]!=row[4];
        if(WorldStoryEvents::shadowApproach(row[0],row[1],row[2],row[3],row[4])!=expected) return false;
    }
    if(fixture.err() || fixture.pos()!=fixture.size()) return false;
    uint hooks=0,restores=0,admissions=0;
    for(uint profile=0;profile<2;++profile) {
        report=Common::String::format("World story525 hook%u setup",profile);
        Common::ScopedPtr<NewGame> owner(new NewGame(archive));NewGame &game=*owner;if(!game.create("APPROACH",0,0)) return false;
        byte fields[256];memcpy(fields,game.world.questFields(),256);fields[56]|=64;fields[59]=6;
        World setup(archive);if(!setup.initialize(0,0,152,175,fields,256)) return false;
        Common::MemoryWriteStreamDynamic bytes(DisposeAfterUse::YES);if(!setup.save(bytes)) return false;
        Common::MemoryReadStream input(bytes.getData(),bytes.size());if(!game.world.load(input)) return false;
        Common::ScopedPtr<WorldInput> command(new WorldInput);if(!command->loadRules(archive,0)) return false;
        WorldStory window;WorldScene scene;WorldActorTurnState turn;LightState light(1,0,0);VisibilityMask visibility;
        NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;
        if(!random.initializeEntropy(119)) return false;
        WorldTravelPlan plan;
        if(WorldTravelRules::enter(0,0,150,168,game.world.field(59),game.world.currentTables()->data()+1536,256,plan)==kTravelSpecialBattle) return false;
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),beforeRng(DisposeAfterUse::YES);
        if(!game.save(before) || !random.save(beforeRng)) return false;
        auto operation=[&](bool first) {
            if(!profile) {
                bool changed=false;
                return command->pollWorldEvents(game,window,messages,text,frame,scene,light,0,changed,&archive,&turn,&random) && changed==first;
            }
            Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_INVALID,0);
            return command->handle(event,archive,game,window,scene,turn,light,visibility,routes,random,messages,text,frame)==kWorldInputIgnored;
        };
        report=Common::String::format("World story525 hook%u actual producer",profile);
        if(!operation(true) || game.world.field(59)!=7 || game.world.field(56)!=fields[56]) return false;
        Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES),afterRng(DisposeAfterUse::YES);
        if(!game.save(after) || !random.save(afterRng) || before.size()!=after.size() || beforeRng.size()!=afterRng.size() ||
           memcmp(beforeRng.getData(),afterRng.getData(),beforeRng.size())) return false;
        uint changes=0;for(uint i=0;i<before.size();++i) if(before.getData()[i]!=after.getData()[i]) {if(before.getData()[i]!=6 || after.getData()[i]!=7) return false;++changes;}
        if(changes!=1 || !operation(false)) return false;
        Common::MemoryWriteStreamDynamic repeated(DisposeAfterUse::YES);if(!game.save(repeated) || repeated.size()!=after.size() || memcmp(repeated.getData(),after.getData(),after.size())) return false;
        Common::ScopedPtr<NewGame> cold(new NewGame(archive));Common::MemoryReadStream core(after.getData(),after.size());
        bool changed=true;if(!cold->load(core) || cold->world.field(59)!=7 || !cold->world.discoverShadowApproach(changed) || changed) return false;
        if(WorldTravelRules::enter(0,0,150,168,cold->world.field(59),cold->world.currentTables()->data()+1536,256,plan)!=kTravelSpecialBattle) return false;
        ++hooks;++restores;++admissions;
    }
    report=Common::String::format("world_story\toriginal_predicates=%u\tproduction_hooks=%u\tidempotent_core_restores=%u\tsubsequent_admission_queries=%u\tcore_changes_one_flag_byte=1\trng_unchanged=1\tprivate_disk_checkpoint=0\tcampaign=0\taudio_suite=0\n",cases,hooks,restores,admissions);return true;
}
} }
#endif
