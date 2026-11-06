/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_RANDOM_VALIDATION_H
#define NOX_NATIVE_RANDOM_VALIDATION_H
#include "nox/native_random.h"
#include "common/savefile.h"
namespace Nox {
inline bool nativeRandomMatches(const Native::GameplayRandom &random, uint32 seed, byte tail, byte value, uint16 attempts) {
    return random.seed() == seed && random.tail() == tail && random.last() == value && random.generationAttempts() == attempts;
}
inline bool readNativeRandomResult(Common::SeekableReadStream &input, Native::GameplayRandom &random, byte lower, byte upper) {
    const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(), expected = input.readByte(); const uint16 attempts = input.readUint16LE(); byte actual = 0;
    return random.next(lower, upper, actual) && actual == expected && nativeRandomMatches(random, seed, tail, expected, attempts);
}
inline bool validateNativeRandom(Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','R','N','G') || input.readUint32LE() != 1 || input.readUint32LE() != 416 || input.readUint32LE() != 260 || input.readUint32LE() != 8) return false;
    for (uint i = 0; i < 416; ++i) {
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(), lower = input.readByte(), upper = input.readByte(); Native::GameplayRandom random;
        if (!random.importState(seed, tail) || !readNativeRandomResult(input, random, lower, upper)) { error = Common::String::format("Native gameplay RNG differs at case%u", i); return false; }
    }
    for (uint i = 0; i < 260; ++i) {
        const uint16 entropy = input.readUint16LE(); const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(); Native::GameplayRandom random;
        if (!random.initializeEntropy(entropy) || random.seed() != seed || random.tail() != tail) { error = "Native RNG entropy initialization differs"; return false; }
    }
    for (uint i = 0; i < 8; ++i) {
        const uint16 entropy = input.readUint16LE(); if (input.readUint16LE() != 16) return false;
        Native::GameplayRandom random; if (!random.initializeEntropy(entropy)) return false;
        for (uint draw = 0; draw < 16; ++draw) {
            const byte lower = input.readByte(), upper = input.readByte();
            if (!readNativeRandomResult(input, random, lower, upper)) { error = Common::String::format("Native retained RNG stream differs at chain%u draw%u", i, draw); return false; }
        }
    }
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-rng\tsingle_cases=416\tentropy_cases=260\tstreams=8\tretained_draws=128\tkeyboard_interrupt=0\tcpu_execution=0\n"; return true;
}
inline bool validateNativeRandomPersistence(Common::SaveFileManager &files, Common::SeekableReadStream &expected,
        bool cold, Common::String &report, Common::String &error) {
    if (expected.readUint32BE() != MKTAG('N','R','G','C') || expected.readUint32LE() != 1) return false;
    const uint16 entropy = expected.readUint16LE(), warmDraws = expected.readUint16LE(), coldDraws = expected.readUint16LE();
    const uint32 seed = expected.readUint32LE(); const byte tail = expected.readByte(), last = expected.readByte();
    if (entropy != 0xa17e || warmDraws != 16 || coldDraws != 16 || expected.size() - expected.pos() != 128) return false;
    Native::GameplayRandom random;
    if (cold) {
        Common::InSaveFile *input = files.openForLoading("nox-native-rng-state.bin"); const bool loaded = input && random.load(*input); delete input;
        if (!loaded || random.seed() != seed || random.tail() != tail || random.last() != last) { error = "Cold native RNG seed/state did not restore"; return false; }
        for (uint i = 0; i < coldDraws; ++i)
            if (!readNativeRandomResult(expected, random, 0, 255)) { error = "Cold native RNG stream diverged from original continuation"; return false; }
        report += "native-rng-state\tphase=cold\toriginal_continuation_draws=16\tcpu_execution=0\n";
    } else {
        if (!random.initializeEntropy(entropy)) return false;
        for (uint i = 0; i < warmDraws; ++i) { byte value; if (!random.next(0, 255, value)) return false; }
        if (random.seed() != seed || random.tail() != tail || random.last() != last) { error = "Native warm RNG stream differs before serialization"; return false; }
        Common::OutSaveFile *output = files.openForSaving("nox-native-rng-state.bin", false); bool saved = output && random.save(*output);
        if (output) { output->finalize(); saved = saved && !output->err(); } delete output;
        if (!saved) return false;
        expected.seek(expected.size()); report += "native-rng-state\tphase=warm\tretained_draws=16\tnative_saved=1\tcpu_execution=0\n";
    }
    return !expected.err() && expected.pos() == expected.size();
}
}
#endif
