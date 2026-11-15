/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SEARCH_LIFECYCLE_VALIDATION_H
#define NOX_NATIVE_SEARCH_LIFECYCLE_VALIDATION_H
#include "nox/native_search_input.h"
namespace Nox { namespace Native {
inline bool validateNativeSearchLifecycle523(HdvArchive &archive,Common::String &report) {
    report="Search retry523 controlled setup";
    Common::ScopedPtr<NewGame> owner(new NewGame(archive));NewGame &game=*owner;
    if (!game.create("RETRY",0,0)) return false;
    byte members[768];for(uint i=0;i<6;++i) memcpy(members+i*128,game.party.member(i)->bytes,128);
    members[31]=members[37]=0;
    if (!game.party.applyBattleMembers(members,768,game.party.memberCount())) return false;
    byte fields[256];memcpy(fields,game.world.questFields(),256);fields[33]=10;
    World setup(archive);if (!setup.initialize(3,31,33,15,fields,256)) return false;
    Common::MemoryWriteStreamDynamic worldBytes(DisposeAfterUse::YES);if (!setup.save(worldBytes)) return false;
    Common::MemoryReadStream worldIn(worldBytes.getData(),worldBytes.size());if (!game.world.load(worldIn)) return false;
    const byte *object=game.world.currentTables()->data()+1052;
    if(object[0]!=33 || object[1]!=14 || object[2]!=8 || object[3]!=53 || (object[7]&63)!=8) return false;
    // Declared component admission lookup before any operation. World projection
    // and the ordinary actor clock are not tested by this focused input batch.
    WorldScene scene;memset(scene.objects.lookup,255,187);scene.objects.lookup[76]=28;
    HgrFrame frame;GameplayRandom random;if(!random.importState(0x89e4ac58U,2)) return false;
    SearchInput search;bool alarm=false;
    auto event=[](Common::KeyCode key,byte ascii=0) {Common::Event e;e.type=Common::EVENT_KEYDOWN;e.kbd=Common::KeyState(key,ascii);return e;};
    if(!search.begin(archive,game,frame) || search.handle(event(Common::KEYCODE_UP),archive,game,scene,random,frame,alarm)!=kSearchInputHandled ||
       search.handle(event(Common::KEYCODE_1,'1'),archive,game,scene,random,frame,alarm)!=kSearchInputHandled ||
       game.party.member(0)->health()!=9 || game.world.field(33)!=9) return false;
    Common::MemoryWriteStreamDynamic pending(DisposeAfterUse::YES),core(DisposeAfterUse::YES),rng(DisposeAfterUse::YES);
    if(!search.save(pending) || !game.save(core) || !random.save(rng)) return false;
    const Common::KeyCode keys[]={Common::KEYCODE_y,Common::KEYCODE_RETURN,Common::KEYCODE_n,Common::KEYCODE_ESCAPE,Common::KEYCODE_SPACE,Common::KEYCODE_UP};
    uint retries=0,closed=0;
    for(uint i=0;i<ARRAYSIZE(keys);++i) {
        report=Common::String::format("Search retry523 answer%u",i);
        SearchInput restored;Common::MemoryReadStream input(pending.getData(),pending.size());
        if(!restored.load(input,archive,true,false,false)) return false;
        const byte ascii=i==0?'Y':i==1?13:i==2?'N':0;
        if(restored.handle(event(keys[i],ascii),archive,game,scene,random,frame,alarm)!=kSearchInputAttempted || restored.active()!=(i<2)) return false;
        Common::MemoryWriteStreamDynamic afterCore(DisposeAfterUse::YES),afterRng(DisposeAfterUse::YES);
        if(!game.save(afterCore) || !random.save(afterRng) || core.size()!=afterCore.size() || memcmp(core.getData(),afterCore.getData(),core.size()) ||
           rng.size()!=afterRng.size() || memcmp(rng.getData(),afterRng.getData(),rng.size())) return false;
        if(i<2) {
            Common::MemoryWriteStreamDynamic retry(DisposeAfterUse::YES);if(!restored.save(retry)) return false;
            SearchInput cold;Common::MemoryReadStream retryIn(retry.getData(),retry.size());
            if(!cold.load(retryIn,archive,true,false,false) || cold.handle(event(Common::KEYCODE_ESCAPE),archive,game,scene,random,frame,alarm)!=kSearchInputClosed || cold.active()) return false;
            ++retries;
        } else ++closed;
    }
    report=Common::String::format("search_retry\tactual_nonfatal_attempt=1\tretry_answers=%u\tclose_answers=%u\tresult_codec_cases=6\tretry_cancel_codecs=2\tack_core_rng_unchanged=1\tordinary_clock_acceptance=0\tcampaign=0\taudio_suite=0\n",retries,closed);return true;
}
} }
#endif
