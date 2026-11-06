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
#include "common/system.h"
namespace Nox { namespace Native {
// One native player world and its continuations. This is deliberately separate
// from the temporary compatibility Runtime and its machine-image serializer.
class PlayerSessionState : public NativePlayerSession {
public:
    explicit PlayerSessionState(HdvArchive &archive) : game(archive),light(1,0,0),redraw(true),sceneReady(false),storyStarted(false),
        animationReady(false),encounterPrepared(false),storyAnimation(2),nextStoryAnimation(0),lastAnimationTick(0),lastAudioTick(0),_archive(archive),_staged(nullptr) {}
    ~PlayerSessionState() override {delete _staged;}
    bool canSave() const override {return sceneReady && animationReady && game.ready() && ((!encounterState.formed && worldInput.atWorldCommand()) || (encounterState.presented && encounterState.resultPhase==4));}
    bool save(Common::WriteStream &out) const override {
        if (!canSave()) return false;
        Common::MemoryWriteStreamDynamic world(DisposeAfterUse::YES),audio(DisposeAfterUse::YES);
        if (!ExplorationSave::save(world,game,window,prepared,turns,routes,random,text,frame,visibility,light,worldInput,animationState,encounterState) || !music.save(audio)) return false;
        out.writeUint32BE(MKTAG('N','P','L','S'));out.writeUint16LE(1);out.writeUint32LE(world.size());out.write(world.getData(),world.size());out.writeUint32LE(audio.size());out.write(audio.getData(),audio.size());return !out.err();
    }
    bool stageLoad(Common::SeekableReadStream &in) override {
        discardLoad();PlayerSessionState *next=new PlayerSessionState(_archive);
        Common::Array<byte> state;const int64 begin=in.pos();const uint32 magic=in.readUint32BE();in.seek(begin);
        if (magic==MKTAG('N','P','L','S')) {
            in.skip(4);if (in.readUint16LE()!=1) {delete next;return false;}
            const uint size=in.readUint32LE();if (!size || size>2097180 || size>in.size()-in.pos()) {delete next;return false;}
            state.resize(size);if (in.read(state.data(),size)!=size) {delete next;return false;}
            const uint audioSize=in.readUint32LE();if (!audioSize || audioSize>32768 || audioSize!=in.size()-in.pos()) {delete next;return false;}
            Common::Array<byte> bytes;bytes.resize(audioSize);if (in.read(bytes.data(),audioSize)!=audioSize) {delete next;return false;}
            Common::MemoryReadStream audio(bytes.data(),audioSize);if (!next->music.load(audio,_archive)) {delete next;return false;}
        } else {
            const int64 size=in.size()-in.pos();if (size<1 || size>2097180) {delete next;return false;}state.resize(uint(size));if (in.read(state.data(),state.size())!=state.size()) {delete next;return false;}
        }
        Common::MemoryReadStream gameplay(state.data(),state.size());
        if (!ExplorationSave::load(gameplay,_archive,next->game,next->window,next->prepared,next->turns,next->routes,next->random,next->text,
                next->frame,next->visibility,next->light,next->worldInput,next->animationState,next->encounterState,magic==MKTAG('N','P','L','S')?nullptr:&next->music) ||
            !next->creation.adoptReadyGame(next->game,_archive) || !next->loadRules(next->game.world.mapType())) {delete next;return false;}
        if (next->worldInput.encounter().battle) {
            if (!EncounterPreparation::prepare(_archive,next->game,next->prepared,next->turns,next->worldInput.encounter(),next->preparedEncounter,next->worldInput.scriptedEncounter()) || !next->encounterState.formed) {delete next;return false;}
            next->encounterPrepared=true;
        }
        next->sceneReady=next->storyStarted=next->animationReady=true;next->redraw=true;_staged=next;return true;
    }
    void commitLoad() override {
        if (!_staged) return;adopt(*_staged);discardLoad();resetPresentationClock();
    }
    void discardLoad() override {delete _staged;_staged=nullptr;}
    void resetPresentationClock() override {lastAudioTick=lastAnimationTick=g_system->getMillis();}
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
    MusicPlayback music;
    bool redraw,sceneReady,storyStarted,animationReady,encounterPrepared;byte storyAnimation,nextStoryAnimation;uint32 lastAnimationTick,lastAudioTick;
private:
    bool loadRules(byte type) {
        return status.load(_archive,type) && story.load(_archive) && actors.load(_archive,type) && lighting.load(_archive,type) &&
            occlusion.load(_archive,type) && painter.load(_archive,type) && wait.load(_archive,type) && animation.load(_archive) &&
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
        music=next.music;resetPresentationClock();
    }
    HdvArchive &_archive;PlayerSessionState *_staged;
};
} }
#endif
