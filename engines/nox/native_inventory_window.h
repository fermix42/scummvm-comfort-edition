/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INVENTORY_WINDOW_H
#define NOX_NATIVE_INVENTORY_WINDOW_H
#include "nox/native_new_game.h"
#include "nox/native_graphics.h"
#include "nox/native_inventory_actions.h"
#include "nox/native_text_window.h"
#include "nox/native_sound_events.h"
#include "nox/native_menu_chrome.h"
#include "nox/native_cast.h"
#include "nox/native_combat_preferences.h"
#include "nox/native_original_messages.h"
#include "common/ptr.h"
namespace Nox { namespace Native {
enum InventoryWindowResult { kInventoryWindowHandled,kInventoryWindowClosed,kInventoryWindowUnsupported };
// Free world inventory selection. Transactions remain in the existing typed
// equipment/book owners; no field or derived-stat edits are performed by UI.
class InventoryWindow {
public:
    InventoryWindow() : _phase(0),_member(0),_type(0),_map(0),_selected(0),_tab(0),_sheetPage(0),_slotsView(false),_combat(false),_sounds(nullptr) {}
    void setSoundEvents(SoundEvents *sounds) {_sounds=sounds;}
    bool active() const {return _phase!=0;}
    byte phase() const {return _phase;}
    bool inCombat() const {return _combat;}
    uint selected() const {return _selected;}
    byte tab() const {return _tab;}
    byte sheetPage() const {return _sheetPage;}
    byte member() const {return _member;}
    const Common::String &message() const {return _message;}
    bool matches(const World &world) const {return !active() || (world.ready() && _type==world.mapType() && _map==world.mapId());}
    bool begin(HdvArchive &archive,const NewGame &game,HgrFrame &frame,bool inCombat=false,byte member=0) {
        if (active() || !game.ready() || member>=game.party.memberCount()) return false;
        InventoryWindow next;if (!next._messages.load(archive)) return false;next._phase=1;next._combat=inCombat;next._member=member;next._sounds=_sounds;next._type=game.world.mapType();next._map=game.world.mapId();next._underlay=frame;
        if (!next.paint(archive,game,frame)) return false;*this=next;return true;
    }
    InventoryWindowResult handle(const Common::Event &event,HdvArchive &archive,NewGame &game,HgrFrame &frame) {
        if (!_messages.load(archive) || !active() || !matches(game.world) || event.type!=Common::EVENT_KEYDOWN) return kInventoryWindowUnsupported;
        const Common::KeyCode key=event.kbd.keycode;const byte ascii=byte(event.kbd.ascii?event.kbd.ascii:(key>=Common::KEYCODE_a && key<=Common::KEYCODE_z?uint(key):0))&95;
        if (_tab==5 && _settingsPrompt!=kSettingsIdle) {
            handleSettingsPrompt(key,event.kbd.ascii);
            return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
        }
        if (_phase==4) {
            if (key==Common::KEYCODE_ESCAPE) {_phase=1;_message.clear();return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;}
            if (_reading.waiting()) return _reading.handle(event) && _reading.paint(frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
            _phase=1;_message.clear();return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
        }
        if (key==Common::KEYCODE_ESCAPE) {
            if (_phase==2 || _phase==3) {_phase=1;_message.clear();}
            else {frame=_underlay;*this=InventoryWindow();return kInventoryWindowClosed;}
        } else {
            Common::Array<uint> slots;if (!collect(archive,game.inventory,slots)) return kInventoryWindowUnsupported;
            if (_tab!=5 && _selected>=slots.size()) _selected=slots.empty()?0:uint16(slots.size()-1);
            if (_phase==3) {
                if (ascii=='Y' || ascii=='N') {
                    if (slots.empty()) return kInventoryWindowUnsupported;
                    const InventoryDropResult result=InventoryActions::drop(archive,game,slots[_selected],ascii=='Y');
                    if (result==kDropUnsupported) return kInventoryWindowUnsupported;
                    if (result==kDropApplied || result==kDropCancelled) _message.clear();
                    else _message=_messages.get(result==kDropProtected?OriginalMessages::kUniqueItem:OriginalMessages::kUnreadyBeforeDrop);
                    _phase=1;slots.clear();if (!collect(archive,game.inventory,slots)) return kInventoryWindowUnsupported;
                    if (_selected>=slots.size()) _selected=slots.empty()?0:uint16(slots.size()-1);
                }
            } else if (_phase==2) {
                if (event.kbd.ascii>='0' && event.kbd.ascii<='9' && !slots.empty()) {
                    const byte bookSlot=byte(event.kbd.ascii-'0');
                    if (!bookAction(archive,game,*game.inventory.item(slots[_selected]),bookSlot)) return kInventoryWindowUnsupported;
                    _phase=1;
                }
            } else if (key==Common::KEYCODE_TAB || (event.kbd.ascii>='1' && event.kbd.ascii<='7' && !(event.kbd.flags&Common::KBD_SHIFT))) {
                _tab=key==Common::KEYCODE_TAB?byte((_tab+1)%7):byte(event.kbd.ascii-'1');_selected=0;_sheetPage=0;_slotsView=false;_message.clear();
            } else if (key==Common::KEYCODE_SPACE) {_member=byte((_member+1)%game.party.memberCount());_message.clear();}
            else if (((event.kbd.flags&Common::KBD_SHIFT) && key>=Common::KEYCODE_1 && key<=Common::KEYCODE_6) || event.kbd.ascii=='!' || event.kbd.ascii=='@' || event.kbd.ascii=='#' || event.kbd.ascii=='$' || event.kbd.ascii=='%' || event.kbd.ascii=='^') {
                int member=-1;const char *symbols="!@#$%^";for (uint i=0;i<6;++i) if (event.kbd.ascii==symbols[i] || (key==Common::KEYCODE_1+int(i) && (event.kbd.flags&Common::KBD_SHIFT))) member=int(i);
                if (member>=0 && uint(member)<game.party.memberCount()) {_member=byte(member);_message.clear();}
            } else if (_tab==0) {
                if (key==Common::KEYCODE_LEFT || key==Common::KEYCODE_PAGEUP) _sheetPage=byte((_sheetPage+3)%4);
                else if (key==Common::KEYCODE_RIGHT || key==Common::KEYCODE_PAGEDOWN) _sheetPage=byte((_sheetPage+1)%4);
            } else if (_tab==5) {
                if (key==Common::KEYCODE_UP) _selected=(_selected+3)%4;
                else if (key==Common::KEYCODE_DOWN) _selected=(_selected+1)%4;
                else if (key==Common::KEYCODE_RETURN) beginSetting();
            }
            else if (_tab==4 && ascii=='P') {_slotsView=!_slotsView;_message.clear();}
            else if (_slotsView) {}
            else if (key==Common::KEYCODE_UP && _selected) {--_selected;_message.clear();}
            else if (key==Common::KEYCODE_DOWN && uint(_selected)+1<slots.size()) {++_selected;_message.clear();}
            else if (key==Common::KEYCODE_LEFT || key==Common::KEYCODE_PAGEUP) {_selected=_selected>=6?_selected-6:0;_message.clear();}
            else if ((key==Common::KEYCODE_RIGHT || key==Common::KEYCODE_PAGEDOWN) && !slots.empty()) {_selected=uint16(MIN<uint>(_selected+6,slots.size()-1));_message.clear();}
            else if (ascii=='I' && !slots.empty()) {
                const InventoryItem &item=*game.inventory.item(slots[_selected]);ItemCatalog catalog;ItemDefinition definition;if (!catalog.load(archive) || !catalog.definition(item.kind(),item.id(),definition)) return kInventoryWindowUnsupported;
                const byte *data=definition.bytes;
                Common::String info=definitionName(definition)+Common::String::format("\rWeight   %u",uint(data[2]));
                // Apple II SRTN.INVENTORY $A383-$A49C shows one of DEX or INT for gear.
                if (item.kind()<2) {
                    if (data[4]==1) info+=Common::String::format("\rDEX      %u",uint(data[3]));
                    else info+=Common::String::format("\rINT      %u",uint(data[4]));
                    if (item.kind()==0) {
                        info+=Common::String::format("\rMax DMG  %u",uint(data[5]));
                        info+=(data[11]&128)?"\r2-Handed":"\r1-Handed";
                        info+=(data[0]&128)?"\rMagic item":"\rCommon item";
                    } else {
                        info+=Common::String::format("\rArmor RT %u\rResist Mg%u",uint(data[5]),uint(data[0]));
                    }
                } else if ((data[12]&31)==8) {
                    info+=Common::String::format("\rINT      %u\rSpell DMG%u",uint(data[4]),uint(data[10]));
                    if (data[8]&16) info+="\rArea-of-effect";
                }
                Common::Array<byte> encoded;for (uint i=0;i<info.size();++i) encoded.push_back(byte(info[i])|128);HgrFrame background;GlyphFont font;TextWindow reading;
                if (!font.load(archive,1) || !MenuChrome::begin(font,background,"Item information",175) || !MenuChrome::text(font,background,2,22,"Esc: back to the selected tab") || !reading.begin(archive,encoded.data(),encoded.size(),1,38,5,20,15,&background) || !reading.paint(background)) return kInventoryWindowUnsupported;
                _reading=reading;_phase=4;frame=background;return kInventoryWindowHandled;
            }
            else if ((ascii=='U' || ascii=='D') && !slots.empty()) {
                const InventoryItem &item=*game.inventory.item(slots[_selected]);ItemCatalog catalog;ItemDefinition definition;
                if (!catalog.load(archive) || !catalog.definition(item.kind(),item.id(),definition)) return kInventoryWindowUnsupported;
                if (_combat && ascii=='U') {
                    _message=_messages.get(OriginalMessages::kNotCombat);if (_sounds) _sounds->push_back(kSpeakerRefusal);
                    return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
                }
                if (ascii=='D') {
                    if (definition.bytes[8]&128) _message=_messages.get(OriginalMessages::kUniqueItem);
                    else {_phase=3;_message.clear();}
                } else if (item.kind()!=2 || !(definition.bytes[8]&64)) _message=_messages.get(OriginalMessages::kNotUseable);
                else {
                    Common::String text;
                    if (definition.bytes[11]&128) {if (!InventoryActions::document(archive,definition,text)) return kInventoryWindowUnsupported;}
                    else if (definition.bytes[11]&32) text=_messages.get(OriginalMessages::kItemX)+Common::String::format(" %u\r",game.world.x())+_messages.get(OriginalMessages::kItemY)+Common::String::format(" %u",game.world.y());
                    else if (definition.bytes[11]&64) {
                        const NavigationResult result=game.navigation.bind(definition.bytes[6],NavigationPoint(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y()));
                        if (result==kNavigationUnsupported) return kInventoryWindowUnsupported;
                        _message=_messages.get(result==kNavigationSelected?OriginalMessages::kEnchanted:OriginalMessages::kNotHere);
                        if (_sounds && result!=kNavigationSelected) _sounds->push_back(kSpeakerRefusal);
                        return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
                    } else return kInventoryWindowUnsupported;
                    Common::Array<byte> encoded;for (uint i=0;i<text.size();++i) encoded.push_back(byte(text[i])|128);
                    HgrFrame background;TextWindow reading;GlyphFont font;
                    if (!font.load(archive,1) || !MenuChrome::begin(font,background,"Item document",175) ||
                        !MenuChrome::text(font,background,2,3,"Space/Enter: next page  Esc: close") ||
                        !MenuChrome::text(font,background,2,22,"Any key returns after the last page") ||
                        !reading.begin(archive,encoded.data(),encoded.size(),1,38,5,20,15,&background) || !reading.paint(background)) return kInventoryWindowUnsupported;
                    _reading=reading;_phase=4;_message.clear();frame=background;return kInventoryWindowHandled;
                }
            }
            else if ((ascii=='R' || ascii=='X' || key==Common::KEYCODE_RETURN) && !slots.empty()) {
                const InventoryItem item=*game.inventory.item(slots[_selected]);ItemCatalog catalog;ItemDefinition definition;EquipmentOwnership ownership;
                if (!catalog.load(archive) || !catalog.definition(item.kind(),item.id(),definition) || !ownership.load(archive)) return kInventoryWindowUnsupported;
                const byte refusal=InventoryActions::readyCombatRefusal(_combat,item.kind(),item.bytes[5],(definition.bytes[12]&31)==8);
                if (refusal) {
                    _message=_messages.get(refusal==6?OriginalMessages::kArmorCombat:OriginalMessages::kBookCombat);
                    if (_sounds) _sounds->push_back(kSpeakerRefusal);
                    return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
                }
                byte mask,clear;if (!ownership.masks(_member,mask,clear)) return kInventoryWindowUnsupported;
                const bool equippedHere=((item.bytes[3]|((item.bytes[5]&31)==1?item.bytes[4]:0))&mask)!=0;
                const bool unready=ascii=='X' || (key==Common::KEYCODE_RETURN && item.kind()<2 && equippedHere);
                if (unready && item.kind()<2 && !equippedHere) {
                    _message=_messages.get(OriginalMessages::kUnreadied);
                    return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
                }
                if (item.kind()==2 && (definition.bytes[12]&31)!=8) {
                    _message=_messages.get(OriginalMessages::kCannotReady);
                    return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
                }
                NewGame next(game);byte result=255,lastRemovedId=255;
                if (item.kind()==0) result=EquipmentActions::selectWeapon(catalog,ownership,_member,item.id(),unready,next.party,next.inventory,&lastRemovedId,_combat);
                else if (item.kind()==1) result=EquipmentActions::selectArmor(catalog,ownership,_member,item.id(),unready,next.party,next.inventory,&lastRemovedId,_combat);
                else if ((definition.bytes[12]&31)==8) {
                    bool ready=false;for (uint i=0;i<10;++i) ready|=game.book.spell(i)==definition.bytes[6];
                    if (ready) {if (!bookAction(archive,game,item,0)) return kInventoryWindowUnsupported;}
                    else if (unready) _message=_messages.get(OriginalMessages::kUnreadied);
                    else {
                        const BookResult admission=next.book.select(catalog,next.inventory,next.party,item.id(),0,_combat,true);
                        if (admission==kBookCancelled) {_phase=2;_message.clear();}
                        else if (admission==kBookIntelligenceRefusal) _message=_messages.get(OriginalMessages::kIntelligence);
                        else return kInventoryWindowUnsupported;
                    }
                    return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
                }
                if (_sounds && ((result>=3 && result<=6) || result==9 || result==12)) _sounds->push_back(kSpeakerRefusal);
                if (result==0 || result==7) {if (lastRemovedId!=255) {game.commerceCounter.kind=lastRemovedId;game.commerceCounter.kindKnown=true;}game.party=next.party;game.inventory=next.inventory;_message=_messages.get(result==0?OriginalMessages::kReadied:OriginalMessages::kUnreadied);}
                else _message=_messages.get(result==2?OriginalMessages::kNoUnreadied:result==3?OriginalMessages::kStrength:result==4?OriginalMessages::kDexterity:result==5?OriginalMessages::kIntelligence:result==12?OriginalMessages::kFreeHand:OriginalMessages::kCannotReady);
            }
        }
        return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
    }
    bool save(Common::WriteStream &out) const {
        if (_tab!=7 || _sheetPage || _slotsView) {
            InventoryWindow base(*this);base._tab=7;base._sheetPage=0;base._slotsView=false;Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);if (!base.save(body)) return false;
            out.writeUint32BE(MKTAG('N','I','W','N'));out.writeUint16LE(4);out.writeUint32LE(body.size());out.write(body.getData(),body.size());out.writeByte(_tab);out.writeByte(_sheetPage);out.writeByte(_slotsView);return !out.err();
        }
        if (_combat) {
            if (_phase!=1 && _phase!=3) return false;
            Common::ScopedPtr<InventoryWindow> base(new InventoryWindow(*this));base->_combat=false;
            Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);if (!base->save(body)) return false;
            out.writeUint32BE(MKTAG('N','I','W','N'));out.writeUint16LE(3);out.writeUint32LE(body.size());out.write(body.getData(),body.size());return !out.err();
        }

