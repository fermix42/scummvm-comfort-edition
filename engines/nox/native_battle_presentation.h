/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_BATTLE_PRESENTATION_H
#define NOX_NATIVE_BATTLE_PRESENTATION_H
#include "nox/native_encounter_state.h"
#include "nox/native_opening_backdrop.h"
#include "nox/native_battle_reticle.h"
#include "nox/native_quick_display.h"
namespace Nox { namespace Native {
// Tactical population and command image. Draws owned records and
// authored assets; does not select a command or execute a combat turn.
class BattlePresentation {
public:
    static byte barLevel(uint16 current,uint16 maximum) {
        if (!current) return 0;
        if (!maximum) return 7;
        const uint percent=uint32(current)*100/maximum;
        const uint thresholds[]={15,30,45,58,72,86};byte level=1;
        for (uint i=0;i<6;++i) if (percent>=thresholds[i]) ++level;
        return level;
    }
    bool load(HdvArchive &archive,byte type=8) {
        Common::Array<byte> game,aux,quick,warning;
        return _painter.load(archive,type) && _font.load(archive,1) && _reticle.load(archive) && _casting.load(archive) && archive.read("NA/GAME",game) &&
            text(game,0x1626,_missed) && text(game,0x12cd,_blocked) && text(game,0x1671,_critical) && text(game,0x1680,_foiled) && text(game,0x1697,_attacks) && text(game,0x16ae,_casts) && text(game,0x16c2,_killed) && text(game,0x15e6,_attack) && text(game,0x15ee,_firstHand) && text(game,0x15f8,_secondHand) &&
            text(game,0x1602,_melee) && text(game,0x1608,_range) && text(game,0x16d8,_target) && text(game,0x16a1,_castPrompt) &&
            resourceText(archive,0x4fb5,_direction) && resourceText(archive,0x8638,_heal) && resourceText(archive,0x84d1,_resurrect) &&
            archive.read("NA/BS.AUX.RTN.BK2",aux) && text(aux,0x1583,_statusNames[0]) && text(aux,0x1596,_statusNames[1]) &&
            text(aux,0x158d,_statusNames[2]) && text(aux,0x1588,_statusNames[3]) && text(aux,0x2f7a,_flee) &&
            text(game,0x170d,_refused) && text(game,0x30fd,_unknown) && text(game,0x13c9,_cannonNotHere) &&
            text(game,0x17e8,_cannonShot) && text(game,0x1802,_cannonPowder) && defeatText(game) &&
            archive.read("NA/BS.AUX.RTN.BK1",quick) && quickMessages(quick) && quickVictoryText(quick) && text(game,0x1537,_modePrompt) &&
            archive.readRange("NA/SRTN.COMBAT.S",0x8b2,96,warning) && modeWarningText(warning);
    }
    const Common::String &invalidCommand() const { return _unknown; }
    const Common::String &quickMessage(byte draw) const { return _quickMessages[draw>>5]; }
    const Common::String &quickVictory() const { return _quickVictory; }
    bool paintWorldDefeat(HgrFrame &frame) const {return prompt(_defeat,frame);}
    const Common::String &cannonNotice(CannonStep step) const {return step==kCannonNotHere?_cannonNotHere:step==kCannonPowder?_cannonPowder:_cannonShot;}
    Common::String modePrompt(bool warning) const { return warning?_modeWarning+"\r"+_modePrompt:_modePrompt; }
    bool paint(const PreparedEncounter &encounter,const EncounterState &battle,byte phase,HgrFrame &output,const Common::String *notice=nullptr) const {
        if (!battle.formed || phase>3 || encounter.terrain.size()!=187) return false;
        const BattleRecords &records=battle.records;HgrFrame next;
        for (uint i=0;i<187;++i) if (!_painter.draw(next,0,encounter.terrain[i],1,phase,i,16)) return false;
        if (!battle.round.partyCount || battle.round.partyCount>6 || battle.round.activeParty>=battle.round.partyCount || battle.round.enemyCount>16 || battle.round.allyCount>16) return false;
        const uint offsets[]={0,1,17,18};
        for (uint group=0;group<3;++group) {
            const uint count=group==0?battle.round.enemyCount:group==1?battle.round.allyCount:battle.round.partyCount;
            for (uint i=0;i<count;++i) {
                const byte *actor=group==2?records.compact+i*4:records.extended+(group==1?128:0)+i*8;
                const byte *stats=records.stats+(group==2?i*128:group==1?1280+i*32:768+i*32);
                if (!READ_LE_UINT16(stats+2) || !actor[2] || actor[3]==255) continue;
                const bool large=group!=2 && (actor[3]&8);const uint origin=uint(actor[1])*17+actor[0];
                if (actor[0]>10 || actor[1]>10 || (large && (actor[0]>9 || actor[1]>9))) return false;
                for (uint foot=0;foot<(large?4U:1U);++foot) {
                    const uint cell=origin+offsets[foot];
                    if (!_painter.draw(next,byte(actor[2]+foot),encounter.terrain[cell],1,phase,cell,16)) return false;
                }
            }
        }
        const byte *party=records.compact+battle.round.activeParty*4;
        // Tactical pane replaces the world sidebar. Its selected actor has
        // the original final scanline underline, independent of sprite pose.
        const uint underline=8+uint(party[1])*16+15;
        if (!next.putByte(party[0]*2,underline,127) || !next.putByte(party[0]*2+1,underline,127)) return false;
        // Eleven visible tactical columns precede the two-byte gutter and
        // sidebar. The logical field still retains all seventeen columns.
        for (uint y=8;y<188;++y) if (!next.putByte(22,y,0) || !next.putByte(23,y,0)) return false;
        for (uint x=0;x<40;++x) if (!next.putByte(x,4,byte((x&1)?170:213)) || !next.putByte(x,188,byte((x&1)?170:213))) return false;
        for (uint y=4;y<189;++y) for (uint x=24;x<40;++x) {
            const bool horizontal=y==4 || y==107 || y==188;
            const byte value=horizontal?byte(((x-24)&1)?170:213):x==24?132:x==39?160:0;
            if (!next.putByte(x,y,(y==107 && x==24)?212:value)) return false;
        }
        for (uint m=0;m<battle.round.partyCount;++m) {
            const byte *member=records.stats+m*128;
            const byte inverse=(battle.command==2 && battle.cast.phase==kBattleCastParty && battle.cast.member==m) || battle.round.lockedParty==m+1?127:0;
            for (uint i=0;i<14 && member[75+i];++i) {
                if (!next.drawGlyph(_font,member[75+i],25+i,1+m*2,inverse)) return false;
                if (m==battle.round.activeParty && !next.drawGlyph(_font,member[75+i],25+i,14,0)) return false;
            }
            const byte health=barLevel(READ_LE_UINT16(member+2),READ_LE_UINT16(member+72)),mana=health?barLevel(member[4],member[18]):0;
            for (uint y=18+m*16;y<20+m*16;++y) for (uint i=0;i<7;++i) {
                if (!next.putByte(31-i,y,i<health?byte((i&1)?42:85):0) || !next.putByte(32+i,y,i<mana?byte((i&1)?42:85):0)) return false;
            }
        }
        const byte status=records.stats[battle.round.activeParty*128];
        if (status && status!=1 && status!=2 && status!=255) return false;
        if (status && !prompt(_statusNames[status==255?3:status],next)) return false;
        if (battle.command==1) {
            if (battle.attack.phase!=1 || !_reticle.paint(battle.target,0,next)) return false;
            for (uint row=15;row<23;++row) if (!next.clearTextSpan(25,row,14)) return false;
            VortexVictim caster;byte kind,range;
            const uint index=battle.round.activeParty*4;const byte cell=byte(records.compact[index+1]*17+records.compact[index]);
            if (!records.actor(cell,caster) || !BattleTargets::hand(caster,battle.attack.current!=0,kind,range)) return false;
            const Common::String prompt=_attack+(battle.attack.current?_secondHand:_firstHand)+(kind?_range:_melee)+_target;
            if (!this->prompt(prompt,next)) return false;
        } else if (battle.command==3) {
            if (!battle.cannon.phase || !_reticle.paint(battle.target,0,next) || !prompt(_target,next)) return false;
        } else if (battle.command==2) {
            if (!battle.cast.phase || !battle.cast.valid()) return false;Common::String message;
            if (battle.cast.phase==kBattleCastSlot) message=_castPrompt;
            else {
                message=_casting.name(battle.cast.prepared.spell);
                if (battle.cast.phase==kBattleCastTarget) {
                    if (!_reticle.paint(battle.target,0,next)) return false;message+=_target;
                } else if (battle.cast.phase==kBattleCastDirection) message+=_direction;
                else if (battle.cast.phase==kBattleCastParty) message+=battle.cast.prepared.spell==49?_resurrect:_heal;
                else return false;
            }
            if (!prompt(message,next)) return false;
        }
        if (battle.exit.phase==1 && !prompt(_flee,next)) return false;
        if (battle.exit.phase==2 && !prompt(_refused,next)) return false;
        if (battle.round.phase==kBattleDefeat && !prompt(_defeat,next)) return false;
        if (notice && !notice->empty() && !prompt(*notice,next)) return false;
        output=next;return true;
    }
    static Common::String singleLine(const Common::String &value) {
        Common::String line;for (uint i=0;i<value.size();++i) {const byte c=value[i]&127;if (c==13) {if (i+1<value.size()) line+=' ';}else if (c>=32) line+=char(c);}return line;
    }
    bool paintQuick(const EncounterState &battle,const QuickBattleDisplay &display,HgrFrame &output,bool victory=false) const {
        const BattleRecords &records=battle.records;
        if (!battle.quick.valid() || battle.phase>3) return false;
        HgrFrame frame;
        for (uint x=0;x<22;++x) {
            const byte edge=(x&1)?170:213;
            if (!frame.putByte(x,4,edge) || !frame.putByte(x,188,edge)) return false;
        }
        for (uint y=4;y<=188;++y) {
            if (!frame.putByte(24,y,132) || !frame.putByte(39,y,160)) return false;
        }
        for (uint x=24;x<40;++x) {
            const byte edge=(x&1)?170:213;
            if (!frame.putByte(x,4,edge) || !frame.putByte(x,107,edge) || !frame.putByte(x,188,edge)) return false;
        }

        // Preserve the admission groups even after the last enemy falls.
        Common::String groups[16];
        byte groupIndex[16];
        uint groupCount=0;
        for (uint i=0;i<battle.quick.enemyCount;++i) {
            const byte *member=records.stats+768+i*32;
            Common::String name;
            for (uint j=17;j<32 && member[j];++j) name+=char(member[j]&127);
            uint group=0;
            while (group<groupCount && groups[group]!=name) ++group;
            if (group==groupCount) groups[groupCount++]=name;
            groupIndex[i]=byte(group);
        }
        const uint columns=groupCount>4?2:1;
        const uint band=groupCount>8?2:4;
        uint sprites[16]={};
        for (uint group=0;group<groupCount;++group) {
            const uint column=(group%columns)*11;
            const uint width=columns==1?22:11;
            if (!quickLine(groups[group],frame,column,2+(group/columns)*band,width)) return false;
        }
        if (band==4) {
            for (uint i=0;i<battle.quick.enemyCount;++i) {
                const byte *actor=records.extended+i*8;
                if (!READ_LE_UINT16(records.stats+768+i*32+2) || !actor[2] || actor[3]==255 || (actor[7]&128)) continue;
                const uint group=groupIndex[i],slot=sprites[group]++;
                const uint first=(group%columns)*6;
                if (slot>= (columns==1?11U:5U)) continue;
                const uint cell=(1+(group/columns)*2)*17+first+slot;
                if (!_painter.draw(frame,actor[2],0,1,battle.phase,cell,16)) return false;
            }
        }

        if (!quickLine("Party",frame,0,18,22)) return false;
        for (uint m=0;m<battle.quick.partyCount;++m) {
            const byte *member=records.stats+m*128;
            const byte *actor=records.compact+m*4;
            if (actor[2] && actor[3]!=255 && !_painter.draw(frame,actor[2],0,1,battle.phase,9*17+m,16)) return false;
            for (uint i=0;i<14 && member[75+i];++i)
                if (!frame.drawGlyph(_font,member[75+i],25+i,1+m*2,0)) return false;
            const byte health=barLevel(READ_LE_UINT16(member+2),READ_LE_UINT16(member+72));
            const byte mana=health?barLevel(member[4],member[18]):0;
            for (uint y=18+m*16;y<20+m*16;++y) for (uint i=0;i<7;++i)
                if (!frame.putByte(31-i,y,i<health?byte((i&1)?42:85):0) ||
                    !frame.putByte(32+i,y,i<mana?byte((i&1)?42:85):0)) return false;
        }

        if (battle.round.phase==kBattleDefeat) {
            if (!prompt(_defeat,frame)) return false;
        } else {
            HgrFrame textFrame;
            uint column=25,row=18;
            Common::String stream;
            for (const Common::String &line : display.lines) stream+=line;
            if (victory) stream+=_quickVictory;
            for (uint i=0;i<stream.size();) {
                if ((stream[i]&127)==13) {
                    column=25;++row;++i;
                } else {
                    uint end=i;
                    while (end<stream.size() && (stream[end]&127)!=13 && (stream[end]&127)!=' ') ++end;
                    if (end<stream.size() && (stream[end]&127)==' ') ++end;
                    if (column>25 && column+end-i>39) {column=25;++row;}
                    while (i<end) {
                        if (column==39) {column=25;++row;}
                        if (row>=23) {
                            if (!textFrame.scrollTextRows(25,14,23,14)) return false;
                            row=22;
                        }
                        if (!textFrame.drawGlyph(_font,stream[i++]&127,column++,row,0)) return false;
                    }
                }
                if (row>=23) {
                    if (!textFrame.scrollTextRows(25,14,23,14)) return false;
                    row=22;
                }
            }
            for (uint y=112;y<184;++y) for (uint x=25;x<39;++x)
                if (!frame.putByte(x,y,textFrame.data()[HgrFrame::rowOffset(y)+x])) return false;
        }
        output=frame;
        return true;
    }

private:
    bool quickLine(const Common::String &line,HgrFrame &frame,uint column,uint row,uint width) const {
        for (uint i=0;i<MIN<uint>(line.size(),width);++i)
            if (!frame.drawGlyph(_font,byte(line[i]),column+i,row,0)) return false;
        return true;
    }
    bool modeWarningText(const Common::Array<byte> &warning) {
        Common::String authored;if (!text(warning,0,authored)) return false;_modeWarning.clear();
        for (uint i=0;i<authored.size();++i) if (authored[i]!=13 || !i || authored[i-1]!=13) _modeWarning+=authored[i];
        return !_modeWarning.empty();
    }
    bool quickMessages(const Common::Array<byte> &quick) {
        if (quick.size()<0x4eb) return false;
        for (uint i=0;i<8;++i) {
            const uint address=READ_LE_UINT16(quick.data()+0x4db+i*2);
            if (address<0xda00 || !text(quick,address-0xda00,_quickMessages[i])) return false;
        }
        return true;
    }
    bool quickVictoryText(const Common::Array<byte> &quick) {
        return text(quick,0x49f,_quickVictory);
    }
    bool defeatText(const Common::Array<byte> &game) {
        if (!text(game,0x1593,_defeat)) return false;
        // Preserve the authored terminal sentence. The following Apple II
        // reboot instruction is hardware recovery,not native gameplay logic.
        for (uint i=0;i<_defeat.size();++i) if (_defeat[i]=='.') { _defeat=Common::String(_defeat.c_str(),i+1);return true; }
        return false;
    }
    bool prompt(const Common::String &message,HgrFrame &frame) const {
        for (uint row=15;row<23;++row) if (!frame.clearTextSpan(25,row,14)) return false;
        uint row=15,column=25;
        for (uint i=0;i<message.size();++i) {
            const byte c=byte(message[i]);if (c==13) { ++row;column=25;continue; }
            if (c==' ' && column==25) continue;
            if (c!=' ' && (!i || message[i-1]==' ' || message[i-1]==13)) {
                uint length=0;while (i+length<message.size() && message[i+length]!=' ' && message[i+length]!=13) ++length;
                if (length<=14 && column+length>39) { ++row;column=25; }
            }
            if (column>=39) { ++row;column=25; }
            if (row>=23 || !frame.drawGlyph(_font,c,column++,row,0)) return false;
        }
        return true;
    }
    static bool resourceText(HdvArchive &archive,uint offset,Common::String &out) {
        Common::Array<byte> data;return archive.readRange("NA/SRTN.COMBAT.S",offset,40,data) && text(data,0,out);
    }
    static bool text(const Common::Array<byte> &game,uint offset,Common::String &out) {
        if (offset>=game.size()) return false;out.clear();
        for (uint i=offset;i<game.size();++i) { const byte c=game[i]&127;if (!c) return true;out+=char(c); }
        return false;
    }
    TilePainter _painter;GlyphFont _font;BattleReticle _reticle;CastRules _casting;
    Common::String _quickMessages[8];
    Common::String _missed,_blocked,_critical,_foiled,_attacks,_casts,_killed;
    Common::String _attack,_firstHand,_secondHand,_melee,_range,_target,_castPrompt,_direction,_heal,_resurrect,_statusNames[4],_flee,_refused,_defeat,_unknown,_quickVictory,_modePrompt,_modeWarning,_cannonNotHere,_cannonShot,_cannonPowder;
};
} }
#endif
