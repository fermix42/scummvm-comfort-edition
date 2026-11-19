/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PLAYER_SESSION_H
#define NOX_NATIVE_PLAYER_SESSION_H
#include "nox/nox.h"
#include "nox/native_character_creation.h"
#include "nox/native_exploration_save.h"
#include "nox/native_world_status.h"
#include "nox/native_opening_backdrop.h"
#include "nox/native_battle_presentation.h"
#include "nox/native_audio.h"
#include "nox/native_legacy_snapshot.h"
#include "common/system.h"
namespace Nox { namespace Native {
// One native player world and its continuations. This is deliberately separate
// from the temporary compatibility Runtime and its machine-image serializer.
class PlayerSessionState : public NativePlayerSession {
public:
    explicit PlayerSessionState(HdvArchive &archive) : game(archive),light(1,0,0),redraw(true),sceneReady(false),storyStarted(false),
        animationReady(false),encounterPrepared(false),storyAnimation(2),nextStoryAnimation(0),lastAnimationTick(0),lastAudioTick(0),_archive(archive),_staged(nullptr),_stagedLegacy(false) {}
    ~PlayerSessionState() override {delete _staged;}
    bool canSave() const override {return sceneReady && animationReady && game.ready() && ((!encounterState.formed && worldInput.atWorldCommand()) || (encounterState.presented && (encounterState.resultPhase==4 || encounterState.resultPhase==5)));}
    bool save(Common::WriteStream &out) const override {
        if (!canSave()) return false;
        Common::MemoryWriteStreamDynamic world(DisposeAfterUse::YES),audio(DisposeAfterUse::YES);
        if (!ExplorationSave::save(world,game,window,prepared,turns,routes,random,text,frame,visibility,light,worldInput,animationState,encounterState) || !music.save(audio)) return false;
        out.writeUint32BE(MKTAG('N','P','L','S'));out.writeUint16LE(1);out.writeUint32LE(world.size());out.write(world.getData(),world.size());out.writeUint32LE(audio.size());out.write(audio.getData(),audio.size());return !out.err();
    }
    bool stageLoad(Common::SeekableReadStream &in) override {
        discardLoad();PlayerSessionState *next=new PlayerSessionState(_archive);
        Common::Array<byte> state;const int64 begin=in.pos();const uint32 magic=in.readUint32BE();in.seek(begin);
        if (magic==0x01000000 || magic==0x02000000) {
            if (!next->importLegacy(in)) {delete next;return false;}
            _staged=next;_stagedLegacy=true;return true;
        }
        if (magic==MKTAG('N','P','L','S')) {
            in.skip(4);if (in.readUint16LE()!=1) {delete next;return false;}
            const uint size=in.readUint32LE();if (!size || size>18*1024*1024+28 || size>in.size()-in.pos()) {delete next;return false;}
            state.resize(size);if (in.read(state.data(),size)!=size) {delete next;return false;}
            const uint audioSize=in.readUint32LE();if (!audioSize || audioSize>32768 || audioSize!=in.size()-in.pos()) {delete next;return false;}
            Common::Array<byte> bytes;bytes.resize(audioSize);if (in.read(bytes.data(),audioSize)!=audioSize) {delete next;return false;}
            Common::MemoryReadStream audio(bytes.data(),audioSize);if (!next->music.load(audio,_archive)) {delete next;return false;}
        } else {
            const int64 size=in.size()-in.pos();if (size<1 || size>18*1024*1024+28) {delete next;return false;}state.resize(uint(size));if (in.read(state.data(),state.size())!=state.size()) {delete next;return false;}
        }
        Common::MemoryReadStream gameplay(state.data(),state.size());
        if (!ExplorationSave::load(gameplay,_archive,next->game,next->window,next->prepared,next->turns,next->routes,next->random,next->text,
                next->frame,next->visibility,next->light,next->worldInput,next->animationState,next->encounterState,magic==MKTAG('N','P','L','S')?nullptr:&next->music) ||
            !next->creation.adoptReadyGame(next->game,_archive) || !next->loadRules(next->game.world.mapType())) {delete next;return false;}
        if (next->worldInput.encounter().battle) {
            if (!EncounterPreparation::prepare(_archive,next->game,next->prepared,next->turns,next->worldInput.encounter(),next->preparedEncounter,next->worldInput.scriptedEncounter()) || !next->encounterState.formed) {delete next;return false;}
            next->encounterPrepared=true;
        }
        if (!next->encounterState.presented && next->worldInput.atWorldCommand()) {
            // Refresh only status chrome in older saves; retain both map images
            // and their independently owned animation phases/history.
            if (!next->status.paintStatus(next->game.gold,next->game.food,next->game.clock,next->frame)) {delete next;return false;}
            for (uint page=0;page<2;++page) if (!next->status.paintStatus(next->game.gold,next->game.food,next->game.clock,next->animationState.pages[page])) {delete next;return false;}
        }
        next->sceneReady=next->storyStarted=next->animationReady=true;next->redraw=true;_staged=next;return true;
    }
    bool stagedLegacy() const override {return _staged && _stagedLegacy;}
    void commitLoad() override {
        if (!_staged) return;adopt(*_staged);discardLoad();resetPresentationClock();
    }
    void discardLoad() override {delete _staged;_staged=nullptr;_stagedLegacy=false;}
    void resetPresentationClock() override {lastAudioTick=lastAnimationTick=g_system->getMillis();if (quickDisplay.holding) quickDisplay.stamp=lastAnimationTick;}
    bool reset() override {
        discardLoad();PlayerSessionState *next=new PlayerSessionState(_archive);
        if (!next->loadRules(8)) {delete next;return false;}adopt(*next);delete next;lastAnimationTick=g_system->getMillis();return true;
    }
    bool render(Graphics::Surface &surface) const override {
        if (!sceneReady || surface.w!=560 || surface.h!=384 || surface.format.bytesPerPixel!=4) return false;
        Common::Array<byte> rgb;(encounterState.presented?encounterState.frame:frame).renderRGB(rgb);
        for (uint y=0;y<384;++y) for (uint x=0;x<560;++x) {
            const byte *pixel=rgb.data()+(y*560+x)*3;
            *static_cast<uint32 *>(surface.getBasePtr(x,y))=surface.format.RGBToColor(pixel[0],pixel[1],pixel[2]);
        }
        return true;
    }
    uint positionX() const override {return game.world.ready()?game.world.x():0;}
    uint positionY() const override {return game.world.ready()?game.world.y():0;}

