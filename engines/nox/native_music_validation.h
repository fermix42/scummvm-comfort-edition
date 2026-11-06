/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_MUSIC_VALIDATION_H
#define NOX_NATIVE_MUSIC_VALIDATION_H
#include "nox/native_audio.h"
namespace Nox {
inline bool validateNativeMusic(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    if (in.readUint32BE()!=MKTAG('N','M','U','S') || in.readUint16LE()!=1) return false;
    const uint count=in.readUint16LE();if (!count || count>128) return false;uint ticks=0,cold=0;uint64 audible=0;
    for (uint i=0;i<count;++i) {
        const uint size=in.readUint16LE();const byte volume=in.readByte();const uint n=in.readUint32LE();
        if (!size || size>16384 || volume>15 || !n || n>100000) return false;
        Common::Array<byte> song;song.resize(size);if (in.read(song.data(),size)!=size) return false;
        Native::MusicSequencer music;if (!music.initialize(archive,song.data(),size,volume)) return false;
        Native::MusicPlayback playback;int16 first[4800],expectedPcm[4800],actualPcm[4800];
        if (!playback.start(archive,song.data(),size,volume) || !playback.render(first,4800)) return false;
        Common::MemoryWriteStreamDynamic audio(DisposeAfterUse::YES);if (!playback.save(audio)) return false;
        Common::MemoryReadStream audioState(audio.getData(),audio.size());Native::MusicPlayback restoredAudio;
        if (!restoredAudio.load(audioState,archive) || !playback.render(expectedPcm,4800) || !restoredAudio.render(actualPcm,4800) || memcmp(expectedPcm,actualPcm,sizeof(expectedPcm))) {error="Native music synthesis continuation mismatch";return false;}
        if (!volume && playback.nonzero()) {error="Zero-volume music produced nonzero PCM";return false;}
        audible+=playback.nonzero();
        for (uint tick=0;tick<n;++tick) {
            const uint cursor=in.readUint16LE();const byte wait=in.readByte();byte expected[24],actual[24];
            if (in.read(expected,24)!=24 || !music.tick()) {error=Common::String::format("Music stream rejected case%u tick%u",i,tick);return false;}
            music.originalFields(actual);
            if (music.cursor()!=cursor || music.wait()!=wait || memcmp(expected,actual,24) || music.playing()!=(tick+1<n)) {
                error=Common::String::format("Original music differs case%u tick%u cursor%u/%u wait%u/%u playing%u",i,tick,music.cursor(),cursor,music.wait(),wait,uint(music.playing()));
                for (uint field=0;field<24;++field) if (expected[field]!=actual[field]) error+=Common::String::format(" field%u=%u/%u",field,actual[field],expected[field]);return false;
            }
            if (tick==n/2 || tick+1==n) {
                Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!music.save(saved)) return false;
                Common::MemoryReadStream source(saved.getData(),saved.size());Native::MusicSequencer restored;
                if (!restored.load(source,archive)) {error="Native music cold restore failed";return false;}
                music=restored;++cold;
            }
            ++ticks;
        }
    }
    uint selections=0;
    if (in.pos()!=in.size()) {
        if (in.readUint32BE()!=MKTAG('N','M','C','U') || in.readUint16LE()!=1) return false;
        selections=in.readUint16LE();if (!selections || selections>4096) return false;
        Common::Array<byte> original;if (!archive.read("NA/SRTN.COMBAT.S",original)) return false;
        for (uint i=0;i<selections;++i) {
            const uint32 seed=in.readUint32LE();const byte tail=in.readByte();const uint32 offset=in.readUint32LE();const uint size=in.readUint16LE();byte rng[8];
            if (in.read(rng,8)!=8 || !size || offset>original.size() || size>original.size()-offset) return false;
            Native::GameplayRandom random;if (!random.importState(seed,tail)) return false;Native::MusicPlayback playback;
            if (!playback.combat(archive,random,7,130) || !playback.battleCue() || random.seed()!=READ_LE_UINT32(rng) || random.tail()!=rng[4] || random.last()!=rng[5] || random.generationAttempts()!=READ_LE_UINT16(rng+6)) {error="Original combat music selection RNG differs";return false;}
            Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);if (!playback.save(saved) || saved.size()<19+size || READ_LE_UINT16(saved.getData()+17)!=size || memcmp(saved.getData()+19,original.data()+offset,size)) {error="Original combat music resource selection differs";return false;}
            Common::MemoryReadStream state(saved.getData(),saved.size());Native::MusicPlayback restored;
            const uint32 retained=random.seed();if (!restored.load(state,archive) || !restored.battleCue() || !restored.combat(archive,random,7,130) || random.seed()!=retained) {error="Cold combat music consumed a second choice";return false;}
        }
    }
    if (in.err() || in.pos()!=in.size()) return false;
    if (!audible) {error="Authored music synthesis was silent";return false;}
    report+=Common::String::format("native-music\twhole_tracks=%u\tticks=%u\tcold_restores=%u\tPCM_cold_continuations=%u\tnonzero_PCM=%llu\tcombat_selections=%u\tcpu_execution=0\tIRQ_delivery=0\thost_audio_acceptance=0\n",count,ticks,cold,count,(unsigned long long)audible,selections);return true;
}
}
#endif
