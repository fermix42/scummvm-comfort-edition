/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_DIALOGUE_VOICE_VALIDATION_H
#define NOX_NATIVE_DIALOGUE_VOICE_VALIDATION_H
#include "nox/native_world_input.h"
#include "nox/native_world_status.h"
#include "nox/native_information_catalog.h"
namespace Nox { namespace Native {
inline bool validateNativeDialogueVoice498(HdvArchive &archive,Common::SeekableReadStream &fixture,Common::String &report) {
    if (fixture.readUint32BE()!=MKTAG('N','V','4','8') || fixture.readUint32LE()!=1) return false;
    Common::String replies[3];for (uint mode=0;mode<3;++mode) {
        const uint length=fixture.readUint16LE();if (!length || length>4096 || int64(length)>fixture.size()-fixture.pos()) return false;
        for (uint at=0;at<length;++at) {const byte c=fixture.readByte();if (c!=10 && (c<32 || c>=127)) return false;replies[mode]+=char(c);}
    }
    if (fixture.err() || fixture.pos()!=fixture.size()) return false;
    report="voice498 actual Town9/Bridgit checkpoint setup";
    Common::ScopedPtr<NewGame> owner(new NewGame(archive));NewGame &game=*owner;if (!game.create("VOICE",0,0)) return false;
    game.clock.hour=9;game.clock.minute=0;
    Common::ScopedPtr<WorldInput> command(new WorldInput);WorldStory window;WorldScene scene;WorldActorTurnState turn;WorldStatus status;StoryMapPatches story;ActorPaintRules actors;TerrainLighting lighting;OcclusionRules occlusion;TilePainter painter;TraversalRules traversal;
    LightState light(1,0,0);VisibilityMask visible;NpcRouteStore routes;GameplayRandom random;FixedMessageCatalog messages;TextGlyphSurface text;HgrFrame frame;byte previous[187]={},emptyRoutes[3584]={};
    if (!status.load(archive,6) || !status.initializeFrame(frame) || !story.load(archive) || !actors.load(archive,6) || !lighting.load(archive,6) || !occlusion.load(archive,6) || !painter.load(archive,6) || !traversal.load(archive,6) || !visible.loadRules(archive) || !routes.importPages(emptyRoutes,3584) || !random.initializeEntropy(119) || !messages.load(archive) || !text.initialize(archive,1) || !command->loadRules(archive,6)) return false;
    const int dx[]={0,0,-1,1},dy[]={1,-1,0,0};const byte near[]={76,110,94,92};byte direction=255;
    for (uint candidate=0;candidate<4;++candidate) {
        World setup(archive);if (!setup.initialize(6,9,byte(19+dx[candidate]),byte(26+dy[candidate]),game.world.questFields(),256)) return false;
        NpcSchedulePolicy policy={false,false};if (NpcScheduler::apply(setup,game.clock.hour,game.clock.minute,policy,game.schedule)!=kNpcScheduleApplied) return false;
        byte ground;if (!setup.terrainAt(setup.x(),setup.y(),ground)) return false;TraversalProbe probe;probe.terrain=ground;TraversalDecision decision;
        if (!traversal.probe(probe,decision) || decision.blocked || decision.requiresInteraction || (ground>=229 && ground<233)) continue;
        Common::MemoryWriteStreamDynamic initial(DisposeAfterUse::YES);if (!setup.save(initial)) return false;Common::MemoryReadStream bytes(initial.getData(),initial.size());if (!game.world.load(bytes)) return false;
        window=WorldStory();if (!window.initialize(game.world,game.world.currentTables()->data()+1536,256) || !WorldViewport::refreshAtWorld(game.world,window,story,actors,lighting,occlusion,painter,game.appearance,game.clock.lightActive,0,previous,turn.presentation,light,visible,frame,&scene)) return false;
        if (scene.actors.lookup[near[candidate]]==16 && (scene.actors.kinds[near[candidate]]&135)==4 && scene.actors.lookup[93]==255) {direction=byte(candidate);break;}
    }
    if (direction==255 || !status.paint(game,scene,light,false,frame) || !text.importPages(frame.data(),frame.data(),8192)) return false;
    const Common::KeyCode arrows[]={Common::KEYCODE_UP,Common::KEYCODE_DOWN,Common::KEYCODE_RIGHT,Common::KEYCODE_LEFT};
    auto send=[&](Common::KeyCode key,uint16 ascii=0) {Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(key,ascii);return command->handle(event,archive,game,window,scene,turn,light,visible,routes,random,messages,text,frame);};
    auto settle=[&]() {
        for (uint page=0;page<16 && command->conversationStage()==2;++page) if (send(Common::KEYCODE_SPACE,' ')!=kWorldInputHandled) return false;
        return command->conversationStage()==3;
    };
    auto type=[&](const char *word) {for (uint i=0;word[i];++i) if (send(Common::KeyCode(word[i]==' '?Common::KEYCODE_SPACE:Common::KEYCODE_a+word[i]-'A'),uint16(word[i]))!=kWorldInputHandled) return false;return true;};
    uint cycles=0,codecs=0,selected=0;
    Common::Array<byte> talk;
    if (!archive.read("NA/SRTN.NPC.TALK",talk)) return false;
    auto label=[&](uint mode) {
        static const uint offsets[]={0xf66,0xf70,0xf7a};
        GlyphFont font;HgrFrame expected=frame;
        if (mode>2 || !font.load(archive,1) || !expected.clearTextSpan(25,21,13) || !expected.clearTextSpan(25,22,13)) return false;
        for (uint line=0;line<2;++line) {
            uint start=line?offsets[mode]:0xf5a,column=line?29:25;
            while (start<talk.size() && talk[start]) {
                if (column>=38 || !expected.drawGlyph(font,talk[start++],column++,21+line,0)) return false;
            }
            if (start==talk.size()) return false;
        }
        return !memcmp(expected.data(),frame.data(),8192);
    };
    auto cycle=[&](uint mode) {
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES),rngBefore(DisposeAfterUse::YES),rngAfter(DisposeAfterUse::YES),turnBefore(DisposeAfterUse::YES),turnAfter(DisposeAfterUse::YES);
        if (!game.save(before) || !random.save(rngBefore) || !turn.save(turnBefore) || send(Common::KEYCODE_TAB,9)!=kWorldInputHandled || !label(mode) || !game.save(after) || !random.save(rngAfter) || !turn.save(turnAfter) ||
            before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size()) || rngBefore.size()!=rngAfter.size() || memcmp(rngBefore.getData(),rngAfter.getData(),rngBefore.size()) || turnBefore.size()!=turnAfter.size() || memcmp(turnBefore.getData(),turnAfter.getData(),turnBefore.size())) return false;
        ++cycles;return true;
    };
    report="voice498 actual Talk admission";
    if (send(Common::KEYCODE_t,'T')!=kWorldInputHandled || send(arrows[direction])!=kWorldInputHandled || !settle() || !label(0)) return false;
    report="voice498 half NAME/cycle andcodec";
    if (!type("  NAXQ") || send(Common::KEYCODE_BACKSPACE,8)!=kWorldInputHandled || send(Common::KEYCODE_DELETE,127)!=kWorldInputHandled || !cycle(1) || !cycle(2)) return false;
    Common::MemoryWriteStreamDynamic core(DisposeAfterUse::YES),modal(DisposeAfterUse::YES);if (!game.save(core) || !command->save(modal)) return false;
    Common::MemoryReadStream coreBytes(core.getData(),core.size());if (!game.load(coreBytes)) return false;
    Common::ScopedPtr<WorldInput> restored(new WorldInput);Common::MemoryReadStream modalBytes(modal.getData(),modal.size());if (!restored->load(modalBytes,archive,6) || !restored->matchesWorld(game.world)) return false;command.reset(restored.release());codecs+=2;
    if (!cycle(0) || !type("ME") || send(Common::KEYCODE_ESCAPE,27)!=kWorldInputHandled || command->visibleReply()!=replies[0] || !settle()) return false;++selected;
    for (uint mode=1;mode<3;++mode) {
        report=Common::String::format("voice498 actual reply mode%u",mode);
        if (!cycle(mode) || !type("ZZZZ") || send(Common::KEYCODE_RETURN,13)!=kWorldInputHandled || command->visibleReply()!=replies[mode] || !settle() || !label(mode)) return false;++selected;
    }
    report="voice498 close whisper/reopen reset";
    if (send(Common::KEYCODE_ESCAPE,27)!=kWorldInputHandled) return false;
    for (uint page=0;page<20 && command->conversationStage();++page) if (send(Common::KEYCODE_SPACE,' ')!=kWorldInputHandled) return false;
    if (!command->atWorldCommand() || send(Common::KEYCODE_t,'T')!=kWorldInputHandled || send(arrows[direction])!=kWorldInputHandled || !settle() || !label(0) || !type("NAME") || send(Common::KEYCODE_RETURN,13)!=kWorldInputHandled || command->visibleReply()!=replies[0]) return false;
    InformationCatalog information;Common::String help;if (!information.load(archive) || !information.help(help) || !help.contains("normal/yell/whisper")) return false;
    report=Common::String::format("dialogue_voice\tactual_reply_modes=%u\tcycles_no_core_RNG_turn_change=%u\tcore_modal_codecs=%u\thalf_query_preserved=1\tEscape_Delete_trim=1\treset_on_reopen=1\tmode_labels=1\tcampaign=0\taudio_suite=0\n",selected,cycles,codecs);return true;
}
} }
#endif
