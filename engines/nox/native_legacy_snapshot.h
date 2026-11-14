/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_LEGACY_SNAPSHOT_H
#define NOX_NATIVE_LEGACY_SNAPSHOT_H
#include "nox/resources.h"
#include "nox/enhancements.h"
#include "common/memstream.h"
#include "common/ptr.h"
#include "nox/native_world.h"
#include "nox/native_new_game.h"
namespace Nox { namespace Native {
// Data decoder for the existing CE development-save schema. Machine registers,
// banks and synthesis fields are consumed as format data, never restored into
// an execution environment. Native gameplay conversion is a separate stage.
class LegacySnapshotData {
public:
    LegacySnapshotData() : _version(0),_waiting(false),_music(false) {}
    bool read(Common::SeekableReadStream &in) {
        Common::ScopedPtr<LegacySnapshotData> owner(new LegacySnapshotData);
        LegacySnapshotData &next=*owner;
        next._version=in.readUint32LE();if(next._version!=1 && next._version!=2) return false;
        next._main.resize(65536);next._aux.resize(65536);
        if(in.read(next._main.data(),65536)!=65536 || in.read(next._aux.data(),65536)!=65536 || !in.seek(32768,SEEK_CUR)) return false;
        byte machine[24];if(in.read(machine,sizeof(machine))!=sizeof(machine)) return false;
        // Match the old schema's structural flags without depending on its PC,
        // instruction origin or registers to continue the imported world.
        if(machine[10]>1 || machine[11]<1 || machine[11]>2 || machine[12]>1 || machine[13]>1 || machine[14]>1) return false;
        for(uint i=15;i<24;++i) if(machine[i]>1) return false;
        next._music=machine[22]!=0;next._waiting=machine[23]!=0;
        if(!in.seek(24,SEEK_CUR)) return false; // Three historical counters.
        const uint32 size=in.readUint32LE();
        if(!size || size>20*1024*1024 || int64(size)+274>in.size()-in.pos()) return false;
        next._image.resize(size);if(in.read(next._image.data(),size)!=size || !in.seek(274,SEEK_CUR)) return false;
        if(next._version==2 && !next._history.read(in)) return false;
        if(in.err() || in.pos()!=in.size()) return false;
        Common::MemoryReadStream image(next._image.data(),next._image.size());HdvArchive archive;
        if(!archive.open(image) || !archive.isBaseV119()) return false;
        *this=next;return true;
    }
    uint version() const {return _version;}
    bool worldWaiting() const {return _waiting;}
    bool musicEnabled() const {return _music;}
    const Common::Array<byte> &mainData() const {return _main;}
    const Common::Array<byte> &auxiliaryData() const {return _aux;}
    const Common::Array<byte> &imageData() const {return _image;}
    const Enhancements &history() const {return _history;}
    // Convert mutable map records through the ordinary native world codec.
    // The active tables have not necessarily been flushed to the disk image.
    bool importWorld(HdvArchive &original,World &world,Common::String &reason) const {
        if (!_waiting || _main.size()!=65536) {reason="Legacy snapshot is not at a world boundary";return false;}
        Common::MemoryReadStream image(_image.data(),_image.size());HdvArchive mutableArchive;
        if (!mutableArchive.open(image)) return false;
        const char *names[]={"SURF","UM","RUIN","TOWN","TOWN2","CSTL","KEEP"};
        const byte types[]={0,2,3,6,5,7,8};
        Common::MemoryWriteStreamDynamic records(DisposeAfterUse::YES);uint count=0;bool active=false;
        for (uint family=0;family<7;++family) {
            const Common::String name=Common::String("NA/DATA.MAP.")+names[family];
            Common::Array<byte> source,baseline;MapResource maps;
            if (!mutableArchive.read(name,source) || !original.read(name,baseline) || source.size()!=baseline.size() || !maps.open(source,family==0,family==1)) {reason="Legacy map resource structure is unsupported";return false;}
            Common::Array<byte> mutableBytes;mutableBytes.resize(source.size());memset(mutableBytes.data(),1,mutableBytes.size());
            // Compare authored extents, excluding unused file padding. Surface's
            // accepted 0/1 header is an initialization marker, not native state.
            uint directoryEnd=(family==0?2:1)+2;
            for (uint n=0;n<maps.maps().size();++n) directoryEnd+=maps.maps()[n].metadata.size();
            memset(mutableBytes.data(),0,directoryEnd);if(family==0) mutableBytes[0]=1;
            for (uint n=0;n<maps.maps().size();++n) {
                const MapDefinition &map=maps.maps()[n];Common::Array<byte> tables;
                if (!maps.stateTables(map.id,tables)) return false;
                const uint begin=map.compressed?map.tiles-514:map.tiles;
                memset(mutableBytes.data()+begin,0,map.tiles+map.size-begin);
                const byte *metadata=map.metadata.data();
                const uint notice=uint(metadata[15])|(uint(metadata[16])<<8)|(uint(metadata[17])<<16);
                const uint noticeSize=READ_LE_UINT16(metadata+18);
                if(notice>source.size() || noticeSize>source.size()-notice) {reason="Legacy notice extent is invalid";return false;}
                memset(mutableBytes.data()+notice,0,noticeSize);
                const bool current=(types[family]==_main[0x267d] || (family==1 && _main[0x267d]==4)) && map.id==_main[0x2af9];
                if (current) {memcpy(tables.data(),_main.data()+0xba00,1536);active=true;}
                records.writeByte(family);records.writeByte(map.id);records.write(tables.data(),tables.size());++count;
            }
            for (uint n=0;n<source.size();++n) if (!mutableBytes[n] && source[n]!=baseline[n]) {reason=Common::String::format("Legacy %s contains a change outside native map records at %u",names[family],n);return false;}
        }
        if (!active || !count || count>76) {reason="Legacy active map is unsupported";return false;}
        Common::MemoryWriteStreamDynamic packet(DisposeAfterUse::YES);
        packet.writeUint32BE(MKTAG('N','W','L','D'));packet.writeUint16LE(1);
        packet.writeByte(_main[0x267d]);packet.writeByte(_main[0x2af9]);packet.writeByte(_main[0x6cec]);packet.writeByte(_main[0x6ced]);packet.writeByte(_main[0x377f]==255);
        packet.write(_main.data()+0x400,256);packet.writeUint16LE(count);packet.write(records.getData(),records.size());
        Common::MemoryReadStream input(packet.getData(),packet.size());
        if (!world.load(input) || !world.restoreLocationCaption(_main.data()+0x7df,256)) {reason="Legacy map state failed native validation";return false;}
        reason.clear();return true;
    }
    // Build a complete core candidate with the existing persistence codecs.
    // No map-entry schedule is rerun: it would overwrite live imported actors.
    bool importCore(HdvArchive &original,NewGame &game,Common::String &reason) const {
        Common::ScopedPtr<World> world(new World(original));
        if (!importWorld(original,*world,reason)) return false;
        Common::MemoryReadStream image(_image.data(),_image.size());HdvArchive mutableArchive;Common::Array<byte> inventory;
        if (!mutableArchive.open(image) || !mutableArchive.read("NA/SRTN.INVENTORY",inventory) || inventory.size()<4096) {reason="Legacy inventory resource is unavailable";return false;}
        Common::MemoryWriteStreamDynamic w(DisposeAfterUse::YES),p(DisposeAfterUse::YES),b(DisposeAfterUse::YES),s(DisposeAfterUse::YES),i(DisposeAfterUse::YES),n(DisposeAfterUse::YES),core(DisposeAfterUse::YES);
        if (!world->save(w)) return false;
        p.writeUint32BE(MKTAG('N','P','A','R'));p.writeUint16LE(1);p.write(_aux.data()+0x1600,768);p.write(inventory.data(),96);
        SpellBook book;InnStorage inn;
        if (!book.importState(_main.data()+0x7043,20) || !book.save(b) || !inn.importState(inventory.data()+512,2048) || !inn.save(i)) {reason="Legacy spellbook or inn state is invalid";return false;}
        s.writeUint32BE(MKTAG('N','N','P','S'));s.writeUint16LE(2);s.write(_main.data()+0x6ed9,160);s.write(_main.data()+0x6f79,160);
        for (uint slot=0;slot<256;slot+=8) {
            const byte hour=_main[0x610+slot],minute=_main[0x611+slot];const uint pointer=READ_LE_UINT16(_main.data()+0x612+slot);uint offset=65535;
            if (minute!=255) {
                if (pointer>=0xba00 && pointer<0xbd00 && !(pointer&7)) offset=pointer-0xba00;
                else if (slot==0 && _main[0x267d]==7 && _main[0x2af9]==15 && pointer==0xe000) offset=65534;
                else {reason="Legacy selected NPC schedule has no native data record";return false;}
            }
            s.writeByte(hour);s.writeByte(minute);s.writeUint16LE(offset);
        }
        s.write(_main.data()+0x50b,56);s.write(_main.data()+0x54b,60);s.writeByte(_main[0x58e]);s.writeByte(_main[0x2afe]);
        n.writeUint32BE(MKTAG('N','N','A','V'));n.writeUint16LE(1);n.writeByte(1);
        n.writeByte(_main[0x36f9]);n.writeByte(_main[0x36f8]);n.writeByte(_main[0x36fa]);n.writeByte(_main[0x36fb]);n.writeByte(_main[0x6133]);n.writeByte(_main[0x6134]);n.write(_main.data()+0x705b,36);
        core.writeUint32BE(MKTAG('N','G','S','T'));core.writeUint16LE(4);
        Common::MemoryWriteStreamDynamic *const parts[]={&w,&p,&b,&s,&i};
        for(uint part=0;part<5;++part) {core.writeUint32LE(parts[part]->size());core.write(parts[part]->getData(),parts[part]->size());}
        core.write(inventory.data()+0xa00,1536);core.writeUint32LE(n.size());core.write(n.getData(),n.size());
        const uint clock[]={0x2bf8,0x3878,0x3879,0x3ffc,0x367f,0x414};for(uint k=0;k<6;++k) core.writeByte(_main[clock[k]]);
        core.writeUint16LE(READ_LE_UINT16(_main.data()+0x7059));core.writeUint16LE(READ_LE_UINT16(_main.data()+0x7057));core.writeByte(_main[0x3f7f]);
        Common::MemoryReadStream input(core.getData(),core.size());
        if (core.err() || !game.load(input)) {reason="Legacy core failed native state validation";return false;}
        // Import the retained commerce inputs as data. A24B compares kind
        // with2; all other byte values intentionally leave counters unchanged.
        game.commerceCounter.kind=_main[0x264];game.commerceCounter.quantity=_main[0x269];
        game.commerceCounter.kindKnown=game.commerceCounter.quantityKnown=true;
        game.discovery=_history;reason.clear();return true;
    }
private:
    uint _version;bool _waiting,_music;
    Common::Array<byte> _main,_aux,_image;Enhancements _history;
};
} }
#endif