        if (!_phase || _phase>4 || _member>=6 || _selected>255 || _message.size()>80) return false;
        out.writeUint32BE(MKTAG('N','I','W','N'));out.writeUint16LE(_phase>=3?2:1);out.writeByte(_phase);out.writeByte(_member);
        out.writeByte(_type);out.writeByte(_map);out.writeUint16LE(_selected);out.writeByte(byte(_message.size()));out.write(_message.c_str(),_message.size());out.write(_underlay.data(),8192);
        if (_phase==4) {Common::MemoryWriteStreamDynamic part(DisposeAfterUse::YES);if (!_reading.save(part)) return false;out.writeUint32LE(part.size());out.write(part.getData(),part.size());}
        return !out.err();
    }
    bool load(Common::SeekableReadStream &in,HdvArchive &archive) {return loadState(in,&archive);}
    bool loadCombat(Common::SeekableReadStream &in) {
        InventoryWindow next;if (!next.loadState(in,nullptr) || !next._combat) return false;*this=next;return true;
    }
private:
    bool loadState(Common::SeekableReadStream &in,HdvArchive *archive) {
        if (in.readUint32BE()!=MKTAG('N','I','W','N')) return false;
        const uint version=in.readUint16LE();
        if (version==4) {
            const uint32 size=in.readUint32LE();if (size<6 || size>1048576 || int64(size)+3!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || READ_LE_UINT16(bytes.data()+4)>=4) return false;
            InventoryWindow next;Common::MemoryReadStream body(bytes.data(),size);if (!next.loadState(body,archive)) return false;
            next._tab=in.readByte();next._sheetPage=in.readByte();const byte view=in.readByte();if (next._tab>7 || next._sheetPage>3 || view>1) return false;next._slotsView=view!=0;if (in.err()) return false;*this=next;return true;
        }
        if (version==3) {
            const uint32 size=in.readUint32LE();if (size<6 || size>1048576 || int64(size)!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size || READ_BE_UINT32(bytes.data())!=MKTAG('N','I','W','N') || READ_LE_UINT16(bytes.data()+4)>=3) return false;
            InventoryWindow next;Common::MemoryReadStream body(bytes.data(),size);
            if (!next.loadState(body,archive) || (next._phase!=1 && next._phase!=3)) return false;
            next._combat=true;*this=next;return true;
        }
        if (version!=1 && version!=2) return false;
        InventoryWindow next;next._tab=7;next._phase=in.readByte();next._member=in.readByte();next._type=in.readByte();next._map=in.readByte();next._selected=in.readUint16LE();const byte length=in.readByte();
        if (!next._phase || next._phase>(version==1?2:4) || next._member>=6 || next._selected>255 || length>80 || in.size()-in.pos()<8192+length) return false;
        for (uint i=0;i<length;++i) {const byte c=in.readByte();if (c<32 || c>=127) return false;next._message+=char(c);}
        byte image[8192];if (in.read(image,8192)!=8192 || !next._underlay.importImage(image,8192) || in.err()) return false;
        if (next._phase==4) {
            const uint32 size=in.readUint32LE();if (!size || size>131072 || int64(size)!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size) return false;Common::MemoryReadStream part(bytes.data(),size);
            if (!archive || !next._reading.load(part,*archive)) return false;
        }
        if (in.err() || in.pos()!=in.size()) return false;*this=next;return true;
    }
private:
    static Common::String definitionName(const ItemDefinition &definition) {Common::String name;for (uint i=13;i<32 && definition.bytes[i];++i) name+=char(definition.bytes[i]&127);return name;}
    bool collect(HdvArchive &archive,const Inventory &inventory,Common::Array<uint> &slots) const {
        ItemCatalog catalog;if (!catalog.load(archive)) return false;
        for (uint i=0;i<256;++i) {
            const InventoryItem &item=*inventory.item(i);if (item.empty()) continue;ItemDefinition d;if (!catalog.definition(item.kind(),item.id(),d)) return false;
            const bool book=item.kind()==2 && (d.bytes[12]&31)==8,special=item.kind()==2 && !book && (d.bytes[8]&128);
            if (_tab==7 || (_tab==1 && item.kind()==0) || (_tab==2 && item.kind()==1) || (_tab==3 && item.kind()==2 && !book && !special) || (_tab==4 && book) || (_tab==6 && special)) slots.push_back(i);
        }
        return true;
    }
    bool bookAction(HdvArchive &archive,NewGame &game,const InventoryItem &item,byte slot) {
        ItemCatalog catalog;if (!catalog.load(archive)) return false;NewGame next(game);
        const BookResult result=next.book.select(catalog,next.inventory,next.party,item.id(),slot,_combat,false);
        if (result==kBookReadied || result==kBookUnreadied) {if (result==kBookUnreadied) {game.commerceCounter.kind=item.id();game.commerceCounter.kindKnown=true;}game.book=next.book;_message=_messages.get(result==kBookReadied?OriginalMessages::kReadied:OriginalMessages::kUnreadied);}
        else _message=_messages.get(result==kBookIntelligenceRefusal?OriginalMessages::kIntelligence:OriginalMessages::kCannotReady);
        return true;
    }
    static bool line(const GlyphFont &font,HgrFrame &frame,uint x,uint y,const Common::String &text) {
        for (uint i=0;i<text.size() && x+i<40;++i) if (!frame.drawGlyph(font,byte(text[i])|128,x+i,y,0)) return false;return true;
    }
    static bool messageLines(const GlyphFont &font,HgrFrame &frame,const Common::String &message) {
        if (message.empty()) return true;
        if (!frame.clearTextSpan(1,19,38) || !frame.clearTextSpan(1,20,38)) return false;
        uint at=0;
        for (uint row=19;row<=20 && at<message.size();++row) {
            while (at<message.size() && message[at]==' ') ++at;
            uint end=MIN<uint>(at+38,message.size());
            if (end<message.size()) {uint split=end;while (split>at && message[split]!=' ') --split;if (split>at) end=split;}
            if (!MenuChrome::text(font,frame,1,row,message.substr(at,end-at))) return false;at=end;
        }
        return at==message.size();
    }
    bool sheet(HdvArchive &archive,const NewGame &game,const GlyphFont &font,HgrFrame &frame) const {
        const byte *r=game.party.member(_member)->bytes;ItemCatalog catalog;if (!catalog.load(archive)) return false;
        auto text=[&](uint row,const Common::String &value)->bool {return MenuChrome::text(font,frame,2,row,value);};
        if (_sheetPage==0) {
            Common::Array<byte> levels,loader;if (!archive.readRange("NA/SRTN.COMBAT.S",0x441a,22,levels) || !archive.readRange("NA/LOADER.P",0x4be,14,loader)) return false;
            static const char *gender[]={"Male","Female","Other"},*race[]={"Human","Halfling","Elf","Dark elf","Dwarf"};const byte g=r[loader[10]],c=r[loader[12]]&63;
            if (!text(5,"MAIN STATISTICS") || !text(7,Common::String("Status: ")+(r[0]==255?"Dead":r[0]==1?"Possessed":r[0]==2?"Sleeping":"Good")) ||
                !text(8,Common::String("Gender: ")+(g<3?gender[g]:"Unknown")) || !text(9,Common::String("Race: ")+(c<5?race[c]:"Unknown")) ||
                !text(10,Common::String::format("Level: %u",uint(r[1]))) || !text(12,Common::String::format("HP: %u / %u   MP: %u / %u",READ_LE_UINT16(r+2),READ_LE_UINT16(r+72),uint(r[4]),uint(r[18]))) ||
                !text(13,r[1]>=10?Common::String::format("XP: %u  (maximum level)",READ_LE_UINT16(r+5)):Common::String::format("XP: %u / %u",READ_LE_UINT16(r+5),READ_LE_UINT16(levels.data()+MIN<uint>(r[1],10)*2))) ||
                !text(15,Common::String::format("STR: %u   Weight: %u / %u",uint(r[16]),uint(r[70]),uint(r[16]))) || !text(16,Common::String::format("DEX: %u   INT: %u",uint(r[17]),uint(r[18])))) return false;
            Common::String hands[2];for (uint hand=0;hand<2;++hand) for (uint i=0;i<19 && r[90+hand*19+i];++i) hands[hand]+=char(r[90+hand*19+i]&127);
            if (!text(18,"LH: "+hands[0]) || !text(19,"RH: "+hands[1])) return false;
        } else if (_sheetPage==1) {
            if (!text(5,"READIED EQUIPMENT")) return false;const EquipmentRecord &equipment=*game.party.equipment(_member);
            static const char *labels[]={"LH","RH","Head","Body","Feet","Hands","Neck","Ring"};
            for (uint i=0;i<8;++i) {const byte kind=equipment.bytes[i*2],id=equipment.bytes[i*2+1];ItemDefinition d;Common::String name=kind==3?Common::String("--"):catalog.definition(kind,id,d)?definitionName(d):Common::String("--");if (!text(7+i,Common::String(labels[i])+": "+name)) return false;}
        } else if (_sheetPage==2) {
            if (!text(5,"COMBAT STATS") || !text(7,Common::String::format("Modified damage - left:  %u",uint(r[9]))) || !text(8,Common::String::format("Modified damage - right: %u",uint(r[10]))) ||
                !text(10,Common::String::format("Armor rating: %u",uint(r[7]))) || !text(11,Common::String::format("Resist magic: %u",uint(r[11]))) || !text(13,Common::String::format("To hit: %u%%",uint((r[8]>>4)*10+(r[8]&15))))) return false;
        } else {
            static const char *skills[]={"Melee","Range","Critical Hit","Dodge-Parry","Lockpicking"};if (!text(5,"SKILLS              VALUE  PROGRESS")) return false;
            for (uint i=0;i<5;++i) if (!text(7+i*2,Common::String::format("%-17s %3u    %u/%u",skills[i],uint(r[25+i*3]),uint(r[26+i*3]),uint(r[27+i*3])))) return false;
        }
        return text(21,Common::String::format("Left/Right: stats page %u/4",_sheetPage+1)) && text(22,Common::String::format("Party gold: %u  Food: %u",uint(game.gold),uint(game.food)));
    }
    bool paint(HdvArchive &archive,const NewGame &game,HgrFrame &frame) const {
        if (_member>=game.party.memberCount()) return false;ItemCatalog catalog;GlyphFont font;EquipmentOwnership ownership;Common::Array<uint> slots;
        if (!catalog.load(archive) || !font.load(archive,1) || !ownership.load(archive) || !collect(archive,game.inventory,slots)) return false;
        HgrFrame next;const byte *r=game.party.member(_member)->bytes;Common::String name;for (uint i=75;i<90 && r[i];++i) name+=char(r[i]&127);
        static const char *tabs[]={"Stats","Weapons","Armor","Items","Spells","Settings","Special","All items"};
        if (!MenuChrome::begin(font,next,Common::String::format("%u %s - %s",_member+1,name.c_str(),tabs[_tab]),191) || !MenuChrome::text(font,next,29,1,"Esc:close") ||
            !MenuChrome::text(font,next,1,2,"1 Stats  2 Weapons  3 Armor  4 Items") || !MenuChrome::text(font,next,1,3,"5 Spells  6 Settings  7 Special") ||
            !MenuChrome::text(font,next,1,20,"Tab/1-7:tabs Space/Shift+1-6:char")) return false;
        if (_phase==1 && _tab==0) {if (!sheet(archive,game,font,next)) return false;frame=next;return true;}
        if (_phase==1 && _tab==5) {
            if (!paintSettings(archive,font,next)) return false;
            frame=next;return true;
        }
        if (_phase==2) {
            if (_selected>=slots.size()) return false;
            const InventoryItem &item=*game.inventory.item(slots[_selected]);
            ItemDefinition definition;
            if (!catalog.definition(item.kind(),item.id(),definition) ||
                !MenuChrome::text(font,next,2,5,definitionName(definition)) ||
                !MenuChrome::text(font,next,2,7,_messages.get(OriginalMessages::kBookSlot)) ||
                !MenuChrome::text(font,next,1,21,"Esc: cancel")) return false;
            frame=next;return true;
        }
        if (_tab==4 && _slotsView) {
            if (!MenuChrome::text(font,next,2,5,"READIED PARTY SPELLS")) return false;
            for (uint i=0;i<10;++i) {
                const byte spell=game.book.spell(i);
                ItemDefinition definition;
                Common::String spellName="--";
                // In v119 the owned spellbook table is indexed by spell number.
                if (spell<64 && catalog.definition(2,spell,definition) && (definition.bytes[12]&31)==8 && definition.bytes[6]==spell)
                    spellName=definitionName(definition);
                if (!MenuChrome::text(font,next,2,7+i,Common::String::format("%u  %s",i,spellName.c_str()))) return false;
            }
            if (!MenuChrome::text(font,next,1,21,"P: back to owned spellbooks") ||
                !MenuChrome::text(font,next,1,22,"Cast from the world with C")) return false;
            frame=next;return true;
        }
        const uint selected=slots.empty()?0:MIN<uint>(_selected,slots.size()-1),page=selected/6*6;
        if (!MenuChrome::text(font,next,2,5,"ITEM                       WT SPARE") || !MenuChrome::text(font,next,1,17,Common::String::format("Page %u/%u  WT:%u/%u HP:%u/%u",selected/6+1,MAX<uint>(1,(slots.size()+5)/6),uint(r[70]),uint(r[16]),READ_LE_UINT16(r+2),READ_LE_UINT16(r+72)))) return false;
        if (slots.empty() && !MenuChrome::text(font,next,2,7,_messages.get(OriginalMessages::kNoUnreadied))) return false;
        for (uint row=0;row<6 && page+row<slots.size();++row) {
            const InventoryItem &item=*game.inventory.item(slots[page+row]);ItemDefinition d;byte copies,mask,clear;if (!catalog.definition(item.kind(),item.id(),d) || !ownership.readyCopies(item,copies) || !ownership.masks(_member,mask,clear)) return false;
            bool equipped=item.kind()<2 && ((item.bytes[3]|((item.bytes[5]&31)==1?item.bytes[4]:0))&mask)!=0;
            if (item.kind()==2 && (d.bytes[12]&31)==8) for (uint i=0;i<10;++i) if (game.book.spell(i)==d.bytes[6]) {++copies;equipped=true;break;}
            const uint spare=item.quantity()>copies?item.quantity()-copies:0;
            if (!MenuChrome::row(font,next,6+row*2,Common::String::format("%c %-25s %2u %3u",equipped?'*':' ',definitionName(d).c_str(),uint(d.bytes[2]),spare),page+row==selected)) return false;
        }
        if (!slots.empty()) {
            const InventoryItem &item=*game.inventory.item(slots[selected]);ItemDefinition d;if (!catalog.definition(item.kind(),item.id(),d)) return false;
            Common::String detail=item.kind()<2?"*: equipped by this character":_tab==4?"P: view readied party spell slots":"U: use/read selected item";
            if (!MenuChrome::text(font,next,1,18,detail)) return false;
        }
        if (!messageLines(font,next,_phase==3?_messages.get(OriginalMessages::kDropAll):_message) ||
            !MenuChrome::text(font,next,1,21,_phase==3?"Y: drop  N/Esc: cancel":"Up/Down:item  Left/Right:page") ||
            !MenuChrome::text(font,next,1,22,_phase==3?"Equipped copies are kept":"Enter:toggle I:info U:use D:drop")) return false;
        frame=next;return true;
    }
    enum SettingsPrompt {kSettingsIdle,kSettingsPassword,kSettingsNewPassword,kSettingsConfirm,kSettingsReboot};

