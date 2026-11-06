/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_COMBAT_MODE_H
#define NOX_NATIVE_COMBAT_MODE_H
#include "common/scummsys.h"
#include "common/stream.h"
#include "common/endian.h"
namespace Nox { namespace Native {
struct CombatModeChoice {
    bool accepted;byte mode,display,stepped;
    CombatModeChoice() : accepted(false),mode(0),display(0),stepped(0) {}
};
struct CombatAdvice { byte average,difference;bool warning; };
struct CombatModeState {
    byte selection,average,difference;bool warning;
    CombatModeState() : selection(0),average(0),difference(0),warning(false) {}
    bool save(Common::WriteStream &out) const {
        if (!selection || selection>3 || !average || average>10) return false;
        out.writeUint32BE(MKTAG('N','M','D','S'));out.writeUint16LE(1);out.writeByte(selection);out.writeByte(average);out.writeByte(difference);out.writeByte(warning);return !out.err();
    }
    bool load(Common::ReadStream &in) {
        if (in.readUint32BE()!=MKTAG('N','M','D','S') || in.readUint16LE()!=1) return false;
        CombatModeState next;next.selection=in.readByte();next.average=in.readByte();next.difference=in.readByte();const byte warned=in.readByte();
        if (!next.selection || next.selection>3 || !next.average || next.average>10 || warned>1 || in.err()) return false;
        next.warning=warned!=0;*this=next;return true;
    }
};
class CombatModeRules {
public:
    static bool forcedTactical(byte category,byte sprite,byte contactFlags,bool &forbidFlee) {
        forbidFlee=(contactFlags&120)!=0 || (category==2 && (sprite==145 || sprite==169 || sprite==170)) ||
            ((category==5 || category==6) && (sprite==134 || sprite==192)) || (category==3 && sprite==146);
        return forbidFlee || (!category && sprite==152) || (category==2 && sprite==147);
    }
    static CombatModeChoice choose(byte preference,bool restricted,byte answer) {
        CombatModeChoice choice;
        if (!(preference&128)) { choice.accepted=true;choice.mode=preference?1:0;choice.display=preference?193:0;return choice; }
        answer|=128;if (answer>=225 && answer<251) answer&=223;
        if (answer==212 || (!restricted && answer==155)) choice.accepted=true;
        else if (answer==209) { choice.accepted=true;choice.mode=1;choice.display=193; }
        else if (!restricted && (answer==161 || answer==163)) {
            choice.accepted=true;choice.mode=1;choice.display=65;choice.stepped=answer==163?193:0;
        }
        return choice;
    }
    static bool advice(const byte *levels,uint count,byte enemyLevel,byte mapId,byte sprite,byte enemies,CombatAdvice &out) {
        if (!levels || !count || count>6 || !enemies || enemies>16) return false;
        byte sum=0;for (uint i=0;i<count;++i) { if (!levels[i] || levels[i]>10) return false;sum=byte(sum+levels[i]); }
        CombatAdvice next;next.average=byte(sum/count+(sum%count>=3?1:0));
        next.difference=(enemyLevel&127)>=next.average?128:byte(next.average-enemyLevel);
        next.warning=false;
        if (next.difference<2 || next.difference==128) {
            next.difference=128;next.warning=mapId || (sprite!=130 && !(sprite==138 && (count>=2 || enemies<3)));
        }
        out=next;return true;
    }
};
} }
#endif
