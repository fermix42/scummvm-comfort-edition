/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PARTY_H
#define NOX_NATIVE_PARTY_H
#include "nox/records.h"
#include "nox/resources.h"
namespace Nox {
namespace Native {
// Owns original character/equipment records. New-game choices use the original
// template and data-only creation descriptors; no original program executes.
class Party {
    friend class EquipmentActions;
    friend class InnMembership;
public:
    Party() : _ready(false) {}
    bool ready() const { return _ready; }
    bool create(HdvArchive &archive, const Common::String &name, byte gender, byte race) {
        if (_ready || gender >= 3 || race >= 5 || name.empty() || name.size() > 15) return false;
        bool nonSpace = false;
        for (uint i = 0; i < name.size(); ++i) {
            const byte c = byte(name[i]); if (c < 32 || c >= 123) return false;
            nonSpace |= c != 32;
        }
        if (!nonSpace) return false;
        Common::Array<byte> data, loader;
        if (!archive.read("NA/DATA.SAVE.GAMEN", data) || data.size() != 8045 || !archive.read("NA/LOADER.P", loader) || loader.size() != 10514) return false;
        Party candidate;
        for (uint i = 0; i < 6; ++i) {
            memcpy(candidate._members[i].bytes, data.data() + 0xc00 + i * 128, 128);
            memcpy(candidate._equipment[i].bytes, data.data() + 0x1e00 + i * 16, 16);
        }
        // Source creation modifies these seven data pairs, then applies them
        // in reverse order. Race selects its corresponding starting stat.
        byte pairs[14]; memcpy(pairs, loader.data() + 0x4be, sizeof(pairs));
        pairs[race * 2 + 1] = 10; pairs[11] = gender; pairs[13] = race;
        for (int i = 12; i >= 0; i -= 2) {
            if (pairs[i] >= 128) return false;
            candidate._members[0].bytes[pairs[i]] = pairs[i + 1];
        }
        // The original copies all15 bytes of its input buffer, including its
        // opaque suffix after the terminator. Preserve that record behavior.
        byte entered[16]; memcpy(entered, loader.data() + 0x8d2, sizeof(entered));
        for (uint i = 0; i < name.size(); ++i) entered[i] = byte(name[i]) | 128;
        entered[name.size()] = 0;
        memcpy(candidate._members[0].bytes + 0x4b, entered, 15);
        candidate._ready = true; *this = candidate; return true;
    }
    uint memberCount() const {
        if (!_ready) return 0;
        uint count = 1;
        while (count < 6 && _members[count].bytes[0] != 170 && !_members[count].bytes[0x45]) ++count;
        return count;
    }
    const CharacterRecord *member(uint index) const { return _ready && index < 6 ? &_members[index] : nullptr; }
    const EquipmentRecord *equipment(uint index) const { return _ready && index < 6 ? &_equipment[index] : nullptr; }
    bool damage(uint index, uint16 amount) {
        if (!_ready || index >= memberCount()) return false;
        _members[index].damage(amount); return true;
    }
    bool applyBattleMembers(const byte *records,uint length,uint count) {
        if (!_ready || !records || length!=768 || count!=memberCount()) return false;
        // Battle cannot recruit/reorder by overwriting unused slots. Equipment
        // ownership remains in its existing independent records.
        for (uint i=count;i<6;++i) if (memcmp(records+i*128,_members[i].bytes,128)) return false;
        for (uint i=0;i<count;++i) memcpy(_members[i].bytes,records+i*128,128);
        return true;
    }
    bool practiceLockSkill(uint index, byte field, bool &promoted) {
        if (!_ready || index >= memberCount() || (field != 32 && field != 38)) return false;
        byte *record = _members[index].bytes;
        StatProgress progress = { record[field - 1], record[field], record[field + 1] };
        promoted = progress.advance();
        record[field - 1] = progress.value; record[field] = progress.practice;
        return true;
    }
    bool save(Common::WriteStream &output) const {
        if (!_ready) return false;
        output.writeUint32BE(MKTAG('N','P','A','R')); output.writeUint16LE(1);
        for (uint i = 0; i < 6; ++i) output.write(_members[i].bytes, 128);
        for (uint i = 0; i < 6; ++i) output.write(_equipment[i].bytes, 16);
        return !output.err();
    }
    bool load(Common::SeekableReadStream &input) {
        if (input.size() - input.pos() != 870 || input.readUint32BE() != MKTAG('N','P','A','R') || input.readUint16LE() != 1) return false;
        Party candidate;
        for (uint i = 0; i < 6; ++i) if (input.read(candidate._members[i].bytes, 128) != 128) return false;
        for (uint i = 0; i < 6; ++i) if (input.read(candidate._equipment[i].bytes, 16) != 16) return false;
        if (input.err()) return false;
        candidate._ready = true; *this = candidate; return true;
    }
private:
    bool _ready;
    CharacterRecord _members[6]; EquipmentRecord _equipment[6];
};
}
}
#endif
