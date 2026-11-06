/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_RANDOM_H
#define NOX_NATIVE_RANDOM_H
#include "common/scummsys.h"
#include "common/stream.h"
namespace Nox { namespace Native {
// Non-interrupted gameplay RNG. Original compact arithmetic expands to the
// LCG below followed by a16-round product reduction and inclusive rejection.
// No CPU flags, registers, bank state or instruction dispatch participate.
class GameplayRandom {
public:
    GameplayRandom() : _ready(false), _seed(0), _tail(0), _last(0), _attempts(0) {}
    bool initializeEntropy(uint16 entropy) {
        if (_ready) return false;
        if (!entropy || entropy == 65535) entropy = 0xdbbd;
        const uint16 swapped = uint16((entropy >> 8) | (entropy << 8));
        return importState(uint32(swapped) | uint32(swapped) << 16, 2);
    }
    // Explicit import before operations, for native saves/legacy conversion or
    // controlled rule contexts. It is never an earned-progression claim.
    bool importState(uint32 seed, byte tail, byte last = 0, uint16 attempts = 0) {
        if (_ready || (tail != 0 && tail != 2) || attempts > 4096) return false;
        _seed = seed; _tail = tail; _last = last; _attempts = attempts; _ready = true; return true;
    }
    bool next(byte lower, byte upper, byte &value) {
        if (!_ready) return false;
        // Constant/inverted ranges do not generate a sample and retain the
        // previous generation counter, matching the source diagnostic word.
        if (upper <= lower) { _last = lower; value = lower; return true; }
        for (uint attempt = 1; attempt <= 4096; ++attempt) {
            const byte result = sample();
            if (attempt == 4096 || (result >= lower && result <= upper)) {
                _attempts = uint16(attempt); _last = attempt == 4096 ? lower : result; value = _last; return true;
            }
        }
        return false;
    }
    uint32 seed() const { return _seed; }
    byte tail() const { return _tail; }
    byte last() const { return _last; }
    uint16 generationAttempts() const { return _attempts; }
    bool save(Common::WriteStream &output) const {
        if (!_ready) return false;
        output.writeUint32BE(MKTAG('N','R','G','S')); output.writeUint16LE(1);
        output.writeUint32LE(_seed); output.writeByte(_tail); output.writeByte(_last); output.writeUint16LE(_attempts);
        return !output.err();
    }
    bool load(Common::SeekableReadStream &input) {
        if (input.size() - input.pos() != 14 || input.readUint32BE() != MKTAG('N','R','G','S') || input.readUint16LE() != 1) return false;
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(), last = input.readByte(); const uint16 attempts = input.readUint16LE();
        GameplayRandom candidate;
        if (input.err() || !candidate.importState(seed, tail, last, attempts)) return false;
        *this = candidate; return true;
    }
private:
    byte sample() {
        _seed = _seed * 1664525U + 1U;
        const uint32 high = _seed & 0xffffff00U;
        uint16 factor = uint16((_seed & 255) << 8) | _tail;
        uint64 product = 0;
        for (uint bit = 0; bit < 16; ++bit) {
            const bool add = (factor & 1) != 0; factor >>= 1;
            _seed = high | (factor >> 8);
            if (add) product += _seed;
            product >>= 1;
        }
        _tail = byte(factor); return byte(product >> 16);
    }
    bool _ready; uint32 _seed; byte _tail, _last; uint16 _attempts;
};
} }
#endif
