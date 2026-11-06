/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_WORLD_TRANSPORT_H
#define NOX_NATIVE_WORLD_TRANSPORT_H
#include "common/scummsys.h"
#include "common/stream.h"
namespace Nox { namespace Native {
// Persistent control metadata is separate from the original map's four-byte
// transport objects. Objects retain position, authored picture and skiff count.
struct WorldTransportState {
    byte object,appearance,baseAppearance,statusAppearance,speed,pace,special,allocator,waterExposure,moveCount,pendingDirection,pendingSteps;
    bool used,alarm,jumpRequested,defeated,alignment;
    WorldTransportState() : object(255),appearance(246),baseAppearance(246),statusAppearance(246),speed(0),pace(0),special(0),allocator(192),waterExposure(0),moveCount(0),pendingDirection(255),pendingSteps(0),used(false),alarm(false),jumpRequested(false),defeated(false),alignment(false) {}
    bool valid() const {return (object==255 || !(object&3)) && (!used?object==255:true) &&
        (pendingSteps?(pendingDirection<4 && pendingSteps<=4 && !defeated):pendingDirection==255);}
    bool save(Common::WriteStream &out) const {
        if (!valid()) return false;out.writeUint32BE(MKTAG('N','T','R','N'));out.writeUint16LE(2);
        out.writeByte(object);out.writeByte(appearance);out.writeByte(baseAppearance);out.writeByte(statusAppearance);
        out.writeByte(speed);out.writeByte(pace);out.writeByte(special);out.writeByte(allocator);out.writeByte(waterExposure);
        out.writeByte(used);out.writeByte(alarm);out.writeByte(jumpRequested);out.writeByte(moveCount);out.writeByte(pendingDirection);out.writeByte(pendingSteps);out.writeByte(defeated);out.writeByte(alignment);return !out.err();
    }
    bool load(Common::ReadStream &in) {
        if (in.readUint32BE()!=MKTAG('N','T','R','N')) return false;
        const uint16 version=in.readUint16LE();if (version!=1 && version!=2) return false;
        WorldTransportState next;next.object=in.readByte();next.appearance=in.readByte();next.baseAppearance=in.readByte();next.statusAppearance=in.readByte();
        next.speed=in.readByte();next.pace=in.readByte();next.special=in.readByte();next.allocator=in.readByte();next.waterExposure=in.readByte();
        const byte wasUsed=in.readByte(),hasAlarm=in.readByte();if (wasUsed>1 || hasAlarm>1 || in.err()) return false;
        next.used=wasUsed!=0;next.alarm=hasAlarm!=0;
        if (version==2) {
            const byte jump=in.readByte();if (jump>1) return false;next.jumpRequested=jump!=0;
            next.moveCount=in.readByte();next.pendingDirection=in.readByte();next.pendingSteps=in.readByte();
            const byte dead=in.readByte();if (dead>1) return false;next.defeated=dead!=0;
            const byte align=in.readByte();if (align>1) return false;next.alignment=align!=0;
        }
        if (!next.valid() || in.err()) return false;*this=next;return true;
    }
};
// Atomic map transition cargo; no player input or save may observe the
// resource-load boundary between detaching and restoring this record.
struct TransportCargo {
    bool active;byte record[4];
    TransportCargo() : active(false) {memset(record,0,4);}
};
struct TransportControlContext {
    byte map,ownership,x,y,overlap;
    byte *objects,*npcs;
    const byte *footer;
};
enum TransportControlResult {kTransportChanged,kTransportRefused,kTransportInvalid};
struct TransportControlOutcome {
    byte x,y;bool alignOrigin,refresh;
    TransportControlOutcome() : x(0),y(0),alignOrigin(false),refresh(false) {}
};
// Complete record/control part of original B/X. The world owner must apply
// origin alignment through its ordered cache, actor, hazard and viewport
// operations, then resume normal world input. This is not a movement dispatcher.
class WorldTransportControl {
public:
    static bool requestJump(WorldTransportState &state) {
        if (!state.valid() || (state.appearance!=127 && state.appearance!=238)) return false;
        state.used=true;state.jumpRequested=true;state.pace=2;return true;
    }
    static bool yell(WorldTransportState &state,byte &message) {
        if (!state.valid()) return false;state.jumpRequested=false;message=255;
        const byte picture=state.appearance;
        if (picture!=127 && picture!=238 && picture!=128) return false;
        state.used=true;
        const byte speed=state.pace?0:picture==128?4:2;
        if (picture==128) {state.special=speed;if (speed) message=39;}
        state.pace=state.speed=speed;return true;
    }
    static TransportControlResult command(bool board,TransportControlContext &context,WorldTransportState &state,TransportControlOutcome &outcome) {
        if (!context.objects || !context.npcs || !context.footer || !state.valid() ||
            (context.overlap!=255 && (context.overlap&3))) return kTransportInvalid;
        byte objects[256],npcs[256];memcpy(objects,context.objects,256);memcpy(npcs,context.npcs,256);
        TransportControlContext next=context;next.objects=objects;next.npcs=npcs;WorldTransportState controls=state;TransportControlOutcome result;
        result.x=context.x;result.y=context.y;
        const TransportControlResult status=board?boarding(next,controls,result):leaving(next,controls,result);
        if (status==kTransportInvalid) return status;
        // Even a refused full-frigate skiff exit has source control effects:
        // speed and pace were already cleared before the capacity test.
        memcpy(context.objects,objects,256);memcpy(context.npcs,npcs,256);state=controls;outcome=result;return status;
    }
    static bool createMount(byte x,byte y,byte picture,byte *objects,uint count,WorldTransportState &state) {
        if (!objects || count!=256 || !state.valid() || state.object!=255 || (picture!=127 && picture!=238)) return false;
        return place(x,y,picture,0,objects,state);
    }
    static bool detachForTravel(byte *objects,uint count,const WorldTransportState &state,TransportCargo &cargo) {
        if (!objects || count!=256 || !state.valid()) return false;
        TransportCargo next;
        if (state.object!=255) {
            if (!state.used || !supported(objects[state.object+2]) || objects[state.object+2]!=state.appearance) return false;
            next.active=true;memcpy(next.record,objects+state.object,4);memset(objects+state.object,0,4);
        }
        cargo=next;return true;
    }
    static bool restoreAfterTravel(byte x,byte y,byte *objects,uint count,WorldTransportState &state,const TransportCargo &cargo) {
        if (!objects || count!=256 || !state.valid()) return false;
        if (!cargo.active) return state.object==255;
        if (state.object==255 || !state.used || !supported(cargo.record[2]) || cargo.record[2]!=state.appearance) return false;
        return place(x,y,cargo.record[2],cargo.record[3],objects,state);
    }
private:
    static bool place(byte x,byte y,byte picture,byte count,byte *objects,WorldTransportState &state) {
        WorldTransportState next=state;byte copy[256];memcpy(copy,objects,256);
        const byte slot=allocate(copy,next.allocator);copy[slot]=x;copy[slot+1]=y;copy[slot+2]=picture;copy[slot+3]=count;
        next.used=true;next.object=slot;next.appearance=picture;
        if (!next.valid()) return false;memcpy(objects,copy,256);state=next;return true;
    }
    static byte allocate(byte *objects,byte &cursor) {
        // GAME8D5A: exact four-zero tail search, then original rotating
        // overwrite cursor. A carry allocatesC0 while retaining wrapped0.
        for (uint slot=192;slot<256;slot+=4) if (!objects[slot] && !objects[slot+1] && !objects[slot+2] && !objects[slot+3]) return byte(slot);
        const uint next=uint(cursor)+4;cursor=byte(next);return next>255?192:cursor;
    }
    static bool supported(byte picture) {return picture==119 || picture==123 || picture==124 || picture==127 || picture==128 || picture==238;}
    static void alarm(TransportControlContext &c,WorldTransportState &state) {
        state.alarm=true;
        for (uint slot=0;slot<256;slot+=8) {
            const byte picture=c.npcs[slot+2];
            if ((picture>=c.footer[64] && picture<c.footer[65]) || picture==c.footer[66] || picture==123) c.npcs[slot+6]=255;
        }
    }
    static TransportControlResult boarding(TransportControlContext &c,WorldTransportState &state,TransportControlOutcome &out) {
        if (c.overlap==255) return kTransportRefused;
        byte *object=c.objects+c.overlap;const byte picture=object[2];if (!supported(picture)) return kTransportRefused;
        const bool town=c.map>=46 && c.map<50;
        if (town && ((picture==124 && !c.ownership) || (picture==123 && !(c.ownership&64)) || (picture==119 && !(c.ownership&128)))) alarm(c,state);
        if (picture==119) {
            if (state.object!=255 && c.objects[state.object+2]==124) {
                if (object[3]&2) return kTransportRefused;
                object[3]=byte((object[3]&1)+1);memset(c.objects+state.object,0,4);
            }
            const byte dx=byte(object[0]-c.x),dy=byte(object[1]-c.y);
            if ((dx!=0 && dx!=255) || (dy!=0 && dy!=255)) return kTransportInvalid;
            out.x=object[0];out.y=object[1];out.alignOrigin=dx || dy;out.refresh=true;
        }
        state.used=true;state.object=c.overlap;state.appearance=picture;return kTransportChanged;
    }
    static void foot(WorldTransportState &state) {state.appearance=state.statusAppearance=state.baseAppearance;}
    static TransportControlResult leaving(TransportControlContext &c,WorldTransportState &state,TransportControlOutcome &out) {
        if (state.object==255) return kTransportRefused;
        const byte old=state.object;byte *object=c.objects+old;const byte picture=object[2];
        if (!supported(picture)) return kTransportInvalid;
        state.used=true;state.object=255;state.pace=state.speed=0;
        if (picture==128) {foot(state);state.special=0;return kTransportChanged;}
        if (picture==119 && (object[3]&3)) {
            object[3]=object[3]&2?1:0;
            const byte slot=allocate(c.objects,state.allocator);
            c.objects[slot]=c.x;c.objects[slot+1]=c.y;c.objects[slot+2]=124;c.objects[slot+3]=0;
            state.object=byte(slot);state.appearance=124;return kTransportChanged;
        }
        if (picture==124 && c.overlap!=255 && c.objects[c.overlap+2]==119) {
            byte *frigate=c.objects+c.overlap;
            if (frigate[3]&2) {state.object=old;return kTransportRefused;}
            frigate[3]=byte((frigate[3]&1)+1);memset(c.objects+old,0,4);
        }
        foot(state);out.refresh=true;return kTransportChanged;
    }
};
} }
#endif
