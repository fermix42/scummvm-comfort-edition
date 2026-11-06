/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_SNAPSHOT_H
#define NOX_SNAPSHOT_H
#include "common/savefile.h"
#include "common/memstream.h"
#include "common/md5.h"
#include "common/array.h"

namespace Nox {
// Two independent generations: an incomplete replacement never truncates the
// newest validated generation. No filesystem rename guarantees are assumed.
class SnapshotStore {
public:
	enum { kMaxPayloadBytes = 20 * 1024 * 1024 }; // Runtime image plus bounded enhancement history.
	SnapshotStore(Common::SaveFileManager &manager, const Common::String &prefix) : _manager(manager), _prefix(prefix) {}
	Common::String filename(uint index) const { return _prefix + Common::String::format(".g%u",index); }
	bool read(uint index, Common::Array<byte> &payload, uint32 &sequence) const {
		Common::InSaveFile *in = _manager.openForLoading(filename(index));
		if (!in) return false;
		bool ok = false;
		if (in->size() >= 32 && in->readUint32BE() == MKTAG('N','X','S','V')) {
			const uint32 version=in->readUint32LE();
			const uint32 seq = in->readUint32LE(), length = in->readUint32LE();
			byte expected[16], actual[16];
			in->read(expected,16);
			if ((version==1||version==2) && seq && length && length <= kMaxPayloadBytes && in->size() == int64(length)+32+(version==2?4:0)) {
				Common::Array<byte> data; data.resize(length);
				if (in->read(data.data(),length) == length && !in->err()) {
					const bool complete=version==1||in->readUint32BE()==MKTAG('N','X','O','K');
					ok = complete && !in->err() && checksum(version,seq,data.data(),data.size(),actual) && !memcmp(expected,actual,16);
					if (ok) { payload = data; sequence = seq; }
				}
			}
		}
		delete in;
		return ok;
	}
	bool latest(Common::Array<byte> &payload, uint32 &sequence, uint &index) const {
		Common::Array<byte> a,b; uint32 sa=0,sb=0;
		const bool va=read(0,a,sa),vb=read(1,b,sb);
		if (!va && !vb) return false;
		index=vb && (!va || sb>sa) ? 1 : 0;
		payload=index?b:a; sequence=index?sb:sa;
		return true;
	}
	bool write(const byte *payload, uint32 length, Common::String &written, int protectedGeneration = -1,
	           void (*partialWriteObserver)(void *) = nullptr, void *observerContext = nullptr) {
		if (!length || length > kMaxPayloadBytes) return false;
		Common::Array<byte> previous; uint32 seq=0; uint newest=1;
		latest(previous,seq,newest);
		if(protectedGeneration>=0 && protectedGeneration<2)newest=protectedGeneration;
		if (seq==0xffffffffU) return false;
		const uint index=1-newest;
		byte digest[16];
		if (!checksum(2,seq+1,payload,length,digest)) return false;
		Common::OutSaveFile *out=_manager.openForSaving(filename(index),false);
		if (!out) return false;
		out->writeUint32BE(MKTAG('N','X','S','V'));out->writeUint32LE(2);
		out->writeUint32LE(seq+1);out->writeUint32LE(length);out->write(digest,16);
		if(partialWriteObserver){
			const uint32 first=length/2;
			out->write(payload,first);
			if(out->flush()&&!out->err())partialWriteObserver(observerContext);
			out->write(payload+first,length-first);
		}else out->write(payload,length);
		out->writeUint32BE(MKTAG('N','X','O','K'));
		out->finalize();const bool failed=out->err();delete out;
		Common::Array<byte> checked;uint32 checkSeq=0;
		if(failed || !read(index,checked,checkSeq) || checkSeq!=seq+1 || checked.size()!=length || memcmp(checked.data(),payload,length)) return false;
		written=filename(index);return true;
	}
private:
	static bool checksum(uint32 version,uint32 sequence, const byte *payload, uint32 length, byte digest[16]) {
		Common::MemoryWriteStreamDynamic encoded(DisposeAfterUse::YES);
		encoded.writeUint32BE(MKTAG('N','X','S','V'));encoded.writeUint32LE(version);
		encoded.writeUint32LE(sequence);encoded.writeUint32LE(length);encoded.write(payload,length);
		if(version==2)encoded.writeUint32BE(MKTAG('N','X','O','K'));
		Common::MemoryReadStream bytes(encoded.getData(),encoded.size());
		return Common::computeStreamMD5(bytes,digest);
	}
	Common::SaveFileManager &_manager;
	Common::String _prefix;
};
} // namespace Nox
#endif
