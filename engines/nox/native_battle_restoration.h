/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_RESTORATION_H
#define NOX_NATIVE_BATTLE_RESTORATION_H
#include "nox/native_battle_roster.h"
#include "nox/native_graphics.h"
namespace Nox { namespace Native {
enum RestorationEffect { kRestorationApplied,kRestorationAlreadyArmored,kRestorationNotDead,kRestorationOccupied };
struct RestorationOutcome {
    RestorationEffect effect;uint affected,moved;bool spentMana,cancelled;
    RestorationOutcome() : effect(kRestorationApplied),affected(0),moved(0),spentMana(false),cancelled(false) {}
};
// One connected standard-player restoration transaction. The book/INT/MP
// preparation owner supplies the accepted spell/cost and collected party target.
// UI suspension/turn scheduling remain callers; no program addresses participate.
class BattleRestoration {
public:
    BattleRestoration() : _ready(false) { memset(_blocking,0,sizeof(_blocking)); }
    bool load(HdvArchive &archive) {
        Common::Array<byte> game;if (!archive.read("NA/GAME",game)) return false;
        for (uint type=0;type<9;++type) if (type!=1) {
            MapAssetRange range;Common::Array<byte> footer;
            if (!resolveMapAssets(game,byte(type),range) || !archive.readRange(range.resource,range.offset+range.size,72,footer)) return false;
            memcpy(_blocking[type],footer.data()+6,4);
        }
        _ready=true;return true;
    }
    const byte *blocking(byte category) const { return _ready && category<9 && category!=1?_blocking[category]:nullptr; }
    bool resolve(byte spell,byte cost,byte caster,byte target,bool cancelled,byte party,byte category,
            const byte *terrain,uint terrainSize,const byte *objects,uint objectSize,byte engagement,
            byte *armorBackup,uint backupSize,BattleRecords &records,RestorationOutcome &outcome) const {
        if (!_ready || !party || party>6 || caster>=party || category>8 || category==1 ||
            !armorBackup || backupSize!=6 || records.stats[uint(caster)*128]==255 ||
            (spell!=32 && spell!=48 && spell!=49 && spell!=50 && spell!=51 && spell!=52)) return false;
        const bool targeted=spell>=49 && spell<=51;
        if (cancelled) { if (!targeted) return false;RestorationOutcome result;result.cancelled=true;outcome=result;return true; }
        if ((targeted && target>=party) || records.stats[uint(caster)*128+4]<cost ||
            ((spell==50 || spell==51) && records.stats[uint(target)*128]==255)) return false;
        BattleRecords next=records;byte backups[6];memcpy(backups,armorBackup,6);RestorationOutcome result;
        const bool before=spell==32 || spell==48 || spell==52;
        if (before) next.stats[uint(caster)*128+4]-=cost;
        bool applied=true;
        if (spell==32) armor(next,caster,backups,result);
        else if (spell==48) applied=engage(next,party,caster,category,terrain,terrainSize,objects,objectSize,engagement,result);
        else if (spell==49) applied=resurrect(next,target,0,true,result);
        else if (spell==50 || spell==51) heal(next,caster,target,spell==51?128:0,result);
        else divine(next,party,0,result);
        if (!applied) return false;
        // Heal and Resurrection commit after the effect/message, including
        // Resurrection's nondead and occupied-cell refusals.
        if (!before) next.stats[uint(caster)*128+4]-=cost;
        result.spentMana=true;records=next;memcpy(armorBackup,backups,6);outcome=result;return true;
    }
    static void armor(BattleRecords &battle,byte caster,byte *backups,RestorationOutcome &outcome) {
        if (backups[caster]) { outcome.effect=kRestorationAlreadyArmored;return; }
        byte *member=battle.stats+uint(caster)*128;backups[caster]=member[7];
        member[7]=MIN<byte>(byte(member[7]+(member[18]>>2)),229);outcome.affected=1;
    }
    static void heal(BattleRecords &battle,byte caster,byte target,byte mode,RestorationOutcome &outcome) {
        const byte *source=battle.stats+uint(caster)*128;byte *member=battle.stats+uint(target)*128;
        const uint16 maximum=READ_LE_UINT16(member+72);
        const uint16 sum=uint16(READ_LE_UINT16(member+2)+(READ_LE_UINT16(source+72)/2));
        const bool full=(mode&128) || sum>=maximum;
        WRITE_LE_UINT16(member+2,full?maximum:sum);
        if (full && (mode&127)) member[4]=member[18];
        outcome.affected=1;
    }
    static bool resurrect(BattleRecords &battle,byte target,byte mode,bool tactical,RestorationOutcome &outcome) {
        byte *member=battle.stats+uint(target)*128;
        if (member[0]!=255) { outcome.effect=kRestorationNotDead;return true; }
        byte *actor=battle.compact+uint(target)*4;
        if (tactical) {
            if (actor[0]>=17 || actor[1]>=11) return false;
            const uint cell=uint(actor[1])*17+actor[0];
            if (battle.indices[cell]!=255) { outcome.effect=kRestorationOccupied;return true; }
            actor[3]=0;battle.indices[cell]=byte(target*4);battle.kinds[cell]=3;
        }
        member[0]=0;WRITE_LE_UINT16(member+2,READ_LE_UINT16(member+72));
        if (mode) member[4]=member[18];outcome.affected=1;return true;
    }
    static void divine(BattleRecords &battle,byte party,byte mode,RestorationOutcome &outcome) {
        for (uint i=0;i<party;++i) {
            byte *member=battle.stats+i*128;if (member[0]==255) continue;
            WRITE_LE_UINT16(member+2,READ_LE_UINT16(member+72));if (mode) member[4]=member[18];++outcome.affected;
        }
    }
    bool engage(BattleRecords &battle,byte party,byte caster,byte category,const byte *terrain,uint terrainSize,
            const byte *objects,uint objectSize,byte engagement,RestorationOutcome &outcome) const {
        if (!_ready || !terrain || terrainSize!=187 || !objects || objectSize!=187 || category>8 || category==1) return false;
        for (uint pass=0;pass<3;++pass) for (uint i=0;i<party;++i) {
            byte *actor=battle.compact+i*4;if (actor[3]==255 || !actor[1]) continue;
            if (actor[0]>=17 || actor[1]>=11) return false;
            const uint old=uint(actor[1])*17+actor[0],cell=old-17;const byte tile=terrain[cell];
            if (battle.indices[cell]!=255 || objects[cell]!=255 ||
                (tile>=_blocking[category][0] && tile<_blocking[category][1]) ||
                (tile>=_blocking[category][2] && tile<_blocking[category][3])) continue;
            --actor[1];battle.indices[cell]=byte(i*4);battle.kinds[cell]=3;
            battle.indices[old]=255;battle.kinds[old]=255;++outcome.moved;
        }
        battle.stats[uint(caster)*128+12]=engagement;outcome.affected=outcome.moved;return true;
    }
private:
    bool _ready;byte _blocking[9][4];
};
} }
#endif
