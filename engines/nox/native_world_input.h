/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_INPUT_H
#define NOX_NATIVE_WORLD_INPUT_H
#include "common/ptr.h"
#include "nox/native_new_game.h"
#include "nox/native_world_move.h"
#include "nox/native_world_travel.h"
#include "nox/native_world_conversation.h"
#include "nox/native_inventory_window.h"
#include "nox/native_party_order.h"
#include "nox/native_information_input.h"
#include "nox/native_player_controls.h"
#include "nox/native_daylight.h"
#include "nox/native_night_actors.h"
#include "nox/native_world_cast.h"
#include "nox/native_world_cannon.h"
#include "nox/native_world_attack.h"
#include "nox/native_world_dig.h"
#include "nox/native_world_open.h"
#include "nox/native_world_portal.h"
#include "nox/native_world_camp.h"
#include "nox/native_search_input.h"
#include "nox/native_npc_navigation.h"
#include "nox/native_text_window.h"
#include "nox/native_opening_backdrop.h"
#include "nox/native_ending.h"
namespace Nox { namespace Native {
enum WorldInputResult { kWorldInputIgnored,kWorldInputHandled,kWorldInputBoundary,kWorldInputUnsupported,kWorldInputEncounter,kWorldInputTransition,kWorldInputDefeat };
// Semantic pending input, including the original notice's key forwarding.
// No key stream carries positions, quest fields or expected outcomes.
class WorldInput {
public:
    WorldInput() : _notice(false),_campNotice(false),_openDirection(false),_leave(false),_fireDirection(false),_fireProtected(false),_scriptedEncounter(false),_refusalSound(false),_attackDirection(false),_attackAlarm(false),_digDirection(false),_noticeTail(false),_noticeDirection(255),_waitHoursPending(false),_sounds(nullptr) {}
    bool loadRules(HdvArchive &archive,byte type=8) {
        return _move.load(archive,type) && _open.load(archive,type) && _wait.load(archive,type) && _story.load(archive) &&
            _actors.load(archive,type) && _lighting.load(archive,type) && _occlusion.load(archive,type) && _painter.load(archive,type) && _nightActors.load(archive) && _population.load(archive,type);
    }
    bool restoreWorldControls(const World &world,const WorldTransportState &state) {
        if (!atWorldCommand() || !state.valid() || !world.ready() || world.onFoot()!=(state.object==255)) return false;
        _transport=state;return true;
    }
    void setSoundEvents(SoundEvents *sounds,bool effectsEnabled=true) {_sounds=sounds;_conversation.setSoundEvents(sounds,effectsEnabled);_inventory.setSoundEvents(sounds);_casting.setSoundEvents(sounds);_move.setSoundEvents(sounds);_move.setEffectsEnabled(effectsEnabled);_wait.setSoundEvents(sounds);}
    bool settlePopulation(HdvArchive &archive,NewGame &game,const WorldStory &window,const WorldScene &scene,
            const LightState &light,WorldActorTurnState &turn,GameplayRandom &random) {
        if (!atWorldCommand() || !turn.population.pending || game.world.mapType()>=4) return true;
        // F9AA settles the clock boundary before choosing the population's
        // day/night frequency. Do not advance a raster/light transition here.
        Common::Array<byte> data;bool dark=false;LightState populationLight=light;
        const CharacterRecord *leader=game.party.member(0);
        if (!leader || !archive.read("NA/GAME",data) || data.size()!=13824 ||
                !Daylight::permanent(game.world.mapId(),data.data()+0x10b1,data.size()-0x10b1,dark) ||
                !Daylight::advance(game.world.mapType(),dark,_transport.speed,game.clock,populationLight)) return false;
        const bool night=game.clock.hour<5 || game.clock.hour>0x20 ||
            (game.clock.hour==5 && game.clock.minute<0x30) || (game.clock.hour==0x20 && game.clock.minute>=0x30);
        bool spawned=false;
        return _population.run(game.world,window,scene.actors.lookup,187,populationLight.mode,night,leader->bytes[1],turn.population,random,spawned);
    }
    bool pollWorldEvents(NewGame &game,WorldStory &window,const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,
            const WorldScene &scene,const LightState &light,byte animation,bool &changed,HdvArchive *archive=nullptr,
            WorldActorTurnState *turn=nullptr,GameplayRandom *random=nullptr) {
        changed=false;
        if (archive && turn && random && !settlePopulation(*archive,game,window,scene,light,*turn,*random)) return false;
        if (!atWorldCommand() || !game.ready() || game.world.mapId()!=65) return true;
        World candidate(game.world);WorldStory next=window;bool reveal=false;
        if (!next.revealKeep(candidate,reveal)) return false;if (!reveal) return true;
        const int column=24-int(candidate.x())+8,row=20-int(candidate.y())+5;
        if (column<0 || column>=17 || row<0 || row>=11) return false;
        const byte cell=byte(row*17+column);HgrFrame image=frame;TextGlyphSurface surface=text;
        if (!_painter.draw(image,166,scene.actors.terrain[cell],light.mode,animation,cell,16) || !message(catalog,27,surface,image)) return false;
        game.world.adoptState(candidate);window=next;frame=image;text=surface;changed=true;
        if (_sounds) _sounds->push_back(kSpeakerSelection);return true;
    }
    bool takeRefusalSound() {const bool sound=_refusalSound;_refusalSound=false;return sound;}
    bool noticeActive() const { return _roster.active() || _waitHoursPending || _information.active() || _partyOrder.active() || _search.active() || _fireDirection || _attackDirection || _digDirection || _notice || _conversation.active() || _inventory.active() || _casting.active(); }
    void inheritCombatHistory(const WorldInput &other) {_fireProtected=other._fireProtected;}
    bool wholeFrameMenu() const { return _roster.active() || _waitHoursPending || _information.active() || _partyOrder.active() || _search.active() || _casting.active() || _inventory.active() || _conversation.stage()>=22; }
    bool atWorldCommand() const {return atTurnBoundary() && !_transport.pendingSteps && !_transport.alignment;}
    bool atTurnBoundary() const {
        return !_roster.active() && !_waitHoursPending && !_information.active() && !_partyOrder.active() && !_search.active() && !_notice && !_openDirection && !_leave && !_fireDirection && !_attackDirection && !_digDirection && !_encounter.battle && !_conversation.active() &&
            !_inventory.active() && !_casting.active() && !_transport.defeated;
    }
    bool matchesWorld(const World &world) const { return _information.matchesWorld(world) && _search.matches(world) && _casting.matches(world) && _inventory.matches(world) && _conversation.matchesWorld(world) && world.onFoot()==(_transport.object==255); }
    bool worldDefeated() const {return _transport.defeated;}
    byte transportObject() const {return _transport.object;}
    byte transportSpeed() const {return _transport.speed;}
    bool canRepositionMountedFixture() const {
        return _transport.used && _transport.object!=255 && !_transport.pendingSteps && !_transport.alignment && !_transport.defeated &&
            !_search.active() && !_fireDirection && !_notice && !_openDirection && !_leave && !_encounter.battle && !_conversation.active() && !_inventory.active() && !_casting.active();
    }
    void markDefeated() {_transport.used=_transport.defeated=true;_transport.pendingSteps=0;_transport.pendingDirection=255;_transport.alignment=false;}
    byte automaticDirection() const {return !_fireDirection && !_attackDirection && !_digDirection && !_encounter.battle && !_notice && !_openDirection && !_leave && !_conversation.active() && !_inventory.active() && !_casting.active() && !_transport.defeated && _transport.pendingSteps?_transport.pendingDirection:255;}
    byte conversationStage() const { return _conversation.stage(); }
    const Common::String &visibleReply() const {return _conversation.visibleReply();}
    byte inventoryStage() const {return _inventory.phase();}
    bool selectingOpen() const { return _openDirection; }
    const WorldActorTurnEvent &encounter() const { return _encounter; }
    bool scriptedEncounter() const {return _scriptedEncounter;}
    // Actor-contact callers retain one pending world tick. Direct manual
    // attacks have no world turn; scripted arrivals have already paid theirs.
    bool completeEncounterClock(World &world,TurnClock &clock) const {
        return _encounter.battle && (!_encounter.clockPending || WorldMove::completeClock(world,clock));
    }

