/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_CANNON_H
#define NOX_NATIVE_BATTLE_CANNON_H
#include "nox/native_battle_targets.h"
#include "nox/native_battle_rewards.h"
namespace Nox { namespace Native {
enum CannonStep { kCannonNotHere,kCannonTarget,kCannonPowder,kCannonShot };
struct BattleCannonState {
    byte stations[15],phase,station;
    BattleCannonState() : phase(0),station(255) { memset(stations,0,15); }
    bool valid() const {
        if (phase>1 || (phase?station>=15:station!=255)) return false;
        for (uint i=0;i<15;++i) if (stations[i]>2) return false;
        return !phase || stations[station]==1;
    }
    bool used() const {for (uint i=0;i<15;++i) if (stations[i]) return true;return phase!=0;}
    bool save(Common::WriteStream &out) const {
        if (!valid()) return false;
        out.writeUint32BE(MKTAG('N','C','A','N'));out.writeUint16LE(1);
        out.write(stations,15);out.writeByte(phase);out.writeByte(station);return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=23 || in.readUint32BE()!=MKTAG('N','C','A','N') || in.readUint16LE()!=1) return false;
        BattleCannonState next;if (in.read(next.stations,15)!=15) return false;
        next.phase=in.readByte();next.station=in.readByte();if (!next.valid() || in.err()) return false;
        *this=next;return true;
    }
};
struct CannonOutcome {
    byte roll,classification,impactX,impactY;uint16 credit;bool removed,suppressed;
    CannonOutcome() : roll(0),classification(0),impactX(0),impactY(0),credit(0),removed(false),suppressed(false) {}
};
// Separate from readied weapons: authored stations, fire/powder/shot cycle,
// retained reload state on target cancellation, and actor dismissal on impact.
class BattleCannon {
public:
    BattleCannon() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> table;
        if (!archive.readRange("NA/GAME",0x8bb,15,table) || table.size()!=15 || !_rewards.load(archive)) return false;
        memcpy(_positions,table.data(),15);_ready=true;return true;
    }
    bool begin(byte casterCell,BattleCannonState &state,CannonStep &step) const {
        if (!_ready || casterCell>=187 || !state.valid() || state.phase) return false;
        int station=-1;
        // Source also inspects a sixteenth byte, aliasing station0's state.
        // Its range0..2 cannot match a valid cell+34, so it is not a station.
        for (int i=14;i>=0;--i) if (_positions[i]==uint(casterCell)+34) {station=i;break;}
        if (station<0) {step=kCannonNotHere;return true;}
        state.stations[station]=byte((state.stations[station]+1)%3);
        step=state.stations[station]==1?kCannonTarget:state.stations[station]==2?kCannonPowder:kCannonShot;
        if (step==kCannonTarget) {state.phase=1;state.station=byte(station);}
        return true;
    }
    static void cancel(BattleCannonState &state) {state.phase=0;state.station=255;}
    bool fire(byte casterCell,byte targetCell,byte category,bool displaceMiss,
            BattleCannonState &state,GameplayRandom &random,BattleRecords &records,CannonOutcome &outcome,bool protectedTargets=false) const {
        VortexVictim caster,target;
        if (!_ready || !state.valid() || !state.phase || !records.actor(casterCell,caster) || caster.kind!=3 ||
            !records.actor(targetCell,target)) return false;
        BattleRecords next=records;GameplayRandom rng=random;CannonOutcome result;
        if (!rng.next(1,255,result.roll)) return false;
        result.classification=result.roll<128?2:1;result.impactX=targetCell%17;result.impactY=targetCell/17;
        if (result.classification==1) {
            result.impactX=result.impactX?byte(result.impactX-1):1;
            // Shared projectile presentation's source miss displacement uses
            // one further draw only when enabled and not at either edge.
            if (displaceMiss) {
                if (result.impactX && result.impactX!=10) {
                    byte jitter;if (!rng.next(1,255,jitter)) return false;
                    if (jitter>=128) --result.impactX;else ++result.impactX;
                } else if (result.impactX==10) --result.impactX;else ++result.impactX;
            }
        } else {
            if (target.kind<3) {
                const uint16 base=target.stats[5]|uint16(target.stats[8])<<8;
                result.credit=_rewards.credit(base,caster.stats[1],target.stats[1]&127,category==5 && target.stats[16]==188);
                next.credit=uint16(next.credit+result.credit);
            }
            // Credit precedes shared removal's protection refusal. Preserve
            // that ordering even when the protected actor remains in battle.
            // Neither branch zeros HP or performs weapon training.
            result.suppressed=protectedTargets;
            if (!protectedTargets) {
                if (!BattleRoster::dismiss(next,target)) return false;
                result.removed=true;
            }
        }
        cancel(state);records=next;random=rng;outcome=result;return true;
    }
private:
    bool _ready;byte _positions[15];BattleRewardRules _rewards;
};
} }
#endif
