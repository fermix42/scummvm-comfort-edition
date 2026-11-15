/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_RECOVERY_VALIDATION_H
#define NOX_NATIVE_NPC_RECOVERY_VALIDATION_H
#include "nox/native_world_population_validation.h"
namespace Nox { namespace Native {
struct NpcRecoveryCase518 {byte type,map,x,y,tx,ty,reachable,tail,last;uint16 minimum,attempts;uint32 seed;};
inline bool npcRecoverySetup518(HdvArchive &archive,World &world,WorldStory &window,NpcScheduleState &schedule,NpcRouteStore &routes,GameplayRandom &random,
        const NpcRecoveryCase518 &first,const NpcRecoveryCase518 *second=nullptr,bool full=false) {
    byte fields[256]={};if (!world.initialize(first.type,first.map,24,24,fields,256) || !schedule.initialize(archive) || !random.importState(first.seed,first.tail,first.last,first.attempts)) return false;
    NpcSchedulePolicy policy={false,false};if (NpcScheduler::apply(world,0x12,0x34,policy,schedule)!=kNpcScheduleApplied) return false;
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES),npc(DisposeAfterUse::YES);if (!world.save(saved) || saved.size()!=2066 || !schedule.save(npc) || npc.size()!=572) return false;
    Common::Array<byte> data;data.resize(saved.size());memcpy(data.data(),saved.getData(),saved.size());
    const byte actor[]={first.x,first.y,137,0,77,126,1,0};memcpy(data.data()+271+1280,actor,8);
    if (second) {const byte other[]={second->x,second->y,137,0,78,126,1,0};memcpy(data.data()+271+1288,other,8);}
    if (full) for (uint slot=8;slot<=112;slot+=8) {const byte other[]={byte(slot/8),1,137,0,byte(slot/8),126,1,0};memcpy(data.data()+271+1280+slot,other,8);}
    Common::MemoryReadStream input(data.data(),data.size());if (!world.load(input) || !window.initialize(world,world.currentTables()->data()+1536,256)) return false;
    byte state[572];memcpy(state,npc.getData(),572);memset(state+454,255,116);state[6]=first.x;state[166]=first.y;
    const byte pending[]={77,0,first.tx,first.ty,0,0};memcpy(state+510,pending,6);
    if (second) {state[7]=second->x;state[167]=second->y;const byte other[]={78,8,second->tx,second->ty,1,0};memcpy(state+516,other,6);}
    byte pages[3584]={};
    if (full) for (uint page=0;page<14;++page) {
        const byte request[]={byte(page+1),byte((page+1)*8),1,byte(page)};memcpy(state+454+page*4,request,4);
        pages[page*256]=2;pages[page*256+2]=byte(page+1);pages[page*256+3]=1;
    }
    Common::MemoryReadStream stateInput(state,572);return schedule.load(stateInput) && routes.importPages(pages,3584);
}
inline bool npcRecoveryCodec518(HdvArchive &archive,World &world,NpcScheduleState &schedule,NpcRouteStore &store,NpcNavigation &navigation,GameplayRandom &random) {
    Common::MemoryWriteStreamDynamic a(DisposeAfterUse::YES),b(DisposeAfterUse::YES),c(DisposeAfterUse::YES),d(DisposeAfterUse::YES);
    if (!schedule.save(a) || !store.save(b) || !navigation.save(c) || !random.save(d)) return false;
    Common::MemoryReadStream ai(a.getData(),a.size()),bi(b.getData(),b.size()),ci(c.getData(),c.size()),di(d.getData(),d.size());
    NpcScheduleState next;NpcRouteStore routes;NpcNavigation work;GameplayRandom rng;
    if (!next.load(ai) || !routes.load(bi) || !work.load(ci,archive,world,next) || !rng.load(di)) return false;
    schedule=next;store=routes;navigation=work;random=rng;return true;
}
inline bool validateNativeNpcRecovery518(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    report="npc518 observed source geometry header";
    if (fixture.readUint32BE()!=MKTAG('N','R','E','C') || fixture.readUint32LE()!=1 || fixture.readUint32LE()!=3) return false;
    NpcRecoveryCase518 cases[3];
    for (uint i=0;i<3;++i) {NpcRecoveryCase518 &c=cases[i];c.type=fixture.readByte();c.map=fixture.readByte();c.x=fixture.readByte();c.y=fixture.readByte();c.tx=fixture.readByte();c.ty=fixture.readByte();c.reachable=fixture.readByte();c.minimum=fixture.readUint16LE();c.seed=fixture.readUint32LE();c.tail=fixture.readByte();c.last=fixture.readByte();c.attempts=fixture.readUint16LE();}
    if (fixture.err() || fixture.pos()!=fixture.size() || cases[0].reachable || cases[1].reachable || !cases[2].reachable) return false;
    uint deferred=0,codecs=0,waiting=0,moved=0;
    FixedMessageCatalog catalog;TextGlyphSurface text;if (!catalog.load(archive) || !text.initialize(archive,1)) return false;
    for (uint i=0;i<3;++i) {
        report=Common::String::format("npc518 cooperative failure/continuation%u",i);
        World world(archive);WorldStory window;NpcScheduleState schedule;NpcRouteStore store;GameplayRandom random;NpcNavigation navigation;
        const NpcRecoveryCase518 &job=cases[i<2?i:2];const bool full=i==2;
        if (!npcRecoverySetup518(archive,world,window,schedule,store,random,job,full?nullptr:&cases[2],full)) return false;
        Common::MemoryWriteStreamDynamic baseline(DisposeAfterUse::YES);if (!world.save(baseline)) return false;
        byte pages[3584],requests[56];memcpy(pages,store.data(),3584);memcpy(requests,schedule.requests(),56);
        NpcNavigationResult result=kNpcNavigationSearching;uint steps=0;
        while (result==kNpcNavigationSearching) {
            if (++steps>1024) return false;result=navigation.advanceForWorld(archive,world,window,schedule,store,random);
            if (!npcRecoveryCodec518(archive,world,schedule,store,navigation,random)) return false;++codecs;
        }
        if (result!=kNpcNavigationDeferred || navigation.active() || schedule.pathBusy() || schedule.recent()[5]!=255 || memcmp(store.data(),pages,3584) || memcmp(schedule.requests(),requests,56)) return false;
        Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES);if (!world.save(after) || baseline.size()!=after.size() || memcmp(baseline.getData(),after.getData(),after.size())) return false;
        ++deferred;const uint32 failedSeed=random.seed();
        // A retired job stays retired through a real codec restore; no replay.
        if (!npcRecoveryCodec518(archive,world,schedule,store,navigation,random) || schedule.recent()[5]!=255 || navigation.active() || random.seed()!=failedSeed) return false;++codecs;
        if (!full) {
            result=kNpcNavigationSearching;steps=0;
            while (result==kNpcNavigationSearching) {if (++steps>1024) return false;result=navigation.advanceForWorld(archive,world,window,schedule,store,random);}
            if (result!=kNpcNavigationStored || schedule.recent()[11]!=255 || navigation.active()) return false;
        } else if (navigation.advanceForWorld(archive,world,window,schedule,store,random)!=kNpcNavigationIdle || random.seed()!=failedSeed) return false;
        byte blocked[4]={};NpcRouteRecoveryInputs input;input.partyX=24;input.partyY=24;input.previewEmpty=true;input.suppressApproach=true;
        NpcActionState action;byte direction=255;bool message=false;const byte x=world.currentTables()->data()[1280],y=world.currentTables()->data()[1281];
        if (NpcActionDispatcher::advance(world,schedule,store,0,blocked,input,action,random,catalog,text,direction,message)!=kNpcActionWaitingForRoute || direction!=4 || world.currentTables()->data()[1280]!=x || world.currentTables()->data()[1281]!=y) return false;++waiting;
        if (!full) {
            const byte ox=world.currentTables()->data()[1288],oy=world.currentTables()->data()[1289];NpcActionState other;
            if (NpcActionDispatcher::advance(world,schedule,store,8,blocked,input,other,random,catalog,text,direction,message)!=kNpcActionAdvanced || direction>3 || (world.currentTables()->data()[1288]==ox && world.currentTables()->data()[1289]==oy)) return false;++moved;
        }
    }
    report="npc518 actual ordinary input continues after no-route queue";
    Common::ScopedPtr<NightActorValidation504> v(new NightActorValidation504(archive));
    if (!tailSetup514(*v,7,3,24,24,0,0,255,255,false,true)) return false;
    World initial(archive);WorldStory cache;NpcScheduleState state;NpcRouteStore routes;GameplayRandom rng;
    if (!npcRecoverySetup518(archive,initial,cache,state,routes,rng,cases[0],&cases[2])) return false;
    Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES);if (!initial.save(core)) return false;
    Common::MemoryReadStream coreInput(core.getData(),core.size());if (!v->game.world.load(coreInput)) return false;
    v->window=cache;v->game.schedule=state;v->routes=routes;v->random=rng;
    const TurnClock clock=v->game.clock;
    if (v->send(Common::KEYCODE_z,'Z')!=kWorldInputHandled || !sameClock513(clock,v->game.clock) || v->game.schedule.pathBusy()) return false;
    for (uint i=0;i<10;++i) if (v->game.schedule.recent()[i*6+5]==0) return false;
    report=Common::String::format("npc_recovery\tobserved_no_route_geometries=2\tcapacity_case=1\tdeferred_jobs=%u\tcomponent_codec_sets=%u\twaiting_actors=%u\tsubsequent_actor_moves=%u\tactual_world_input=1\told_1495_suite=0\tcampaign=0\n",deferred,codecs,waiting,moved);return true;
}
} }
#endif
