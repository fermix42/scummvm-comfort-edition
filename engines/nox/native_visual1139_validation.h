/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_VISUAL1139_VALIDATION_H
#define NOX_NATIVE_VISUAL1139_VALIDATION_H
#include "nox/native_player_session.h"
namespace Nox {
inline bool validateNativeVisual1139(HdvArchive &archive,Common::SeekableReadStream &input,Common::WriteStream &images,Common::String &report) {
    using namespace Native;
#define VIS1139(x) do {if (!(x)) {report=Common::String::format("Visual1139 failed at line%u",uint(__LINE__));return false;}} while (0)
    VIS1139(input.readUint32BE()==MKTAG('V','I','S','9') && input.readUint16LE()==1);const uint cases=input.readUint16LE();VIS1139(cases==3);
    uint masked=0,frames=0;
    const auto black=[&](const HgrFrame &frame,const VisibilityMask &mask) -> bool {
        for (uint cell=0;cell<187;++cell) if (mask.cells()[cell]) for (uint y=0;y<16;++y) for (uint x=0;x<2;++x)
            if (frame.data()[HgrFrame::rowOffset(8+cell/17*16+y)+cell%17*2+x]) return false;return true;
    };
    images.writeUint32BE(MKTAG('V','I','M','9'));images.writeUint16LE(cases);
    for (uint n=0;n<cases;++n) {
        const uint size=input.readUint32LE();VIS1139(size && size<2000000 && size<=input.size()-input.pos());Common::Array<byte> bytes;bytes.resize(size);VIS1139(input.read(bytes.data(),size)==size);
        Common::MemoryReadStream fixture(bytes.data(),bytes.size());Common::ScopedPtr<PlayerSessionState> owner(new PlayerSessionState(archive));VIS1139(owner->stageLoad(fixture));owner->commitLoad();
        VIS1139(owner->game.ready() && owner->worldInput.atWorldCommand() && !owner->encounterState.formed);
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES);VIS1139(owner->game.save(before));
        Common::ScopedPtr<World> world(new World(owner->game.world));WorldStory window=owner->window;PresentationState presentation=owner->turns.presentation;
        LightState light=owner->light;VisibilityMask visibility=owner->visibility;WorldScene scene;HgrFrame frame=owner->frame;
        VIS1139(WorldViewport::refreshAtWorld(*world,window,owner->story,owner->actors,owner->lighting,owner->occlusion,owner->painter,
            owner->game.appearance,owner->game.clock.lightActive,owner->animationState.phase,owner->visibility.cells(),presentation,light,visibility,frame,&scene));
        VIS1139(black(frame,visibility));
        VIS1139(owner->status.paint(owner->game,scene,light,false,frame));
        images.writeByte(world->mapType());images.writeByte(world->mapId());images.writeByte(world->x());images.writeByte(world->y());images.write(visibility.cells(),187);images.write(frame.data(),8192);
        Common::ScopedPtr<WorldAnimationState> state(new WorldAnimationState(owner->animationState));VIS1139(state->synchronize(frame));
        WorldAnimationContext context;context.appearance=owner->game.appearance;context.partyObject=presentation.partyObject;context.speed=owner->worldInput.transportSpeed();
        // Contaminate only a hidden raster pixel in each retained image. The
        // source-derived mask must dominate redraws on every physical target.
        uint hidden=187;for (uint cell=0;cell<187;++cell) if (visibility.cells()[cell]) {++masked;hidden=cell;}
        VIS1139(hidden<187);
        for (uint page=0;page<2;++page) VIS1139(state->pages[page].putByte((hidden%17)*2,8+(hidden/17)*16,127));
        WorldAnimation animation;VIS1139(animation.load(archive,world->mapType()));byte recovery=owner->turns.terrainRecovery;
        for (uint tick=0;tick<16;++tick) {
            Common::Array<WorldAnimationDraw> draws;VIS1139(animation.run(*world,scene,visibility.cells(),187,light,context,*state,draws,nullptr,&recovery));
            VIS1139(black(state->pages[state->target^1],visibility));++frames;
        }
        VIS1139(owner->game.save(after) && before.size()==after.size() && !memcmp(before.getData(),after.getData(),before.size()));
        // Use actual input and the authored fixed-message catalog for Open.
        FixedMessageCatalog messages;TextGlyphSurface text;VIS1139(messages.load(archive) && text.initialize(archive,1) && text.importPages(frame.data(),frame.data(),8192));
        Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_o,'O');
        const WorldInputResult opened=owner->worldInput.handle(event,archive,owner->game,owner->window,owner->prepared,owner->turns,owner->light,owner->visibility,owner->routes,owner->random,messages,text,frame,0);
        VIS1139(opened==kWorldInputHandled && owner->worldInput.selectingOpen());
        TextGlyphSurface expected;VIS1139(expected.initialize(archive,1) && expected.importPages(frame.data(),frame.data(),8192) && showFixedMessageStrip(messages,12,expected));
        for (uint y=184;y<192;++y) VIS1139(!memcmp(frame.data()+HgrFrame::rowOffset(y)+7,expected.page(0)+HgrFrame::rowOffset(y)+7,25));
    }
    VIS1139(input.pos()==input.size() && !input.err() && !images.err());
    report=Common::String::format("Visual1139 PASS\tcases=%u\thidden_cells=%u\tmasked_animation_frames=%u\tfull_refresh=PASS\tOpen_prompt=PASS\tgameplay_unchanged=PASS\tinterval_ms=%u\n",cases,masked,frames,uint(WorldAnimation::kIntervalMs));
#undef VIS1139
    return true;
}
}
#endif
