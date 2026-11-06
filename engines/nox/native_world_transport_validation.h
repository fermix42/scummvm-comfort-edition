/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_TRANSPORT_VALIDATION_H
#define NOX_NATIVE_WORLD_TRANSPORT_VALIDATION_H
#include "nox/native_world_transport.h"
#include "common/memstream.h"
namespace Nox {
inline bool readTransportControl(Common::ReadStream &in,Native::WorldTransportState &state) {
    state.object=in.readByte();state.appearance=in.readByte();state.baseAppearance=in.readByte();state.statusAppearance=in.readByte();
    state.speed=in.readByte();state.pace=in.readByte();state.special=in.readByte();state.allocator=in.readByte();state.waterExposure=in.readByte();
    const byte used=in.readByte(),alarm=in.readByte();if (used>1 || alarm>1) return false;
    state.used=used!=0;state.alarm=alarm!=0;return state.valid() && !in.err();
}
inline bool validateNativeTransportControl(HdvArchive &,Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    if (in.readUint32BE()!=MKTAG('N','T','C','R')) return false;
    const uint16 version=in.readUint16LE();if (version!=1 && version!=2) return false;
    const uint32 count=in.readUint32LE();if (!count || count>10000) return false;
    uint boarded=0,left=0,refused=0,alignments=0;
    for (uint c=0;c<count;++c) {
        const byte board=in.readByte();if (board>1) return false;
        Native::TransportControlContext context;context.map=in.readByte();context.ownership=in.readByte();context.x=in.readByte();context.y=in.readByte();context.overlap=in.readByte();
        Native::WorldTransportState state,expected;
        byte objects[256],npcs[256],footer[72],expectedObjects[256],expectedNpcs[256];
        if (!readTransportControl(in,state) || in.read(objects,256)!=256 || in.read(npcs,256)!=256 || in.read(footer,72)!=72) return false;
        const byte changed=in.readByte(),x=in.readByte(),y=in.readByte();
        if (changed>1 || !readTransportControl(in,expected) || in.read(expectedObjects,256)!=256 || in.read(expectedNpcs,256)!=256) return false;
        context.objects=objects;context.npcs=npcs;context.footer=footer;Native::TransportControlOutcome outcome;
        const Native::TransportControlResult result=Native::WorldTransportControl::command(board!=0,context,state,outcome);
        if (result==Native::kTransportInvalid || (result==Native::kTransportChanged)!=(changed!=0) || outcome.x!=x || outcome.y!=y ||
            memcmp(objects,expectedObjects,256) || memcmp(npcs,expectedNpcs,256)) {
            error=Common::String::format("Transport records/origin/refusal differ case%u",c);return false;
        }
        Common::MemoryWriteStreamDynamic actual(DisposeAfterUse::YES),wanted(DisposeAfterUse::YES);
        if (!state.save(actual) || !expected.save(wanted) || actual.size()!=wanted.size() || memcmp(actual.getData(),wanted.getData(),actual.size())) {
            error=Common::String::format("Transport control metadata differs case%u",c);return false;
        }
        Common::MemoryReadStream restore(actual.getData(),actual.size());Native::WorldTransportState continued;
        if (!continued.load(restore) || restore.pos()!=restore.size()) return false;
        if (changed) {if (board) ++boarded;else ++left;} else ++refused;
        if (outcome.alignOrigin) ++alignments;
    }
    if (version==2) {
        if (in.readUint32BE()!=MKTAG('N','P','A','C')) return false;
        const uint32 paceCount=in.readUint32LE();if (!paceCount || paceCount>1000) return false;
        for (uint c=0;c<paceCount;++c) {
            Native::WorldTransportState state;state.appearance=in.readByte();state.object=state.appearance==246?255:0;state.used=state.object!=255;
            state.pace=in.readByte();state.speed=in.readByte();state.special=in.readByte();state.jumpRequested=in.readByte()!=0;
            const byte command=in.readByte(),pace=in.readByte(),speed=in.readByte(),special=in.readByte(),jump=in.readByte(),accepted=in.readByte(),message=in.readByte();
            byte notice=255;const bool result=command?Native::WorldTransportControl::yell(state,notice):Native::WorldTransportControl::requestJump(state);
            if (command>1 || result!=(accepted!=0) || state.pace!=pace || state.speed!=speed || state.special!=special || state.jumpRequested!=(jump!=0) || notice!=message) {
                error=Common::String::format("Transport J/Y control differs case%u",c);return false;
            }
        }
        report+=Common::String::format("native-transport-pace\tcases=%u\tcpu_execution=0\twhole_world_workflow=0\n",paceCount);
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report+=Common::String::format("native-transport-control\tcases=%u\tboarded=%u\tleft=%u\trefused=%u\torigin_alignments=%u\tstate_roundtrips=%u\tcpu_execution=0\twhole_world_workflow=0\n",count,boarded,left,refused,alignments,count);return true;
}
}
#endif
