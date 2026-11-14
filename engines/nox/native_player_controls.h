/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_PLAYER_CONTROLS_H
#define NOX_NATIVE_PLAYER_CONTROLS_H
#include "nox/native_party.h"
#include "nox/native_clock.h"
#include "nox/native_world_transport.h"
#include "nox/native_vortex_cast.h"
#include "nox/native_world_presentation.h"
#include "nox/native_random.h"
#include "common/events.h"
#include "nox/native_opening_backdrop.h"
namespace Nox { namespace Native {
enum TorchResult {kTorchEmpty,kTorchAlreadyLit,kTorchIgnited};
class PlayerTorch {
public:
    static TorchResult ignite(byte &torches,TurnClock &clock) {
        if (!torches) return kTorchEmpty;
        if (clock.lightActive) return kTorchAlreadyLit;
        --torches;clock.lightTicks=120;clock.lightActive=1;return kTorchIgnited;
    }
};
class PlayerIcon {
public:
    static bool requested(const Common::KeyState &key) {
        return key.ascii=='=' || (key.keycode==Common::KEYCODE_EQUALS && !key.ascii && !(key.flags&Common::KBD_SHIFT));
    }
    static bool hero(byte value) {return value>=246 && value<254;}
    static byte next(byte value) {const byte result=byte(value+1);return result==254?246:result;}
    static bool world(byte &appearance,WorldTransportState &state,bool onFoot) {
        if (!onFoot || !hero(appearance) || (state.used && (state.object!=255 || state.baseAppearance!=appearance))) return false;
        appearance=next(appearance);state.appearance=state.baseAppearance=state.statusAppearance=appearance;return true;
    }
    static bool battle(BattleRecords &records,byte partyCount,byte member) {
        if (!partyCount || partyCount>6 || member>=partyCount || records.stats[member*128]==255 || !READ_LE_UINT16(records.stats+member*128+2)) return false;
        const byte picture=next(records.stats[member*128+24]);
        records.stats[member*128+24]=picture;records.compact[member*4+2]=picture;return true;
    }
};
class PartyRoster {
public:
    PartyRoster() : _active(false) {}
    bool active() const {return _active;}
    static byte segments(uint16 current,uint16 maximum) {
        if (!current) return 0;
        if (!maximum || current>=maximum) return 7;
        const uint percent=uint32(current)*100/maximum;
        static const byte thresholds[]={15,30,45,58,72,86};byte count=1;
        for (uint i=0;i<ARRAYSIZE(thresholds);++i) if (percent>=thresholds[i]) ++count;
        return count;
    }
    bool begin(HdvArchive &archive,const Party &party,HgrFrame &frame) {
        if (_active || !party.ready() || !party.memberCount() || party.memberCount()>6) return false;
        GlyphFont font;HgrFrame image=frame;
        if (!font.load(archive,1) || !OpeningBackdrop::renderNoticeFrame(image)) return false;
        for (uint i=0;i<party.memberCount();++i) {
            const byte *record=party.member(i)->bytes;
            for (uint j=0;j<13 && record[75+j];++j) if (!image.drawGlyph(font,record[75+j],25+j,1+i*2,0)) return false;
            const byte hp=segments(READ_LE_UINT16(record+2),READ_LE_UINT16(record+72));
            const byte mp=hp?segments(record[4],record[18]):0;
            for (uint pixel=18+i*16;pixel<=19+i*16;++pixel) {
                for (uint segment=1;segment<=7;++segment) {
                    const byte pattern=segment&1?0x55:0x2a;
                    if (!image.putByte(32-segment,pixel,hp>=segment?pattern:0) || !image.putByte(31+segment,pixel,mp>=segment?pattern:0)) return false;
                }
                if ((!hp && !image.putByte(31,pixel,0x40)) || (!mp && !image.putByte(32,pixel,1))) return false;
            }
            if ((record[0]==1 || record[0]==2) && !image.drawGlyph(font,record[0]==1?'P':'S',37,1+i*2,127)) return false;
        }
        _underlay=frame;frame=image;_active=true;return true;
    }
    bool close(HgrFrame &frame) {if (!_active) return false;frame=_underlay;_active=false;return true;}
private:
    bool _active;HgrFrame _underlay;
};
enum HourWaitResult {kHourWaitApplied,kHourWaitDanger,kHourWaitInvalid};
class HourWait {
public:
    HourWait() : _ready(false) {}
    bool load(HdvArchive &archive) {
        Common::Array<byte> data;if (!archive.read("NA/SRTN.INVENTORY",data) || data.size()!=67130) return false;
        HourWait next;
        if (!text(data,0xc086+0x118,next._prompt) || !text(data,0xc086+0x139,next._danger)) return false;
        for (uint i=0;i<5;++i) {
            const uint pointer=READ_LE_UINT16(data.data()+0xc086+0x152+i*2);
            if (pointer<0x9600 || !text(data,0xc086+pointer-0x9600,next._flavor[i])) return false;
        }
        next._ready=true;*this=next;return true;
    }
    const Common::String &prompt() const {return _prompt;}
    const Common::String &danger() const {return _danger;}
    static bool safe(byte type,byte map,byte appearance,bool alarm,const WorldScene &scene) {
        if (!alarm && (type>=7 || map==44)) return true;
        const byte ground=scene.actors.terrain[93];
        const bool waterParty=(appearance>=119 && appearance<125) || (ground>=229 && ground<233);
        // The source alarm path probes terrain before its actor-kind loop.
        // Native command context supplies cell0 instead of a loader cursor.
        if (alarm) {const byte first=scene.actors.terrain[0];if ((first>=229 && first<233)==waterParty) return false;}
        for (uint cell=0;cell<187;++cell) {
            if (scene.actors.kinds[cell]>=2) continue;
            const byte tile=scene.actors.terrain[cell];const bool water=tile>=229 && tile<233;
            if (water==waterParty) return false;
        }
        return true;
    }
    HourWaitResult apply(byte hours,TurnClock &clock,byte &counter,GameplayRandom &random,Common::String &reply) const {
        if (!_ready || !hours || hours>9 || counter!=clock.hourlyCounter || clock.hour>=0x24 || (clock.hour&15)>9) return kHourWaitInvalid;
        ClockAdvance valid;if (!advanceTurnClock(clock,valid)) return kHourWaitInvalid;
        TurnClock next=clock;GameplayRandom after=random;Common::String last;
        uint hour=(next.hour>>4)*10+(next.hour&15);
        for (uint i=0;i<hours;++i) {
            hour=(hour+1)%24;if (next.hourlyCounter) --next.hourlyCounter;
            // Explicit native presentation phase: original page-return0 branch.
            // No dependence on an Apple II display-page register is retained.
            if (!(i&1)) {byte flavor;if (!after.next(0,4,flavor)) return kHourWaitInvalid;last=_flavor[flavor];}
        }
        next.hour=byte((hour/10)*16+hour%10);next.lightTicks=next.lightActive=0;
        clock=next;counter=next.hourlyCounter;random=after;reply=last;return kHourWaitApplied;
    }
private:
    static bool text(const Common::Array<byte> &data,uint at,Common::String &out) {
        if (at>=data.size()) return false;Common::String value;
        for (uint i=0;i<=25 && at+i<data.size();++i) {
            const byte c=data[at+i];if (!c) {out=value;return true;}
            if ((c&127)<32) return false;value+=char(c&127);
        }
        return false;
    }
    bool _ready;Common::String _prompt,_danger,_flavor[5];
};
} }
#endif