    static bool settingsText(HdvArchive &archive,uint offset,uint length,Common::String &output) {
        Common::Array<byte> bytes;
        if (!archive.readRange("NA/SRTN.INVENTORY",offset,length,bytes)) return false;
        output.clear();
        for (byte c : bytes) output+=char(c&127);
        return true;
    }

    static bool matureDialog() {
        return !ConfMan.hasKey("nox_mature_dialog") || ConfMan.getBool("nox_mature_dialog");
    }

    void beginSetting() {
        const Common::String &domain=ConfMan.getActiveDomainName();
        switch (_selected) {
        case 0:
            CombatPreferences::write(ConfMan,domain,(uint(CombatPreferences::read(ConfMan,domain))+1)%3);
            ConfMan.flushToDisk();
            break;
        case 1: case 2:
            _settingsPrompt=kSettingsPassword;
            break;
        case 3:
            ConfMan.setBool("nox_alternate_font",!(ConfMan.hasKey("nox_alternate_font") && ConfMan.getBool("nox_alternate_font")),domain);
            ConfMan.flushToDisk();
            _settingsPrompt=kSettingsReboot;
            break;
        }
    }

    void clearSettingsPrompt() {
        _settingsPrompt=kSettingsIdle;
        _settingsDigits=0;
        _settingsValue=0;
        _newPassword=0;
    }

