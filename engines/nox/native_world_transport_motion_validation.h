/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_TRANSPORT_MOTION_VALIDATION_H
#define NOX_NATIVE_WORLD_TRANSPORT_MOTION_VALIDATION_H
#include "nox/native_world_transport_motion.h"
namespace Nox {
inline bool readTransportCell(Common::ReadStream &in,Native::TransportMoveCell &cell) {
    cell.terrain=in.readByte();cell.objectTile=in.readByte();cell.objectState=in.readByte();const byte actor=in.readByte(),object=in.readByte();
    if (actor>1 || object>1 || in.err()) return false;cell.actor=actor!=0;cell.object=object!=0;return true;
}
inline bool validateNativeTransportMotion(HdvArchive &archive,Common::SeekableReadStream &in,Common::String &report,Common::String &error) {
    if (in.readUint32BE()!=MKTAG('N','T','M','R') || in.readUint16LE()!=1) return false;
    const uint32 count=in.readUint32LE();if (!count || count>200000) return false;
    Native::WorldTransportMotion rules;byte loaded=255;uint blocked=0,interrupted=0,seas=0,protectedSeas=0,farther=0;
    for (uint c=0;c<count;++c) {
        const byte type=in.readByte();if (type!=loaded) {if (!rules.load(archive,type)) return false;loaded=type;}
        Native::TransportMotionContext context;context.picture=in.readByte();context.speed=in.readByte();context.currentTerrain=in.readByte();context.phase=in.readByte();context.movementFlag=in.readByte();
        const byte flags=in.readByte(),exposure=in.readByte();if (flags>15) return false;
        context.ignoreCollision=(flags&1)!=0;context.jumpRequested=(flags&2)!=0;context.jumpBusy=(flags&4)!=0;context.seaProtection=(flags&8)!=0;
        if (!readTransportCell(in,context.near) || !readTransportCell(in,context.far) || !readTransportCell(in,context.leading[0]) || !readTransportCell(in,context.leading[1])) return false;
        const uint32 seed=in.readUint32LE();const byte tail=in.readByte(),last=in.readByte();const uint16 attempts=in.readUint16LE();
        Native::GameplayRandom random;if (!random.importState(seed,tail,last,attempts)) return false;
        const byte expectedBlocked=in.readByte(),expectedInterrupted=in.readByte(),expectedExposure=in.readByte();const uint16 damage=in.readUint16LE();
        const byte seaEvent=in.readByte(),seaImpact=in.readByte(),message=in.readByte(),far=in.readByte();byte expectedRandom[8];if (in.read(expectedRandom,8)!=8) return false;
        Native::WorldTransportState state;state.used=true;state.object=0;state.appearance=context.picture;state.waterExposure=exposure;Native::TransportMotionOutcome result;
        if (!rules.probe(context,state,random,result) || result.requiresInteraction || result.blocked!=(expectedBlocked!=0) || result.interrupted!=(expectedInterrupted!=0) ||
            state.waterExposure!=expectedExposure || result.damage!=damage || result.seaEvent!=(seaEvent!=0) || result.seaImpact!=(seaImpact!=0) || result.message!=message || result.checkedFar!=(far!=0)) {
            error=Common::String::format("Transport motion differs case%u type%u picture%u terrain%u flags%u expected(%u,%u,%u,%u,%u,%u,%u,%u) actual(%u,%u,%u,%u,%u,%u,%u,%u)",
                c,type,context.picture,context.near.terrain,flags,expectedBlocked,expectedInterrupted,expectedExposure,damage,seaEvent,seaImpact,message,far,
                result.blocked,result.interrupted,state.waterExposure,result.damage,result.seaEvent,result.seaImpact,result.message,result.checkedFar);return false;
        }
        byte actual[8];WRITE_LE_UINT32(actual,random.seed());actual[4]=random.tail();actual[5]=random.last();WRITE_LE_UINT16(actual+6,random.generationAttempts());
        if (memcmp(actual,expectedRandom,8)) {error=Common::String::format("Transport RNG differs case%u",c);return false;}
        blocked+=result.blocked;interrupted+=result.interrupted;seas+=result.seaEvent;protectedSeas+=result.seaEvent&&!result.seaImpact;farther+=result.checkedFar;
    }
    if (in.err() || in.pos()!=in.size()) return false;
    report+=Common::String::format("native-transport-motion\tcases=%u\tblocked=%u\tinterrupted=%u\tsea_effects=%u\tprotected_sea_effects=%u\tfarther_probes=%u\tcpu_execution=0\twhole_world_workflow=0\n",count,blocked,interrupted,seas,protectedSeas,farther);return true;
}
}
#endif
