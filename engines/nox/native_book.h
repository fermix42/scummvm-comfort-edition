/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BOOK_H
#define NOX_NATIVE_BOOK_H
#include "nox/native_inventory.h"
#include "nox/native_party.h"
namespace Nox { namespace Native {
enum BookResult { kBookReadied, kBookUnreadied, kBookIntelligenceRefusal, kBookCombatRefusal, kBookCancelled, kBookUnsupported, kBookNotOwned };
// Own the ten global spell shortcuts plus their original requirement bytes.
// Inventory presentation, equipment and casting remain separate operations.
class SpellBook {
public:
    SpellBook() : _ready(false) { memset(_state, 0, sizeof(_state)); }
    bool importState(const byte *state, uint size) {
        if (_ready || !state || size != sizeof(_state)) return false;
        memcpy(_state, state, sizeof(_state)); _ready = true; return true;
    }
    bool exportState(byte *state, uint size) const {
        if (!_ready || !state || size != sizeof(_state)) return false;
        memcpy(state, _state, sizeof(_state)); return true;
    }
    byte spell(uint slot) const { return _ready && slot < 10 ? _state[slot] : 255; }
    byte requirement(uint slot) const { return _ready && slot < 10 ? _state[10 + slot] : 0; }
    bool save(Common::WriteStream &output) const {
        if (!_ready) return false;
        output.writeUint32BE(MKTAG('N','B','S','K')); output.writeUint16LE(1); output.write(_state, sizeof(_state)); return !output.err();
    }
    bool load(Common::SeekableReadStream &input) {
        if (input.size() - input.pos() != 26 || input.readUint32BE() != MKTAG('N','B','S','K') || input.readUint16LE() != 1) return false;
        byte state[20]; SpellBook candidate;
        if (input.read(state, sizeof(state)) != sizeof(state) || input.err() || !candidate.importState(state, sizeof(state))) return false;
        *this = candidate; return true;
    }
    BookResult select(const ItemCatalog &catalog, const Inventory &inventory, const Party &party,
            byte itemId, byte chosenSlot, bool inCombat, bool cancelled) {
        if (!_ready || !party.ready()) return kBookUnsupported;
        const int owned = inventory.find(2, itemId);
        if (owned < 0 || !inventory.item(owned)->quantity()) return kBookNotOwned;
        ItemDefinition definition;
        if (!catalog.definition(2, itemId, definition) || (definition.bytes[12] & 31) != 8) return kBookUnsupported;
        if (inCombat) return kBookCombatRefusal;
        for (uint slot = 0; slot < 10; ++slot) if (_state[slot] == definition.bytes[6]) {
            // Original unready leaves its requirement byte unchanged.
            _state[slot] = 255; return kBookUnreadied;
        }
        bool eligible = false;
        for (uint i = 0; i < party.memberCount(); ++i) {
            const CharacterRecord *member = party.member(i);
            if (member && !member->bytes[0] && member->bytes[18] >= definition.bytes[4]) { eligible = true; break; }
        }
        if (!eligible) return kBookIntelligenceRefusal;
        if (cancelled) return kBookCancelled;
        if (chosenSlot >= 10) return kBookUnsupported;
        _state[chosenSlot] = definition.bytes[6]; _state[10 + chosenSlot] = definition.bytes[4]; return kBookReadied;
    }
private:
    bool _ready; byte _state[20];
};
} }
#endif
