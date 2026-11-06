/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_ATTACK_H
#define NOX_NATIVE_BATTLE_ATTACK_H
#include "nox/native_battle_targets.h"
#include "common/memstream.h"
namespace Nox { namespace Native {
struct BattleAttackState {
    byte hands[2],current,acted,phase,rememberedKind[6],rememberedIndex[6];bool partyTargeting;
    BattleAttackState() : current(0),acted(0),phase(0),partyTargeting(false) {
        memset(hands,0,2);memset(rememberedKind,255,6);memset(rememberedIndex,255,6);
    }
    void resetAction() { hands[0]=hands[1]=current=acted=phase=0; }
    bool valid() const { return hands[0]<=2 && hands[1]<=2 && current<2 && acted<=2 && phase<=3; }
    bool save(Common::WriteStream &out) const {
        if (!valid()) return false;out.writeUint32BE(MKTAG('N','A','T','K'));out.writeUint16LE(1);
        out.write(hands,2);out.writeByte(current);out.writeByte(acted);out.writeByte(phase);
        out.write(rememberedKind,6);out.write(rememberedIndex,6);out.writeByte(partyTargeting?1:0);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=24 || in.readUint32BE()!=MKTAG('N','A','T','K') || in.readUint16LE()!=1) return false;
        BattleAttackState next;if (in.read(next.hands,2)!=2) return false;
        next.current=in.readByte();next.acted=in.readByte();next.phase=in.readByte();
        if (in.read(next.rememberedKind,6)!=6 || in.read(next.rememberedIndex,6)!=6) return false;
        const byte targeting=in.readByte();if (targeting>1 || !next.valid() || in.err()) return false;next.partyTargeting=targeting!=0;
        *this=next;return true;
    }
};
// One Attack command, including both hand continuations and cancellation after
// a committed hand. Phase1 awaits a target,2 retires the action,3 cancels freely.
class BattleAttack {
public:
    static bool begin(const BattleRecords &battle,byte casterCell,bool partyTargeting,BattleAttackState &state,byte &target) {
        if (state.phase) return false;VortexVictim caster;if (!battle.actor(casterCell,caster) || caster.kind!=3) return false;
        state.resetAction();state.phase=1;
        for (uint hand=0;hand<2;++hand) { byte kind,range;if (!BattleTargets::hand(caster,hand!=0,kind,range)) state.hands[hand]=2; }
        return select(battle,casterCell,partyTargeting,state,target);
    }
    static bool switchHand(const BattleRecords &battle,byte casterCell,bool partyTargeting,BattleAttackState &state,byte &target) {
        if (state.phase!=1) return false;
        if (!state.hands[state.current^1]) state.current^=1;
        return select(battle,casterCell,partyTargeting,state,target);
    }
    static bool cycle(const BattleRecords &battle,byte casterCell,bool partyTargeting,byte direction,const BattleAttackState &state,byte &target) {
        VortexVictim caster;byte kind,range;
        return state.phase==1 && battle.actor(casterCell,caster) && BattleTargets::hand(caster,state.current!=0,kind,range) &&
               BattleTargets::cycle(battle,casterCell,range,partyTargeting,direction,target);
    }
    static bool used(const BattleRecords &battle,byte casterCell,bool partyTargeting,BattleAttackState &state,byte &nextTarget) {
        if (state.phase!=1 || state.hands[state.current]) return false;
        VortexVictim caster;if (!battle.actor(casterCell,caster) || caster.kind!=3 || caster.index/4>=6) return false;
        // A killed target has already left lookup. Its identity was captured
        // by the caller before resolution through remember(), below.
        state.hands[state.current]=1;++state.acted;return select(battle,casterCell,partyTargeting,state,nextTarget);
    }
    static bool remember(const BattleRecords &battle,byte casterCell,byte targetCell,BattleAttackState &state) {
        VortexVictim caster,target;
        if (!battle.actor(casterCell,caster) || caster.kind!=3 || !battle.actor(targetCell,target)) return false;
        const uint member=caster.index/4;if (member>=6) return false;
        state.rememberedKind[member]=battle.kinds[targetCell];state.rememberedIndex[member]=target.index;return true;
    }
    static void cancel(BattleAttackState &state) { state.phase=state.acted?2:3; }
private:
    static bool select(const BattleRecords &battle,byte casterCell,bool partyTargeting,BattleAttackState &state,byte &target) {
        VortexVictim caster;if (!battle.actor(casterCell,caster) || caster.kind!=3) return false;
        for (uint attempt=0;attempt<2;++attempt) {
            if (state.hands[state.current]) state.current^=1;
            if (state.hands[state.current]) break;
            byte kind,range;if (!BattleTargets::hand(caster,state.current!=0,kind,range)) return false;
            const uint member=caster.index/4;
            if (!BattleTargets::initial(battle,casterCell,range,partyTargeting,state.rememberedKind[member],state.rememberedIndex[member],target)) return false;
            if (target!=255) { state.phase=1;return true; }
            state.hands[state.current]=2;state.current^=1;
        }
        target=255;state.phase=state.acted?2:3;return true;
    }
};
} }
#endif
