/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INVENTORY_WINDOW_H
#define NOX_NATIVE_INVENTORY_WINDOW_H
#include "nox/native_new_game.h"
#include "nox/native_graphics.h"
#include "nox/native_inventory_actions.h"
#include "nox/native_text_window.h"
namespace Nox { namespace Native {
enum InventoryWindowResult { kInventoryWindowHandled,kInventoryWindowClosed,kInventoryWindowUnsupported };
// Free world inventory selection. Transactions remain in the existing typed
// equipment/book owners; no field or derived-stat edits are performed by UI.
class InventoryWindow {
public:
    InventoryWindow() : _phase(0),_member(0),_type(0),_map(0),_selected(0) {}
    bool active() const {return _phase!=0;}
    byte phase() const {return _phase;}
    bool matches(const World &world) const {return !active() || (world.ready() && _type==world.mapType() && _map==world.mapId());}
    bool begin(HdvArchive &archive,const NewGame &game,HgrFrame &frame) {
        if (active() || !game.ready()) return false;
        InventoryWindow next;next._phase=1;next._type=game.world.mapType();next._map=game.world.mapId();next._underlay=frame;
        if (!next.paint(archive,game,frame)) return false;*this=next;return true;
    }
    InventoryWindowResult handle(const Common::Event &event,HdvArchive &archive,NewGame &game,HgrFrame &frame) {
        if (!active() || !matches(game.world) || event.type!=Common::EVENT_KEYDOWN) return kInventoryWindowUnsupported;
        const Common::KeyCode key=event.kbd.keycode;const byte ascii=byte(event.kbd.ascii)&95;
        if (_phase==4) {
            if (_reading.waiting()) return _reading.handle(event) && _reading.paint(frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
            _phase=1;_message.clear();return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
        }
        if (key==Common::KEYCODE_ESCAPE) {
            if (_phase==2 || _phase==3) {_phase=1;_message.clear();}
            else {frame=_underlay;*this=InventoryWindow();return kInventoryWindowClosed;}
        } else {
            Common::Array<uint> slots;collect(game.inventory,slots);
            if (_selected>=slots.size()) _selected=slots.empty()?0:uint16(slots.size()-1);
            if (_phase==3) {
                if (ascii=='Y' || ascii=='N') {
                    if (slots.empty()) return kInventoryWindowUnsupported;
                    const InventoryDropResult result=InventoryActions::drop(archive,game,slots[_selected],ascii=='Y');
                    if (result==kDropUnsupported) return kInventoryWindowUnsupported;
                    _message=result==kDropApplied?"Dropped.":result==kDropCancelled?"Cancelled.":result==kDropProtected?"You must keep that item.":"You must unready the item first.";
                    _phase=1;slots.clear();collect(game.inventory,slots);
                    if (_selected>=slots.size()) _selected=slots.empty()?0:uint16(slots.size()-1);
                }
            } else if (_phase==2) {
                if (event.kbd.ascii>='0' && event.kbd.ascii<='9' && !slots.empty()) {
                    const byte bookSlot=event.kbd.ascii=='0'?9:byte(event.kbd.ascii-'1');
                    if (!bookAction(archive,game,*game.inventory.item(slots[_selected]),bookSlot)) return kInventoryWindowUnsupported;
                    _phase=1;
                }
            } else if (event.kbd.ascii>='1' && event.kbd.ascii<='6') {
                const byte member=event.kbd.ascii-'1';if (member<game.party.memberCount()) {_member=member;_message.clear();}
            } else if (key==Common::KEYCODE_UP && _selected) --_selected;
            else if (key==Common::KEYCODE_DOWN && uint(_selected)+1<slots.size()) ++_selected;
            else if (key==Common::KEYCODE_LEFT) _selected=_selected>=6?_selected-6:0;
            else if (key==Common::KEYCODE_RIGHT && uint(_selected)+6<slots.size()) _selected+=6;
            else if ((ascii=='U' || ascii=='D') && !slots.empty()) {
                const InventoryItem &item=*game.inventory.item(slots[_selected]);ItemCatalog catalog;ItemDefinition definition;
                if (!catalog.load(archive) || !catalog.definition(item.kind(),item.id(),definition)) return kInventoryWindowUnsupported;
                if (ascii=='D') {
                    if (definition.bytes[8]&128) _message="You must keep that item.";
                    else {_phase=3;_message.clear();}
                } else if (item.kind()!=2 || !(definition.bytes[8]&64)) _message="Item not useable.";
                else {
                    Common::String text;
                    if (definition.bytes[11]&128) {if (!InventoryActions::document(archive,definition,text)) return kInventoryWindowUnsupported;}
                    else if (definition.bytes[11]&32) text=Common::String::format("X-AXIS %u\rY-AXIS %u",game.world.x(),game.world.y());
                    else if (definition.bytes[11]&64) {
                        const NavigationResult result=game.navigation.bind(definition.bytes[6],NavigationPoint(game.world.mapType(),game.world.mapId(),game.world.x(),game.world.y()));
                        if (result==kNavigationUnsupported) return kInventoryWindowUnsupported;
                        _message=result==kNavigationSelected?"Enchantment cast!":"Not here!";
                        return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
                    } else return kInventoryWindowUnsupported;
                    Common::Array<byte> encoded;for (uint i=0;i<text.size();++i) encoded.push_back(byte(text[i])|128);
                    HgrFrame background;TextWindow reading;
                    if (!reading.begin(archive,encoded.data(),encoded.size(),1,38,1,22,12,&background) || !reading.paint(background)) return kInventoryWindowUnsupported;
                    _reading=reading;_phase=4;_message.clear();frame=background;return kInventoryWindowHandled;
                }
            }
            else if ((ascii=='R' || key==Common::KEYCODE_SPACE || key==Common::KEYCODE_RETURN) && !slots.empty()) {
                const InventoryItem item=*game.inventory.item(slots[_selected]);ItemCatalog catalog;ItemDefinition definition;EquipmentOwnership ownership;
                if (!catalog.load(archive) || !catalog.definition(item.kind(),item.id(),definition) || !ownership.load(archive)) return kInventoryWindowUnsupported;
                const bool unready=key==Common::KEYCODE_SPACE;NewGame next(game);byte result=255;
                if (item.kind()==0) result=EquipmentActions::selectWeapon(catalog,ownership,_member,item.id(),unready,next.party,next.inventory);
                else if (item.kind()==1) result=EquipmentActions::selectArmor(catalog,ownership,_member,item.id(),unready,next.party,next.inventory);
                else if ((definition.bytes[12]&31)==8) {
                    bool ready=false;for (uint i=0;i<10;++i) ready|=game.book.spell(i)==definition.bytes[6];
                    if (ready) {if (!bookAction(archive,game,item,0)) return kInventoryWindowUnsupported;}
                    else if (unready) _message="This book is not readied.";
                    else {_phase=2;_message.clear();}
                    return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
                }
                if (result==0 || result==7) {game.party=next.party;game.inventory=next.inventory;_message=result==0?"Readied.":"Unreadied.";}
                else _message=result==2?"No spare copies.":result==3?"Too much weight.":result==4?"More dexterity needed.":result==5?"More intelligence needed.":"Cannot ready that item here.";
            }
        }
        return paint(archive,game,frame)?kInventoryWindowHandled:kInventoryWindowUnsupported;
    }
    bool save(Common::WriteStream &out) const {
        if (!_phase || _phase>4 || _member>=6 || _selected>255 || _message.size()>80) return false;
        out.writeUint32BE(MKTAG('N','I','W','N'));out.writeUint16LE(_phase>=3?2:1);out.writeByte(_phase);out.writeByte(_member);
        out.writeByte(_type);out.writeByte(_map);out.writeUint16LE(_selected);out.writeByte(byte(_message.size()));out.write(_message.c_str(),_message.size());out.write(_underlay.data(),8192);
        if (_phase==4) {Common::MemoryWriteStreamDynamic part(DisposeAfterUse::YES);if (!_reading.save(part)) return false;out.writeUint32LE(part.size());out.write(part.getData(),part.size());}
        return !out.err();
    }
    bool load(Common::SeekableReadStream &in,HdvArchive &archive) {
        if (in.readUint32BE()!=MKTAG('N','I','W','N')) return false;
        const uint version=in.readUint16LE();if (version!=1 && version!=2) return false;
        InventoryWindow next;next._phase=in.readByte();next._member=in.readByte();next._type=in.readByte();next._map=in.readByte();next._selected=in.readUint16LE();const byte length=in.readByte();
        if (!next._phase || next._phase>(version==1?2:4) || next._member>=6 || next._selected>255 || length>80 || in.size()-in.pos()<8192+length) return false;
        for (uint i=0;i<length;++i) {const byte c=in.readByte();if (c<32 || c>=127) return false;next._message+=char(c);}
        byte image[8192];if (in.read(image,8192)!=8192 || !next._underlay.importImage(image,8192) || in.err()) return false;
        if (next._phase==4) {
            const uint32 size=in.readUint32LE();if (!size || size>131072 || int64(size)!=in.size()-in.pos()) return false;
            Common::Array<byte> bytes;bytes.resize(size);if (in.read(bytes.data(),size)!=size) return false;Common::MemoryReadStream part(bytes.data(),size);
            if (!next._reading.load(part,archive)) return false;
        }
        if (in.err() || in.pos()!=in.size()) return false;*this=next;return true;
    }
private:
    static void collect(const Inventory &inventory,Common::Array<uint> &slots) {for (uint i=0;i<256;++i) if (!inventory.item(i)->empty()) slots.push_back(i);}
    bool bookAction(HdvArchive &archive,NewGame &game,const InventoryItem &item,byte slot) {
        ItemCatalog catalog;if (!catalog.load(archive)) return false;NewGame next(game);
        const BookResult result=next.book.select(catalog,next.inventory,next.party,item.id(),slot,false,false);
        if (result==kBookReadied || result==kBookUnreadied) {game.book=next.book;_message=result==kBookReadied?"Book readied.":"Book unreadied.";}
        else _message=result==kBookIntelligenceRefusal?"More intelligence needed.":"Cannot ready that book.";
        return true;
    }
    static bool line(const GlyphFont &font,HgrFrame &frame,uint x,uint y,const Common::String &text) {
        for (uint i=0;i<text.size() && x+i<40;++i) if (!frame.drawGlyph(font,byte(text[i])|128,x+i,y,0)) return false;return true;
    }
    bool paint(HdvArchive &archive,const NewGame &game,HgrFrame &frame) const {
        if (_member>=game.party.memberCount()) return false;
        ItemCatalog catalog;GlyphFont font;EquipmentOwnership ownership;Common::Array<uint> slots;
        if (!catalog.load(archive) || !font.load(archive,1) || !ownership.load(archive)) return false;
        collect(game.inventory,slots);HgrFrame next;const CharacterRecord *member=game.party.member(_member);Common::String name;
        for (uint i=75;i<91 && member->bytes[i];++i) name+=char(member->bytes[i]&127);
        if (!line(font,next,1,1,Common::String::format("Inventory - %u %s",_member+1,name.c_str())) || !line(font,next,1,3,"1-6: party member  Arrows: select")) return false;
        const uint page=_selected/6*6;
        for (uint row=0;row<6 && page+row<slots.size();++row) {
            const InventoryItem &item=*game.inventory.item(slots[page+row]);ItemDefinition definition;byte ready;
            if (!catalog.definition(item.kind(),item.id(),definition) || !ownership.readyCopies(item,ready)) return false;
            Common::String title;for (uint j=13;j<32 && definition.bytes[j];++j) title+=char(definition.bytes[j]&127);
            if (!line(font,next,1,6+row*2,Common::String::format("%c %s",page+row==_selected?'>':' ',title.c_str())) ||
                !line(font,next,29,6+row*2,Common::String::format("%u/%u",item.quantity(),ready))) return false;
        }
        if (!line(font,next,1,19,_message) || !line(font,next,1,21,_phase==2?"Ready book in slot 1-9,0":_phase==3?"Drop spare copies? Y/N":"R/Return: ready  Space: unready") || !line(font,next,1,23,"U:use  D:drop  Escape:return")) return false;
        frame=next;return true;
    }
    byte _phase,_member,_type,_map;uint16 _selected;Common::String _message;HgrFrame _underlay;TextWindow _reading;
};
} }
#endif