    void handleSettingsPrompt(Common::KeyCode key,uint16 ascii) {
        if (key==Common::KEYCODE_ESCAPE || _settingsPrompt==kSettingsReboot) {
            clearSettingsPrompt();
            return;
        }
        if (key==Common::KEYCODE_BACKSPACE || key==Common::KEYCODE_LEFT) {
            if (_settingsDigits) {--_settingsDigits;_settingsValue>>=4;}
            return;
        }
        if (ascii>='0' && ascii<='9' && _settingsDigits<4) {
            _settingsValue=uint16((_settingsValue<<4)|(ascii-'0'));
            ++_settingsDigits;
            return;
        }
        if (key!=Common::KEYCODE_RETURN) return;
        const uint16 value=_settingsValue;
        _settingsDigits=0;_settingsValue=0;
        const Common::String &domain=ConfMan.getActiveDomainName();
        if (_settingsPrompt==kSettingsPassword) {
            const int password=ConfMan.hasKey("nox_dialogue_password",domain)?ConfMan.getInt("nox_dialogue_password",domain):0;
            if (value==password) {
                if (_selected==2) {_settingsPrompt=kSettingsNewPassword;return;}
                ConfMan.setBool("nox_mature_dialog",!matureDialog(),domain);
                ConfMan.flushToDisk();
            } else if (_sounds) _sounds->push_back(kSpeakerRefusal);
        } else if (_settingsPrompt==kSettingsNewPassword) {
            _newPassword=value;_settingsPrompt=kSettingsConfirm;return;
        } else if (value==_newPassword) {
            ConfMan.setInt("nox_dialogue_password",value,domain);
            ConfMan.flushToDisk();
        } else if (_sounds) _sounds->push_back(kSpeakerRefusal);
        clearSettingsPrompt();
    }

