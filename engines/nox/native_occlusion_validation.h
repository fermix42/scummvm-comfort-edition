/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_OCCLUSION_VALIDATION_H
#define NOX_NATIVE_OCCLUSION_VALIDATION_H
#include "nox/native_occlusion.h"
namespace Nox {
inline bool validateNativeOcclusion(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','O','C','C') || input.readUint32LE() != 1 || input.readUint32LE() != 402) return false;
    for (uint i = 0; i < 402; ++i) {
        const byte cell = input.readByte(); byte mask[187],previous[187],expected[187];
        if (input.read(mask,187) != 187 || input.read(previous,187) != 187 || input.read(expected,187) != 187) return false;
        const uint count = input.readUint16LE(); Common::Array<Native::CellPaint> actions;
        if (!Native::OcclusionRules::shadow(cell,mask,previous,actions) || memcmp(mask,expected,187) || actions.size() != count) {
            error = Common::String::format("Native geometric shadow differs at case%u cell%u",i,uint(cell)); return false;
        }
        for (uint j = 0; j < count; ++j) if (actions[j].kind != Native::kCellClear || actions[j].cell != input.readByte()) return false;
    }
    if (input.readUint32LE() != 40448) return false;
    Native::OcclusionRules rules; byte last = 255;
    for (uint i = 0; i < 40448; ++i) {
        const byte type = input.readByte(),terrain = input.readByte(),tile = input.readByte(),state = input.readByte(),cell = input.readByte(),present = input.readByte(),expected = input.readByte();
        if (type != last) { if (!rules.load(archive,type)) return false; last = type; }
        bool opaque;
        if (!rules.opaque(terrain,present != 0,tile,state,cell,opaque) || byte(opaque) != expected) {
            error = Common::String::format("Native opacity differs at case%u type%u",i,uint(type)); return false;
        }
    }
    if (input.readUint32LE() != 160) return false;
    uint total = 0; Native::VisibilityMask visibility; if (!visibility.loadRules(archive)) return false;
    for (uint i = 0; i < 160; ++i) {
        const byte type = input.readByte(); Native::OcclusionContext context;
        context.direction = input.readByte(); context.presentation = input.readByte() != 0; context.ignoreBlockers = input.readByte() != 0;
        byte mask[187],expected[187];
        if (input.read(context.terrain,187) != 187 || input.read(context.objects,187) != 187 || input.read(context.records,256) != 256 ||
            input.read(mask,187) != 187 || input.read(context.previous,187) != 187 || input.read(expected,187) != 187) return false;
        const uint count = input.readUint16LE(); Common::Array<Native::CellPaint> actions;
        if (!rules.load(archive,type) || !visibility.importMask(mask,187) || !rules.apply(context,visibility,actions) || memcmp(visibility.cells(),expected,187) || actions.size() != count) {
            error = Common::String::format("Native visibility repaint differs at case%u type%u actions%u expected%u",i,uint(type),actions.size(),count); return false;
        }
        for (uint j = 0; j < count; ++j) {
            const byte kind = input.readByte(),cell = input.readByte();
            if (uint(actions[j].kind) != kind || actions[j].cell != cell) {
                error = Common::String::format("Native ordered visibility repaint differs at case%u action%u",i,j); return false;
            }
        }
        total += count;
    }
    if (total != 15646 || input.err() || input.pos() != input.size()) return false;
    report += "native-occlusion\tgeometric_cases=402\topacity_cases=40448\tfull_phases=160\tstyles=8\tordered_actions=15646\tfull_mask=1\tcomplete_rendered_frame=0\tcpu_execution=0\n"; return true;
}
}
#endif
