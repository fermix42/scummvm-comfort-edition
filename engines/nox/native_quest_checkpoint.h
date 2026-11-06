/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_QUEST_CHECKPOINT_H
#define NOX_NATIVE_QUEST_CHECKPOINT_H
#include "nox/native_world.h"
#include "common/memstream.h"
namespace Nox { namespace Native {
// Development checkpoint of these semantic owners only. This does not replace
// the complete game save: turn/light/camera/AI/combat/audio/session state remains
// outside this version and must be integrated before native release acceptance.
inline bool saveQuestCheckpoint(const World &world, const Party &party, const Inventory &inventory,
        const GameplayRandom &random, const SearchState &search, Common::WriteStream &output) {
    Common::MemoryWriteStreamDynamic worldBytes(DisposeAfterUse::YES), partyBytes(DisposeAfterUse::YES), randomBytes(DisposeAfterUse::YES);
    byte records[1536];
    if (!world.save(worldBytes) || !party.save(partyBytes) || !random.save(randomBytes) || !inventory.exportRecords(records, sizeof(records)) ||
            worldBytes.size() > 150000 || partyBytes.size() != 870 || randomBytes.size() != 14) return false;
    output.writeUint32BE(MKTAG('N','Q','S','T')); output.writeUint16LE(1); output.writeUint32LE(worldBytes.size());
    output.write(worldBytes.getData(), worldBytes.size()); output.write(partyBytes.getData(), 870);
    output.write(randomBytes.getData(), 14); output.write(records, 1536); output.writeByte(search.priorLockAttempts);
    return !output.err();
}
inline bool loadQuestCheckpoint(World &world, Party &party, Inventory &inventory, GameplayRandom &random,
        SearchState &search, Common::SeekableReadStream &input) {
    if (input.size() - input.pos() < 10 || input.readUint32BE() != MKTAG('N','Q','S','T') || input.readUint16LE() != 1) return false;
    const uint32 worldSize = input.readUint32LE();
    if (worldSize > 150000 || input.size() - input.pos() != int64(worldSize) + 870 + 14 + 1536 + 1) return false;
    Common::Array<byte> worldBytes; worldBytes.resize(worldSize);
    byte partyBytes[870], randomBytes[14], records[1536];
    if (input.read(worldBytes.data(), worldSize) != worldSize || input.read(partyBytes, 870) != 870 ||
            input.read(randomBytes, 14) != 14 || input.read(records, 1536) != 1536) return false;
    SearchState nextSearch; nextSearch.priorLockAttempts = input.readByte();
    Party nextParty; Inventory nextInventory; GameplayRandom nextRandom;
    Common::MemoryReadStream partyInput(partyBytes, 870), randomInput(randomBytes, 14), worldInput(worldBytes.data(), worldSize);
    if (input.err() || !nextParty.load(partyInput) || !nextRandom.load(randomInput) || !nextInventory.importRecords(records, 1536) || !world.load(worldInput)) return false;
    // World::load is itself transactional. Every other owner has been staged
    // successfully before its commit, so a rejected candidate changes none.
    party = nextParty; inventory = nextInventory; random = nextRandom; search = nextSearch; return true;
}
} }
#endif
