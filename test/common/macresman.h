#include <cxxtest/TestSuite.h>

#include "common/archive.h"
#include "common/crc.h"
#include "common/endian.h"
#include "common/macresman.h"
#include "common/memstream.h"
#include "test/system/null_osystem.h"

class MacResManagerTestSuite : public CxxTest::TestSuite {
	class ProbeStream : public Common::MemoryReadStream {
		int &_live;
		int64 _reportedSize;
		bool &_outside;
		bool _failNameRead;
	public:
		ProbeStream(const byte *data, uint32 size, int64 reportedSize, int &live, bool &outside, bool failNameRead)
			: MemoryReadStream(data, size), _live(live), _reportedSize(reportedSize), _outside(outside), _failNameRead(failNameRead) { ++_live; }
		~ProbeStream() override { --_live; }
		uint32 read(void *data, uint32 size) override {
			if (_failNameRead && pos() == 75 && size == 3)
				return 0;
			return MemoryReadStream::read(data, size);
		}
		int64 size() const override { return _reportedSize; }
		bool seek(int64 offset, int whence = SEEK_SET) override {
			if (whence == SEEK_SET && (offset < 0 || offset > MemoryReadStream::size())) {
				_outside = true;
				return false;
			}
			return MemoryReadStream::seek(offset, whence);
		}
	};

	// Only a synthetic .rsrc member is exposed; no filesystem or SearchMan state.
	class ForkArchive : public Common::Archive {
	public:
		Common::Array<byte> bytes;
		int64 reportedSize;
		mutable int live;
		mutable bool outside;
		bool failNameRead;
		ForkArchive(const Common::Array<byte> &data, int64 size = -1, bool fail = false)
			: bytes(data), reportedSize(size < 0 ? data.size() : size), live(0), outside(false), failNameRead(fail) {}
		bool hasFile(const Common::Path &path) const override { return path == Common::Path("test.rsrc"); }
		int listMembers(Common::ArchiveMemberList &) const override { return 0; }
		const Common::ArchiveMemberPtr getMember(const Common::Path &) const override { return Common::ArchiveMemberPtr(); }
		Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override {
			return hasFile(path) ? new ProbeStream(bytes.data(), bytes.size(), reportedSize, live, outside, failNameRead) : nullptr;
		}
	};

	Common::Array<byte> fork() {
		Common::Array<byte> b;
		b.resize(78);
		memset(b.data(), 0, b.size());
		WRITE_BE_UINT32(b.data(), 16);
		WRITE_BE_UINT32(b.data() + 4, 24);
		WRITE_BE_UINT32(b.data() + 8, 8);
		WRITE_BE_UINT32(b.data() + 12, 54);
		WRITE_BE_UINT32(b.data() + 16, 4);
		memcpy(b.data() + 20, "DATA", 4);
		WRITE_BE_UINT16(b.data() + 48, 28); // type list, relative to map
		WRITE_BE_UINT16(b.data() + 50, 50); // names, relative to map
		WRITE_BE_UINT32(b.data() + 54, MKTAG('T', 'E', 'S', 'T'));
		WRITE_BE_UINT16(b.data() + 60, 10); // references, relative to type list
		WRITE_BE_UINT16(b.data() + 62, 128);
		b[74] = 3;
		memcpy(b.data() + 75, "one", 3);
		return b;
	}

	void reject(const Common::Array<byte> &bytes, int64 reportedSize = -1, bool early = false) {
		ForkArchive archive(bytes, reportedSize);
		{
			Common::MacResManager manager;
			TS_ASSERT(!manager.open(Common::Path("test"), archive));
			TS_ASSERT(!manager.hasResFork());
			TS_ASSERT_EQUALS(manager.getResTagArray().size(), 0U);
			manager.close();
		}
		TS_ASSERT_EQUALS(archive.live, 0);
		if (early)
			TS_ASSERT(!archive.outside);
	}

	void checkResource(Common::MacResManager &manager) {
		const uint32 tag = MKTAG('T', 'E', 'S', 'T');
		TS_ASSERT_EQUALS(manager.getResIDArray(tag).size(), 1U);
		TS_ASSERT_EQUALS(manager.getResLength(tag, 128), 4U);
		TS_ASSERT_EQUALS(manager.getResName(tag, 128), "one");
		TS_ASSERT_EQUALS(manager.computeResForkMD5AsString(8), "c186736af9cfd3f0f31ab94f61fd6134");
		Common::SeekableReadStream *streams[] = {
			manager.getResource(tag, (uint16)128), manager.getResource("one"), manager.getResource(tag, "one")
		};
		for (uint i = 0; i < ARRAYSIZE(streams); ++i) {
			TS_ASSERT(streams[i]);
			if (streams[i]) {
				TS_ASSERT_EQUALS(streams[i]->readUint32BE(), MKTAG('D', 'A', 'T', 'A'));
				delete streams[i];
			}
		}
	}

public:
	void setUp() { Common::install_null_g_system(); }
	void tearDown() { Common::uninstall_null_g_system(); }

