/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_INN_STORAGE_H
#define NOX_NATIVE_INN_STORAGE_H
#include "nox/resources.h"
namespace Nox { namespace Native {
// Original waiting-list data, separated from its obsolete overlay addresses.
// Preserve every byte, including the opaque gap between locations and records.
class InnStorage {
    friend class InnMembership;
public:
    InnStorage() : _ready(false) {memset(_bytes,0,sizeof(_bytes));}
    bool ready() const {return _ready;}
    bool initialize(HdvArchive &archive) {
        Common::Array<byte> data;if (!archive.readRange("NA/SRTN.INVENTORY",512,2048,data)) return false;
        return importState(data.data(),data.size());
    }
    bool importState(const byte *data,uint size) {
        if (!data || size!=sizeof(_bytes)) return false;memcpy(_bytes,data,size);_ready=true;return true;
    }
    const byte *data() const {return _ready?_bytes:nullptr;}
    int firstFree() const {if (_ready) for (uint i=0;i<12;++i) if (_bytes[i*8]&128) return int(i);return -1;}
    int firstAt(byte map) const {if (_ready) for (uint i=0;i<12;++i) if (_bytes[i*8]==map) return int(i);return -1;}
    const byte *member(uint slot) const {return _ready && slot<12?_bytes+512+slot*128:nullptr;}
    bool save(Common::WriteStream &out) const {
        if (!_ready) return false;out.writeUint32BE(MKTAG('N','I','S','T'));out.writeUint16LE(1);out.write(_bytes,sizeof(_bytes));return !out.err();
    }
    bool load(Common::SeekableReadStream &in) {
        if (in.size()-in.pos()!=2054 || in.readUint32BE()!=MKTAG('N','I','S','T') || in.readUint16LE()!=1) return false;
        InnStorage next;if (in.read(next._bytes,2048)!=2048 || in.err()) return false;next._ready=true;*this=next;return true;
    }
private:
    byte _bytes[2048];bool _ready;
};
} }
#endif
