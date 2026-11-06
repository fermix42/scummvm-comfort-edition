/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_ANIMATION_VALIDATION_H
#define NOX_NATIVE_WORLD_ANIMATION_VALIDATION_H
#include "nox/native_world_animation.h"
#include "common/md5.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeWorldAnimation(HdvArchive &archive,Common::SeekableReadStream &input,Common::SaveFileManager &files,
        bool cold,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','W','A','N') || input.readUint32LE()!=1 || input.readUint32LE()!=2) return false;
    Native::WorldAnimation animation;if (!animation.load(archive)) return false;uint operations=0,drawCount=0;
    for (uint group=0;group<2;++group) {
        byte setup[11],fields[256],tables[1792],mask[187],phases[7];Native::WorldScene scene;
        if (input.read(setup,11)!=11 || input.read(fields,256)!=256 || input.read(tables,1792)!=1792 ||
            input.read(scene.actors.terrain,187)!=187 || input.read(scene.objects.lookup,187)!=187 || input.read(scene.actors.lookup,187)!=187 ||
            input.read(scene.actors.kinds,187)!=187 || input.read(mask,187)!=187 || input.read(phases,7)!=7) return false;
        Native::WorldAnimationContext context;context.appearance=setup[4];context.partyObject=setup[5];context.notice=setup[6]!=0;
        context.left=setup[7];context.right=setup[8];context.top=setup[9];context.bottom=setup[10];
        Native::WorldAnimationState state;state.phase=phases[0];state.pattern=phases[1];memcpy(state.groups,phases+2,5);state.target=input.readByte();
        for (uint page=0;page<2;++page) { byte image[8192];if (input.read(image,8192)!=8192 || !state.pages[page].importImage(image,8192)) return false; }
        const uint count=input.readUint16LE();if (count!=32 || setup[6]>1 || !state.valid()) return false;
        Native::World world(archive);const bool restored=cold && group==0;
        if (restored) {
            Common::InSaveFile *w=files.openForLoading("nox-native-animation-world.bin"),*s=files.openForLoading("nox-native-animation-state.bin");
            const bool ok=w && s && world.load(*w) && state.load(*s);delete w;delete s;if (!ok) { error="Native animation cold checkpoint rejected";return false; }
            if (world.mapType()!=setup[0] || world.mapId()!=setup[1] || world.x()!=setup[2] || world.y()!=setup[3] ||
                memcmp(world.questFields(),fields,256) || memcmp(world.currentTables()->data(),tables,1792)) return false;
        } else {
            if (!world.initialize(setup[0],setup[1],setup[2],setup[3],fields,256)) return false;
            Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES);if (!world.save(encoded) || encoded.size()!=2063) return false;
            Common::Array<byte> controlled;controlled.resize(encoded.size());memcpy(controlled.data(),encoded.getData(),encoded.size());memcpy(controlled.data()+271,tables,1792);
            Common::MemoryReadStream initial(controlled.data(),controlled.size());if (!world.load(initial)) return false;
        }
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES);if (!world.save(before)) return false;
        for (uint operation=0;operation<count;++operation) {
            byte expected[8],enabled[5],digests[32];if (input.read(expected,8)!=8 || input.read(enabled,5)!=5) return false;
            const uint draws=input.readUint16LE();if (draws>187) return false;Common::Array<byte> request;request.resize(draws*3);
            if ((draws && input.read(request.data(),request.size())!=request.size()) || input.read(digests,32)!=32) return false;
            if (restored && operation<7) continue;
            const bool checkpoint=restored && operation==7;
            if (!checkpoint) {
                Native::LightState light(1,0,0);Common::Array<Native::WorldAnimationDraw> actual;byte actualEnabled[5];
                if (!animation.run(world,scene,mask,187,light,context,state,actual,actualEnabled)) { error=Common::String::format("Native animation unsupported case%u operation%u",group,operation);return false; }
                if (actual.size()!=draws || memcmp(actualEnabled,enabled,5)) { error=Common::String::format("Native animation draw/pattern count differs case%u operation%u",group,operation);return false; }
                for (uint i=0;i<draws;++i) if (actual[i].cell!=request[i*3] || actual[i].tile!=request[i*3+1] || actual[i].phase!=request[i*3+2]) {
                    error=Common::String::format("Native animation raster request differs case%u operation%u draw%u: %u/%u/%u vs%u/%u/%u",group,operation,i,uint(actual[i].cell),uint(actual[i].tile),uint(actual[i].phase),uint(request[i*3]),uint(request[i*3+1]),uint(request[i*3+2]));return false;
                }
                ++operations;drawCount+=draws;
            }
            if (state.phase!=expected[0] || state.pattern!=expected[1] || memcmp(state.groups,expected+2,5) || state.target!=expected[7]) { error="Native animation phase/cadence state differs";return false; }
            for (uint page=0;page<2;++page) {
                Common::MemoryWriteStreamDynamic visible(DisposeAfterUse::YES);
                for (uint row=0;row<192;++row) visible.write(state.pages[page].data()+Native::HgrFrame::rowOffset(row),40);
                byte digest[16];Common::MemoryReadStream plane(visible.getData(),visible.size());
                if (!Common::computeStreamMD5(plane,digest) || memcmp(digest,digests+page*16,16)) { error=Common::String::format("Native animation image history differs case%u operation%u page%u",group,operation,page);return false; }
            }
            if (!cold && group==0 && operation==7) {
                Common::OutSaveFile *w=files.openForSaving("nox-native-animation-world.bin",false),*s=files.openForSaving("nox-native-animation-state.bin",false);
                bool ok=w && s && world.save(*w) && state.save(*s);
                if (w) { w->finalize();ok=ok&&!w->err(); }if (s) { s->finalize();ok=ok&&!s->err(); }delete w;delete s;if (!ok) return false;
            }
        }
        Common::MemoryWriteStreamDynamic after(DisposeAfterUse::YES);if (!world.save(after) || before.size()!=after.size() || memcmp(before.getData(),after.getData(),before.size())) { error="Idle animation mutated native gameplay world";return false; }
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report+=Common::String::format("native-world-animation\tphase=%s\toperations=%u\traster_requests=%u\tphase_groups=5\timage_pages=2\tactual_phase_pages_restored=%u\tcontinued_first_case=%u\tworld_unchanged=1\tcpu_execution=0\tlive_idle_integration=0\n",cold?"cold":"warm",operations,drawCount,uint(cold),cold?24:32);return true;
}
}
#endif
