/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_PRESENTATION_VALIDATION_H
#define NOX_NATIVE_BATTLE_PRESENTATION_VALIDATION_H
#include "nox/native_battle_presentation.h"
namespace Nox {
inline bool validateNativeBattleBars(Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','A','R') || input.readUint32LE()!=1 || input.readUint32LE()!=567 || input.size()!=12+567*5) return false;
    for (uint i=0;i<567;++i) {
        const uint16 current=input.readUint16LE(),maximum=input.readUint16LE();const byte expected=input.readByte();
        if (!maximum || current>maximum || Native::BattlePresentation::barLevel(current,maximum)!=expected) return false;
    }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battle-bars\tcases=567\tcpu_execution=0\n";return true;
}
inline bool validateNativeBattlePresentation(HdvArchive &archive,Common::SeekableReadStream &input,Common::String &report) {
    if (input.readUint32BE()!=MKTAG('N','B','P','I') || input.readUint32LE()!=1 || input.size()!=8+1+2560+187+8192) return false;
    const byte phase=input.readByte();Native::EncounterState battle;battle.formed=true;Native::PreparedEncounter prepared;prepared.terrain.resize(187);
    byte expected[8192];Native::BattlePresentation painter;Native::HgrFrame result;
    if (input.read(battle.records.stats,2048)!=2048 || input.read(battle.records.compact,256)!=256 || input.read(battle.records.extended,256)!=256 ||
        input.read(prepared.terrain.data(),187)!=187 || input.read(expected,8192)!=8192 || !painter.load(archive) || !painter.paint(prepared,battle,phase,result)) return false;
    uint differences=0;Common::String samples;
    for (uint y=0;y<192;++y) for (uint x=0;x<40;++x) {
        const uint at=Native::HgrFrame::rowOffset(y)+x;
        if (result.data()[at]!=expected[at]) {
            if (differences<16) samples+=Common::String::format(" %u/%u:%u/%u",x,y,uint(result.data()[at]),uint(expected[at]));
            ++differences;
        }
    }
    if (differences) { report=Common::String::format("Native tactical image differs in %u visible bytes; actual/expected:%s",differences,samples.c_str());return false; }
    if (input.err() || input.pos()!=input.size()) return false;
    report="native-battle-presentation\tvisible_bytes=7680\texcluded_hgr_gaps=512\tcontrolled_setup=1\tcpu_execution=0\ttactical_input=0\n";return true;
}
}
#endif
