/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENDING_VALIDATION_H
#define NOX_NATIVE_ENDING_VALIDATION_H
#include "nox/native_ending.h"
#include "nox/native_world_travel.h"
namespace Nox {
inline bool validateNativeEndingBranches(Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    if (in.readUint32BE()!=MKTAG('N','S','E','N') || in.readUint16LE()!=1) return false;
    const uint gates=in.readUint16LE();if (!gates || gates>1000) return false;
    for (uint i=0;i<gates;++i) {
        const byte map=in.readByte(),x=in.readByte(),y=in.readByte(),field=in.readByte(),expected=in.readByte();byte spawns[256];
        if (expected>1 || in.read(spawns,256)!=256) return false;Native::WorldTravelPlan plan;
        const Native::WorldTravelResult result=Native::WorldTravelRules::enter(0,map,x,y,field,spawns,256,plan);
        if ((result==Native::kTravelSpecialBattle)!=(expected!=0)) {error=Common::String::format("Original special combat entry differs case%u",i);return false;}
    }
    const uint count=in.readUint16LE();if (!count || count>1000) return false;
    for (uint i=0;i<count;++i) {
        const byte map=in.readByte(),sprite=in.readByte(),ending=in.readByte();
        if (ending>1 || Native::EndingPresentation::selected(map,sprite)!=(ending!=0)) {error="Original ending selection differs";return false;}
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report+=Common::String::format("native-ending-branches\tauthored_gates=%u\tpost_victory_selections=%u\tcpu_execution=0\tactual_presentation_acceptance=0\n",gates,count);return true;
}
}
#endif
