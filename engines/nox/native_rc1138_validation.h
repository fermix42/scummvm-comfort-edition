/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_RC1138_VALIDATION_H
#define NOX_NATIVE_RC1138_VALIDATION_H
#include "nox/native_title.h"
#include "nox/native_audio.h"
#include "nox/native_search_input.h"
#include "common/ptr.h"
namespace Nox {
inline bool validateNativeRC1138(HdvArchive &archive,Common::String &report,Common::Array<byte> &art,Common::Array<uint64> &edges,Common::Array<int16> &voice) {
    using namespace Native;
#define RC1138_CHECK(x) do {if (!(x)) {report=Common::String::format("RC1138 check failed at line%u",uint(__LINE__));return false;}} while (0)
    TitlePresentation title;RC1138_CHECK(title.load(archive) && title.speech(voice,&edges));
    bool changed;RC1138_CHECK(title.advance(uint64(NOX_CPU_HZ)*10,changed) && changed && title.finished() && title.cycle()==8683694 && edges.size()==159688);
    art=title.planes();TitlePresentation split;RC1138_CHECK(split.load(archive));
    for (uint64 t=12345;t<uint64(NOX_CPU_HZ)*10;t+=12345) RC1138_CHECK(split.advance(t,changed));
    RC1138_CHECK(split.finished() && split.cycle()==title.cycle() && split.planes()==art);
    MusicPlayback music;RC1138_CHECK(music.splash(archive));int16 pcm[4800];uint chunks=0;
    while (music.playing() && chunks++<3000) RC1138_CHECK(music.render(pcm,4800));
    RC1138_CHECK(!music.playing() && music.nonzero());
    for (uint i=0;i<10;++i) RC1138_CHECK(music.render(pcm,4800));
    for (uint i=0;i<4800;++i) RC1138_CHECK(pcm[i]==0);
    Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);RC1138_CHECK(music.save(saved));
    // Reproduce the old stopped-save defect in an isolated component blob.
    Common::Array<byte> stale;stale.resize(saved.size());memcpy(stale.data(),saved.getData(),saved.size());
    const uint synth=47+READ_LE_UINT32(stale.data()+7);RC1138_CHECK(synth+128+16==stale.size());
    for (uint chip=0;chip<2;++chip) for (uint r=8;r<=10;++r) stale[synth+chip*64+r]=7;
    MusicPlayback adopted;Common::MemoryReadStream old(stale.data(),stale.size());RC1138_CHECK(adopted.load(old,archive));
    for (uint i=0;i<10;++i) RC1138_CHECK(adopted.render(pcm,4800));
    for (uint i=0;i<4800;++i) RC1138_CHECK(pcm[i]==0);
    RC1138_CHECK(adopted.restart(archive) && adopted.playing() && adopted.render(pcm,4800) && adopted.nonzero());
    // Real Search input, empty adjacent object lookup. Seed all pixels so any
    // change outside the original bottom strip is observable without a screenshot.
    Common::ScopedPtr<NewGame> game(new NewGame(archive));RC1138_CHECK(game->create("RC1138",0,0));
    Common::ScopedPtr<HgrFrame> before(new HgrFrame),frame(new HgrFrame);
    for (uint y=0;y<192;++y) for (uint x=0;x<40;++x) RC1138_CHECK(before->putByte(x,y,byte(0x55^x^y)));
    *frame=*before;SearchInput search;RC1138_CHECK(search.begin(archive,*game,*frame));WorldScene scene;GameplayRandom random;bool alarm=false;
    Common::Event event;event.type=Common::EVENT_KEYDOWN;event.kbd=Common::KeyState(Common::KEYCODE_UP);
    RC1138_CHECK(search.handle(event,archive,*game,scene,random,*frame,alarm)==kSearchInputHandled);
    for (uint y=0;y<192;++y) for (uint x=0;x<40;++x) if (y<184 || x<7 || x>=32) RC1138_CHECK(frame->data()[HgrFrame::rowOffset(y)+x]==before->data()[HgrFrame::rowOffset(y)+x]);
    event.kbd=Common::KeyState(Common::KEYCODE_SPACE,32);RC1138_CHECK(search.handle(event,archive,*game,scene,random,*frame,alarm)==kSearchInputClosed && !memcmp(frame->data(),before->data(),8192));
    // Long responses page within that same one-row surface and survive cold
    // serialization at the actual wait boundary, including explicit CR.
    const char *text="Found an ancient scroll containing a lengthy inscription.\x8dTake it? Y/N/A";TextWindow window;
    *frame=*before;RC1138_CHECK(frame->clearTextSpan(7,23,25) && window.begin(archive,reinterpret_cast<const byte *>(text),strlen(text),7,25,23,23,1,frame.get()));
    uint pages=0;
    while (window.waiting() && pages++<10) {
        Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);RC1138_CHECK(window.save(out));Common::MemoryReadStream in(out.getData(),out.size());TextWindow cold;
        RC1138_CHECK(cold.load(in,archive) && cold.handle(event) && cold.paint(*frame));window=cold;
        for (uint y=0;y<192;++y) for (uint x=0;x<40;++x) if (y<184 || x<7 || x>=32) RC1138_CHECK(frame->data()[HgrFrame::rowOffset(y)+x]==before->data()[HgrFrame::rowOffset(y)+x]);
    }
    RC1138_CHECK(window.ended() && pages>1 && pages<10);
    report=Common::String::format("RC1138 PASS\ntitle_cycles=%llu\ttitle_edges=%u\ttitle_PCM_samples=%u\tsplit_frame_equivalence=PASS\nwhole_authored_song_chunks=%u\tend_silence=PASS\told_stopped_save_silence=PASS\trestart=PASS\nactual_Search_bounds=PASS\tunderlay_restoration=PASS\tlong_response_cold_pages=%u\n",(unsigned long long)title.cycle(),edges.size(),voice.size(),chunks,pages);
#undef RC1138_CHECK
    return true;
}
}
#endif
