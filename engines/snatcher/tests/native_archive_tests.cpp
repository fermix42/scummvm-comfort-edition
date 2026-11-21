/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../native_archive.h"
#include <cstdio>
#include <stdexcept>
#include <limits>
namespace {unsigned checks=0;void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}}
int main(){using namespace Snatcher;try{
 NativeArchive writer;uint8_t byte=0xa5;int16_t negative=-2;uint32_t word=0x12345678;uint64_t wide=UINT64_C(0x0123456789abcdef);bool yes=true,no=false;writer.field(byte);writer.field(negative);writer.field(word);writer.field(wide);writer.flag(yes);writer.flag(no);
 const std::vector<uint8_t> golden={0xa5,0xff,0xfe,0x12,0x34,0x56,0x78,1,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,1,0};check(writer.finished()&&writer.bytes==golden,"manual big-endian signed/unsigned wire image");
 NativeArchive reader(golden);uint8_t readByte=0;int16_t readNegative=0;uint32_t readWord=0;uint64_t readWide=0;bool readYes=false,readNo=true;reader.field(readByte);reader.field(readNegative);reader.field(readWord);reader.field(readWide);reader.flag(readYes);reader.flag(readNo);check(reader.finished()&&readByte==byte&&readNegative==-2&&readWord==word&&readWide==wide&&readYes&&!readNo,"independent golden decode");
 for(size_t length=0;length<golden.size();++length){NativeArchive truncated(std::vector<uint8_t>(golden.begin(),golden.begin()+length));uint8_t b=0;int16_t n=0;uint32_t w=0;uint64_t l=0;bool a=false,c=false;truncated.field(b);truncated.field(n);truncated.field(w);truncated.field(l);truncated.flag(a);truncated.flag(c);check(!truncated.finished(),"every truncated field boundary is rejected");}
 auto extra=golden;extra.push_back(0);NativeArchive tail(extra);tail.field(readByte);tail.field(readNegative);tail.field(readWord);tail.field(readWide);tail.flag(readYes);tail.flag(readNo);check(!tail.finished(),"trailing bytes cannot masquerade as a complete owner");
 NativeArchive badBool(std::vector<uint8_t>{2});bool truth=false;badBool.flag(truth);check(!badBool.finished(),"noncanonical boolean rejected");
 NativeArchive scalarShort(std::vector<uint8_t>{0x12});uint32_t sentinel=0xaabbccdd;scalarShort.field(sentinel);check(!scalarShort.ok&&sentinel==0xaabbccdd,"truncated scalar does not assign a partial value");
 NativeArchive numbers;size_t maximum=UINT32_MAX;numbers.number(maximum);check(numbers.finished()&&numbers.bytes==std::vector<uint8_t>({255,255,255,255}),"semantic32bit maximum encodes exactly");NativeArchive maximumRead(numbers.bytes);size_t full=0;maximumRead.number(full);check(maximumRead.finished()&&full==UINT32_MAX,"semantic32bit maximum decodes on64bit host");
 NativeArchive overflow;uint64_t tooLarge=UINT64_C(0x100000000);overflow.number(tooLarge);check(!overflow.finished()&&overflow.bytes.empty(),"semantic32bit write overflow rejected before narrowing");NativeArchive signedNumber;int32_t invalidNegative=-1;signedNumber.number(invalidNegative);check(!signedNumber.finished()&&signedNumber.bytes.empty(),"negative semantic count rejected");
 NativeArchive vectorRead(std::vector<uint8_t>{0,0,0,2,0x12,0x34,0xab,0xcd});std::vector<uint16_t> elements;vectorRead.vector(elements,2);check(vectorRead.finished()&&elements==std::vector<uint16_t>({0x1234,0xabcd}),"manual vector count and element widths");
 NativeArchive vectorWrite;vectorWrite.vector(elements,2);check(vectorWrite.finished()&&vectorWrite.bytes==vectorRead.bytes,"vector writer agrees with manual source format");
 NativeArchive hugeCount(std::vector<uint8_t>{255,255,255,255});std::vector<uint32_t> bounded(1,0x12345678);hugeCount.vector(bounded,64);check(!hugeCount.finished()&&bounded.size()==1&&bounded[0]==0x12345678,"untrusted huge count rejected before resize/allocation");
 NativeArchive missingBody(std::vector<uint8_t>{0,0,0,64,0});bounded.assign(1,7);missingBody.vector(bounded,64);check(!missingBody.finished()&&bounded.size()==1&&bounded[0]==7,"plausible count with insufficient payload rejected before resize");
 NativeArchive excessiveWrite;excessiveWrite.vector(elements,1);check(!excessiveWrite.finished(),"writer enforces declared vector maximum");
 NativeArchive flagVector(std::vector<uint8_t>{0,0,0,3,0,1,0});std::vector<bool> bits;flagVector.flags(bits,3);check(flagVector.finished()&&bits.size()==3&&!bits[0]&&bits[1]&&!bits[2],"canonical packed-owner boolean list");NativeArchive badFlagVector(std::vector<uint8_t>{0,0,0,3,0,2,0});badFlagVector.flags(bits,3);check(!badFlagVector.finished(),"invalid boolean inside vector rejected");
 NativeArchive hugeFlags(std::vector<uint8_t>{255,255,255,255});bits.assign(2,true);hugeFlags.flags(bits,128);check(!hugeFlags.finished()&&bits.size()==2,"oversized boolean vector cannot allocate from forged count");
 NativeArchive narrow(std::vector<uint8_t>{0,1,0,0});uint16_t narrowSentinel=77;narrow.number(narrowSentinel);check(!narrow.finished()&&narrowSentinel==77,"semantic32bit payload cannot truncate into narrow owner type");
 NativeArchive signedOverflow(std::vector<uint8_t>{128,0,0,0});int32_t signedSentinel=99;signedOverflow.number(signedSentinel);check(!signedOverflow.finished()&&signedSentinel==99,"positive semantic count cannot overflow signed owner");
 NativeArchive shortFlag(std::vector<uint8_t>{});bool preserved=true;shortFlag.flag(preserved);check(!shortFlag.finished()&&preserved,"truncated boolean does not mutate owner value");
 std::vector<uint8_t> oversized(4*1024*1024+1,0);NativeArchive tooBig(oversized);check(!tooBig.finished()&&tooBig.bytes.empty(),"owner input budget rejects oversized copy allocation");
 std::printf("native archive: %u checks,0 failures\n",checks);return 0;
 }catch(const std::exception& error){std::fprintf(stderr,"native archive failure: %s\n",error.what());return 1;}}
