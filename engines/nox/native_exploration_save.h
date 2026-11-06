/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_EXPLORATION_SAVE_H
#define NOX_NATIVE_EXPLORATION_SAVE_H
#include "nox/native_new_game.h"
#include "nox/native_world_actor_turn.h"
#include "nox/native_lighting.h"
#include "nox/native_world_input.h"
#include "nox/native_world_animation.h"
#include "nox/native_encounter_state.h"
#include "nox/native_audio.h"
#include "common/md5.h"
namespace Nox { namespace Native {
// Versioned semantic exploration checkpoint. This preserves the admitted world
// phase and initial battle construction/presentation, not a CPU image. Tactical
// commands/dialogue, sessions/assistance and audio remain outside this partial
// contract; this is not the final game-save contract.
class ExplorationSave {
public:
    static bool save(Common::WriteStream &out,const NewGame &game,const WorldStory &window,const WorldScene &scene,
            const WorldActorTurnState &turn,const NpcRouteStore &routes,const GameplayRandom &random,const TextGlyphSurface &text,
            const HgrFrame &frame,const VisibilityMask &visibility,const LightState &light,const WorldInput &input,const WorldAnimationState &animation,const EncounterState &battle,const MusicPlayback *audio=nullptr) {
        if (!game.ready() || !input.matchesWorld(game.world) || input.transportObject()!=turn.presentation.partyObject || !window.matchesWorld(game.world) || !visibility.ready() || light.mode>3 || light.level>4) return false;
        if (!animation.valid() || memcmp(frame.data(),animation.pages[animation.target^1].data(),8192)) return false;
        Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),w(DisposeAfterUse::YES),t(DisposeAfterUse::YES),r(DisposeAfterUse::YES),g(DisposeAfterUse::YES),x(DisposeAfterUse::YES),f(DisposeAfterUse::YES),s(DisposeAfterUse::YES),u(DisposeAfterUse::YES),a(DisposeAfterUse::YES),b(DisposeAfterUse::YES),payload(DisposeAfterUse::YES);
        if (!game.save(core) || !window.save(w) || !turn.save(t) || !routes.save(r) || !random.save(g) || !text.save(x) || !input.save(u) || !animation.save(a)) return false;
        f.write(frame.data(),8192);s.write(scene.actors.terrain,187);s.write(scene.objects.lookup,187);s.write(scene.actors.lookup,187);s.write(scene.actors.kinds,187);
        s.writeByte(scene.objects.portal);s.writeByte(scene.actors.hiddenByTurn);s.write(visibility.cells(),187);s.writeByte(light.mode);s.writeByte(light.level);s.writeByte(light.transitionTicks);
        if (battle.formed!=input.encounter().battle || !battle.save(b)) return false;
        const uint32 tags[]={MKTAG('C','O','R','E'),MKTAG('W','I','N','D'),MKTAG('T','U','R','N'),MKTAG('R','O','U','T'),MKTAG('R','A','N','D'),MKTAG('T','E','X','T'),MKTAG('F','R','A','M'),MKTAG('S','C','E','N'),MKTAG('I','N','P','T'),MKTAG('A','N','I','M'),MKTAG('B','T','T','L')};
        Common::MemoryWriteStreamDynamic *const parts[]={&core,&w,&t,&r,&g,&x,&f,&s,&u,&a,&b};
        for (uint i=0;i<11;++i) { if (parts[i]->err()) return false;payload.writeUint32BE(tags[i]);payload.writeUint32LE(parts[i]->size());payload.write(parts[i]->getData(),parts[i]->size()); }
        if (audio && audio->ready()) {
            Common::MemoryWriteStreamDynamic sound(DisposeAfterUse::YES);
            if (!audio->save(sound) || sound.err()) return false;
            payload.writeUint32BE(MKTAG('A','U','D','I'));payload.writeUint32LE(sound.size());payload.write(sound.getData(),sound.size());
        }
        if (payload.err() || payload.size()>2097152) return false;
        byte digest[16];Common::MemoryReadStream check(payload.getData(),payload.size());if (!Common::computeStreamMD5(check,digest)) return false;
        out.writeUint32BE(MKTAG('N','P','X','S'));out.writeUint16LE(4);out.writeUint16LE(119);out.writeUint32LE(payload.size());out.write(digest,16);out.write(payload.getData(),payload.size());return !out.err();
    }
    static bool load(Common::SeekableReadStream &in,HdvArchive &archive,NewGame &game,WorldStory &window,WorldScene &scene,
            WorldActorTurnState &turn,NpcRouteStore &routes,GameplayRandom &random,TextGlyphSurface &text,
            HgrFrame &frame,VisibilityMask &visibility,LightState &light,WorldInput &input,WorldAnimationState &animation,EncounterState &battle,MusicPlayback *audio=nullptr) {
        if (in.size()-in.pos()<28 || in.readUint32BE()!=MKTAG('N','P','X','S')) return false;
        const uint16 version=in.readUint16LE();if ((version!=3 && version!=4) || in.readUint16LE()!=119) return false;
        const uint32 length=in.readUint32LE();byte expected[16],digest[16];
        if (!length || length>2097152 || in.read(expected,16)!=16 || int64(length)!=in.size()-in.pos()) return false;
        Common::Array<byte> payload;payload.resize(length);if (in.read(payload.data(),length)!=length || in.err()) return false;
        Common::MemoryReadStream check(payload.data(),length);if (!Common::computeStreamMD5(check,digest) || memcmp(expected,digest,16)) return false;
        NewGame next(archive);WorldStory nextWindow;WorldScene nextScene;WorldActorTurnState nextTurn;NpcRouteStore nextRoutes;
        GameplayRandom nextRandom;TextGlyphSurface nextText;HgrFrame nextFrame;VisibilityMask nextVisibility;LightState nextLight;WorldInput nextInput;WorldAnimationState nextAnimation;EncounterState nextBattle;
        if (!nextVisibility.loadRules(archive)) return false;
        const uint32 tags[]={MKTAG('C','O','R','E'),MKTAG('W','I','N','D'),MKTAG('T','U','R','N'),MKTAG('R','O','U','T'),MKTAG('R','A','N','D'),MKTAG('T','E','X','T'),MKTAG('F','R','A','M'),MKTAG('S','C','E','N'),MKTAG('I','N','P','T'),MKTAG('A','N','I','M'),MKTAG('B','T','T','L')};
        Common::MemoryReadStream source(payload.data(),length);
        for (uint i=0;i<(version==4?11U:10U);++i) {
            if (source.size()-source.pos()<8 || source.readUint32BE()!=tags[i]) return false;
            const uint32 size=source.readUint32LE();if (!size || size>1048576 || int64(size)>source.size()-source.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (source.read(bytes.data(),size)!=size) return false;Common::MemoryReadStream part(bytes.data(),size);
            if (i==0) { if (!next.load(part)) return false; }
            else if (i==1) { if (!nextWindow.load(part,next.world)) return false; }
            else if (i==2) { if (!nextTurn.load(part)) return false; }
            else if (i==3) { if (!nextRoutes.load(part)) return false; }
            else if (i==4) { if (!nextRandom.load(part)) return false; }
            else if (i==5) { if (!nextText.load(part,archive)) return false; }
            else if (i==6) { if (size!=8192 || !nextFrame.importImage(bytes.data(),size)) return false; }
            else if (i==8) { if (!nextInput.load(part,archive,next.world.mapType())) return false; }
            else if (i==9) { if (!nextAnimation.load(part)) return false; }
            else if (i==10) { if (!nextBattle.load(part)) return false; }
            else {
                if (size!=940 || part.read(nextScene.actors.terrain,187)!=187 || part.read(nextScene.objects.lookup,187)!=187 || part.read(nextScene.actors.lookup,187)!=187 || part.read(nextScene.actors.kinds,187)!=187) return false;
                nextScene.objects.portal=part.readByte();const byte hidden=part.readByte();byte cells[187];if (hidden>1 || part.read(cells,187)!=187) return false;
                if (nextScene.objects.portal!=255 && (nextScene.objects.portal&3)) return false;
                for (uint cell=0;cell<187;++cell) if (cells[cell]>1 || (nextScene.objects.lookup[cell]!=255 && (nextScene.objects.lookup[cell]&3)) || (nextScene.actors.lookup[cell]!=255 && (nextScene.actors.lookup[cell]&3))) return false;
                nextScene.actors.hiddenByTurn=hidden!=0;nextLight.mode=part.readByte();nextLight.level=part.readByte();nextLight.transitionTicks=part.readByte();
                if (part.err() || nextLight.mode>3 || nextLight.level>4 || !nextVisibility.importMask(cells,187)) return false;
            }
        }
        MusicPlayback nextAudio;
        if (source.pos()!=source.size()) {
            if (!audio || version!=4 || source.size()-source.pos()<8 || source.readUint32BE()!=MKTAG('A','U','D','I')) return false;
            const uint32 size=source.readUint32LE();if (!size || size>32768 || int64(size)!=source.size()-source.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (source.read(bytes.data(),size)!=size) return false;
            Common::MemoryReadStream sound(bytes.data(),size);if (!nextAudio.load(sound,archive) || !nextAudio.ready()) return false;
        }
        if (source.err() || source.pos()!=source.size() || !nextInput.matchesWorld(next.world) || nextInput.transportObject()!=nextTurn.presentation.partyObject || nextTurn.presentation.hidden!=nextScene.actors.hiddenByTurn || memcmp(nextFrame.data(),nextAnimation.pages[nextAnimation.target^1].data(),8192)) return false;
        if (version==4 && nextBattle.formed!=nextInput.encounter().battle) return false;
        if (version==3 && nextInput.encounter().battle) {
            PreparedEncounter encounter;
            if (!EncounterPreparation::prepare(archive,next,nextScene,nextTurn,nextInput.encounter(),encounter,nextInput.scriptedEncounter()) || !nextBattle.begin(archive,next,encounter,nextRandom)) return false;
        }
        game.adoptState(next);window=nextWindow;scene=nextScene;turn=nextTurn;routes=nextRoutes;random=nextRandom;text=nextText;frame=nextFrame;visibility=nextVisibility;light=nextLight;input=nextInput;animation=nextAnimation;battle=nextBattle;if (audio) *audio=nextAudio;return true;
    }
};
} }
#endif
