/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLEFIELDS_H
#define NOX_NATIVE_BATTLEFIELDS_H
#include "nox/native_lz.h"
#include "nox/resources.h"
#include "common/endian.h"
#include "common/util.h"
namespace Nox { namespace Native {
struct BattlefieldChoice { byte field,layoutFlags,deepWater; };
// Authored tactical map data. Selection/admission, roster construction and
// battle turns are separate responsibilities. No program bytes are executed.
class BattlefieldCatalog {
public:
    BattlefieldCatalog() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> packet,size,substitutions,rules,decoded;
        // The caller's declared packet begins eight bytes before its decoded
        // destination; its final two-byte size word is not compressed data.
        // Native storage is independent and never aliases code or scratch.
        if (!archive.readRange("NA/SRTN.COMBAT.S",2965,677,packet) ||
            !archive.readRange("NA/SRTN.COMBAT.S",3642,2,size) || READ_BE_UINT16(size.data())!=3740 ||
            !archive.readRange("NA/SRTN.COMBAT.S",2693,32,substitutions) ||
            !archive.readRange("NA/SRTN.COMBAT.S",2725,240,rules) ||
            !decodeReversePacket(packet.data(),packet.size(),3740,decoded)) return false;
        _fields=decoded;_rules=rules;memcpy(_substitutions,substitutions.data(),32);_ready=true;return true;
    }
    bool choose(byte mapType,byte terrain,byte contactTerrain,byte encounterFlags,BattlefieldChoice &output) const {
        if (!_ready || mapType>8 || mapType==1 || (encounterFlags&31)) return false;
        const byte type=mapType==5?6:mapType;BattlefieldChoice next={0,0,0};
        if (encounterFlags&192) {
            next.field=(encounterFlags&192)==192?12:((encounterFlags&64) && (encounterFlags&32))?13:type==6?19:11;
            next.layoutFlags=128;output=next;return true;
        }
        next.deepWater=contactTerrain>=229 && contactTerrain<233;
        for (uint at=0;at<240;at+=5) {
            const byte *rule=_rules.data()+at;
            if (rule[0]!=type) continue;
            if (!(rule[4]&128) && (rule[4]!=next.deepWater || terrain<rule[1] || terrain>=rule[2])) continue;
            next.layoutFlags=rule[3];next.field=rule[3]&127;if (next.field>=20) return false;
            output=next;return true;
        }
        return false;
    }
    bool project(byte field,byte mapType,Common::Array<byte> &output) const {
        if (!_ready || field>=20 || mapType>8 || mapType==1) return false;
        const byte type=mapType==5?6:mapType;const uint column=type<6?type:type-1;
        Common::Array<byte> cells;cells.resize(187);
        for (uint cell=0;cell<187;++cell) {
            const byte tile=_fields[uint(field)*187+cell];
            cells[cell]=tile<4?_substitutions[uint(tile)*8+column]:tile;
        }
        output=cells;return true;
    }
private:
    bool _ready;Common::Array<byte> _fields,_rules;byte _substitutions[32];
};
} }
#endif