    bool continueTempleVictory(HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,WorldActorTurnState &turn,
            LightState &light,VisibilityMask &visibility,HgrFrame &frame,byte animation) {
        if (!_encounter.battle || !EndingPresentation::revelation(game.world.mapId(),game.world.field(56),game.world.field(59))) return false;
        WorldTravelPlan plan;plan.type=7;plan.map=58;plan.x=20;plan.y=13;plan.explicitArrival=true;
        const byte oldHour=game.clock.hour;game.clock.hour=9;
        if (!travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation)) {game.clock.hour=oldHour;return false;}
        // G:7139..7153 refreshes the arrival again after map-entry scheduling
        // before selecting Archus. Ordinary travel retains its original order.
        byte painted[187]={};
        return WorldViewport::refreshAtWorld(game.world,window,_story,_actors,_lighting,_occlusion,_painter,game.appearance,
                game.clock.lightActive,animation,painted,turn.presentation,light,visibility,frame,&scene);
    }
    bool beginTempleConversation(HdvArchive &archive,NewGame &game,const WorldScene &scene,HgrFrame &frame,GameplayRandom *random=nullptr,byte speechCursor=5) {
        // G:7149 explicitly selects NPC8 after R's teleport. The authored
        // scheduled NPC is immediately north of20,13; do not guess another.
        if (game.world.mapType()!=7 || game.world.mapId()!=58 || game.world.x()!=20 || game.world.y()!=13 ||
            !atWorldCommand() || scene.actors.lookup[76]!=8 || (scene.actors.kinds[76]&135)!=4 || !_conversation.choose(game.world,frame)) return false;
        Common::Event north;north.type=Common::EVENT_KEYDOWN;north.kbd=Common::KeyState(Common::KEYCODE_UP,0);byte message=255;
        return _conversation.handle(north,archive,game,scene,frame,message,&_transport,random,speechCursor)==kWorldConversationHandled;
    }
    bool continueScriptedEncounter(HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,WorldActorTurnState &turn,
            LightState &light,VisibilityMask &visibility,HgrFrame &frame,byte animation) {
        if (!_scriptedEncounter || !_encounter.battle || !window.matchesWorld(game.world) || !window.state()) return false;
        WorldTravelPlan plan;
        if (WorldTravelRules::enter(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),game.world.field(59),window.state()->spawns,256,plan,true)!=kTravelSelected) return false;
        // M:F582 returns from combat directly into F585's authored entrance.
        // Reuse the full travel owner, including navigation/light/schedule.
        return travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation);
    }
    bool queueEncounter(const WorldActorTurnEvent &event) {
        if (_fireDirection || _encounter.battle || _notice || _openDirection || _leave || _conversation.active() || _inventory.active() || _casting.active() || !event.battle || event.bank>1 ||
            (event.slot&3) || (event.bank && (event.slot&7)) || event.direction>3 || event.cell>=187) return false;
        _encounter=event;return true;
    }
    bool completeAttackReturn(HdvArchive &archive,NewGame &game) {
        if (!_attackAlarm) return true;
        if (!_encounter.battle || !game.ready()) return false;
        Common::Array<byte> data,footer;MapAssetRange range;
        if (!archive.read("NA/GAME",data) || !resolveMapAssets(data,game.world.mapType(),range) || !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
        World candidate(game.world);WorldTransportState controls=_transport;
        if (!controls.used) controls.appearance=controls.baseAppearance=controls.statusAppearance=game.appearance;
        WorldAttack::alarm(candidate._states[candidate._current].tables.data()+1280,footer.data(),controls);
        game.world.adoptState(candidate);_transport=controls;_attackAlarm=false;return true;
    }
    bool finishEncounter(bool fireProtected=false) {
        if (!_encounter.battle || _notice || _openDirection || _attackAlarm) return false;
        _encounter.battle=false;_scriptedEncounter=false;_fireProtected=fireProtected;return true;
    }
    static bool showMessage(const FixedMessageCatalog &catalog,byte id,TextGlyphSurface &surface,HgrFrame &frame) {
        return message(catalog,id,surface,frame);
    }
    WorldInputResult handle(const Common::Event &input,HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,
            WorldActorTurnState &turn,LightState &light,VisibilityMask &visibility,NpcRouteStore &routes,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,byte animation=0) {
        if (input.type==Common::EVENT_KEYDOWN && atWorldCommand()) {
            if (!settlePopulation(archive,game,window,scene,light,turn,random)) return kWorldInputUnsupported;
            turn.population.action=input.kbd.keycode==Common::KEYCODE_UP?0:input.kbd.keycode==Common::KEYCODE_DOWN?1:
                input.kbd.keycode==Common::KEYCODE_RIGHT?2:input.kbd.keycode==Common::KEYCODE_LEFT?3:4;
        }
        TurnClock checked=game.clock;
        const WorldInputResult result=handleCommand(input,archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation,checked);
        if ((result==kWorldInputHandled || result==kWorldInputIgnored) && atTurnBoundary() &&
                (checked.phase!=game.clock.phase || checked.hour!=game.clock.hour || checked.minute!=game.clock.minute)) {
            const WorldInputResult portalResult=finishTurn(archive,game,window,scene,turn,light,visibility,catalog,text,frame,animation);
            if (portalResult!=kWorldInputHandled) return portalResult;
        }
        return result;
    }
    WorldInputResult finishTurn(HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,WorldActorTurnState &turn,
            LightState &light,VisibilityMask &visibility,const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,byte animation=0) {
        if (!atTurnBoundary()) return kWorldInputHandled;
        WorldTravelPlan plan;const WorldPortalResult result=WorldPortal::prepare(game.world,scene,plan);
        if (result==kWorldPortalNone) return kWorldInputHandled;
        if (result==kWorldPortalInvalid) return kWorldInputUnsupported;
        if (result==kWorldPortalNeedsFoot) {if (_sounds) _sounds->push_back(kSpeakerRefusal);return message(catalog,23,text,frame)?kWorldInputHandled:kWorldInputUnsupported;}
        Common::String body;if (!WorldPortal::text(archive,result==kWorldPortalSelected,body) || !message(body,text,frame)) return kWorldInputUnsupported;
        if (result==kWorldPortalLocked) return kWorldInputHandled;
        if (!travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation)) return kWorldInputUnsupported;
        _transport.pendingSteps=0;_transport.pendingDirection=255;_transport.alignment=false;
        if (_sounds) _sounds->push_back(kSpeakerSelection); // Source portal9CC6 ->GAME9142.
        return text.importPages(frame.data(),frame.data(),8192)?kWorldInputTransition:kWorldInputUnsupported;
    }
