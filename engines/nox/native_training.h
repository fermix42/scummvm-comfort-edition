/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_TRAINING_H
#define NOX_NATIVE_TRAINING_H
#include "nox/native_new_game.h"
namespace Nox { namespace Native {
enum TrainingResult {kTrainingApplied,kTrainingCancelled,kTrainingNoPractice,kTrainingPoor,kTrainingInvalid};
struct TrainingQuote {
    byte skill,available,current;uint16 price;
    TrainingQuote() : skill(0),available(0),current(0),price(0) {}
};
// SRTN.NPC.TALK training overlay1561: one complete payment/record commit.
// Member/quantity input, authored presentation and continuation are separate.
class TrainingService {
public:
    static bool quote(HdvArchive &archive,const NewGame &game,byte request,byte member,TrainingQuote &output) {
        if (!game.ready() || !request || member>=game.party.memberCount()) return false;
        byte skill=0;for (byte bits=request;bits>>=1;) ++skill;
        // Six authored skill triplets; never interpret adjacent record bytes.
        if (skill>=6) return false;
        const CharacterRecord *record=game.party.member(member);const byte level=record->bytes[1];
        if (record->bytes[0]==255 || !level || level>10) return false;
        Common::Array<byte> data;if (!archive.readRange("NA/SRTN.NPC.TALK",0x1561+0x2f8+uint(level)*2,2,data)) return false;
        TrainingQuote next;next.skill=skill;next.available=record->bytes[43+skill];next.current=record->bytes[25+uint(skill)*3];next.price=READ_LE_UINT16(data.data());output=next;return true;
    }
    static TrainingResult apply(HdvArchive &archive,NewGame &game,byte request,byte member,byte quantity) {
        TrainingQuote offer;if (!quote(archive,game,request,member,offer)) return kTrainingInvalid;
        if (!offer.available) return kTrainingNoPractice;
        if (!quantity) return kTrainingCancelled;
        if (quantity>offer.available) return kTrainingInvalid;
        // Original FBE commits the low16-bit product before BB3E payment.
        const uint16 total=uint16(uint32(offer.price)*quantity);
        if (game.gold<total) return kTrainingPoor;
        Party next=game.party;CharacterRecord &record=next._members[member];
        record.bytes[25+uint(offer.skill)*3]=byte(MIN<uint>(255,uint(offer.current)+quantity));
        record.bytes[43+offer.skill]=byte(offer.available-quantity);
        game.party=next;game.gold=uint16(game.gold-total);return kTrainingApplied;
    }
};
} }
#endif
