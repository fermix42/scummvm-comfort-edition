/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_INPUT_H
#define NOX_NATIVE_WORLD_INPUT_H
#include "nox/native_new_game.h"
#include "nox/native_world_move.h"
#include "nox/native_world_travel.h"
#include "nox/native_world_conversation.h"
#include "nox/native_inventory_window.h"
#include "nox/native_world_cast.h"
#include "nox/native_world_cannon.h"
#include "nox/native_world_open.h"
#include "nox/native_text_window.h"
#include "nox/native_opening_backdrop.h"
#include "nox/native_ending.h"
namespace Nox { namespace Native {
enum WorldInputResult { kWorldInputIgnored,kWorldInputHandled,kWorldInputBoundary,kWorldInputUnsupported,kWorldInputEncounter,kWorldInputTransition,kWorldInputDefeat };
// Semantic pending input, including the original notice's key forwarding.
// No key stream carries positions, quest fields or expected outcomes.
class WorldInput {
public:
    WorldInput() : _notice(false),_openDirection(false),_leave(false),_fireDirection(false),_fireProtected(false),_scriptedEncounter(false) {}
    bool loadRules(HdvArchive &archive,byte type=8) {
        return _move.load(archive,type) && _open.load(archive,type) && _wait.load(archive,type) && _story.load(archive) &&
            _actors.load(archive,type) && _lighting.load(archive,type) && _occlusion.load(archive,type) && _painter.load(archive,type);
    }
    bool noticeActive() const { return _fireDirection || _notice || _conversation.active() || _inventory.active() || _casting.active(); }
    void inheritCombatHistory(const WorldInput &other) {_fireProtected=other._fireProtected;}
    bool wholeFrameMenu() const { return _casting.active() || _inventory.active() || _conversation.stage()>=22; }
    bool atWorldCommand() const {
        return !_notice && !_openDirection && !_leave && !_fireDirection && !_encounter.battle && !_conversation.active() &&
            !_inventory.active() && !_casting.active() && !_transport.defeated && !_transport.pendingSteps && !_transport.alignment;
    }
    bool matchesWorld(const World &world) const { return _casting.matches(world) && _inventory.matches(world) && _conversation.matchesWorld(world) && world.onFoot()==(_transport.object==255); }
    bool worldDefeated() const {return _transport.defeated;}
    byte transportObject() const {return _transport.object;}
    bool canRepositionMountedFixture() const {
        return _transport.used && _transport.object!=255 && !_transport.pendingSteps && !_transport.alignment && !_transport.defeated &&
            !_fireDirection && !_notice && !_openDirection && !_leave && !_encounter.battle && !_conversation.active() && !_inventory.active() && !_casting.active();
    }
    void markDefeated() {_transport.used=_transport.defeated=true;_transport.pendingSteps=0;_transport.pendingDirection=255;_transport.alignment=false;}
    byte automaticDirection() const {return !_fireDirection && !_encounter.battle && !_notice && !_openDirection && !_leave && !_conversation.active() && !_inventory.active() && !_casting.active() && !_transport.defeated && _transport.pendingSteps?_transport.pendingDirection:255;}
    byte conversationStage() const { return _conversation.stage(); }
    byte inventoryStage() const {return _inventory.phase();}
    bool selectingOpen() const { return _openDirection; }
    const WorldActorTurnEvent &encounter() const { return _encounter; }
    bool scriptedEncounter() const {return _scriptedEncounter;}
    bool continueTempleVictory(HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,WorldActorTurnState &turn,
            LightState &light,VisibilityMask &visibility,HgrFrame &frame,byte animation) {
        if (!_encounter.battle || !EndingPresentation::revelation(game.world.mapId(),game.world.field(56),game.world.field(59))) return false;
        WorldTravelPlan plan;plan.type=7;plan.map=58;plan.x=20;plan.y=13;plan.explicitArrival=true;
        const byte oldHour=game.clock.hour;game.clock.hour=9;
        if (!travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation)) {game.clock.hour=oldHour;return false;}
        return true;
    }
    bool beginTempleConversation(HdvArchive &archive,NewGame &game,const WorldScene &scene,HgrFrame &frame) {
        // G:7149 explicitly selects NPC8 after R's teleport. The authored
        // scheduled NPC is immediately north of20,13; do not guess another.
        if (game.world.mapType()!=7 || game.world.mapId()!=58 || game.world.x()!=20 || game.world.y()!=13 ||
            !atWorldCommand() || scene.actors.lookup[76]!=8 || (scene.actors.kinds[76]&135)!=4 || !_conversation.choose(game.world,frame)) return false;
        Common::Event north;north.type=Common::EVENT_KEYDOWN;north.kbd=Common::KeyState(Common::KEYCODE_UP,0);byte message=255;
        return _conversation.handle(north,archive,game,scene,frame,message,&_transport)==kWorldConversationHandled;
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
    bool finishEncounter(bool fireProtected=false) {
        if (!_encounter.battle || _notice || _openDirection) return false;
        _encounter.battle=false;_scriptedEncounter=false;_fireProtected=fireProtected;return true;
    }
    static bool showMessage(const FixedMessageCatalog &catalog,byte id,TextGlyphSurface &surface,HgrFrame &frame) {
        return message(catalog,id,surface,frame);
    }
    WorldInputResult handle(const Common::Event &input,HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,
            WorldActorTurnState &turn,LightState &light,VisibilityMask &visibility,NpcRouteStore &routes,GameplayRandom &random,
            const FixedMessageCatalog &catalog,TextGlyphSurface &text,HgrFrame &frame,byte animation=0) {
        if (animation>3) return kWorldInputUnsupported;
        if (input.type!=Common::EVENT_KEYDOWN) return kWorldInputIgnored;
        if (_transport.defeated) return kWorldInputDefeat;
        if (_encounter.battle) return kWorldInputBoundary;
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
        if (_inventory.active()) {
            const InventoryWindowResult result=_inventory.handle(input,archive,game,frame);
            if (result==kInventoryWindowUnsupported) return kWorldInputBoundary;
            if (result==kInventoryWindowClosed && !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (_conversation.active()) {
            const byte priorObject=_transport.object;
            const byte dialogueActor=_conversation.actorSlot();const bool plainDialogue=_conversation.plainDialogue();
            byte id=255;const WorldConversationResult result=_conversation.handle(input,archive,game,scene,frame,id,&_transport);
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
            if (result==kWorldConversationFinished) {
                // Plain Talk returns its saved exploration raster. Source
                // callbacks own explicit world redraws;do not replay actor
                // admission/facing or apply cache-load patches merely to close.
                if (!window.captureRecords(game.world) || !text.importPages(frame.data(),frame.data(),8192)) return kWorldInputUnsupported;
                if (plainDialogue && (game.world.field(17)&32)) {
                    if (dialogueActor>240 || (dialogueActor&7)) return kWorldInputUnsupported;
                    const byte *actor=game.world.currentTables()->data()+1280+dialogueActor;
                    const byte x=byte(actor[0]-game.world.x()+8),y=byte(actor[1]-game.world.y()+5);
                    if (x>=17 || y>=11 || !actor[2] || actor[3]==255) return kWorldInputUnsupported;
                    _encounter=WorldActorTurnEvent();_encounter.battle=true;_encounter.bank=1;_encounter.slot=dialogueActor;_encounter.cell=byte(y*17+x);
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
            const WorldTravelResult selected=WorldTravelRules::leave(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),state->spawns,256,plan);
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
            if (!WorldMove::completeClock(game.world,game.clock)) return kWorldInputUnsupported;
            _notice=false;
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
            if (opened==kWorldOpenInteraction || opened==kWorldOpenLock) return kWorldInputBoundary;
            _openDirection=false;
            byte painted[187]={};
            if (opened==kWorldOpenChanged && !WorldViewport::refreshAtWorld(game.world,window,_story,_actors,_lighting,_occlusion,_painter,
                    246,game.clock.lightActive,animation,painted,turn.presentation,light,visibility,frame,&scene)) return kWorldInputUnsupported;
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
            byte notice=255;if (input.kbd.keycode==Common::KEYCODE_y) WorldTransportControl::yell(_transport,notice);else WorldTransportControl::requestJump(_transport);
            if (notice!=255 && !message(catalog,notice,text,frame)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_c) {
            if (!_casting.begin(archive,game,frame)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_i) {
            if (!_inventory.begin(archive,game,frame)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_t) {
            if (!_conversation.choose(game.world,frame) || !message(catalog,12,text,frame)) return kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (input.kbd.keycode==Common::KEYCODE_o) { _openDirection=true;return kWorldInputHandled; }
        if (!arrow) return kWorldInputIgnored;
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
            game.schedule,routes,random,catalog,text,frame,notice,encounter,animation,&_transport,&game.party,_transport.alignment);
        if (moved==kWorldMoveDefeat) {markDefeated();return kWorldInputDefeat;}
        if (moved!=kWorldMoveUnsupported && moved!=kWorldMoveBoundary && moved!=kWorldMoveLeave) finishTransportStep(scene);
        if (moved==kWorldMoveLeave) {
            if (!message(catalog,37,text,frame)) return kWorldInputUnsupported;
            _leave=true;return kWorldInputHandled;
        }
        if (moved==kWorldMoveArrival) {
            WorldTravelPlan plan;const StoryMapState *state=window.state();if (!state) return kWorldInputUnsupported;
            const WorldTravelResult selected=WorldTravelRules::enter(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y(),game.world.field(59),state->spawns,256,plan);
            if (selected==kTravelInvalid) return kWorldInputBoundary;
            if (selected==kTravelSpecialBattle) {
                //M:F560 invokes an authored Shadow encounter directly. No
                //world actor is manufactured or retired for this caller.
                _encounter=WorldActorTurnEvent();_encounter.battle=true;_encounter.bank=0;_encounter.slot=0;_encounter.cell=93;
                _encounter.direction=input.kbd.keycode==Common::KEYCODE_DOWN?1:input.kbd.keycode==Common::KEYCODE_RIGHT?2:input.kbd.keycode==Common::KEYCODE_LEFT?3:0;
                _scriptedEncounter=true;return kWorldInputEncounter;
            }
            if (selected==kTravelSelected) return travel(archive,game,window,scene,turn,light,visibility,frame,plan,animation)?kWorldInputTransition:kWorldInputUnsupported;
            return kWorldInputHandled;
        }
        if (moved==kWorldMoveEncounter) { _encounter=encounter;return kWorldInputEncounter; }
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
            _window=next;_notice=true;
        }
        return kWorldInputHandled;
    }
    bool save(Common::WriteStream &out) const {
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
        uint version=in.readUint16LE();if (version<1 || version>9) return false;
        WorldInput next;
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
        const WorldWaitResult result=_wait.run(candidate,nextWindow,nextScene,nextTurn,clock,nextLight,mask,schedule,nextRoutes,after,catalog,nextText,image,game.appearance,animation,encounter,true,4,nullptr,&party);
        if (result!=kWorldWaitAdvanced && result!=kWorldWaitBattle && result!=kWorldWaitDefeat) return kWorldInputBoundary;
        game.world.adoptState(candidate);game.party=party;game.clock=clock;game.schedule=schedule;window=nextWindow;scene=nextScene;turn=nextTurn;
        routes=nextRoutes;light=nextLight;visibility=mask;random=after;text=nextText;frame=image;_transport=controls;_fireDirection=false;
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
        World candidate(game.world);NpcScheduleState schedule=game.schedule;WorldStory nextWindow;WorldScene nextScene;
        WorldActorTurnState nextTurn=turn;LightState nextLight=light;VisibilityMask nextVisibility=visibility;HgrFrame nextFrame=frame;
        WorldTransportState nextTransport=_transport;
        WorldNavigation navigation=game.navigation;
        if (recordOrigin && !navigation.enter(NavigationPoint(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y()),plan.type)) return false;
        if (!WorldTravel::commit(candidate,plan,&nextTransport)) return false;
        nextTurn.presentation.partyObject=nextTransport.object;
        if (!nextWindow.initialize(candidate,candidate.currentTables()->data()+1536,256) || !loadRules(archive,plan.type)) return false;
        // M:FB0C selects permanent dark maps, otherwise the source day/night
        // boundary5:30..20:30. Preserve the light level/transition counter.
        Common::Array<byte> data;if (!archive.read("NA/GAME",data)) return false;
        if (innRest) nextLight.mode=1;
        else if (!WorldTravelRules::lightMode(plan.type,plan.map,game.clock.hour,game.clock.minute,data.data()+0x10b1,data.size()-0x10b1,nextLight.mode)) return false;
        byte painted[187]={};
        if (!WorldViewport::refreshAtWorld(candidate,nextWindow,_story,_actors,_lighting,_occlusion,_painter,game.appearance,game.clock.lightActive,
                animation,painted,nextTurn.presentation,nextLight,nextVisibility,nextFrame,&nextScene)) return false;
        // M:F69B refresh precedes F69E map-entry schedule. Capture its actual
        // resulting records for persistence without replaying actor admission.
        NpcSchedulePolicy policy={false,false};
        if (NpcScheduler::apply(candidate,game.clock.hour,game.clock.minute,policy,schedule)==kNpcScheduleUnsupported || !nextWindow.captureRecords(candidate)) return false;
        TurnClock nextClock=game.clock;
        if (innRest) {
            byte hours;
            if (!InnService::sleep(nextClock,hours) || !WorldTravelRules::lightMode(plan.type,plan.map,nextClock.hour,nextClock.minute,
                    data.data()+0x10b1,data.size()-0x10b1,nextLight.mode)) return false;
        }
        game.world.adoptState(candidate);game.schedule=schedule;window=nextWindow;scene=nextScene;turn=nextTurn;light=nextLight;visibility=nextVisibility;frame=nextFrame;
        game.clock=nextClock;game.navigation=navigation;_transport=nextTransport;
        return true;
    }
    static bool message(const FixedMessageCatalog &catalog,byte id,TextGlyphSurface &surface,HgrFrame &frame) {
        if (!surface.importPages(frame.data(),frame.data(),8192) || !showFixedMessageStrip(catalog,id,surface)) return false;
        for (uint y=184;y<192;++y) for (uint x=7;x<32;++x)
            if (!frame.putByte(x,y,surface.page(0)[HgrFrame::rowOffset(y)+x])) return false;
        return true;
    }
    bool _notice,_openDirection,_leave,_fireDirection,_fireProtected,_scriptedEncounter;TextWindow _window;HgrFrame _underlay;
    WorldActorTurnEvent _encounter;WorldConversation _conversation;WorldTransportState _transport;InventoryWindow _inventory;WorldCast _casting;
    WorldMove _move;WorldOpen _open;WorldWait _wait;StoryMapPatches _story;ActorPaintRules _actors;TerrainLighting _lighting;OcclusionRules _occlusion;TilePainter _painter;
};
} }
#endif