    bool paintSettings(HdvArchive &archive,const GlyphFont &font,HgrFrame &frame) const {
        static const uint stateOffsets[]={0x64bb,0x64bf,0x64c3};
        const CombatPreference quick=CombatPreferences::read(ConfMan,ConfMan.getActiveDomainName());
        for (uint row=0;row<4;++row) {
            Common::String label,status;
            if (!settingsText(archive,0x643e+row*16,15,label)) return false;
            if (row<2 && !settingsText(archive,stateOffsets[row==0?uint(quick):uint(matureDialog())],3,status)) return false;
            if (!MenuChrome::row(font,frame,6+row*2,label+status,row==_selected)) return false;
        }
        Common::String text;
        switch (_settingsPrompt) {
        case kSettingsPassword: if (!settingsText(archive,0x647e,9,text)) return false;break;
        case kSettingsNewPassword: if (!settingsText(archive,0x6488,13,text)) return false;break;
        case kSettingsConfirm: if (!settingsText(archive,0x6496,14,text)) return false;break;
        case kSettingsReboot: {
            Common::String next;
            return settingsText(archive,0x64a5,9,text) && settingsText(archive,0x64af,11,next) &&
                MenuChrome::text(font,frame,2,15,text) && MenuChrome::text(font,frame,2,16,next);
        }
        case kSettingsIdle: return true;
        }
        text+=' ';
        for (uint i=0;i<_settingsDigits;++i) text+='*';
        return MenuChrome::text(font,frame,2,15,text);
    }

    SettingsPrompt _settingsPrompt=kSettingsIdle;
    byte _settingsDigits=0;
    uint16 _settingsValue=0,_newPassword=0;
    byte _phase,_member,_type,_map;uint16 _selected;byte _tab,_sheetPage;bool _slotsView;Common::String _message;HgrFrame _underlay;TextWindow _reading;bool _combat;SoundEvents *_sounds;OriginalMessages _messages;
};
} }
#endif
