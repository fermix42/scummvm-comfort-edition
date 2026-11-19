/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_SEARCH_INPUT_H
#define NOX_NATIVE_SEARCH_INPUT_H
#include "nox/native_new_game.h"
#include "nox/native_world_presentation.h"
#include "nox/native_text_window.h"
#include "nox/native_battle_loot.h"
#include "nox/native_original_messages.h"
#include "nox/native_opening_backdrop.h"
namespace Nox { namespace Native {
enum SearchInputResult { kSearchInputHandled,kSearchInputClosed,kSearchInputAttempted,kSearchInputUnsupported };
// Player continuation for the existing semantic Search/lock/reward owner.
// Unsupported treasure/trap records remain explicit handoffs, never awards.
class SearchInput {
public:
    SearchInput() : _phase(0),_type(0),_map(0),_originX(0),_originY(0),_x(0),_y(0),_slot(255),_retry(false),_spent(false),_theftAuthorized(false),_lootLevel(0),_lootDescriptor(0),_door(false) {}
    bool doorPending() const {return active() && _door;}
    bool generatedPending() const {return _phase==5;}
    bool active() const {return _phase!=0;}
    bool matches(const World &world) const {return !active() || (world.mapType()==_type && world.mapId()==_map && world.x()==_originX && world.y()==_originY);}
    bool begin(HdvArchive &archive,const NewGame &game,HgrFrame &frame) {
        if (active() || !game.ready()) return false;
        SearchInput next;if (!next._messages.load(archive)) return false;next._phase=1;next._type=game.world.mapType();next._map=game.world.mapId();next._originX=game.world.x();next._originY=game.world.y();next._underlay=frame;
        // Search965F -> GAME7CD0 clears the old command strip before the object UI.
        if (!next._underlay.clearTextSpan(7,23,25)) return false;
        if (!next.prompt(archive,frame,next._messages.get(OriginalMessages::kDirection))) return false;*this=next;return true;
    }
    bool beginDoor(HdvArchive &archive,const NewGame &game,byte slot,HgrFrame &frame) {
        if (active() || !game.ready() || slot>248 || (slot&3)) return false;
        const byte *object=game.world.currentTables()->data()+1024+slot;
        if (object[3]!=17) return false;
        SearchInput next;if (!next._messages.load(archive)) return false;next._door=true;next._phase=2;next._type=game.world.mapType();next._map=game.world.mapId();next._originX=game.world.x();next._originY=game.world.y();
        next._x=object[0];next._y=object[1];next._slot=slot;next._underlay=frame;
        if (!next._underlay.clearTextSpan(7,23,25)) return false;
        if (!game.world.field(33)) {if (next.show(archive,frame,next._messages.get(OriginalMessages::kNoPicks),false,false)!=kSearchInputHandled) return false;}
        else if (!next.prompt(archive,frame,next._messages.get(OriginalMessages::kWhoPicks))) return false;
        *this=next;return true;
    }
    SearchInputResult handle(const Common::Event &event,HdvArchive &archive,NewGame &game,const WorldScene &scene,GameplayRandom &random,HgrFrame &frame,bool &alarm) {
        if (!_messages.load(archive) || !active() || !matches(game.world)) return kSearchInputUnsupported;
        if (event.type!=Common::EVENT_KEYDOWN) return kSearchInputHandled;
        if (_phase==5) {
            if (_window.waiting()) return _window.handle(event) && _window.paint(frame)?kSearchInputHandled:kSearchInputUnsupported;
            const byte answer=byte(event.kbd.ascii)&95;
            if (answer!='T' && answer!='L' && answer!='A' && answer!='Y' && answer!='N') return kSearchInputHandled;
            BattleLoot policy;if (!policy.load(archive) || !policy.chooseGroup(answer!='N' && answer!='L',answer=='A',_lootLevel,_lootDescriptor,game.inventory,random,_loot)) return kSearchInputUnsupported;
            return treasurePresentation(archive,game,policy,frame);
        }
        if (_phase==3) {
            if (_window.waiting()) return _window.handle(event) && _window.paint(frame)?kSearchInputHandled:kSearchInputUnsupported;
            if (!_window.ended()) return kSearchInputUnsupported;
            frame=_underlay;
            // Open97DB/Search9B5E retry only on Y or Return. Other answers
            // close after retiring this completed attempt exactly once.
            if (_retry && (event.kbd.keycode==Common::KEYCODE_y || event.kbd.keycode==Common::KEYCODE_RETURN)) {
                _phase=2;_spent=false;return kSearchInputAttempted;
            }
            _phase=0;return _spent?kSearchInputAttempted:kSearchInputClosed;
        }
        if (event.kbd.keycode==Common::KEYCODE_ESCAPE) {_phase=0;frame=_underlay;return kSearchInputClosed;}
        SearchRules rules;ItemCatalog catalog;if (!rules.load(archive,_type) || !catalog.load(archive)) return kSearchInputUnsupported;
        if (_phase==4) {
            const byte answer=byte(event.kbd.ascii)&95;
            if (answer=='N') {_phase=0;frame=_underlay;return kSearchInputClosed;}
            if (answer!='Y') return kSearchInputHandled;
            bool raised;if (!game.world.raiseSearchAlarm(rules,raised)) return kSearchInputUnsupported;
            alarm|=raised;_theftAuthorized=true;
            const byte *object=game.world.currentTables()->data()+1024+_slot;
            if ((object[3]&191)!=53) return show(archive,frame,_messages.get(OriginalMessages::kNothingFound),false,false);
            if (rules.locked(object[2]) && game.world.field(33)) {_phase=2;return prompt(archive,frame,_messages.get(OriginalMessages::kWhoPicks))?kSearchInputHandled:kSearchInputUnsupported;}
            _phase=0; // Confirmed unlocked attempt continues in this event.
        }
        if (_phase==1) {
            byte cell=255;_x=_originX;_y=_originY;
            switch(event.kbd.keycode) {
            case Common::KEYCODE_UP:--_y;cell=76;break;case Common::KEYCODE_DOWN:++_y;cell=110;break;
            case Common::KEYCODE_LEFT:--_x;cell=92;break;case Common::KEYCODE_RIGHT:++_x;cell=94;break;
            default:return kSearchInputHandled;
            }
            _slot=scene.objects.lookup[cell];
            if (_slot==255) return show(archive,frame,_messages.get(OriginalMessages::kNothingFound),false,false);
            if ((_slot&3) || _slot>244) return kSearchInputUnsupported;
            const byte *object=game.world.currentTables()->data()+1024+_slot;
            if (object[0]!=_x || object[1]!=_y) return kSearchInputUnsupported;
            if ((object[3]&64) && !_theftAuthorized) {_phase=4;return prompt(archive,frame,_messages.get(OriginalMessages::kSteal))?kSearchInputHandled:kSearchInputUnsupported;}
            if ((object[3]&191)!=53) return show(archive,frame,_messages.get(OriginalMessages::kNothingFound),false,false);
            if (rules.locked(object[2]) && game.world.field(33)) {_phase=2;return prompt(archive,frame,_messages.get(OriginalMessages::kWhoPicks))?kSearchInputHandled:kSearchInputUnsupported;}
        }
        byte member=1;
        if (_phase==2) {
            const uint ascii=event.kbd.ascii;
            if (ascii<'1' || ascii>'6' || ascii-'0'>game.party.memberCount()) return kSearchInputHandled;
            member=byte(ascii-'0');
            // Search9A86 sets living-only selection before GAME8600.
            if (game.party.member(member-1)->bytes[0]==255) return kSearchInputHandled;
        }
        if (_door) {
            LockResult lockOutcome=kLockUnsupported;bool promoted=false;const SearchResult result=game.world.pickDoor(_x,_y,_slot,member,rules,_state,random,game.party,promoted,&lockOutcome);
            if (result==kSearchAwarded) return show(archive,frame,_messages.get(OriginalMessages::kDoorUnlocked),false,true);
            if (result==kSearchRetry) return show(archive,frame,_messages.get(lockOutcome==kLockTrapped?OriginalMessages::kTrapAgain:OriginalMessages::kPickAgain),true,true);
            if (result==kSearchNoTools) return show(archive,frame,_messages.get(OriginalMessages::kNoPicks),false,false);
            return kSearchInputUnsupported;
        }
        const byte *object=game.world.currentTables()->data()+1024+_slot;
        ItemDefinition definition;Common::String title;
        const bool currency=(object[7]&128)!=0,foodReward=(object[7]&64)!=0;const uint16 beforeCurrency=foodReward?game.food:game.gold;
        if ((object[7]&192)==64 && catalog.definition(object[4],object[5],definition))
            for (uint i=13;i<32 && definition.bytes[i];++i) title+=char(definition.bytes[i]&127);
        SearchTreasure treasure;LockResult lockOutcome=kLockUnsupported;bool promoted=false;const SearchResult result=game.world.searchQuestItem(_x,_y,_slot,member,false,rules,catalog,_state,random,game.party,game.inventory,promoted,&game.food,&game.gold,_theftAuthorized,&treasure,&lockOutcome);
        switch(result) {
        case kSearchTreasure: {
            BattleLoot policy;_loot=BattleLootState();_lootLevel=treasure.level;_lootDescriptor=treasure.descriptor;
            if (!policy.load(archive) || !policy.begin(treasure.count,game.party.memberCount(),game.food,game.gold,_loot) ||
                !policy.advanceGroup(_lootLevel,_lootDescriptor,game.inventory,random,_loot)) return kSearchInputUnsupported;
            return treasurePresentation(archive,game,policy,frame);
        }
        case kSearchAwarded:return show(archive,frame,currency?Common::String::format("%u",uint(uint16((foodReward?game.food:game.gold)-beforeCurrency)))+_messages.get(foodReward?OriginalMessages::kFoodFound:OriginalMessages::kGoldFound):title+_messages.get(OriginalMessages::kItemFound),false,true);
        case kSearchCapacity:return show(archive,frame,_messages.get(OriginalMessages::kInventoryFull),false,true);
        case kSearchRepeated:return show(archive,frame,_messages.get(OriginalMessages::kNothingFound),false,true);
        case kSearchNothing:return show(archive,frame,_messages.get(OriginalMessages::kNothingFound),false,false);
        case kSearchNoTools:return show(archive,frame,_messages.get(OriginalMessages::kNoPicks),false,false);
        case kSearchRetry:return show(archive,frame,_messages.get(lockOutcome==kLockTrapped?OriginalMessages::kTrapAgain:OriginalMessages::kPickAgain),true,true);
        case kSearchCancelled:_phase=0;frame=_underlay;return kSearchInputClosed;
        default:warning("Native Search rejected result %u",uint(result));return kSearchInputUnsupported;
        }
    }
    void cancel(HgrFrame *frame=nullptr) {if (frame) *frame=_underlay;_phase=0;}
    bool refresh(HdvArchive &archive,HgrFrame &frame) {if (!_messages.load(archive)) return false;_underlay=frame;return !active() || (_phase==2 && prompt(archive,frame,_messages.get(OriginalMessages::kWhoPicks)));}
    bool save(Common::WriteStream &out) const {
        if (!active() || _phase>5) return false;
        out.writeByte(_theftAuthorized);if (_door) out.writeByte(1);
        out.writeByte(_phase);out.writeByte(_type);out.writeByte(_map);out.writeByte(_originX);out.writeByte(_originY);out.writeByte(_x);out.writeByte(_y);out.writeByte(_slot);out.writeByte(_state.priorLockAttempts);out.writeByte(_retry);out.writeByte(_spent);
        out.write(_underlay.data(),8192);
        if (_phase==5) {out.writeByte(_lootLevel);out.writeByte(_lootDescriptor);if (!_loot.save(out) || !_window.save(out)) return false;}
        if (_phase==3 && !_window.save(out)) return false;return !out.err();
    }
    bool load(Common::SeekableReadStream &in,HdvArchive &archive,bool guardedVersion=false,bool generatedVersion=false,bool doorVersion=false) {
        SearchInput next;if (guardedVersion) {const byte allowed=in.readByte();if (allowed>1) return false;next._theftAuthorized=allowed!=0;}if (doorVersion) {if (in.readByte()!=1) return false;next._door=true;}next._phase=in.readByte();next._type=in.readByte();next._map=in.readByte();next._originX=in.readByte();next._originY=in.readByte();next._x=in.readByte();next._y=in.readByte();next._slot=in.readByte();next._state.priorLockAttempts=in.readByte();const byte retry=in.readByte(),spent=in.readByte();next._retry=retry!=0;next._spent=spent!=0;
        byte image[8192];if (!next._phase || next._phase>(generatedVersion?5:guardedVersion?4:3) || retry>1 || spent>1 || ((next._phase==2 || next._phase==4) && (next._slot>(next._door?248:244) || (next._slot&3))) || in.read(image,8192)!=8192 || !next._underlay.importImage(image,8192) || (next._phase==3 && !next._window.load(in,archive)) || in.err() || (next._phase!=5 && in.pos()!=in.size())) return false;
        if (next._phase==5) {
            next._lootLevel=in.readByte();next._lootDescriptor=in.readByte();byte bytes[20];if (in.read(bytes,20)!=20) return false;
            Common::MemoryReadStream part(bytes,20);
            if (!next._loot.load(part) || !next._loot.pending || next._loot.finished || next._loot.scripted ||
                !next._lootLevel || next._lootLevel>10 || (next._lootDescriptor&15)>9 || (next._lootDescriptor>>4)>9 || !next._window.load(in,archive)) return false;
        }
        if (in.err() || in.pos()!=in.size()) return false;
        *this=next;return true;
    }
private:
    SearchInputResult treasurePresentation(HdvArchive &archive,NewGame &game,const BattleLoot &policy,HgrFrame &frame) {
        if (_loot.finished) {
            if (!policy.commit(game.food,game.gold,_loot)) return kSearchInputUnsupported;
            return show(archive,frame,Common::String::format("%u",_loot.amount)+_messages.get(_loot.food?OriginalMessages::kLootFood:OriginalMessages::kLootGold),false,true);
        }
        Common::String title;if (!_loot.pending || !policy.describe(_loot,title)) return kSearchInputUnsupported;
        // The authored loot choices are a multi-line panel, not a rewritten
        // sentence paged word-by-word through the one-line movement strip.
        Common::String body=_messages.get(OriginalMessages::kYouFound)+"\r"+title+"\r"+_messages.get(OriginalMessages::kLootChoices);
        Common::Array<byte> encoded;for (uint i=0;i<body.size();++i) encoded.push_back(byte(body[i])|128);
        HgrFrame background=_underlay;if (!OpeningBackdrop::renderNoticeFrame(background)) return kSearchInputUnsupported;
        TextWindow window;if (!window.begin(archive,encoded.data(),encoded.size(),25,13,1,12,11,&background) || !window.paint(background)) return kSearchInputUnsupported;
        _window=window;frame=background;_phase=5;return kSearchInputHandled;
    }
    bool prompt(HdvArchive &archive,HgrFrame &frame,const Common::String &text) const {
        GlyphFont font;if (!font.load(archive,1)) return false;
        if (text.size()>25) return false;
        for (uint y=184;y<192;++y) for(uint x=7;x<32;++x) if(!frame.putByte(x,y,0)) return false;
        for(uint i=0;i<text.size();++i) if(!frame.drawGlyph(font,byte(text[i])|128,7+(25-text.size())/2+i,23,0)) return false;return true;
    }
    SearchInputResult show(HdvArchive &archive,HgrFrame &frame,const Common::String &text,bool retry,bool spent) {
        TextWindow window;frame=_underlay;
        // Search's GAME7C32 -> 7C35 selects column7,row23,width25.
        if (!frame.clearTextSpan(7,23,25)) return kSearchInputUnsupported;
        const byte pad=text.size()<=25?byte((25-text.size())/2):0;
        if(!window.begin(archive,reinterpret_cast<const byte *>(text.c_str()),text.size(),7+pad,25-pad,23,23,1,&frame) || !window.paint(frame)) return kSearchInputUnsupported;
        _window=window;_phase=3;_retry=retry;_spent=spent;return kSearchInputHandled;
    }
    byte _phase,_type,_map,_originX,_originY,_x,_y,_slot;bool _retry,_spent,_theftAuthorized;SearchState _state;HgrFrame _underlay;TextWindow _window;BattleLootState _loot;byte _lootLevel,_lootDescriptor;bool _door;OriginalMessages _messages;
};
} }
#endif
