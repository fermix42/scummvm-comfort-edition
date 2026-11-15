/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NIGHT_ACTORS_VALIDATION_H
#define NOX_NATIVE_NIGHT_ACTORS_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_fixture.h"
#include "nox/native_world_status.h"
namespace Nox { namespace Native {
struct NightActorValidation504 {
    explicit NightActorValidation504(HdvArchive &source) : archive(source),game(source),light(1,0,0) {}
    HdvArchive &archive;NewGame game;WorldInput input;WorldStory window;WorldScene scene;WorldActorTurnState turn;
    WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;
    LightState light;VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;
    bool setup(byte map,byte x,byte y,byte hour,byte minute,byte mode,const byte *compact,byte flags=0) {
        if (!game.create("NIGHT",0,0)) return false;
        game.clock.phase=0;game.clock.hour=hour;game.clock.minute=minute;
        byte fields[256];memcpy(fields,game.world.questFields(),256);fields[39]=flags;
        const byte type=map>=46 && map<=48?5:8;World initial(archive);
        if (!initial.initialize(type,map,x,y,fields,256)) return false;
        Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!initial.save(saved) || saved.size()!=269+1794+3) return false;
        Common::Array<byte> data;data.resize(saved.size());memcpy(data.data(),saved.getData(),saved.size());
        if (compact) memcpy(data.data()+271+768,compact,24); // Declared starting checkpoint, before any input.
        Common::MemoryReadStream bytes(data.data(),data.size());if (!game.world.load(bytes)) return false;
        const NpcSchedulePolicy policy={false,false};if (NpcScheduler::apply(game.world,hour,minute,policy,game.schedule)==kNpcScheduleUnsupported) return false;
        byte emptyRoutes[3584]={},previous[187]={};light=LightState(mode,mode>=2?4:0,0);
        if (!status.load(archive,type) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,type) || !lighting.load(archive,type) ||
                !occlusion.load(archive,type) || !painter.load(archive,type) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) ||
                !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !input.loadRules(archive,type) ||
                !window.initialize(game.world,game.world.currentTables()->data()+1536,256) ||
                !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,game.clock.lightActive,0,previous,turn.presentation,light,visible,frame,&scene) ||
                !text.importPages(frame.data(),frame.data(),8192)) return false;
        return WorldCheckpointFixture::standing(archive,game,scene);
    }
    WorldInputResult send(Common::KeyCode key,uint16 ascii=0) {
        Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);
        return input.handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);
    }
    bool enter() {
        // Authored Town2 47 north entrance9,16 -> map46 arrival38,16.
        // Initial adjacent source floor is declared before the first input.
        const byte x=game.world.x(),y=game.world.y();
        const Common::KeyCode direction=x==9 && y==15?Common::KEYCODE_DOWN:x==8 && y==16?Common::KEYCODE_RIGHT:x==10 && y==16?Common::KEYCODE_LEFT:Common::KEYCODE_UP;
        return send(direction)==kWorldInputTransition && game.world.mapType()==5 && game.world.mapId()==46 &&
            game.world.x()==38 && game.world.y()==16 && WorldCheckpointFixture::standing(archive,game,scene);
    }
    bool codecs() {
        Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),win(DisposeAfterUse::YES),command(DisposeAfterUse::YES);
        if (!game.save(core) || !window.save(win) || !input.save(command)) return false;
        Common::ScopedPtr<NewGame> next(new NewGame(archive));Common::MemoryReadStream a(core.getData(),core.size()),b(win.getData(),win.size()),c(command.getData(),command.size());
        WorldStory nextWindow;WorldInput nextInput;
        if (!next->load(a) || !nextWindow.load(b,next->world) || !nextInput.load(c,archive,5) ||
                memcmp(next->world.currentTables()->data(),game.world.currentTables()->data(),1792)) return false;
        Common::MemoryReadStream restored(core.getData(),core.size());if (!game.load(restored)) return false;window=nextWindow;input=nextInput;
        byte before[24];memcpy(before,game.world.currentTables()->data()+768,24);
        return send(Common::KEYCODE_o,'O')==kWorldInputHandled && send(Common::KEYCODE_ESCAPE,27)==kWorldInputHandled &&
            !memcmp(before,game.world.currentTables()->data()+768,24) && send(Common::KEYCODE_g,'G')==kWorldInputHandled && input.atWorldCommand();
    }
};
inline bool validateNativeNightActors504(HdvArchive &archive,Common::String &report) {
    Common::Array<byte> source;if (!archive.readRange("NA/BS.ROUTINES.BK2",0x2992,24,source)) return false;
    NightActors rules;if (!rules.load(archive)) return false;
    byte zero[24]={};uint policies=0;
    for (uint map=0;map<2;++map) for (byte mode=0;mode<4;++mode) {
        byte tables[1792],expected[1792];memset(tables,85,1792);memcpy(expected,tables,1792);
        if (!map) memcpy(expected+768,mode>=2?source.data():zero,24);
        bool changed=false;if (!rules.apply(map?47:46,mode,tables,1792,changed) || memcmp(tables,expected,1792) || changed==bool(map)) return false;
        ++policies;
    }
    uint cases=0,visibleCases=0;
    for (uint scenario=0;scenario<10;++scenario) {
        report=Common::String::format("night504 setup%u",scenario);
        Common::ScopedPtr<NightActorValidation504> owner(new NightActorValidation504(archive));
        const bool entry=scenario<2 || scenario==7,near=scenario>=8;
        const byte hour=scenario==0?0x12:scenario==2 || scenario==4 || scenario==8?0x20:scenario==3 || scenario==5 || scenario==9?5:0x22;
        const byte minute=scenario==2 || scenario==3 || near?0x30:0;
        const bool initialNight=scenario==3 || scenario==5 || scenario==6 || scenario==9;
        byte records[24];memcpy(records,initialNight?source.data():zero,24);if (scenario==6) ++records[0];
        if (entry) {
            const byte positions[][3]={{9,15,110},{8,16,94},{10,16,92},{9,17,76}};bool found=false;
            for (uint p=0;p<ARRAYSIZE(positions);++p) {
                Common::ScopedPtr<NightActorValidation504> candidate(new NightActorValidation504(archive));
                if (candidate->setup(47,positions[p][0],positions[p][1],hour,minute,1,nullptr,scenario==7?7:0) && candidate->scene.actors.lookup[positions[p][2]]==255) {owner.reset(candidate.release());found=true;break;}
            }
            if (!found) return false;
        } else if (near) {
            // Choose legal source floor before the first tested input; no
            // outcome, clock or actor mutation occurs once input starts.
            const byte positions[][2]={{24,17},{23,17},{25,17},{24,18},{23,18},{25,18}};bool found=false;
            for (uint p=0;p<ARRAYSIZE(positions);++p) {
                Common::ScopedPtr<NightActorValidation504> candidate(new NightActorValidation504(archive));
                if (candidate->setup(46,positions[p][0],positions[p][1],hour,minute,initialNight?3:1,records)) {owner.reset(candidate.release());found=true;break;}
            }
            if (!found) return false;
        } else if (!owner->setup(46,38,32,hour,minute,initialNight?3:1,records)) return false;
        NightActorValidation504 &s=*owner;
        const bool finalNight=scenario==1 || scenario==2 || scenario==4 || scenario==6 || scenario==7 || scenario==8;
        report=Common::String::format("night504 operation%u",scenario);
        if (entry) {if (!s.enter()) return false;}
        else if (scenario==4 || scenario==5) {
            if (s.send(Common::KEYCODE_w,'W')!=kWorldInputHandled || s.send(Common::KEYCODE_1,'1')!=kWorldInputHandled ||
                    s.game.clock.hour!=byte(hour+1)) return false;
        } else {
            const TurnClock before=s.game.clock;
            if (s.send(Common::KEYCODE_o,'O')!=kWorldInputHandled || s.send(Common::KEYCODE_ESCAPE,27)!=kWorldInputHandled ||
                    memcmp(&before,&s.game.clock,sizeof(before))) return false;
        }
        byte expected[24];memcpy(expected,finalNight?source.data():zero,24);if (scenario==6) ++expected[0];
        if (scenario==7) for (uint slot=0;slot<24;slot+=8) expected[slot+2]=0;
        const byte *actual=s.game.world.currentTables()->data()+768;
        for (uint i=0;i<24;++i) if (i%8==3?(actual[i]&63)!=(expected[i]&63):actual[i]!=expected[i]) {
            report=Common::String::format("night504 case%u byte%u actual%u expected%u",scenario,i,uint(actual[i]),uint(expected[i]));return false;
        }
        if (near) {
            const byte cell=byte(93+int(source[0])-s.game.world.x()+(int(source[1])-s.game.world.y())*17);
            const bool present=s.scene.actors.lookup[cell]==0 && (s.scene.actors.kinds[cell]&7)==0;
            if (present!=finalNight) {report=Common::String::format("night504 lookup case%u cell%u",scenario,uint(cell));return false;}
            ++visibleCases;
        }
        if (scenario==7 && s.game.world.field(39)!=7) return false;
        report=Common::String::format("night504 codecs/subsequent%u",scenario);if (!s.codecs()) return false;
        ++cases;
    }
    report=Common::String::format("night_actors\tbyte_policies=%u\tactual_entries=3\tdawn_dusk_W_and_retained_cases=%u\tvisible_lookup_edges=%u\tcore_window_input_codecs=30\tno_load_reinstall=1\tsubsequent_commands=10\tcampaign=0\taudio_suite=0\n",policies,cases,visibleCases);
    return true;
}
} }
#endif