	void test_validRawAndOwnership() {
		ForkArchive archive(fork());
		{
			Common::MacResManager manager;
			TS_ASSERT(manager.open(Common::Path("test"), archive));
			checkResource(manager);
			TS_ASSERT_EQUALS(archive.live, 1);
		}
		TS_ASSERT_EQUALS(archive.live, 0);
	}

	void test_partialPopulationCleanupAndReuse() {
		ForkArchive bad(fork(), -1, true), good(fork());
		Common::MacResManager manager;
		TS_ASSERT(!manager.open(Common::Path("test"), bad));
		TS_ASSERT_EQUALS(bad.live, 0);
		TS_ASSERT(!manager.hasResFork());
		TS_ASSERT(manager.open(Common::Path("test"), good));
		checkResource(manager);
		manager.close();
		TS_ASSERT_EQUALS(good.live, 0);
	}

	void test_nonDefaultTypeListAndUnsignedNameOffset() {
		Common::Array<byte> b = fork();
		b.resize(82);
		memmove(b.data() + 56, b.data() + 52, 26);
		WRITE_BE_UINT32(b.data() + 12, 58);
		WRITE_BE_UINT16(b.data() + 48, 32);
		WRITE_BE_UINT16(b.data() + 50, 54);
		WRITE_BE_UINT16(b.data() + 52, 0xffff); // not the actual type count
		ForkArchive moved(b);
		Common::MacResManager manager;
		TS_ASSERT(manager.open(Common::Path("test"), moved));
		checkResource(manager);
		manager.close();
		b = fork();
		b.resize(74 + 0x8000 + 4);
		memset(b.data() + 78, 0, b.size() - 78);
		WRITE_BE_UINT32(b.data() + 12, b.size() - 24);
		WRITE_BE_UINT16(b.data() + 64, 0x8000);
		b[74 + 0x8000] = 3;
		memcpy(b.data() + 75 + 0x8000, "one", 3);
		ForkArchive highName(b);
		TS_ASSERT(manager.open(Common::Path("test"), highName));
		checkResource(manager);
	}

	void test_reportedAgsHeaderRejectedBeforeMapSeek() {
		Common::Array<byte> b = fork();
		WRITE_BE_UINT32(b.data(), 1129072962U);
		WRITE_BE_UINT32(b.data() + 4, 437584134U);
		WRITE_BE_UINT32(b.data() + 8, 2118512U);
		WRITE_BE_UINT32(b.data() + 12, 1919513701U);
		reject(b, 2037147807, true);
	}

	void test_truncatedHeadersAndMaps() {
		for (uint32 size = 1; size < 78; ++size) {
			Common::Array<byte> b = fork();
			b.resize(size);
			reject(b);
		}
	}

	void test_overflowAndCompleteRegions() {
		const uint32 fields[][2] = {
			{0, 0xfffffff0U}, {4, 0xfffffff0U}, {8, 0xffffffffU}, {12, 0xffffffffU},
			{8, 63}, {12, 55}, {0, 0}, {4, 8}, {4, 20}, {12, 27}
		};
		for (uint i = 0; i < ARRAYSIZE(fields); ++i) {
			Common::Array<byte> b = fork();
			WRITE_BE_UINT32(b.data() + fields[i][0], fields[i][1]);
			reject(b);
		}
	}

	void test_invalidNestedRanges() {
		const uint32 fields[][2] = {
			{48, 53}, {48, 0}, {50, 55}, {52, 0xfffe}, // type list / name list / counts
			{58, 0xffff}, {60, 44}, {64, 54}, {64, 0xfffe} // refs and names
		};
		for (uint i = 0; i < ARRAYSIZE(fields); ++i) {
			Common::Array<byte> b = fork();
			WRITE_BE_UINT16(b.data() + fields[i][0], fields[i][1]);
			reject(b);
		}
		Common::Array<byte> b = fork();
		b[74] = 4; // Pascal string extends past map
		reject(b);
		b = fork();
		WRITE_BE_UINT32(b.data() + 66, 5); // no room for length word
		reject(b);
		b = fork();
		WRITE_BE_UINT32(b.data() + 16, 0xffffffffU); // payload length wraps
		reject(b);
		b = fork();
		WRITE_BE_UINT32(b.data() + 16, 5); // payload extends into map
		reject(b);
	}

	void test_emptyTypeListAndUnnamedResource() {
		Common::Array<byte> b = fork();
		WRITE_BE_UINT16(b.data() + 52, 0xffff);
		ForkArchive empty(b);
		Common::MacResManager manager;
		TS_ASSERT(manager.open(Common::Path("test"), empty));
		TS_ASSERT_EQUALS(manager.getResTagArray().size(), 0U);
		manager.close();
		b = fork();
		WRITE_BE_UINT16(b.data() + 64, 0xffff);
		ForkArchive unnamed(b);
		TS_ASSERT(manager.open(Common::Path("test"), unnamed));
		TS_ASSERT_EQUALS(manager.getResName(MKTAG('T','E','S','T'), 128), "");
		TS_ASSERT_EQUALS(manager.getResLength(MKTAG('T','E','S','T'), 128), 4U);
	}

