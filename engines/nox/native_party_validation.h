/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PARTY_VALIDATION_H
#define NOX_NATIVE_PARTY_VALIDATION_H
#include "nox/native_party.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativePartyCreation(HdvArchive &archive, Common::SeekableReadStream &input,
        Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','P','C','R') || input.readUint32LE() != 1 || input.readUint32LE() != 105) return false;
    for (uint i = 0; i < 105; ++i) {
        const byte gender = input.readByte(), race = input.readByte(), length = input.readByte();
        if (!length || length > 15) return false;
        Common::String name; for (uint j = 0; j < length; ++j) name += char(input.readByte());
        byte expected[864];
        if (input.read(expected, sizeof(expected)) != sizeof(expected)) return false;
        Native::Party party;
        if (!party.create(archive, name, gender, race) || party.memberCount() != 1) return false;
        for (uint member = 0; member < 6; ++member) {
            if (memcmp(party.member(member)->bytes, expected + member * 128, 128) || memcmp(party.equipment(member)->bytes, expected + 768 + member * 16, 16)) {
                error = Common::String::format("Native creation differs from original at case%u", i); return false;
            }
        }
        Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES), after(DisposeAfterUse::YES);
        if (!party.save(before) || party.create(archive, "REPLACE", 0, 0) || !party.save(after) || before.size() != after.size() || memcmp(before.getData(), after.getData(), before.size())) return false;
    }
    Native::Party refused;
    if (refused.create(archive, " ", 0, 0) || refused.create(archive, "", 0, 0) || refused.create(archive, "ABCDEFGHIJKLMNOP", 0, 0) ||
        refused.create(archive, "NAME", 3, 0) || refused.create(archive, "NAME", 0, 5) || refused.ready()) return false;
    if (input.err() || input.pos() != input.size()) return false;
    report += "native-party-creation\tcases=105\tgenders=3\traces=5\tfull_records=864\tatomic_reinitialize_refusal=1\tcpu_execution=0\n";
    return true;
}
inline bool validateNativePartyPersistence(HdvArchive &archive, Common::SaveFileManager &files,
        bool cold, Common::String &report, Common::String &error) {
    Native::Party party;
    if (cold) {
        Common::InSaveFile *input = files.openForLoading("nox-native-party-state.bin");
        const bool loaded = input && party.load(*input); delete input;
        if (!loaded || party.memberCount() != 1 || party.member(0)->health() != 43 || !party.damage(0, 43) ||
            party.member(0)->health() || party.member(0)->bytes[0] != 255) { error = "Cold native party damage continuation failed"; return false; }
        report += "native-party\tphase=cold\tfull_records_restored=1\tdamage_continued=1\tcpu_execution=0\n"; return true;
    }
    if (!party.create(archive, "TRACE", 0, 0) || party.member(0)->health() != 50 || !party.damage(0, 7) || party.member(0)->health() != 43 || party.damage(1, 1)) return false;
    Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES), after(DisposeAfterUse::YES);
    if (!party.save(before)) return false;
    Common::MemoryReadStream truncated(before.getData(), before.size() - 1);
    if (party.load(truncated) || !party.save(after) || before.size() != after.size() || memcmp(before.getData(), after.getData(), before.size())) return false;
    Common::OutSaveFile *output = files.openForSaving("nox-native-party-state.bin", false);
    bool saved = output && party.save(*output);
    if (output) { output->finalize(); saved = saved && !output->err(); } delete output;
    if (!saved) return false;
    report += "native-party\tphase=warm\tcreated=1\tdamage=1\tatomic_load_refusal=1\tcomplete_new_game=0\tcpu_execution=0\n"; return true;
}
}
#endif
