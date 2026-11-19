/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_ORIGINAL_MESSAGES_H
#define NOX_NATIVE_ORIGINAL_MESSAGES_H
#include "nox/resources.h"
namespace Nox { namespace Native {
// Authored data coordinates only. No original code, pointers or message prose
// is copied into the engine. Strip the inventory window's decorative separator;
// callers own placement, while every character of the message comes from HDV.
class OriginalMessages {
public:
    enum Id { kDirection,kNothingFound,kNoPicks,kItemFound,kDoorUnlocked,kWhoPicks,kTrapAgain,kPickAgain,kSteal,kFoodFound,kGoldFound,kInventoryFull,kReadied,kUnreadied,kCannotReady,kArmorCombat,kBookCombat,kNotCombat,kNotUseable,kNotHere,kEnchanted,kStrength,kFreeHand,kDexterity,kIntelligence,kNoUnreadied,kUnreadyBeforeDrop,kUniqueItem,kDropAll,kOrbInactive,kOnlyFoot,kNothingHappens,kNotDead,kItemX,kItemY,kWorldX,kWorldY,kYouFound,kLootChoices,kCastWhich,kWhoCasts,kHealWho,kResurrectWho,kGateWhich,kBookSlot,kLootGold,kLootFood,kCount };
    OriginalMessages() : _ready(false) {}
    bool load(HdvArchive &archive) {
        if (_ready) return true;
        const char *names[]={"NA/GAME","NA/SRTN.INVENTORY","NA/SRTN.COMBAT.S"};
        Common::Array<byte> resources[3];
        for (uint i=0;i<3;++i) if (!archive.read(names[i],resources[i])) return false;
        struct Binding { byte resource;uint offset; };
        static const Binding bindings[]={{0,0x13be},
            {0,0x14df},
            {0,0x1559},
            {0,0x14e6},
            {1,0x9603},
            {1,0x900c},
            {1,0x9020},
            {1,0x9033},
            {1,0x8ce8},
            {1,0x8cce},
            {1,0x8cdb},
            {1,0x8159},
            {1,0x5818},
            {1,0x5829},
            {1,0x583c},
            {1,0x5856},
            {1,0x5888},
            {1,0x58bb},
            {1,0x58cd},
            {1,0x58e2},
            {1,0x58f0},
            {1,0x5919},
            {1,0x5937},
            {1,0x5959},
            {1,0x5978},
            {1,0x5801},
            {1,0x5aed},
            {1,0x5b19},
            {1,0x5b44},
            {2,0x881a},
            {2,0x882a},
            {2,0x87e0},
            {2,0x84e2},{1,0x5906},{1,0x590e},{2,0x8885},{2,0x888b},{1,0x814d},{1,0x8168},{2,0x46be},{2,0x46d0},{2,0x8638},{2,0x84d1},{2,0x8801},{1,0x599a},{1,0x8121},{1,0x8130}};
        static_assert(ARRAYSIZE(bindings)==kCount,"Original message bindings");
        OriginalMessages next;
        for (uint i=0;i<kCount;++i) {
            const Common::Array<byte> &raw=resources[bindings[i].resource];uint at=bindings[i].offset;
            if (at>=raw.size()) return false;
            while (at<raw.size() && raw[at]) {
                const byte ch=raw[at++]&127;
                if ((ch<32 && ch!=13) || ch>=127 || next._text[i].size()>=80) return false;
                next._text[i]+=char(ch);
            }
            if (at==raw.size()) return false;
            Common::String &text=next._text[i];
            if (text.size()>=4 && text.substr(text.size()-4)=="\r--\r") text=text.substr(0,text.size()-4);
            while (!text.empty() && text[0]==13) text=text.substr(1);
            while (!text.empty() && text.lastChar()==13) text.deleteLastChar();
            for (uint j=0;j<text.size();++j) if (byte(text[j])<32 && text[j]!=13) return false;
        }
        next._ready=true;*this=next;return true;
    }
    const Common::String &get(Id id) const {assert(_ready && id<kCount);return _text[id];}
private:
    bool _ready;Common::String _text[kCount];
};
} }
#endif