    NewGame game;CharacterCreation creation;WorldStatus status;
    WorldStory window;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;
    WorldScene prepared;WorldActorTurnState turns;WorldWait wait;NpcRouteStore routes;GameplayRandom random;
    TextGlyphSurface text;HgrFrame frame;VisibilityMask visibility;LightState light;WorldInput worldInput;
    WorldAnimation animation;WorldAnimationState animationState;PreparedEncounter preparedEncounter;EncounterState encounterState;
    BattlePresentation battlePresentation;TextWindow storyWindow;OpeningBackdrop storyBackdrop;
    MusicPlayback music;QuickBattleDisplay quickDisplay;
    bool redraw,sceneReady,storyStarted,animationReady,encounterPrepared;byte storyAnimation,nextStoryAnimation;uint32 lastAnimationTick,lastAudioTick;
private:
    bool importLegacy(Common::SeekableReadStream &in) {
        Common::ScopedPtr<LegacySnapshotData> legacy(new LegacySnapshotData);Common::String reason;
        if (!legacy->read(in) || !legacy->importCore(_archive,game,reason)) return false;
        const byte *main=legacy->mainData().data(),*aux=legacy->auxiliaryData().data();
        if (!loadRules(game.world.mapType()) || !creation.adoptReadyGame(game,_archive) ||
            !routes.importPages(aux+0x5c00,3584) || !random.importState(READ_LE_UINT32(main+0x6307),main[0x6306],main[0x6300],READ_LE_UINT16(main+0x27f8))) return false;
        WorldTransportState transport;transport.used=true;
        transport.object=main[0x377f];transport.appearance=main[0x367d];transport.baseAppearance=main[0x60dc];transport.statusAppearance=main[0x3f7f];
        transport.speed=main[0x367e];transport.pace=main[0x37f8];transport.special=main[0x37fa];transport.allocator=main[0x60c3];transport.waterExposure=main[0x612c];transport.alarm=main[0x257f]!=0;transport.jumpRequested=main[0x29f9]!=0;
        if (!worldInput.restoreWorldControls(game.world,transport)) return false;
        if (main[0x6113]<=5) {turns.praevocCursor=main[0x6113];turns.praevocUsed=true;}
        if (main[0x6103]<=16) turns.terrainRecovery=main[0x6103];
        turns.population.pending=main[0x3778]!=0;
        turns.population.action=main[0x60f3]==139?0:main[0x60f3]==138?1:main[0x60f3]==149?2:main[0x60f3]==136?3:main[0x60f3]?4:5;
        turns.population.stormAttempts=main[0x367a];
        if (main[0x60ab]>=100 && !(main[0x60ab]&3)) turns.population.cursor=main[0x60ab];
        if (main[0x6e5f]<4) turns.population.monsterChoice=main[0x6e5f];
        turns.presentation.facing=main[0x257e];turns.presentation.partyObject=transport.object;turns.presentation.hidden=main[0x247d]!=0;
        if(turns.presentation.facing>3) return false;
        turns.compact.freezeTimer=turns.npc.freezeTimer=main[0x68b1];turns.compact.enabled=turns.npc.enabled=main[0x38fc];
        turns.effects.active=main[0x37f9];turns.effects.queued=main[0x608b];turns.effects.code=main[0x608c];
        light=LightState(main[0x387d],main[0x2a7d],main[0x2a7f]);
        if(light.mode>3 || light.level>4 || !visibility.loadRules(_archive) || !visibility.importMask(main+0x8bb,187) ||
            !status.initializeFrame(frame) || !text.initialize(_archive,1) || !window.initialize(game.world,game.world.currentTables()->data()+1536,256)) return false;
        byte previous[187];memcpy(previous,main+0x8bb,187);
        // Scene preparation settles doors and rotates actors. Reconstruct its
        // raster on a presentation-only copy; loading cannot spend that phase.
        Common::ScopedPtr<World> renderWorld(new World(game.world));PresentationState renderPresentation=turns.presentation;
        LightState renderLight=light;VisibilityMask renderVisibility=visibility;
        if (!WorldViewport::refreshAtWorld(*renderWorld,window,story,actors,lighting,occlusion,painter,game.appearance,game.clock.lightActive,0,previous,renderPresentation,renderLight,renderVisibility,frame,&prepared) || !window.captureRecords(game.world)) return false;
        // The saved quiescent source lookups describe the unadvanced world.
        memcpy(prepared.actors.terrain,main+0x800,187);memcpy(prepared.objects.lookup,main+0x450,187);
        memcpy(prepared.actors.lookup,main+0xa31,187);memcpy(prepared.actors.kinds,main+0xaec,187);
        prepared.objects.portal=main[0x347b];prepared.actors.hiddenByTurn=turns.presentation.hidden;
        if (prepared.objects.portal!=255 && (prepared.objects.portal&3)) return false;
        if (!status.paint(game,prepared,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192) || !animationState.synchronize(frame)) return false;
        // Restart presentation audio; this must not draw from gameplay RNG.
        GameplayRandom presentationRandom=random;
        if (!music.splash(_archive) || !music.ambient(_archive,presentationRandom,game.world.mapType(),game.world.mapId())) return false;
        sceneReady=storyStarted=animationReady=true;redraw=true;return true;
    }
    bool loadRules(byte type) {
        return status.load(_archive,type) && story.load(_archive) && actors.load(_archive,type) && lighting.load(_archive,type) &&
            occlusion.load(_archive,type) && painter.load(_archive,type) && wait.load(_archive,type) && animation.load(_archive,type) &&
            battlePresentation.load(_archive,type) && worldInput.loadRules(_archive,type);
    }
    void adopt(const PlayerSessionState &next) {
        game.adoptState(next.game);creation=next.creation;status=next.status;window=next.window;story=next.story;actors=next.actors;
        lighting=next.lighting;occlusion=next.occlusion;painter=next.painter;prepared=next.prepared;turns=next.turns;wait=next.wait;
        routes=next.routes;random=next.random;text=next.text;frame=next.frame;visibility=next.visibility;light=next.light;
        worldInput=next.worldInput;animation=next.animation;animationState=next.animationState;preparedEncounter=next.preparedEncounter;
        encounterState=next.encounterState;battlePresentation=next.battlePresentation;storyWindow=next.storyWindow;storyBackdrop=next.storyBackdrop;
        redraw=next.redraw;sceneReady=next.sceneReady;storyStarted=next.storyStarted;animationReady=next.animationReady;
        encounterPrepared=next.encounterPrepared;storyAnimation=next.storyAnimation;nextStoryAnimation=next.nextStoryAnimation;
        music=next.music;quickDisplay.reset();resetPresentationClock();
    }
    HdvArchive &_archive;PlayerSessionState *_staged;bool _stagedLegacy;
};
} }
#endif
