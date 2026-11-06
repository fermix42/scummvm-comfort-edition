/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_ROUTE_RECOVERY_VALIDATION_H
#define NOX_NATIVE_NPC_ROUTE_RECOVERY_VALIDATION_H
#include "nox/native_npc_route_recovery.h"
#include "nox/native_text_glyph_validation.h"
namespace Nox {
inline bool validateNativeNpcRouteRecovery(HdvArchive &archive, Common::SeekableReadStream &input, Common::SeekableReadStream &continuation,
        Common::SaveFileManager &files, bool cold, Common::String &report, Common::String &error) {
    if (input.readUint32BE() != MKTAG('N','R','R','C') || input.readUint32LE() != 5 || input.readUint32LE() != 2304 ||
        continuation.readUint32BE() != MKTAG('N','R','C','C') || continuation.readUint32LE() != 1) return false;
    const uint checkpointGroup = continuation.readUint32LE(),checkpointOperation = continuation.readUint32LE();
    if (checkpointGroup >= 2304 || continuation.pos() != continuation.size()) return false;
    const char *names[6] = {"nox-native-recovery-world.bin","nox-native-recovery-schedule.bin","nox-native-recovery-store.bin","nox-native-recovery-state.bin","nox-native-recovery-rng.bin","nox-native-recovery-text.bin"};
    Native::FixedMessageCatalog catalog; if (!catalog.load(archive)) return false;
    uint total = 0,messages = 0;
    for (uint group = 0; group < (cold ? checkpointGroup + 1 : 2304U); ++group) {
        byte context[6],actor[8],payload[256],fields[256] = {};
        if (input.read(context,6) != 6) return false;
        const byte phase = input.readByte(),initialApproaches = input.readByte();
        if (phase > 1 || (initialApproaches != 0 && initialApproaches != 2) || input.read(actor,8) != 8 || input.read(payload,256) != 256) return false;
        const uint32 seed = input.readUint32LE(); const byte tail = input.readByte(),last = input.readByte(); const uint16 attempts = input.readUint16LE(); const uint count = input.readUint16LE();
        if (cold && group < checkpointGroup) { if (!input.seek(count * 364,SEEK_CUR)) return false; continue; }
        Native::World world(archive); Native::NpcScheduleState schedule; Native::NpcRouteStore store; Native::NpcRouteRecoveryState state; Native::GameplayRandom random; Native::TextGlyphSurface surface;
        if (cold) {
            Common::InSaveFile *in[6] = {}; bool ok = true;
            for (uint i = 0; i < 6; ++i) { in[i] = files.openForLoading(names[i]); ok = ok && in[i] != nullptr; }
            ok = ok && world.load(*in[0]) && schedule.load(*in[1]) && store.load(*in[2]) && state.load(*in[3]) && random.load(*in[4]) && surface.load(*in[5],archive);
            for (uint i = 0; i < 6; ++i) delete in[i]; if (!ok) return false;
        } else {
            byte pages[3584] = {}; memcpy(pages,payload,256); state.motion.movementPhase = phase != 0;
            if (!world.initialize(8,7,24,38,fields,256) || !schedule.initialize(archive) || !store.importPages(pages,3584) || !random.importState(seed,tail,last,attempts) || !surface.initialize(archive,1)) return false;
            Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES); if (!world.save(w) || !schedule.save(s)) return false;
            Common::Array<byte> data; data.resize(w.size()); memcpy(data.data(),w.getData(),w.size()); memcpy(data.data() + 271 + 1280,actor,8);
            Common::MemoryReadStream ws(data.data(),data.size()); if (!world.load(ws)) return false;
            byte npc[572]; memcpy(npc,s.getData(),572); memset(npc + 454,255,56); npc[454] = 77; npc[455] = 0; npc[456] = 1; npc[457] = 0; npc[570] = initialApproaches;
            Common::MemoryReadStream ss(npc,572); if (!schedule.load(ss)) return false;
        }
        for (uint operation = 0; operation < count; ++operation) {
            byte blocked[4],expectedActor[8],expectedRequests[56],expectedPayload[256],preferences[4]; if (input.read(blocked,4) != 4) return false;
            Native::NpcRouteRecoveryInputs inputs; inputs.partyX = input.readByte(); inputs.partyY = input.readByte(); inputs.previewEmpty = input.readByte() != 0; inputs.suppressApproach = false;
            const byte expectedDirection = input.readByte(),hidden = input.readByte(),changed = input.readByte();
            if (input.read(expectedActor,8) != 8 || input.read(expectedRequests,56) != 56 || input.read(expectedPayload,256) != 256) return false;
            const byte relativeX = input.readByte(),relativeY = input.readByte(),movementPhase = input.readByte(); if (input.read(preferences,4) != 4) return false;
            const uint32 afterSeed = input.readUint32LE(); const byte afterTail = input.readByte(),afterLast = input.readByte(); const uint16 afterAttempts = input.readUint16LE();
            const byte approaches = input.readByte(),emitted = input.readByte(),screenCell = input.readByte(); Common::String frame;
            if (!readTextGlyphDigest(input,frame)) return false;
            if (cold && operation < checkpointOperation) continue;
            const bool execute = !cold || operation != checkpointOperation; byte direction = 4; bool message = false;
            if (execute && Native::NpcRouteRecovery::advance(world,schedule,store,0,blocked,inputs,state,random,catalog,surface,direction,message) != Native::kNpcRouteRecoveryAdvanced) {
                error = Common::String::format("Native route recovery failed group%u operation%u",group,operation); return false;
            }
            if ((execute && (direction != expectedDirection || byte(message) != emitted)) || byte(state.motion.hidden) != hidden || byte(state.appearanceChanged) != changed ||
                state.motion.relativeX != relativeX || state.motion.relativeY != relativeY || byte(state.motion.movementPhase) != movementPhase || state.screenCell != screenCell ||
                memcmp(state.preferences,preferences,4) || schedule.approachCount() != approaches || memcmp(world.currentTables()->data() + 1280,expectedActor,8) ||
                memcmp(schedule.requests(),expectedRequests,56) || memcmp(store.data(),expectedPayload,256) || random.seed() != afterSeed || random.tail() != afterTail || random.last() != afterLast || random.generationAttempts() != afterAttempts || textGlyphDigest(surface) != frame) {
                error = Common::String::format("Native route recovery differs group%u operation%u direction%u/%u hidden%u/%u phase%u/%u cell%u/%u count%u/%u message%u/%u RNG%08X/%08X",group,operation,uint(direction),uint(expectedDirection),uint(state.motion.hidden),uint(hidden),uint(state.motion.movementPhase),uint(movementPhase),uint(state.screenCell),uint(screenCell),uint(schedule.approachCount()),uint(approaches),uint(message),uint(emitted),random.seed(),afterSeed); return false;
            }
            Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),s(DisposeAfterUse::YES),r(DisposeAfterUse::YES),n(DisposeAfterUse::YES),g(DisposeAfterUse::YES),t(DisposeAfterUse::YES);
            if (!world.save(w) || !schedule.save(s) || !store.save(r) || !state.save(n) || !random.save(g) || !surface.save(t)) return false;
            Common::MemoryReadStream ws(w.getData(),w.size()),ss(s.getData(),s.size()),rs(r.getData(),r.size()),ns(n.getData(),n.size()),gs(g.getData(),g.size()),ts(t.getData(),t.size());
            if (!world.load(ws) || !schedule.load(ss) || !store.load(rs) || !state.load(ns) || !random.load(gs) || !surface.load(ts,archive)) return false;
            if (!cold && group == checkpointGroup && operation == checkpointOperation) {
                if (!message) return false;
                for (uint i = 0; i < 6; ++i) {
                    Common::OutSaveFile *out = files.openForSaving(names[i],false); if (!out) return false;
                    bool ok = i == 0 ? world.save(*out) : i == 1 ? schedule.save(*out) : i == 2 ? store.save(*out) : i == 3 ? state.save(*out) : i == 4 ? random.save(*out) : surface.save(*out);
                    out->finalize(); ok = ok && !out->err(); delete out; if (!ok) return false;
                }
            }
            if (execute) { ++total; messages += message; }
        }
    }
    if (input.err() || (!cold && (input.pos() != input.size() || total != 34953 || messages != 241))) return false;
    report += cold ? Common::String::format("native-npc-route-recovery\tphase=cold\tactual_message_result_restored=1\tcontinued_actual_moves_RNG=1\tcontinued_operations=%u\tfull_actor_turn=0\tcpu_execution=0\n",total) :
        "native-npc-route-recovery\tphase=warm\tchains=2304\toperations=34953\tactual_approach_messages=241\tactual_message_clear_render_continue=1\tretained_world_requests_cursor_RNG=1\tstate_roundtrip_between_operations=1\tinitial_count0_or_controlled2=1\tfull_actor_turn=0\tcpu_execution=0\n";
    return true;
}
}
#endif
