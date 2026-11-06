/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_REWARDS_H
#define NOX_NATIVE_BATTLE_REWARDS_H
#include "nox/native_vortex_cast.h"
namespace Nox { namespace Native {
struct BattleRewardOutcome {
    uint16 pool,perMember;
    BattleRewardOutcome() : pool(0),perMember(0) {}
};
// Source reward arithmetic shared by lethal actions and battle results.
// Authored percentages are data; no instruction addresses execute here.
class BattleRewardRules {
public:
    BattleRewardRules() : _ready(false),_special(0) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> data,special;
        if (!archive.readRange("NA/BS.AUX.RTN.BK2",0x2688,6,data) || data.size()!=6 ||
            !archive.readRange("NA/BS.AUX.RTN.BK2",0x2744,1,special) || special.size()!=1) return false;
        memcpy(_percent,data.data(),6);_special=special[0];_ready=true;return true;
    }
    uint16 credit(uint16 base,byte attackerLevel,byte targetLevel,bool special) const {
        const uint difference=MIN<uint>(ABS(int(attackerLevel)-int(targetLevel)),5);
        const byte percent=special?_special:_percent[difference];
        const uint16 adjustment=uint16(uint32(base&32767)*percent)/100;
        return special || attackerLevel>=targetLevel?uint16(base-adjustment):uint16(base+adjustment);
    }
    bool distribute(BattleRecords &records,byte partyCount,byte category,bool fled,uint16 &gold,
            const byte *armorBackup,uint backupLength,BattleRewardOutcome &outcome) const {
        if (!_ready || !partyCount || partyCount>6 || !armorBackup || backupLength!=6) return false;
        BattleRecords next=records;BattleRewardOutcome result;result.pool=next.credit;
        for (uint i=0;i<partyCount;++i) if (armorBackup[i]) next.stats[i*128+7]=armorBackup[i];
        if (fled) { gold>>=1;records=next;outcome=result;return true; }
        uint member=0;
        // The source walks all32 nonparty actor slots and uses these exact
        // state predicates, not a substituted HP-based eligibility test.
        for (uint i=0;i<32;++i) {
            const byte *actor=next.extended+i*8;
            if (actor[4]==3 || !actor[3] || actor[3]==255) continue;
            const byte *target=next.stats+768+i*32;
            const uint16 base=target[5]|uint16(target[8])<<8;
            const bool special=category==5 && target[16]==188;
            next.credit=uint16(next.credit+credit(base,next.stats[member*128+1],target[1]&127,special));
            if (++member==partyCount) member=0;
        }
        result.pool=next.credit;
        result.perMember=MAX<uint16>(1,uint16(next.credit/partyCount+(next.credit%partyCount?1:0)));
        for (uint i=0;i<partyCount;++i) {
            byte *record=next.stats+i*128;
            WRITE_LE_UINT16(record+5,uint16(READ_LE_UINT16(record+5)+result.perMember));
        }
        records=next;outcome=result;return true;
    }
private:
    bool _ready;byte _percent[6],_special;
};
} }
#endif
