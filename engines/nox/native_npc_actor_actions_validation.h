/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ACTOR_ACTIONS_VALIDATION_H
#define NOX_NATIVE_NPC_ACTOR_ACTIONS_VALIDATION_H
#include "nox/native_npc_actor_actions.h"
#include "common/memstream.h"
namespace Nox {
inline bool validateNativeNpcActorActions(HdvArchive &archive, Common::SeekableReadStream &input, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','A','A','2') || input.readUint32LE() != 1 || input.readUint32LE() != 2) return false;
    uint total = 0,advanced = 0,needsNavigation = 0;
    for (uint group = 0; group < 2; ++group) {
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(),last = input.readByte(); const uint16 attempts = input.readUint16LE();
        Native::GameplayRandom random; if (!random.importState(seed,tail,last,attempts) || input.readUint32LE() != 12) return false;
        for (uint chain = 0; chain < 12; ++chain) {
            byte record[8],fields[256] = {}; if (input.read(record,8) != 8 || input.readUint32LE() != 64) return false;
            Native::World world(archive); Native::NpcScheduleState schedule;
            if (!world.initialize(8,7,24,38,fields,256) || !schedule.initialize(archive)) return false;
            Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES),npc(DisposeAfterUse::YES);
            if (!world.save(encoded) || !schedule.save(npc)) return false;
            Common::Array<byte> setup; setup.resize(encoded.size()); memcpy(setup.data(),encoded.getData(),setup.size()); memcpy(setup.data() + 271 + 1280,record,8);
            Common::MemoryReadStream worldSetup(setup.data(),setup.size()); if (!world.load(worldSetup)) return false;
            byte goals[572]; if (npc.size() != 572) return false; memcpy(goals,npc.getData(),572); goals[6] = goals[166] = 24;
            Common::MemoryReadStream npcSetup(goals,572); if (!schedule.load(npcSetup)) return false;
            Native::ActorIntentState shared; shared.movementPhase = shared.hidden = shared.appearanceChanged = false; shared.pattern = 0;
            for (uint i = 0; i < 4; ++i) shared.preferences[i] = byte(i);
            for (uint turn = 0; turn < 64; ++turn) {
                byte blocked[4],preferences[4],tables[1792]; if (input.read(blocked,4) != 4) return false;
                const byte unsupported = input.readByte(),expectedDirection = input.readByte(),hidden = input.readByte(),changed = input.readByte(),pattern = input.readByte();
                if (input.read(preferences,4) != 4 || input.read(tables,1792) != 1792) return false;
                const uint32 afterSeed = input.readUint32LE(); const byte afterTail = input.readByte(),afterLast = input.readByte(); const uint16 afterAttempts = input.readUint16LE();
                byte direction = 4;
                const Native::NpcActorActionResult result = Native::NpcActorActions::advance(world,schedule,0,blocked,shared,random,direction);
                if (result != (unsupported ? Native::kNpcActorNeedsNavigation : Native::kNpcActorAdvanced) || direction != expectedDirection ||
                    memcmp(world.currentTables()->data(),tables,1792) || byte(shared.hidden) != hidden || byte(shared.appearanceChanged) != changed ||
                    shared.pattern != pattern || memcmp(shared.preferences,preferences,4) || random.seed() != afterSeed || random.tail() != afterTail ||
                    random.last() != afterLast || random.generationAttempts() != afterAttempts) {
                    uint mismatch = 0; while (mismatch < 1792 && world.currentTables()->data()[mismatch] == tables[mismatch]) ++mismatch;
                    error = Common::String::format("Native NPC logical action differs group%u chain%u turn%u result%u dir%u/%u table%u native%u/source%u hidden%u/%u changed%u/%u pattern%u/%u RNG%08X/%08X tail%u/%u last%u/%u attempts%u/%u",group,chain,turn,uint(result),uint(direction),uint(expectedDirection),mismatch,mismatch < 1792 ? world.currentTables()->data()[mismatch] : 0,mismatch < 1792 ? tables[mismatch] : 0,uint(shared.hidden),uint(hidden),uint(shared.appearanceChanged),uint(changed),uint(shared.pattern),uint(pattern),random.seed(),afterSeed,uint(random.tail()),uint(afterTail),uint(random.last()),uint(afterLast),uint(random.generationAttempts()),uint(afterAttempts)); return false;
                }
                Common::MemoryWriteStreamDynamic checkpoint(DisposeAfterUse::YES); if (!world.save(checkpoint)) return false;
                Native::World restored(archive); Common::MemoryReadStream saved(checkpoint.getData(),checkpoint.size());
                if (!restored.load(saved) || memcmp(restored.currentTables()->data(),tables,1792)) return false;
                // Continue from the actual restored map owner on the next turn.
                Common::MemoryReadStream retained(checkpoint.getData(),checkpoint.size()); if (!world.load(retained)) return false;
                advanced += !unsupported; needsNavigation += unsupported; ++total;
            }
        }
    }
    if (total != 1536 || input.err() || input.pos() != input.size()) return false;
    report += Common::String::format("native-npc-actor-actions\tcases=1536\tretained_chains=24\tadvanced=%u\texplicit_needs_navigation=%u\tactual_map_records=1\tcontinued_from_restored_owner=1\tresolved_collision_setup=1\tinteraction_raster_turn=0\tcpu_execution=0\n",advanced,needsNavigation); return true;
}
}
#endif