private:
    WorldInputResult handleCommand(const Common::Event &input,HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,
            WorldActorTurnState &turn,LightState &light,VisibilityMask &visibility,NpcRouteStore &routes,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,byte animation,TurnClock &checkedClock) {
        if (animation>3) return kWorldInputUnsupported;
        if (input.type!=Common::EVENT_KEYDOWN) return kWorldInputIgnored;
        if (_transport.defeated) return kWorldInputDefeat;
        if (_encounter.battle) return kWorldInputBoundary;
        if (_digDirection) {
            if (input.kbd.keycode==Common::KEYCODE_ESCAPE) {
                if (!frame.clearTextSpan(7,23,25) || !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
                _digDirection=false;return kWorldInputHandled;
            }
            byte direction;switch (input.kbd.keycode) {
            case Common::KEYCODE_UP:direction=0;break;case Common::KEYCODE_DOWN:direction=1;break;
            case Common::KEYCODE_RIGHT:direction=2;break;case Common::KEYCODE_LEFT:direction=3;break;default:return kWorldInputHandled;}
            const byte near[]={76,110,94,92};const byte cell=near[direction],slot=scene.objects.lookup[cell];
            if (slot!=255 && (slot&3)) return kWorldInputUnsupported;
            const byte tile=slot==255?scene.actors.terrain[cell]:game.world.currentTables()->data()[1024+slot+2];
            const byte reply=WorldDig::result(game.world.mapType(),tile,slot!=255);
            if (reply!=255) {
                if (!message(catalog,reply,text,frame)) return kWorldInputUnsupported;
                _refusalSound=true;_digDirection=false;return kWorldInputHandled;
            }
            const int dx[]={0,0,1,-1},dy[]={-1,1,0,0};World candidate(game.world);WorldStory nextWindow=window;WorldScene nextScene;
            PresentationState presentation=turn.presentation;LightState nextLight=light;VisibilityMask mask=visibility;HgrFrame image=frame;TextGlyphSurface surface=text;byte painted[187]={};
            if (!WorldDig::open(candidate,slot,byte(int(candidate.x())+dx[direction]),byte(int(candidate.y())+dy[direction])) ||
                !image.clearTextSpan(7,23,25) || !WorldViewport::refreshAtWorld(candidate,nextWindow,_story,_actors,_lighting,_occlusion,_painter,game.appearance,game.clock.lightActive,animation,painted,presentation,nextLight,mask,image,&nextScene) ||
                !surface.importPages(image.data(),image.data(),8192)) return kWorldInputUnsupported;
            game.world.adoptState(candidate);window=nextWindow;scene=nextScene;turn.presentation=presentation;light=nextLight;visibility=mask;frame=image;text=surface;_digDirection=false;return kWorldInputHandled;
        }
        if (_attackDirection) {
            if (input.kbd.keycode==Common::KEYCODE_ESCAPE) {
                _attackDirection=false;return frame.clearTextSpan(7,23,25) && text.importPages(frame.data(),frame.data(),8192)?kWorldInputHandled:kWorldInputUnsupported;
            }
            byte direction;
            switch (input.kbd.keycode) {case Common::KEYCODE_UP:direction=0;break;case Common::KEYCODE_DOWN:direction=1;break;case Common::KEYCODE_RIGHT:direction=2;break;case Common::KEYCODE_LEFT:direction=3;break;default:return kWorldInputHandled;}
            const WorldAttackContext context={game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),game.appearance,_transport.speed,_transport.object!=255 && (_transport.appearance==119 || _transport.appearance==123)};
            WorldActorTurnEvent event;bool alarm=false;const WorldAttackResult result=WorldAttack::choose(context,direction,scene,game.world.currentTables()->data(),event,alarm);
            if (result==kWorldAttackInvalid) return kWorldInputUnsupported;
            _attackDirection=false;
            if (result==kWorldAttackSelected) {_encounter=event;_attackAlarm=alarm;return kWorldInputEncounter;}
            if (result==kWorldAttackNoTarget) _refusalSound=true;
            return message(catalog,result==kWorldAttackNoTarget?24:18,text,frame)?kWorldInputHandled:kWorldInputUnsupported;
        }

        // Roster selection is a free, exclusive command. Handle it before
        // the world input-loop schedule services; no time/AI/RNG advances.
        if (_partyOrder.active()) {
            const PartyOrderResult result=_partyOrder.handle(input,archive,game.party,game.inventory,frame);
            if (result==kPartyOrderUnsupported) return kWorldInputUnsupported;
            if (result==kPartyOrderClosed && !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (_information.active()) {
            const InformationResult result=_information.handle(input,archive,game.world,scene,game.appearance,frame);
            if (result==kInformationUnsupported) return kWorldInputUnsupported;
            if (result==kInformationClosed && !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (atWorldCommand() && (input.kbd.keycode==Common::KEYCODE_g || input.kbd.keycode==Common::KEYCODE_l || input.kbd.ascii=='?' || input.kbd.ascii=='/')) {
            const InformationCommand command=input.kbd.ascii=='?'?kInformationHelp:input.kbd.ascii=='/'?kInformationQuests:input.kbd.keycode==Common::KEYCODE_g?kInformationLocation:kInformationLook;
            if (!_information.begin(command,archive,game.world,frame)) return kWorldInputUnsupported;
            if (command==kInformationLook && !message(catalog,12,text,frame)) return kWorldInputUnsupported;
            if (!_information.active() && !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (atWorldCommand() && input.kbd.keycode==Common::KEYCODE_n)
            return game.ready() && _partyOrder.begin(archive,game.party,frame)?kWorldInputHandled:kWorldInputUnsupported;
        if (_roster.active()) {
            const bool inventory=input.kbd.keycode==Common::KEYCODE_TAB || input.kbd.keycode==Common::KEYCODE_SPACE;
            if (!_roster.close(frame)) return kWorldInputUnsupported;
            if (inventory) return _inventory.begin(archive,game,frame)?kWorldInputHandled:kWorldInputUnsupported;
            return text.importPages(frame.data(),frame.data(),8192)?kWorldInputHandled:kWorldInputUnsupported;
        }
        if (_waitHoursPending) {
            _waitHoursPending=false;frame=_waitHoursUnderlay;
            if (input.kbd.ascii<'1' || input.kbd.ascii>'9')
                return text.importPages(frame.data(),frame.data(),8192)?kWorldInputHandled:kWorldInputUnsupported;
            return freeControl(kControlWait,byte(input.kbd.ascii-'0'),archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation);
        }
        if (atWorldCommand()) {
            if (input.kbd.keycode==Common::KEYCODE_TAB) return _roster.begin(archive,game.party,frame)?kWorldInputHandled:kWorldInputUnsupported;
            if (input.kbd.keycode==Common::KEYCODE_i) return freeControl(kControlTorch,0,archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation);
            if (PlayerIcon::requested(input.kbd)) return freeControl(kControlIcon,0,archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation);
            if (input.kbd.keycode==Common::KEYCODE_w) {
                if (!_hourWait.load(archive)) return kWorldInputUnsupported;
                if (!HourWait::safe(game.world.mapType(),game.world.mapId(),game.appearance,_transport.alarm,scene))
                    return message(_hourWait.danger(),text,frame)?kWorldInputHandled:kWorldInputUnsupported;
                _waitHoursUnderlay=frame;_waitHoursPending=true;
                return message(_hourWait.prompt(),text,frame)?kWorldInputHandled:kWorldInputUnsupported;
            }
        }
        if (atWorldCommand()) {
            World candidate(game.world);WorldStory nextWindow=window;NpcScheduleState schedule=game.schedule;NpcRouteStore store=routes;GameplayRandom after=random;
            LightState nextLight=light;Common::Array<byte> data;bool dark=false;
            if (!archive.read("NA/GAME",data) || data.size()!=13824 ||
                    !Daylight::permanent(candidate.mapId(),data.data()+0x10b1,data.size()-0x10b1,dark) ||
                    !Daylight::advance(candidate.mapType(),dark,_transport.speed,game.clock,nextLight) ||
                    !prepareClockInput(archive,candidate,nextWindow,game.clock,schedule,store,after)) return kWorldInputUnsupported;
            if ((light.mode>=2)!=(nextLight.mode>=2) && candidate.mapId()==46 &&
                    !refreshNightActors(candidate,nextWindow,scene,turn.presentation,nextLight,visibility,text,frame,game.appearance,game.clock.lightActive,animation)) return kWorldInputUnsupported;
            game.world.adoptState(candidate);window=nextWindow;game.schedule=schedule;routes=store;random=after;light=nextLight;
        }
        if (_casting.active()) {
            const WorldCastResult result=_casting.handle(input,archive,game,_transport.object!=255,frame);
            if (result==kWorldCastUnsupported) return kWorldInputBoundary;
            if (result==kWorldCastTravel) {
                const NavigationPoint point=_casting.destination();WorldTravelPlan plan;
                plan.type=point.type;plan.map=point.map;plan.x=point.x;plan.y=point.y;plan.explicitArrival=true;
                if (!travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation) || !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
                return kWorldInputTransition;
            }
            if (result==kWorldCastClosed) {
                byte painted[187]={};
                if (!WorldViewport::refreshAtWorld(game.world,window,_story,_actors,_lighting,_occlusion,_painter,game.appearance,game.clock.lightActive,
                    animation,painted,turn.presentation,light,visibility,frame,&scene) || !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
            }
            return kWorldInputHandled;
        }
        if (_search.active()) return handleSearch(input,archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation);
        if (_inventory.active()) {
            const InventoryWindowResult result=_inventory.handle(input,archive,game,frame);
            if (result==kInventoryWindowUnsupported) return kWorldInputBoundary;
            if (result==kInventoryWindowClosed && !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (_conversation.active()) {
            const uint priorPartyCount=game.party.memberCount();
            const byte priorObject=_transport.object;
            const byte dialogueActor=_conversation.actorSlot();const bool plainDialogue=_conversation.plainDialogue();
            byte id=255;const WorldConversationResult result=_conversation.handle(input,archive,game,scene,frame,id,&_transport,&random,turn.praevocCursor);
            if (result==kWorldConversationUnsupported) return kWorldInputBoundary;
            if (_transport.object!=priorObject) {
                turn.presentation.partyObject=_transport.object;
                if (!window.captureRecords(game.world)) return kWorldInputUnsupported;
            }
            if (result==kWorldConversationTravel) {
                WorldTravelPlan plan;
                if (!_conversation.takeInnDestination(plan,frame) || !travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation,true) ||
                        !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
                return kWorldInputTransition;
            }
            if (result==kWorldConversationMessage && !message(catalog,id,text,frame)) return kWorldInputUnsupported;
            if (!window.captureRecords(game.world)) return kWorldInputUnsupported;
            if (result==kWorldConversationFinished) {
                // Plain Talk returns its saved exploration raster. Source
                // callbacks own explicit world redraws;do not replay actor
                // admission/facing or apply cache-load patches merely to close.
                const bool callbackRefresh=_conversation.takeWorldRefresh();
                if (plainDialogue && (game.party.memberCount()!=priorPartyCount || callbackRefresh)) {
                    // Recruitment removes the source NPC and returns through
                    // an explicit world redraw. Ordinary Talk keeps its raster.
                    byte painted[187]={};
                    if (!WorldViewport::refreshAtWorld(game.world,window,_story,_actors,_lighting,_occlusion,_painter,game.appearance,game.clock.lightActive,
                        animation,painted,turn.presentation,light,visibility,frame,&scene)) return kWorldInputUnsupported;
                }
                if (!window.captureRecords(game.world) || !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
                if (plainDialogue && (game.world.field(17)&32)) {
                    if (dialogueActor>240 || (dialogueActor&7)) return kWorldInputUnsupported;
                    const byte *actor=game.world.currentTables()->data()+1280+dialogueActor;
                    const byte x=byte(actor[0]-game.world.x()+8),y=byte(actor[1]-game.world.y()+5);
                    if (x>=17 || y>=11 || !actor[2] || actor[3]==255) return kWorldInputUnsupported;
                    _encounter=WorldActorTurnEvent();_encounter.battle=true;_encounter.clockPending=false;_encounter.bank=1;_encounter.slot=dialogueActor;_encounter.cell=byte(y*17+x);
                    _encounter.direction=turn.presentation.facing;return kWorldInputEncounter;
                }
            }
            return kWorldInputHandled;
        }
        if (_leave) {
            if (input.kbd.keycode!=Common::KEYCODE_y && input.kbd.keycode!=Common::KEYCODE_n) return kWorldInputHandled;
            // Source G:7CD0 clears the25-column message strip for both
            // decisions before returning to input or loading the parent map.
            if (!frame.clearTextSpan(7,23,25) || !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
            if (input.kbd.keycode==Common::KEYCODE_n) { _leave=false;_transport.pendingSteps=0;_transport.pendingDirection=255;return kWorldInputHandled; }
            WorldTravelPlan plan;const StoryMapState *state=window.state();
            if (!state) return kWorldInputBoundary;
            const WorldTravelResult selected=WorldTravelRules::leave(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),state->spawns,256,plan,game.appearance);
            if (selected==kTravelParentFallback) {
                const NavigationPoint &parent=game.navigation.parent();
                plan.type=parent.type;plan.map=parent.map;plan.x=parent.x;plan.y=parent.y;plan.explicitArrival=true;
            } else if (selected!=kTravelSelected) return kWorldInputBoundary;
            // F471-F4BA changes the map without calling the origin-capture helper.
            if (!travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation,false,false)) return kWorldInputUnsupported;
            // Main F46A/F4BD discards the nested movement callers and
            // returns to ordinary input, so no automatic substep crosses it.
            _transport.pendingSteps=0;_transport.pendingDirection=255;
            _leave=false;return kWorldInputTransition;
        }
        if (_notice) {
            if (_window.waiting()) return _window.handle(input) && _window.paint(frame)?kWorldInputHandled:kWorldInputUnsupported;
            if (!_window.ended()) return kWorldInputUnsupported;
            for (uint y=4;y<112;++y) for (uint x=24;x<40;++x)
                if (!frame.putByte(x,y,_underlay.data()[HgrFrame::rowOffset(y)+x])) return kWorldInputUnsupported;
            if (_campNotice) {_campNotice=false;_notice=false;return kWorldInputHandled;}
            if (_noticeTail) {
                const WorldActorTurnEvent contact=_noticeEncounter;const byte direction=_noticeDirection;
                _notice=false;_noticeTail=false;_noticeDirection=255;_noticeEncounter=WorldActorTurnEvent();
                const WorldInputResult completed=completeMovement(archive,game,window,scene,turn,light,visibility,random,catalog,text,frame,animation,direction,contact);
                if (completed!=kWorldInputHandled) return completed;
                checkedClock=game.clock;
            } else {
                // Historical native notice saves already applied their effects.
                if (!WorldMove::completeClock(game.world,game.clock)) return kWorldInputUnsupported;
                _notice=false;checkedClock=game.clock;
                const WorldInputResult portalResult=finishTurn(archive,game,window,scene,turn,light,visibility,catalog,text,frame,animation);
                if (portalResult!=kWorldInputHandled) return portalResult;
            }
            // The original notice does not acknowledge this key. Continue
            // handling this same event as an ordinary command.
        }
        Direction direction=kNorth;bool arrow=true;
        switch (input.kbd.keycode) {
        case Common::KEYCODE_UP:direction=kNorth;break;case Common::KEYCODE_DOWN:direction=kSouth;break;
        case Common::KEYCODE_RIGHT:direction=kEast;break;case Common::KEYCODE_LEFT:direction=kWest;break;
        default:arrow=false;break;
        }
        if (_fireDirection) {
            if (input.kbd.keycode==Common::KEYCODE_ESCAPE) {
                _fireDirection=false;
                for (uint y=184;y<192;++y) for (uint x=7;x<32;++x) if (!frame.putByte(x,y,0)) return kWorldInputUnsupported;
                return text.importPages(frame.data(),frame.data(),8192)?kWorldInputHandled:kWorldInputUnsupported;
            }
            if (!arrow) return kWorldInputHandled;
            return fireWorldCannon(direction,archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation);
        }
        if (_openDirection) {
            if (input.kbd.keycode==Common::KEYCODE_ESCAPE) { _openDirection=false;return kWorldInputHandled; }
            if (!arrow) return kWorldInputHandled;
            WorldOpenEvent event;const WorldOpenResult opened=_open.run(game.world,scene,direction,event);
            if (opened==kWorldOpenUnsupported) return kWorldInputUnsupported;
            if (opened==kWorldOpenInteraction) {
                _openDirection=false;
                if (!_search.begin(archive,game,frame)) return kWorldInputUnsupported;
                // Original Open passes the already selected cell to Search0.
                // Consume this same arrow once without repeating world input setup.
                return handleSearch(input,archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation);
            }
            if (opened==kWorldOpenLock) {
                if (event.object>248 || game.world.currentTables()->data()[1024+event.object+3]!=17) return kWorldInputBoundary;
                _openDirection=false;return _search.beginDoor(archive,game,event.object,frame)?kWorldInputHandled:kWorldInputUnsupported;
            }
            _openDirection=false;_refusalSound=event.sound;
            byte painted[187]={};
            if (opened==kWorldOpenChanged && !WorldViewport::refreshAtWorld(game.world,window,_story,_actors,_lighting,_occlusion,_painter,
                    game.appearance,game.clock.lightActive,animation,painted,turn.presentation,light,visibility,frame,&scene)) return kWorldInputUnsupported;
            if (event.message!=255 && !message(catalog,event.message,text,frame)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_b || input.kbd.keycode==Common::KEYCODE_x)
            return transportCommand(input.kbd.keycode==Common::KEYCODE_b,archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation);
        if (input.kbd.keycode==Common::KEYCODE_f) {
            if (game.appearance!=119 && game.appearance!=123) return message(catalog,18,text,frame)?kWorldInputHandled:kWorldInputUnsupported;
            _fireDirection=true;return message(catalog,12,text,frame)?kWorldInputHandled:kWorldInputUnsupported;
        }
        if (input.kbd.keycode==Common::KEYCODE_y || input.kbd.keycode==Common::KEYCODE_j) {
            if (!_transport.used) {_transport.appearance=_transport.baseAppearance=_transport.statusAppearance=game.appearance;}
            byte notice=255;if (input.kbd.keycode==Common::KEYCODE_y) WorldTransportControl::yell(_transport,notice);
            else {_refusalSound=!WorldTransportControl::requestJump(_transport);}
            if (notice!=255 && !message(catalog,notice,text,frame)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_h) {
            Common::String reply;const WorldCampResult camp=WorldCamp::run(game,scene,reply,archive);
            if (camp==kCampUnsupported) return kWorldInputUnsupported;
            if (camp==kCampCastleRefused) return message(catalog,18,text,frame)?kWorldInputHandled:kWorldInputUnsupported;
            if (camp==kCampRested) {
                Common::Array<byte> data;if (!archive.read("NA/GAME",data) ||
                    !WorldTravelRules::lightMode(game.world.mapType(),game.world.mapId(),game.clock.hour,game.clock.minute,data.data()+0x10b1,data.size()-0x10b1,light.mode)) return kWorldInputUnsupported;
                byte painted[187]={};
                if (!WorldViewport::refreshAtWorld(game.world,window,_story,_actors,_lighting,_occlusion,_painter,game.appearance,0,
                        animation,painted,turn.presentation,light,visibility,frame,&scene) || !window.captureRecords(game.world)) return kWorldInputUnsupported;
            } else if (!WorldCamp::refusal(archive,camp,reply)) return kWorldInputUnsupported;
            Common::Array<byte> body;for (uint i=0;i<reply.size();++i) body.push_back(reply[i]=='\n'?141:byte(reply[i])|128);
            _underlay=frame;HgrFrame background=frame;if (!OpeningBackdrop::renderNoticeFrame(background)) return kWorldInputUnsupported;
            frame=background;
            TextWindow next;if (!next.begin(archive,body.data(),body.size(),25,13,1,12,11,&background) || !next.paint(frame)) return kWorldInputUnsupported;
            _window=next;_notice=_campNotice=true;return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_c) {
            if (!_casting.begin(archive,game,frame)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_d) {
            if (!WorldDig::available(game.world.field(52))) {_refusalSound=true;return message(catalog,28,text,frame)?kWorldInputHandled:kWorldInputUnsupported;}
            if (!message(catalog,12,text,frame)) return kWorldInputUnsupported;_digDirection=true;return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_a) {_attackDirection=true;return message(catalog,12,text,frame)?kWorldInputHandled:kWorldInputUnsupported;}
        if (input.kbd.keycode==Common::KEYCODE_t) {
            if (!_conversation.choose(game.world,frame) || !message(catalog,12,text,frame)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_s) return _search.begin(archive,game,frame)?kWorldInputHandled:kWorldInputUnsupported;
        if (input.kbd.keycode==Common::KEYCODE_o) { _openDirection=true;return kWorldInputHandled; }
        if (!arrow) {
            const byte command=byte(input.kbd.ascii)&127;
            // Original command-table entries have dedicated owners. A truly
            // unknown printable key gives the authored free refusal instead
            // of silently swallowing input; CE-owned keys are handled above.
            static const char *known="3!GT\tOA/?N WLSVDHFCJYXBQ=I";
            const byte upper=command>='a' && command<='z'?byte(command-32):command;
            if (command>=32 && command<127 && !strchr(known,upper)) {
                _refusalSound=true;return message(catalog,16,text,frame)?kWorldInputHandled:kWorldInputUnsupported;
            }
            return kWorldInputIgnored;
        }
        if (!_transport.pendingSteps && _transport.object!=255) {
            if (_transport.pace>4) return kWorldInputBoundary;
            _transport.pendingSteps=MAX<byte>(1,_transport.pace);_transport.pendingDirection=byte(direction);_transport.moveCount=0;
        }
        if (_transport.pendingSteps && _transport.pendingDirection!=byte(direction)) return kWorldInputUnsupported;
        if (_transport.alignment) {
            byte *object=game.world._states[game.world._current].tables.data()+1024+_transport.object;
            if (direction==kWest) ++object[0];else if (direction==kNorth) ++object[1];else return kWorldInputUnsupported;
        }
        if (!_transport.alignment && !message(catalog,byte(5+uint(direction)),text,frame)) return kWorldInputUnsupported;
        Common::Array<byte> notice;WorldActorTurnEvent encounter;const WorldMoveResult moved=_move.run(input,game.world,window,scene,turn,game.clock,light,visibility,
            game.schedule,routes,random,catalog,text,frame,notice,encounter,animation,&_transport,&game.party,_transport.alignment,game.appearance,true);
        if (moved==kWorldMoveDefeat) {markDefeated();return kWorldInputDefeat;}
        if (moved!=kWorldMoveUnsupported && moved!=kWorldMoveBoundary && moved!=kWorldMoveLeave) finishTransportStep(scene);
        if (moved==kWorldMoveLeave) {
            if (!message(catalog,37,text,frame)) return kWorldInputUnsupported;
            _leave=true;return kWorldInputHandled;
        }
        if (moved==kWorldMoveAdvanced || moved==kWorldMoveArrival || moved==kWorldMoveEncounter) {
            const byte movementDirection=input.kbd.keycode==Common::KEYCODE_DOWN?1:input.kbd.keycode==Common::KEYCODE_RIGHT?2:input.kbd.keycode==Common::KEYCODE_LEFT?3:0;
            const WorldInputResult completed=completeMovement(archive,game,window,scene,turn,light,visibility,random,catalog,text,frame,animation,movementDirection,encounter);
            checkedClock=game.clock;return completed;
        }
        if (moved==kWorldMoveUnsupported) return kWorldInputUnsupported;
        if (moved==kWorldMoveBoundary) return kWorldInputBoundary;
        if (moved==kWorldMoveBlocked || moved==kWorldMoveInterrupted) {
            if (!message(catalog,moved==kWorldMoveInterrupted?26:10,text,frame)) return kWorldInputUnsupported;
            WorldActorTurnEvent event;
            const WorldWaitResult waited=_wait.run(game.world,window,scene,turn,game.clock,light,visibility,game.schedule,routes,random,catalog,text,frame,game.appearance,animation,event,true,4,nullptr,&game.party);
        if (waited==kWorldWaitDefeat) {markDefeated();return kWorldInputDefeat;}
            if (waited==kWorldWaitBattle) {_encounter=event;return kWorldInputEncounter;}
            return waited==kWorldWaitAdvanced?kWorldInputHandled:kWorldInputBoundary;
        }
        if (moved==kWorldMoveNotice) {
            _underlay=frame;HgrFrame background=frame;
            if (!OpeningBackdrop::renderNoticeFrame(background)) return kWorldInputUnsupported;
            TextWindow next;frame=background;
            if (!next.begin(archive,notice.data(),notice.size(),25,13,1,12,11,&background) || !next.paint(frame)) return kWorldInputUnsupported;
            _window=next;_notice=true;_noticeTail=true;_noticeEncounter=encounter;
            _noticeDirection=input.kbd.keycode==Common::KEYCODE_DOWN?1:input.kbd.keycode==Common::KEYCODE_RIGHT?2:input.kbd.keycode==Common::KEYCODE_LEFT?3:0;
        }
        return kWorldInputHandled;
    }
    WorldInputResult completeMovement(HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,WorldActorTurnState &turn,
            LightState &light,VisibilityMask &visibility,GameplayRandom &random,const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,
            byte animation,byte direction,const WorldActorTurnEvent &contact) {
        if (direction>3) return kWorldInputUnsupported;
        // Portal travel abandons the original movement callers and pending
        // actor contact before their clock/entrance/hazard services.
        const WorldInputResult portalResult=finishTurn(archive,game,window,scene,turn,light,visibility,catalog,text,frame,animation);
        if (portalResult!=kWorldInputHandled) return portalResult;
        if (!WorldMove::completeClock(game.world,game.clock)) return kWorldInputUnsupported;
        turn.population.arm(direction);
        WorldTravelPlan plan;const StoryMapState *state=window.state();if (!state) return kWorldInputUnsupported;
        const WorldTravelResult selected=WorldTravelRules::enter(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),game.world.field(59),state->spawns,256,plan);
        if (selected==kTravelInvalid) return kWorldInputBoundary;
        if (selected==kTravelSpecialBattle) {
            _encounter=WorldActorTurnEvent();_encounter.battle=true;_encounter.clockPending=false;_encounter.bank=0;_encounter.slot=0;_encounter.cell=93;_encounter.direction=direction;
            _scriptedEncounter=true;return kWorldInputEncounter;
        }
        if (selected==kTravelSelected) return travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation)?kWorldInputTransition:kWorldInputUnsupported;
        bool defeated=false;
        if (!_move.completeEffects(game.world,scene,turn,random,catalog,text,frame,game.party,defeated)) return kWorldInputUnsupported;
        if (defeated) {markDefeated();return kWorldInputDefeat;}
        if (contact.battle) {_encounter=contact;_encounter.clockPending=false;return kWorldInputEncounter;}
        return kWorldInputHandled;
    }
public:
    bool save(Common::WriteStream &out) const {
        if (_noticeTail) {
            if (!_notice || _campNotice || _encounter.battle || _noticeDirection>3) return false;
            Common::ScopedPtr<WorldInput> base(new WorldInput(*this));base->_noticeTail=false;base->_noticeDirection=255;base->_noticeEncounter=WorldActorTurnEvent();
            Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);if (!base->save(body)) return false;
            out.writeUint32BE(MKTAG('N','W','I','N'));out.writeUint16LE(18);out.writeByte(_noticeDirection);out.writeByte(_noticeEncounter.battle);
            if (_noticeEncounter.battle) {out.writeByte(_noticeEncounter.bank);out.writeByte(_noticeEncounter.slot);out.writeByte(_noticeEncounter.direction);out.writeByte(_noticeEncounter.cell);}
            out.writeUint32LE(body.size());out.write(body.getData(),body.size());return !out.err();
        }

        if (_encounter.battle && !_encounter.clockPending) {
            Common::ScopedPtr<WorldInput> base(new WorldInput(*this));base->_encounter.clockPending=true;
            Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);if (!base->save(body)) return false;
            out.writeUint32BE(MKTAG('N','W','I','N'));out.writeUint16LE(17);out.writeUint32LE(body.size());out.write(body.getData(),body.size());return !out.err();
        }

        if (_digDirection) {
            Common::ScopedPtr<WorldInput> base(new WorldInput(*this));base->_digDirection=false;
            Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);if (!base->atWorldCommand() || !base->save(body)) return false;
            out.writeUint32BE(MKTAG('N','W','I','N'));out.writeUint16LE(16);out.writeUint32LE(body.size());out.write(body.getData(),body.size());return !out.err();
        }

        if (_attackDirection || _attackAlarm) {
            if ((_attackDirection && _attackAlarm) || (_attackDirection && _encounter.battle) || (_attackAlarm && !_encounter.battle)) return false;
            Common::ScopedPtr<WorldInput> base(new WorldInput(*this));base->_attackDirection=base->_attackAlarm=false;
            Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);if (!base->save(body)) return false;
            out.writeUint32BE(MKTAG('N','W','I','N'));out.writeUint16LE(15);out.writeByte(_attackDirection?1:2);out.writeUint32LE(body.size());out.write(body.getData(),body.size());return !out.err();
        }

        if (_roster.active() || _waitHoursPending || _partyOrder.active() || _information.active()) return false; // Normal CE save eligibility excludes transient selectors.
        if (_campNotice) {
            if (!_notice) return false;
            Common::ScopedPtr<WorldInput> baseOwner(new WorldInput(*this));baseOwner->_campNotice=false;
            Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);if (!baseOwner->save(body)) return false;
            out.writeUint32BE(MKTAG('N','W','I','N'));out.writeUint16LE(12);out.writeUint32LE(body.size());out.write(body.getData(),body.size());return !out.err();
        }
        if (_search.active()) {
            Common::ScopedPtr<WorldInput> baseOwner(new WorldInput(*this));WorldInput &base=*baseOwner;base._search.cancel();
            if (!base.atWorldCommand()) return false;
            Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);if (!base.save(state)) return false;
            out.writeUint32BE(MKTAG('N','W','I','N'));out.writeUint16LE(_search.doorPending()?14:_search.generatedPending()?13:11);out.writeUint32LE(state.size());out.write(state.getData(),state.size());return _search.save(out);
        }
        if (_fireDirection && (_notice || _openDirection || _leave || _encounter.battle || _conversation.active() || _inventory.active() || _casting.active())) return false;
        if ((_conversation.active() && (_notice || _openDirection || _leave || _encounter.battle)) || (_leave && (_notice || _openDirection || _encounter.battle)) || (_notice && _openDirection) || (_encounter.battle && (_notice || _openDirection))) return false;
        if (!_transport.valid() || (_inventory.active() && (_notice || _openDirection || _leave || _encounter.battle || _conversation.active())) ||
            (_casting.active() && (_notice || _openDirection || _leave || _encounter.battle || _conversation.active() || _inventory.active()))) return false;
        const byte baseVersion=_casting.active()?7:_inventory.active()?6:_transport.used?5:4;
        if (_scriptedEncounter && !_encounter.battle) return false;
        out.writeUint32BE(MKTAG('N','W','I','N'));out.writeUint16LE(_scriptedEncounter?9:(_fireDirection || _fireProtected)?8:baseVersion);
        if (_scriptedEncounter || _fireDirection || _fireProtected) {out.writeByte(baseVersion);out.writeByte(_fireDirection);out.writeByte(_fireProtected);if (_scriptedEncounter) out.writeByte(1);}
        if (_inventory.active() || _casting.active()) out.writeByte(_transport.used);
        if (_transport.used && !_transport.save(out)) return false;
        out.writeByte(_notice);out.writeByte(_openDirection);out.writeByte(_leave);
        out.writeByte(_encounter.battle);
        if (_encounter.battle) { out.writeByte(_encounter.bank);out.writeByte(_encounter.slot);out.writeByte(_encounter.direction);out.writeByte(_encounter.cell); }
        if (_notice) {
            Common::MemoryWriteStreamDynamic window(DisposeAfterUse::YES);if (!_window.save(window)) return false;
            out.writeUint32LE(window.size());out.write(window.getData(),window.size());out.write(_underlay.data(),8192);
        }
        out.writeByte(_conversation.active()?1:0);
        if (_conversation.active() && !_conversation.save(out)) return false;
        if (_inventory.active() && !_inventory.save(out)) return false;
        if (_casting.active() && !_casting.save(out)) return false;
        return !out.err();
    }
    bool load(Common::SeekableReadStream &in,HdvArchive &archive,byte type=8) {
        if (in.readUint32BE()!=MKTAG('N','W','I','N')) return false;
        uint version=in.readUint16LE();
        if (version==18) {
            const byte direction=in.readByte(),pending=in.readByte();if (direction>3 || pending>1) return false;
            WorldActorTurnEvent contact;contact.battle=pending!=0;
            if (pending) {
                contact.bank=in.readByte();contact.slot=in.readByte();contact.direction=in.readByte();contact.cell=in.readByte();
                if (contact.bank>1 || (contact.slot&3) || (contact.bank && (contact.slot&7)) || contact.direction>3 || contact.cell>=187) return false;
            }
            const uint32 size=in.readUint32LE();if (size<6 || size>1048576 || int64(size)!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || READ_BE_UINT32(bytes.data())!=MKTAG('N','W','I','N') || READ_LE_UINT16(bytes.data()+4)>=18) return false;
            Common::MemoryReadStream body(bytes.data(),size);Common::ScopedPtr<WorldInput> next(new WorldInput);
            if (!next->load(body,archive,type) || !next->_notice || next->_campNotice || next->_encounter.battle) return false;
            next->_noticeTail=true;next->_noticeDirection=direction;next->_noticeEncounter=contact;*this=*next;return true;
        }

        if (version==17) {
            const uint32 size=in.readUint32LE();if (size<6 || size>1048576 || int64(size)!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || READ_BE_UINT32(bytes.data())!=MKTAG('N','W','I','N') || READ_LE_UINT16(bytes.data()+4)>=17) return false;
            Common::MemoryReadStream body(bytes.data(),size);Common::ScopedPtr<WorldInput> next(new WorldInput);
            if (!next->load(body,archive,type) || !next->_encounter.battle) return false;
            next->_encounter.clockPending=false;*this=*next;return true;
        }

        if (version==16) {
            const uint32 size=in.readUint32LE();if (size<6 || size>1048576 || int64(size)!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || READ_BE_UINT32(bytes.data())!=MKTAG('N','W','I','N') || READ_LE_UINT16(bytes.data()+4)>=16) return false;
            Common::ScopedPtr<WorldInput> next(new WorldInput);Common::MemoryReadStream body(bytes.data(),size);
            if (!next->load(body,archive,type) || !next->atWorldCommand()) return false;
            next->_digDirection=true;*this=*next;return true;
        }
        if (version==15) {
            const byte phase=in.readByte();const uint32 size=in.readUint32LE();
            if ((phase!=1 && phase!=2) || size<6 || size>1048576 || int64(size)!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || READ_BE_UINT32(bytes.data())!=MKTAG('N','W','I','N') || READ_LE_UINT16(bytes.data()+4)>=15) return false;
            Common::MemoryReadStream body(bytes.data(),size);Common::ScopedPtr<WorldInput> next(new WorldInput);
            if (!next->load(body,archive,type) || (phase==1?!next->atWorldCommand():!next->_encounter.battle)) return false;
            next->_attackDirection=phase==1;next->_attackAlarm=phase==2;*this=*next;return true;
        }

        if (version==12) {
            const uint size=in.readUint32LE();if (size<6 || size>1048576 || size!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || READ_LE_UINT16(bytes.data()+4)>=10) return false;
            Common::MemoryReadStream body(bytes.data(),size);Common::ScopedPtr<WorldInput> nextOwner(new WorldInput);
            if (!nextOwner->load(body,archive,type) || !nextOwner->_notice) return false;nextOwner->_campNotice=true;*this=*nextOwner;return true;
        }
        if (version==10 || version==11 || version==13 || version==14) {
            const uint size=in.readUint32LE();if (size<6 || size>1048576 || size>in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || READ_LE_UINT16(bytes.data()+4)>=10) return false;
            Common::MemoryReadStream body(bytes.data(),size);Common::ScopedPtr<WorldInput> nextOwner(new WorldInput);WorldInput &next=*nextOwner;
            if (!next.load(body,archive,type) || !next.atWorldCommand() || !next._search.load(in,archive,version>=11,version==13,version==14)) return false;
            *this=next;return true;
        }
        if (version<1 || version>9) return false;
        Common::ScopedPtr<WorldInput> nextOwner(new WorldInput);WorldInput &next=*nextOwner;
        const bool scripted=version==9;
        if (version>=8) {version=in.readByte();const byte fire=in.readByte(),protection=in.readByte();if (version<4 || version>7 || fire>1 || protection>1 || (!fire && !protection && !scripted)) return false;next._fireDirection=fire!=0;next._fireProtected=protection!=0;if (scripted && in.readByte()!=1) return false;next._scriptedEncounter=scripted;}
        const byte transport=version>=6?in.readByte():version==5?1:0;
        if (transport>1 || (transport && (!next._transport.load(in) || !next._transport.used))) return false;
        const byte notice=in.readByte(),open=in.readByte();if (notice>1 || open>1 || (notice && open)) return false;
        next._notice=notice!=0;next._openDirection=open!=0;
        if (version>=3) { const byte leave=in.readByte();if (leave>1 || (leave && (notice || open))) return false;next._leave=leave!=0; }
        if (version>=2) {
            const byte pending=in.readByte();if (pending>1 || (pending && (notice || open || next._leave))) return false;
            next._encounter.battle=pending!=0;
            if (pending) {
                next._encounter.bank=in.readByte();next._encounter.slot=in.readByte();next._encounter.direction=in.readByte();next._encounter.cell=in.readByte();
                if (next._encounter.bank>1 || (next._encounter.slot&3) || (next._encounter.bank && (next._encounter.slot&7)) || next._encounter.direction>3 || next._encounter.cell>=187) return false;
            }
        }
        if (notice) {
            const uint32 size=in.readUint32LE();if (size>131072 || in.size()-in.pos()!=int64(size)+8192+(version>=4?1:0)) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size) return false;
            Common::MemoryReadStream part(bytes.data(),size);if (!next._window.load(part,archive)) return false;
            byte image[8192];if (in.read(image,8192)!=8192 || !next._underlay.importImage(image,8192)) return false;
        }
        if (version>=4) {
            const byte talking=in.readByte();if (talking>1 || (talking && (next._notice || next._openDirection || next._leave || next._encounter.battle))) return false;
            if (version>=6 && (talking || next._notice || next._openDirection || next._leave || next._encounter.battle)) return false;
            if (talking && !next._conversation.load(in,archive)) return false;
            if (version==6 && !next._inventory.load(in,archive)) return false;
            if (version==7 && !next._casting.load(in)) return false;
        }
        if ((next._scriptedEncounter && (!next._encounter.battle || type!=0 || next._encounter.bank || next._encounter.slot || next._encounter.cell!=93)) || (next._fireDirection && (next._notice || next._openDirection || next._leave || next._encounter.battle || next._conversation.active() || next._inventory.active() || next._casting.active())) || in.err() || in.pos()!=in.size() || !next.loadRules(archive,type)) return false;
        *this=next;return true;
    }
private:
    WorldInputResult handleSearch(const Common::Event &input,HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,
            WorldActorTurnState &turn,LightState &light,VisibilityMask &visibility,NpcRouteStore &routes,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,byte animation) {
            const SearchInputResult result=_search.handle(input,archive,game,scene,random,frame,_transport.alarm);
            if (_transport.alarm) _transport.used=true;
            if (result==kSearchInputUnsupported) return kWorldInputBoundary;
            // Search can mutate object/guard records before its result is closed.
            // Keep the owned viewport cache coherent at that save boundary.
            if (!window.captureRecords(game.world)) return kWorldInputUnsupported;
            // LWR1AC2 checks terminal party death before Search may retry or
            // spend a world turn. Trap damage has already committed here.
            bool alive=false;for (uint i=0;i<game.party.memberCount();++i) alive|=game.party.member(i)->bytes[0]!=255;
            if (!alive) {_search.cancel(&frame);markDefeated();return kWorldInputDefeat;}
            if (result==kSearchInputClosed || result==kSearchInputAttempted) {
                byte painted[187]={};
                if (!window.captureRecords(game.world) || !WorldViewport::refreshAtWorld(game.world,window,_story,_actors,_lighting,_occlusion,_painter,game.appearance,game.clock.lightActive,
                        animation,painted,turn.presentation,light,visibility,frame,&scene) || !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
                if (result==kSearchInputAttempted) {
                    WorldActorTurnEvent raised;
                    const WorldWaitResult advanced=_wait.run(game.world,window,scene,turn,game.clock,light,visibility,game.schedule,routes,random,catalog,text,frame,game.appearance,animation,raised,true,4,nullptr,&game.party);
                    if (advanced==kWorldWaitBattle) {_search.cancel();_encounter=raised;return kWorldInputEncounter;}
                    if (advanced==kWorldWaitDefeat) {_search.cancel();markDefeated();return kWorldInputDefeat;}
                    if (advanced!=kWorldWaitAdvanced) return kWorldInputBoundary;
                }
                if (!_search.refresh(archive,frame)) return kWorldInputUnsupported;
            }
            return kWorldInputHandled;
    }
    static bool prepareClockInput(HdvArchive &archive,World &world,WorldStory &window,const TurnClock &clock,NpcScheduleState &schedule,NpcRouteStore &store,GameplayRandom &random) {
        if (world.mapType()<4 || world.mapId()==69) return true;
        if (NpcScheduler::activateAtClock(world,clock.hour,clock.minute)==kNpcScheduleUnsupported ||
                NpcScheduler::prefetchAtClock(world,clock.hour,clock.minute,schedule)==kNpcScheduleUnsupported) return false;
        Common::ScopedPtr<NpcNavigation> navigation(new NpcNavigation);bool complete=false;
        for (uint tick=0;tick<4096;++tick) {
            const NpcNavigationResult result=navigation->advance(archive,world,window,schedule,store,random);
            if (result==kNpcNavigationIdle || (result==kNpcNavigationStored && !navigation->active())) {complete=true;break;}
            if (result!=kNpcNavigationSearching) {warning("Native scheduled route result %u",uint(result));return false;}
        }
        return complete && window.captureRecords(world);
    }
    enum FreeControl {kControlTorch,kControlIcon,kControlWait};
    WorldInputResult freeControl(FreeControl action,byte hours,HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,
            WorldActorTurnState &turn,LightState &light,VisibilityMask &visibility,NpcRouteStore &routes,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,byte animation) {
        World candidate(game.world);WorldStory nextWindow=window;WorldScene nextScene=scene;
        TurnClock nextClock=game.clock;LightState nextLight=light;VisibilityMask nextVisibility=visibility;
        PresentationState nextPresentation=turn.presentation;WorldTransportState controls=_transport;GameplayRandom after=random;
        NpcScheduleState schedule=game.schedule;NpcRouteStore store=routes;
        TextGlyphSurface surface=text;HgrFrame image=frame;byte appearance=game.appearance,notice=255;Common::String reply;
        if (action==kControlTorch) {
            const TorchResult result=PlayerTorch::ignite(candidate._fields[32],nextClock);
            if (result==kTorchEmpty) return message(catalog,17,text,frame)?kWorldInputHandled:kWorldInputUnsupported;
            if (result==kTorchAlreadyLit) return kWorldInputHandled;
            notice=1;
        } else if (action==kControlIcon) {
            if (!PlayerIcon::world(appearance,controls,game.world.onFoot())) {
                _refusalSound=true;return message(catalog,18,text,frame)?kWorldInputHandled:kWorldInputUnsupported;
            }
        } else {
            if (_hourWait.apply(hours,nextClock,candidate._fields[20],after,reply)!=kHourWaitApplied) return kWorldInputUnsupported;
            Common::Array<byte> data;
            if (!archive.read("NA/GAME",data) || data.size()!=13824 || !WorldTravelRules::lightMode(candidate.mapType(),candidate.mapId(),nextClock.hour,nextClock.minute,data.data()+0x10b1,data.size()-0x10b1,nextLight.mode)) return kWorldInputUnsupported;
        }
        if (action==kControlWait && !prepareClockInput(archive,candidate,nextWindow,nextClock,schedule,store,after)) return kWorldInputUnsupported;
        if (action==kControlWait && (light.mode>=2)!=(nextLight.mode>=2)) {
            bool changed=false;if (!_nightActors.apply(candidate.mapId(),nextLight.mode,candidate._states[candidate._current].tables.data(),1792,changed)) return kWorldInputUnsupported;
        }
        byte painted[187]={};
        if (!WorldViewport::refreshAtWorld(candidate,nextWindow,_story,_actors,_lighting,_occlusion,_painter,
                appearance,nextClock.lightActive,animation,painted,nextPresentation,nextLight,nextVisibility,image,&nextScene) ||
                !surface.importPages(image.data(),image.data(),8192)) return kWorldInputUnsupported;
        if (notice!=255 && !message(catalog,notice,surface,image)) return kWorldInputUnsupported;
        if (!reply.empty() && !message(reply,surface,image)) return kWorldInputUnsupported;
        game.world.adoptState(candidate);game.clock=nextClock;game.appearance=appearance;window=nextWindow;scene=nextScene;
        turn.presentation=nextPresentation;light=nextLight;visibility=nextVisibility;random=after;text=surface;frame=image;_transport=controls;
        game.schedule=schedule;routes=store;
        return kWorldInputHandled;
    }
    static bool message(const Common::String &body,TextGlyphSurface &surface,HgrFrame &frame) {
        if (body.size()>25 || !surface.importPages(frame.data(),frame.data(),8192) || !surface.clearTextSpan(7,23,25) ||
                !surface.setCursor(byte(7+(25-body.size())/2),23) || !surface.setStyle(3,0)) return false;
        for (uint i=0;i<body.size();++i) if (byte(body[i])<32 || !surface.print(byte(body[i])|128)) return false;
        for (uint y=184;y<192;++y) for (uint x=7;x<32;++x)
            if (!frame.putByte(x,y,surface.page(0)[HgrFrame::rowOffset(y)+x])) return false;
        return true;
    }
    WorldInputResult fireWorldCannon(Direction direction,HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,
            WorldActorTurnState &turn,LightState &light,VisibilityMask &visibility,NpcRouteStore &routes,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,byte animation) {
        if (!_fireDirection || uint(direction)>3) return kWorldInputUnsupported;
        World candidate=game.world;WorldStory nextWindow=window;WorldScene nextScene=scene;WorldActorTurnState nextTurn=turn;
        WorldTransportState controls=_transport;GameplayRandom after=random;Party party=game.party;TurnClock clock=game.clock;
        LightState nextLight=light;VisibilityMask mask=visibility;NpcScheduleState schedule=game.schedule;NpcRouteStore nextRoutes=routes;
        TextGlyphSurface nextText=text;HgrFrame image=frame;Common::Array<byte> data,footer;MapAssetRange range;
        if (!archive.read("NA/GAME",data) || !resolveMapAssets(data,candidate.mapType(),range) || !archive.readRange(range.resource,range.offset+range.size,72,footer)) return kWorldInputUnsupported;
        static const byte directions[]={139,138,149,136};byte *records=candidate._states[candidate._current].tables.data();WorldProjectileOutcome shot;
        if (WorldCannon::fire(game.appearance,directions[uint(direction)],candidate.mapId(),_fireProtected,nextTurn.effects.projectileMode,controls.alarm,
                footer.data(),records+768,records+1280,nextScene.actors.lookup,nextScene.actors.kinds,after,shot)!=kWorldCannonFired || !nextWindow.captureRecords(candidate)) return kWorldInputUnsupported;
        for (uint y=184;y<192;++y) for (uint x=7;x<32;++x) if (!image.putByte(x,y,0)) return kWorldInputUnsupported;
        if (!nextText.importPages(image.data(),image.data(),8192)) return kWorldInputUnsupported;
        WorldActorTurnEvent encounter;
        const uint soundStart=_sounds?_sounds->size():0;
        const WorldWaitResult result=_wait.run(candidate,nextWindow,nextScene,nextTurn,clock,nextLight,mask,schedule,nextRoutes,after,catalog,nextText,image,game.appearance,animation,encounter,true,4,nullptr,&party);
        if (result!=kWorldWaitAdvanced && result!=kWorldWaitBattle && result!=kWorldWaitDefeat) return kWorldInputBoundary;
        game.world.adoptState(candidate);game.party=party;game.clock=clock;game.schedule=schedule;window=nextWindow;scene=nextScene;turn=nextTurn;
        routes=nextRoutes;light=nextLight;visibility=mask;random=after;text=nextText;frame=image;_transport=controls;_fireDirection=false;
        if (_sounds) _sounds->insert_at(soundStart,kSpeakerProjectile);
        if (result==kWorldWaitBattle) {_encounter=encounter;return kWorldInputEncounter;}
        if (result==kWorldWaitDefeat) {markDefeated();return kWorldInputDefeat;}
        return kWorldInputHandled;
    }
    void finishTransportStep(WorldScene &scene) {
        if (!_transport.pendingSteps) return;
        ++_transport.moveCount;--_transport.pendingSteps;
        if (_transport.pendingSteps) {if (_transport.alignment) _transport.pendingDirection=0;return;}
        if (_transport.alignment) clearBoardedFrigate(scene);
        _transport.pendingDirection=255;_transport.alignment=false;
        if (_transport.jumpRequested) {_transport.jumpRequested=false;if (!_transport.speed) _transport.pace=0;}
    }
    static void clearBoardedFrigate(WorldScene &scene) {
        // INVENTORY overlay9742-974D, after the final alignment world phase.
        scene.objects.lookup[93]=scene.objects.lookup[94]=scene.objects.lookup[110]=scene.objects.lookup[111]=255;
    }
    WorldInputResult transportCommand(bool board,HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,
            WorldActorTurnState &turn,LightState &light,VisibilityMask &visibility,NpcRouteStore &routes,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,byte animation) {
        World candidate(game.world);WorldTransportState controls=_transport;
        if (!controls.used) {controls.appearance=controls.baseAppearance=controls.statusAppearance=game.appearance;}
        controls.jumpRequested=false;controls.pendingSteps=0;controls.pendingDirection=255;controls.alignment=false;
        byte *records=candidate._states[candidate._current].tables.data();Common::Array<byte> gameData,footer;MapAssetRange range;
        if (!archive.read("NA/GAME",gameData) || !resolveMapAssets(gameData,candidate.mapType(),range) || !archive.readRange(range.resource,range.offset+range.size,72,footer)) return kWorldInputUnsupported;
        TransportControlContext context={candidate.mapId(),candidate.field(66),candidate.x(),candidate.y(),scene.objects.lookup[93],records+1024,records+1280,footer.data()};
        TransportControlOutcome outcome;const TransportControlResult result=WorldTransportControl::command(board,context,controls,outcome);
        if (result==kTransportInvalid) return kWorldInputBoundary;
        candidate._onFoot=controls.object==255;game.world.adoptState(candidate);game.appearance=controls.appearance;_transport=controls;turn.presentation.partyObject=controls.object;
        if (!window.captureRecords(game.world)) return kWorldInputUnsupported;
        if (result==kTransportRefused) return kWorldInputHandled;
        if (outcome.alignOrigin) {
            _transport.alignment=true;_transport.moveCount=0;_transport.pendingSteps=byte((outcome.x!=game.world.x())+(outcome.y!=game.world.y()));
            _transport.pendingDirection=outcome.x!=game.world.x()?3:0;
            Common::Event movement;movement.type=Common::EVENT_KEYDOWN;movement.kbd.keycode=_transport.pendingDirection==3?Common::KEYCODE_LEFT:Common::KEYCODE_UP;
            return handle(movement,archive,game,window,scene,turn,light,visibility,routes,random,catalog,text,frame,animation);
        }
        WorldActorTurnEvent encounter;const WorldWaitResult waited=_wait.run(game.world,window,scene,turn,game.clock,light,visibility,game.schedule,routes,random,catalog,text,frame,game.appearance,animation,encounter,true,4,nullptr,&game.party);
        if (board && controls.appearance==119) clearBoardedFrigate(scene);
        if (waited==kWorldWaitDefeat) {markDefeated();return kWorldInputDefeat;}
        if (waited==kWorldWaitBattle) {_encounter=encounter;return kWorldInputEncounter;}
        return waited==kWorldWaitAdvanced?kWorldInputHandled:kWorldInputBoundary;
    }
    bool travel(HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,WorldActorTurnState &turn,
            LightState &light,VisibilityMask &visibility,HgrFrame &frame,const WorldTravelPlan &plan,byte animation,bool innRest=false,bool recordOrigin=true) {
        const auto reject=[&](const char *phase) {
            warning("Native travel %u/%u to%u/%u at%u,%u failed: %s",uint(game.world.mapType()),uint(game.world.mapId()),
                    uint(plan.type),uint(plan.map),uint(plan.x),uint(plan.y),phase);return false;
        };
        World candidate(game.world);NpcScheduleState schedule=game.schedule;WorldStory nextWindow;WorldScene nextScene;
        WorldActorTurnState nextTurn=turn;LightState nextLight=light;VisibilityMask nextVisibility=visibility;HgrFrame nextFrame=frame;
        WorldTransportState nextTransport=_transport;
        WorldNavigation navigation=game.navigation;
        if (recordOrigin && !navigation.enter(NavigationPoint(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y()),plan.type)) return reject("navigation");
        if (!WorldTravel::commit(candidate,plan,&nextTransport)) return reject("owned map/cargo");
        nextTurn.presentation.partyObject=nextTransport.object;
        nextTurn.effects.queued=0; // source map-load abandons the origin projectile
        nextTurn.population.arm();
        if (!nextWindow.initialize(candidate,candidate.currentTables()->data()+1536,256)) return reject("window");
        if (!loadRules(archive,plan.type)) return reject("rules");
        // M:FB0C selects permanent dark maps, otherwise the source day/night
        // boundary5:30..20:30. Preserve the light level/transition counter.
        Common::Array<byte> data;if (!archive.read("NA/GAME",data)) return reject("source data");
        if (innRest) nextLight.mode=1;
        else if (!WorldTravelRules::lightMode(plan.type,plan.map,game.clock.hour,game.clock.minute,data.data()+0x10b1,data.size()-0x10b1,nextLight.mode)) return reject("light mode");
        bool nightChanged=false;
        if (!_nightActors.apply(candidate.mapId(),nextLight.mode,candidate._states[candidate._current].tables.data(),1792,nightChanged)) return reject("night actors");
        byte painted[187]={};
        if (!WorldViewport::refreshAtWorld(candidate,nextWindow,_story,_actors,_lighting,_occlusion,_painter,game.appearance,game.clock.lightActive,
                animation,painted,nextTurn.presentation,nextLight,nextVisibility,nextFrame,&nextScene)) return reject("viewport");
        // M:F69B refresh precedes F69E map-entry schedule. Capture its actual
        // resulting records for persistence without replaying actor admission.
        NpcSchedulePolicy policy={false,false};
        if (NpcScheduler::apply(candidate,game.clock.hour,game.clock.minute,policy,schedule)==kNpcScheduleUnsupported) return reject("NPC schedule");
        if (!nextWindow.captureRecords(candidate)) return reject("record capture");
        TurnClock nextClock=game.clock;
        if (innRest) {
            byte hours;
            if (!InnService::sleep(nextClock,hours) || !WorldTravelRules::lightMode(plan.type,plan.map,nextClock.hour,nextClock.minute,
                    data.data()+0x10b1,data.size()-0x10b1,nextLight.mode)) return reject("light mode");
        }
        game.world.adoptState(candidate);game.schedule=schedule;window=nextWindow;scene=nextScene;turn=nextTurn;light=nextLight;visibility=nextVisibility;frame=nextFrame;
        game.clock=nextClock;game.navigation=navigation;_transport=nextTransport;
        return true;
    }
    bool refreshNightActors(World &world,WorldStory &window,WorldScene &scene,PresentationState &presentation,LightState &light,
            VisibilityMask &visibility,TextGlyphSurface &text,HgrFrame &frame,byte appearance,byte extraLight,byte animation) {
        bool changed=false;if (!_nightActors.apply(world.mapId(),light.mode,world._states[world._current].tables.data(),1792,changed)) return false;
        if (!changed) return true;
        WorldScene nextScene;PresentationState nextPresentation=presentation;LightState nextLight=light;VisibilityMask mask=visibility;
        HgrFrame image=frame;TextGlyphSurface surface=text;byte painted[187]={};
        if (!WorldViewport::refreshAtWorld(world,window,_story,_actors,_lighting,_occlusion,_painter,appearance,extraLight,
                animation,painted,nextPresentation,nextLight,mask,image,&nextScene) || !window.captureRecords(world) ||
                !surface.importPages(image.data(),image.data(),8192)) return false;
        scene=nextScene;presentation=nextPresentation;light=nextLight;visibility=mask;text=surface;frame=image;return true;
    }
    static bool message(const FixedMessageCatalog &catalog,byte id,TextGlyphSurface &surface,HgrFrame &frame) {
        if (!surface.importPages(frame.data(),frame.data(),8192) || !showFixedMessageStrip(catalog,id,surface)) return false;
        for (uint y=184;y<192;++y) for (uint x=7;x<32;++x)
            if (!frame.putByte(x,y,surface.page(0)[HgrFrame::rowOffset(y)+x])) return false;
        return true;
    }
    bool _notice,_campNotice,_openDirection,_leave,_fireDirection,_fireProtected,_scriptedEncounter,_refusalSound,_attackDirection,_attackAlarm,_digDirection,_noticeTail;byte _noticeDirection;WorldActorTurnEvent _noticeEncounter;TextWindow _window;HgrFrame _underlay;
    WorldActorTurnEvent _encounter;WorldConversation _conversation;WorldTransportState _transport;InventoryWindow _inventory;WorldCast _casting;PartyOrderInput _partyOrder;InformationInput _information;PartyRoster _roster;HourWait _hourWait;HgrFrame _waitHoursUnderlay;bool _waitHoursPending;
    SearchInput _search;WorldMove _move;WorldOpen _open;WorldWait _wait;NightActors _nightActors;WorldPopulation _population;StoryMapPatches _story;ActorPaintRules _actors;TerrainLighting _lighting;OcclusionRules _occlusion;TilePainter _painter;SoundEvents *_sounds;
};
} }
#endif
