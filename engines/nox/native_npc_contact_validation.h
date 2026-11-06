/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_NPC_CONTACT_VALIDATION_H
#define NOX_NATIVE_NPC_CONTACT_VALIDATION_H
#include "nox/native_npc_contact.h"
#include "common/memstream.h"
#include "common/savefile.h"
namespace Nox {
inline bool validateNativeNpcContact(HdvArchive &archive,Common::SeekableReadStream &input,Common::SaveFileManager &files,bool cold,Common::String &report,Common::String &error) {
    if (input.readUint32BE()!=MKTAG('N','N','C','T') || input.readUint32LE()!=1 || input.readUint32LE()!=1566752) return false;
    Native::NpcContactRules rules; Native::NpcContactState state; byte last=255; uint linked=0;
    if (cold) { Common::InSaveFile *saved=files.openForLoading("nox-native-npc-contact.bin"); const bool ok=saved && state.load(*saved); delete saved; if (!ok) return false; }
    for (uint i=0;i<1566752;++i) {
        const byte type=input.readByte(); Native::WorldActor actor; actor.kind=4; actor.state=input.readByte(); actor.flags=input.readByte();
        const byte slot=input.readByte(),cell=input.readByte(),terrain=input.readByte(); byte before[3],after[3]; input.read(before,3); input.read(after,3);
        if (type!=last) { if (!rules.load(archive,type)) return false; last=type; }
        const uint local=i%195844; const bool chain=local>=195840;
        if (cold && !(type==0 && chain && local>195840)) continue;
        if (!chain || local==195840) { state.availability=before[0]; state.slot=before[1]; state.cell=before[2]; }
        if (state.availability!=before[0] || state.slot!=before[1] || state.cell!=before[2] || !rules.consider(actor,slot,cell,terrain,state) ||
            state.availability!=after[0] || state.slot!=after[1] || state.cell!=after[2]) {
            error=Common::String::format("Native NPC contact differs case%u type%u",i,uint(type)); return false;
        }
        if (chain) ++linked;
        if (chain || !(i&1023)) {
            Common::MemoryWriteStreamDynamic bytes(DisposeAfterUse::YES); if (!state.save(bytes)) return false;
            Common::MemoryReadStream restored(bytes.getData(),bytes.size()); if (!state.load(restored)) return false;
        }
        if (!cold && !type && local==195840) {
            Common::OutSaveFile *saved=files.openForSaving("nox-native-npc-contact.bin",false); if (!saved) return false;
            bool ok=state.save(*saved); saved->finalize(); ok=ok&&!saved->err(); delete saved; if (!ok) return false;
        }
        Native::NpcContactState original=state; actor.state=255;
        if (rules.consider(actor,slot,cell,terrain,state) || state.availability!=original.availability || state.slot!=original.slot || state.cell!=original.cell) return false;
    }
    if (input.err() || input.pos()!=input.size() || linked!=(cold?3U:32U)) return false;
    report+=cold ? "native-npc-contact\tphase=cold\tactual_request_restored=1\tcontinued_requests_refusals=3\tcpu_execution=0\n" :
        "native-npc-contact\tphase=warm\tcases=1566752\tlinked_operations=32\tstyles=8\tabsent_actor_refused=1\tconversation_UI=0\tcpu_execution=0\n"; return true;
}
}
#endif
