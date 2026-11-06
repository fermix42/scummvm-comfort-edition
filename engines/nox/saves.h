/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_SAVES_H
#define NOX_SAVES_H
#include "nox/snapshot.h"
#include "nox/resources.h"
#include "nox/save_policy.h"
#include "engines/ce_achievements.h"
#include "common/algorithm.h"

namespace Nox { namespace Saves {
enum { kMaximumSlot=kMaximumSaveSlot, kMaximumMetadata=65536,
	kPreviewWidth=160, kPreviewHeight=120, kPreviewBytes=kPreviewWidth*kPreviewHeight*2 };
struct Metadata {
	Common::String target,session,description,resourceIdentity=HdvArchive::baseV119Identity();
	uint32 slot=0,playtime=0;
	SaveMode mode=kModernSaveMode;
	uint16 year=0;
	byte month=0,day=0,hour=0,minute=0,second=0;
	Common::Array<byte> preview; // Optional P565: fixed 160x120 RGB565, little-endian pixels.
};
inline bool validDate(const Metadata &meta) {
	if(meta.year<1900||meta.year>9999||meta.month<1||meta.month>12||meta.hour>23||meta.minute>59||meta.second>59)return false;
	const byte days[]={31,28,31,30,31,30,31,31,30,31,30,31};
	const bool leap=meta.year%4==0&&(meta.year%100!=0||meta.year%400==0);
	return meta.day>=1&&meta.day<=days[meta.month-1]+(meta.month==2&&leap?1:0);
}
struct Entry { Common::String filename,prefix; Metadata metadata; uint32 sequence=0,offset=0; uint generation=0; };
inline Common::String key(const Common::String &value) {
	Common::MemoryReadStream bytes((const byte *)value.c_str(),value.size());
	return Common::computeStreamMD5AsString(bytes);
}
inline Common::String targetPrefix(const Common::String &target) { return "nox-ce-"+key(target)+"-"; }
inline Common::String slotPrefix(const Common::String &target,const Common::String &session,int slot) {
	return targetPrefix(target)+key(session)+Common::String::format("-slot%03d",slot);
}
inline void writeText(Common::WriteStream &out,const Common::String &text) { out.writeUint16LE(text.size());out.write(text.c_str(),text.size()); }
inline bool readText(Common::SeekableReadStream &in,Common::String &text,uint max) {
	uint length=in.readUint16LE();
	if(in.eos()||in.err()||length>max||in.size()-in.pos()<length)return false;
	text.clear();for(uint i=0;i<length;++i){char ch=in.readByte();if(!ch)return false;text+=ch;}
	return !in.err()&&!in.eos();
}
inline void writeMetadata(Common::WriteStream &out,const Metadata &meta) {
	Common::MemoryWriteStreamDynamic body(DisposeAfterUse::YES);
	writeText(body,meta.target);writeText(body,meta.session);writeText(body,meta.description);
	body.writeUint32LE(meta.slot);body.writeUint32LE(meta.playtime);
	body.writeUint32BE(MKTAG('R','S','R','C'));body.writeByte(1);body.writeUint32LE(meta.resourceIdentity.size());
	body.write(meta.resourceIdentity.c_str(),meta.resourceIdentity.size());
	body.writeUint32BE(MKTAG('M','O','D','E'));body.writeByte(1);body.writeUint32LE(1);body.writeByte(meta.mode);
	if(meta.year){
		body.writeUint32BE(MKTAG('D','A','T','E'));body.writeByte(0);body.writeUint32LE(7);
		body.writeUint16LE(meta.year);body.writeByte(meta.month);body.writeByte(meta.day);
		body.writeByte(meta.hour);body.writeByte(meta.minute);body.writeByte(meta.second);
	}
	if(!meta.preview.empty()){
		body.writeUint32BE(MKTAG('P','5','6','5'));body.writeByte(0);body.writeUint32LE(meta.preview.size());
		body.write(meta.preview.data(),meta.preview.size());
	}
	out.writeUint32BE(MKTAG('N','S','M','2'));out.writeUint32LE(body.size());out.write(body.getData(),body.size());
}
inline bool readMetadata(Common::SeekableReadStream &in,Metadata &meta) {
	const uint32 magic=in.readUint32BE();
	Metadata candidate;
	if(magic==MKTAG('N','S','M','1')){
		// The M1 reader supported only base v119 and slots 0..999. Preserve
		// that explicit Modern policy, regardless of current new-game options.
		if(!readText(in,candidate.target,256)||!readText(in,candidate.session,128)||!readText(in,candidate.description,512))return false;
		candidate.slot=in.readUint32LE();candidate.playtime=in.readUint32LE();
	}else if(magic==MKTAG('N','S','M','2')){
		const uint32 length=in.readUint32LE();
		if(in.err()||in.eos()||!length||length>kMaximumMetadata||int64(length)>in.size()-in.pos())return false;
		Common::Array<byte> data;data.resize(length);
		if(in.read(data.data(),length)!=length||in.err())return false;
		Common::MemoryReadStream body(data.data(),data.size());
		if(!readText(body,candidate.target,256)||!readText(body,candidate.session,128)||!readText(body,candidate.description,512))return false;
		candidate.slot=body.readUint32LE();candidate.playtime=body.readUint32LE();
		bool resource=false,mode=false,date=false,preview=false;
		while(body.pos()<body.size()){
			if(body.size()-body.pos()<9)return false;
			const uint32 tag=body.readUint32BE();const byte flags=body.readByte();const uint32 chunkLength=body.readUint32LE();
			if(flags>1||int64(chunkLength)>body.size()-body.pos())return false;
			if(tag==MKTAG('R','S','R','C')){
				if(resource||!chunkLength||chunkLength>128)return false;
				candidate.resourceIdentity.clear();
				for(uint32 i=0;i<chunkLength;++i){const char value=body.readByte();if(!value)return false;candidate.resourceIdentity+=value;}
				resource=true;
			}else if(tag==MKTAG('M','O','D','E')){
				if(mode||chunkLength!=1)return false;
				const byte value=body.readByte();if(value>kClassicSaveMode)return false;
				candidate.mode=SaveMode(value);mode=true;
			}else if(tag==MKTAG('D','A','T','E')){
				if(date||chunkLength!=7)return false;
				candidate.year=body.readUint16LE();candidate.month=body.readByte();candidate.day=body.readByte();
				candidate.hour=body.readByte();candidate.minute=body.readByte();candidate.second=body.readByte();
				if(!validDate(candidate))return false;
				date=true;
			}else if(tag==MKTAG('P','5','6','5')){
				if(preview||chunkLength!=kPreviewBytes)return false;
				candidate.preview.resize(chunkLength);
				if(body.read(candidate.preview.data(),chunkLength)!=chunkLength)return false;
				preview=true;
			}else if(flags||!body.seek(chunkLength,SEEK_CUR))return false;
		}
		if(body.err()||body.eos()||!resource||!mode)return false;
	}else return false;
	if(in.err()||in.eos()||candidate.slot>kMaximumSlot||!allowedSaveSlot(candidate.mode,candidate.slot)||
	   candidate.target.empty()||candidate.session.empty()||candidate.resourceIdentity!=HdvArchive::baseV119Identity())return false;
	meta=candidate;return true;
}
inline bool readGeneration(Common::SaveFileManager &manager,const Common::String &prefix,uint generation,const Common::String &target,Entry &entry,Common::Array<byte> &payload) {
	SnapshotStore store(manager,prefix);uint32 sequence=0;
	if(!store.read(generation,payload,sequence))return false;
	Common::MemoryReadStream in(payload.data(),payload.size());Metadata meta;
	if(!readMetadata(in,meta)||meta.target!=target||prefix!=slotPrefix(target,meta.session,meta.slot))return false;
	const Common::String filename=store.filename(generation);
	// Session sidecar is the commit condition. A crash before its creation
	// leaves the previous generation available; stale sidecars are removed
	// before a generation is reused by the writer.
	if(Common::CEAchievements().getSavePlaythroughForFile(filename)!=meta.session)return false;
	entry.filename=filename;entry.prefix=prefix;entry.metadata=meta;
	entry.sequence=sequence;entry.offset=in.pos();entry.generation=generation;
	return true;
}
inline bool best(Common::SaveFileManager &manager,const Common::String &prefix,const Common::String &target,Entry &entry,Common::Array<byte> &payload) {
	bool found=false;
	for(uint i=0;i<2;++i){Entry candidate;Common::Array<byte> data;
		if(readGeneration(manager,prefix,i,target,candidate,data)&&(!found||candidate.sequence>entry.sequence)){
			entry=candidate;payload=data;found=true;
		}
	}
	return found;
}
struct Compare { bool operator()(const Entry &a,const Entry &b) const { return a.prefix<b.prefix; } };
inline bool removeLogical(Common::SaveFileManager &manager,const Entry &entry) {
	struct Backup { Common::String name;Common::Array<byte> data; };
	Common::Array<Backup> backups;
	// Preserve every existing generation/commit before the first deletion. A
	// readable file held by another program may still deny its eventual removal.
	for(uint generation=0;generation<2;++generation)for(uint sidecar=0;sidecar<2;++sidecar){
		Backup backup;backup.name=SnapshotStore(manager,entry.prefix).filename(generation)+(sidecar?".ce-achievements.json":"");
		if(!manager.exists(backup.name))continue;
		Common::InSaveFile *in=manager.openForLoading(backup.name);if(!in)return false;
		const int64 size=in->size();
		if(size<0||size>SnapshotStore::kMaxPayloadBytes+36){delete in;return false;}
		backup.data.resize(size);const bool ok=(!size||in->read(backup.data.data(),size)==uint32(size))&&!in->err();delete in;
		if(!ok)return false;backups.push_back(backup);
	}
	for(uint removed=0;removed<backups.size();++removed){
		if(manager.removeSavefile(backups[removed].name))continue;
		// Some managers evict a failed deletion from their cache. Refresh before
		// restoring only files whose deletion actually succeeded.
		Common::StringArray none;manager.updateSavefilesList(none);manager.clearError();
		for(uint i=0;i<removed;++i){
			const Backup &backup=backups[i];Common::OutSaveFile *out=manager.openForSaving(backup.name,false);bool ok=false;
			if(out){ok=backup.data.empty()||out->write(backup.data.data(),backup.data.size())==backup.data.size();out->finalize();ok=ok&&!out->err();delete out;}
			if(!ok)warning("Unable to restore Nox save after failed deletion: %s",backup.name.c_str());
		}
		return false;
	}
	return true;
}
inline Common::Array<Entry> entries(Common::SaveFileManager &manager,const Common::String &target) {
	Common::Array<Entry> result;Common::StringArray seen;
	const Common::StringArray names=manager.listSavefiles(targetPrefix(target)+"*-slot###.g*");
	for(const auto &name:names){
		if(!name.hasSuffix(".g0")&&!name.hasSuffix(".g1"))continue;
		const Common::String prefix=name.substr(0,name.size()-3);bool duplicate=false;
		for(const auto &previous:seen)if(previous==prefix)duplicate=true;
		if(duplicate)continue;seen.push_back(prefix);
		Entry entry;Common::Array<byte> payload;
		if(best(manager,prefix,target,entry,payload))result.push_back(entry);
	}
	Common::sort(result.begin(),result.end(),Compare());return result;
}
} } // namespace Nox::Saves
#endif
