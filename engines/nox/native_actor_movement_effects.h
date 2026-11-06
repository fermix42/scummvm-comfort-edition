/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ACTOR_MOVEMENT_EFFECTS_H
#define NOX_NATIVE_ACTOR_MOVEMENT_EFFECTS_H
#include "nox/native_world_presentation.h"
namespace Nox { namespace Native {
struct ActorMovementEffectState {
    byte active,queued,code,originX,originY;bool originKnown,projectileMode;
    ActorMovementEffectState() : active(0),queued(0),code(0),originX(8),originY(5),originKnown(false),projectileMode(false) {}
    bool extended() const {return originKnown || projectileMode;}
    bool save(Common::WriteStream &out) const {
        out.writeUint32BE(MKTAG('N','A','M','F')); out.writeUint16LE(extended()?2:1); out.writeByte(active); out.writeByte(queued); out.writeByte(code);
        if (extended()) {out.writeByte(originX);out.writeByte(originY);out.writeByte(byte(originKnown)|(byte(projectileMode)<<1));}return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        const int64 size=in.size()-in.pos();if (in.readUint32BE()!=MKTAG('N','A','M','F')) return false;const uint version=in.readUint16LE();
        if ((version!=1 && version!=2) || size!=(version==1?9:12)) return false;
        ActorMovementEffectState next; next.active=in.readByte(); next.queued=in.readByte(); next.code=in.readByte();
        if (version==2) {next.originX=in.readByte();next.originY=in.readByte();const byte flags=in.readByte();if (flags>3 || next.originX>=17 || next.originY>=11) return false;next.originKnown=(flags&1)!=0;next.projectileMode=(flags&2)!=0;}
        if (in.err()) return false; *this=next; return true;
    }
};
struct ActorMovementEffectContext { byte mapType,tile,capability,cell,direction; bool outside; };
struct ActorMovementEffectPlan {
    byte originalDirection,direction,objectState; bool objectChanged;
    ActorMovementEffectState effects;
};
// Movement-side object and ranged-request effects, before graphics and actual
// position commit. A queued effect does not execute projectile/damage logic.
class ActorMovementEffects {
public:
    static bool destination(const ActorMovementEffectContext &input,int &cell) {
        if ((!input.outside && input.cell>=187) || input.direction>4) return false;
        cell=input.cell;
        if (input.direction==4 || input.outside) return true;
        const uint column=input.cell%17,row=input.cell/17;
        if ((input.direction==0 && !row) || (input.direction==1 && row==10) ||
            (input.direction==2 && column==16) || (input.direction==3 && !column)) return false;
        cell+=input.direction==0?-17:input.direction==1?17:input.direction==2?1:-1; return true;
    }
    static bool plan(const ActorMovementEffectContext &input,bool objectPresent,byte objectState,
            const ActorMovementEffectState &before,ActorMovementEffectPlan &output) {
        int cell; if (!destination(input,cell)) return false;
        ActorMovementEffectPlan result; result.originalDirection=result.direction=input.direction; result.objectState=objectState; result.objectChanged=false; result.effects=before;
        if (!input.outside && input.direction<4 && objectPresent && objectState==16) { result.objectState=35; result.objectChanged=true; }
        const bool ranged=input.tile==154 || (input.tile==123 && (input.capability&128));
        if ((input.mapType==0 || input.mapType==5) && ranged && !before.active && !input.outside) {
            const uint column=input.cell%17,row=input.cell/17; byte effect=0;
            // Source E1EF stores these even if this actor does not queue a
            // direction. A later eligible actor can replace only the origin.
            result.effects.originX=byte(column);result.effects.originY=byte(row);result.effects.originKnown=true;
            if (row==5 && column>=4 && column<13) effect=column<8?149:136;
            else if (column==8 && row>=1 && row<10) effect=row>=5?139:138;
            if (effect) { result.effects.code=effect; result.effects.queued=1; result.direction=4; }
        }
        output=result; return true;
    }
    static bool apply(World &world,const WorldScene &scene,const ActorMovementEffectContext &input,
            ActorMovementEffectState &state,byte &direction) {
        if (!world.ready() || world.mapType()!=input.mapType) return false;
        int cell; if (!destination(input,cell)) return false;
        const byte slot=!input.outside && input.direction<4?scene.objects.lookup[cell]:255;
        if (slot!=255 && (slot>252 || (slot&3))) return false;
        const byte original=slot==255?0:(*world.currentTables())[1024+slot+3]; ActorMovementEffectPlan result;
        if (!plan(input,slot!=255,original,state,result)) return false;
        if (result.objectChanged) world._states[world._current].tables[1024+slot+3]=result.objectState;
        state=result.effects; direction=result.direction; return true;
    }
};
} }
#endif