	void test_macBinaryAndEmbeddedBoundary() {
		Common::Array<byte> raw = fork(), b;
		b.resize(128 + raw.size() + 128);
		memset(b.data(), 0, b.size());
		b[1] = 1; b[2] = 'x';
		WRITE_BE_UINT32(b.data() + 87, raw.size());
		memcpy(b.data() + 128, raw.data(), raw.size());
		Common::MacResManager manager;
		manager.setBaseFileName(Common::Path("test"));
		Common::MemoryReadStream *stream = new Common::MemoryReadStream(b.data(), b.size());
		TS_ASSERT(manager.loadFromMacBinary(stream));
		checkResource(manager);
		manager.close();
		WRITE_BE_UINT32(b.data() + 128 + 12, 55); // fits container, not embedded fork
		stream = new Common::MemoryReadStream(b.data(), b.size());
		TS_ASSERT(!manager.loadFromMacBinary(stream));
		delete stream; // failed load must leave ownership with caller
		WRITE_BE_UINT32(b.data() + 83, 0xffffffffU); // padded data fork must not wrap
		stream = new Common::MemoryReadStream(b.data(), b.size());
		TS_ASSERT(!manager.loadFromMacBinary(stream));
		delete stream;
	}

	void test_wideMacBinaryDataForkEnd() {
		byte header[128] = {};
		header[1] = 1; header[2] = 'x';
		WRITE_BE_UINT32(header + 83, 0xffffffffU);
		Common::CRC_BINHEX crc;
		WRITE_BE_UINT16(header + 124, crc.crcFast(header, 124));
		int live = 0;
		bool outside = false;
		ProbeStream *source = new ProbeStream(header, sizeof(header), 0x100000080LL, live, outside, false);
		Common::SeekableReadStream *data = Common::MacResManager::openDataForkFromMacBinary(source, DisposeAfterUse::YES);
		TS_ASSERT(data);
		if (data) {
			TS_ASSERT_EQUALS(data->size(), 0xffffffffLL);
			delete data;
		} else {
			delete source;
		}
		TS_ASSERT_EQUALS(live, 0);
		TS_ASSERT(!outside);
	}

	void test_macBinaryDataForkStreamBoundsAndOwnership() {
		byte bytes[256] = {};
		bytes[1] = 1; bytes[2] = 'x';
		WRITE_BE_UINT32(bytes + 83, 4);
		Common::CRC_BINHEX crc;
		WRITE_BE_UINT16(bytes + 124, crc.crcFast(bytes, 124));
		memcpy(bytes + 128, "DATA", 4);
		int live = 0;
		bool outside = false;
		ProbeStream *source = new ProbeStream(bytes, sizeof(bytes), sizeof(bytes), live, outside, false);
		Common::SeekableReadStream *data = Common::MacResManager::openDataForkFromMacBinary(source, DisposeAfterUse::NO);
		TS_ASSERT(data);
		if (data) {
			byte buffer[8] = {};
			TS_ASSERT_EQUALS(data->read(buffer, sizeof(buffer)), 4U);
			TS_ASSERT_EQUALS(memcmp(buffer, "DATA", 4), 0);
			TS_ASSERT(data->eos());
			TS_ASSERT(data->seek(-2, SEEK_END));
			TS_ASSERT(!data->eos());
			TS_ASSERT_EQUALS(data->readUint16BE(), 0x5441);
			TS_ASSERT(!data->seek(1, SEEK_END));
			TS_ASSERT(!data->seek(-5, SEEK_CUR));
			TS_ASSERT_EQUALS(data->pos(), 4);
			data->clearErr();
			TS_ASSERT(!data->err());
			delete data;
		}
		TS_ASSERT_EQUALS(live, 1);
		delete source;
		TS_ASSERT_EQUALS(live, 0);
		TS_ASSERT(!outside);
	}

	void test_appleDoubleAndEmbeddedBoundary() {
		Common::Array<byte> raw = fork(), b;
		b.resize(38 + raw.size() + 128);
		memset(b.data(), 0, b.size());
		WRITE_BE_UINT32(b.data(), 0x00051607);
		WRITE_BE_UINT16(b.data() + 24, 1);
		WRITE_BE_UINT32(b.data() + 26, 2);
		WRITE_BE_UINT32(b.data() + 30, 38);
		WRITE_BE_UINT32(b.data() + 34, raw.size());
		memcpy(b.data() + 38, raw.data(), raw.size());
		ForkArchive archive(b);
		Common::MacResManager manager;
		TS_ASSERT(manager.open(Common::Path("test"), archive));
		checkResource(manager);
		manager.close();
		WRITE_BE_UINT32(b.data() + 38 + 12, 55);
		reject(b);
		WRITE_BE_UINT32(b.data() + 30, 0xfffffff0U);
		reject(b);
		WRITE_BE_UINT16(b.data() + 24, 0xffff);
		reject(b);
	}
};
