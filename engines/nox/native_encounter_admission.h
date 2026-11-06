/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ENCOUNTER_ADMISSION_H
#define NOX_NATIVE_ENCOUNTER_ADMISSION_H
#include "nox/resources.h"
namespace Nox { namespace Native {
struct EncounterContext {
    byte mapType,mapId,location,definition,tile,specialLower,specialUpper;
};
struct EncounterAdmission {
    uint16 descriptor;
    byte raw[4],group,monster,sprite,spell;
    bool fallback;
};
// Descriptor/context resolution over authored image data. Resource-loading sort,
// machine scratch and bank switches are not part of this gameplay decision.
class EncounterAdmissionRules {
public:
    static byte initialNoFlee(byte category,byte contextFlags) { return category>=4 || (contextFlags&192)==192?128:0; }
    static bool protection(const byte *tables,uint size,const byte *fields,uint fieldSize,byte bank,byte slot,bool &output) {
        if (!tables || size!=1792 || !fields || fieldSize!=256 || bank>1 || (slot&3) || (bank && (slot&7))) return false;
        output=false;if (!bank) return true;
        const byte *actor=tables+1280+slot;
        if (!(actor[3]&2)) return true;
        // Source indexes this definition in the compact table and can cross
        // its page into following object bytes at the final NPC slots.
        const byte *definition=tables+768+slot;
        output=actor[5]!=126 || !(definition[0]&32) || !(fields[definition[9]]&definition[11]);return true;
    }
    static byte transportFlags(byte mapType,byte tile,byte appearance,bool transport,byte waterLower,byte waterUpper) {
        byte flags=transport && (appearance==119 || appearance==123)?64:0;
        if (tile>=waterLower && tile<waterUpper) flags|=32;
        if ((mapType==0 || mapType==5) && (tile==154 || tile==123)) flags|=128;
        return flags;
    }
    bool load(HdvArchive &archive) {
        return archive.readRange("NA/SRTN.COMBAT.S",0xe82,141*4,_descriptors) &&
            archive.readRange("NA/SRTN.COMBAT.S",0xe3c,14*5,_overrides);
    }
    bool select(const EncounterContext &context,EncounterAdmission &output) const {
        if (_descriptors.size()!=141*4 || _overrides.size()!=14*5 || context.mapType>8) return false;
        EncounterAdmission next;next.descriptor=0;next.monster=next.sprite=next.spell=0;next.fallback=true;
        const byte category=context.mapType==5?6:context.mapType;
        for (uint i=0;i<141;++i) {
            const byte *row=_descriptors.data()+i*4;const byte type=row[0]&191;
            if (!(type&128) && type!=category) continue;
            if (row[1]!=context.tile) continue;
            next.descriptor=uint16(i);next.fallback=false;break;
        }
        memcpy(next.raw,_descriptors.data()+next.descriptor*4,4);next.group=next.raw[2];
        bool resolved=false;
        for (int i=13;i>=0;--i) {
            const byte *row=_overrides.data()+i*5;
            if (row[0]!=context.location || (!(row[1]&128) && row[1]!=context.mapId) || row[2]!=context.definition) continue;
            next.sprite=context.tile;next.monster=row[3];next.spell=row[4];
            if (!(next.spell&128)) { next.group=124;resolved=true;break; }
            // Negative selectors continue the original scan/context rules;
            // they are not an unconditional first-match template replacement.
        }
        if (!resolved) {
            if (context.mapId==0x29) next.sprite=0;
            else if (context.tile>=context.specialLower && context.tile<context.specialUpper) {
                next.sprite=context.tile;next.group=119;resolved=true;
            }
            if (!resolved) {
                if (context.tile>=246 && context.tile<254) next.sprite=context.tile;
                if (next.fallback) { next.sprite=context.tile;next.monster=130;next.group=124; }
                else if (next.raw[0]&192) { next.monster=next.raw[2];next.group=124; }
            }
        }
        if (!next.spell) next.spell=next.raw[3];
        if (next.group>=128 || next.monster>=142) return false;
        output=next;return true;
    }
private:
    Common::Array<byte> _descriptors,_overrides;
};
} }
#endif
