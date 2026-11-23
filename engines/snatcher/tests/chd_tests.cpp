/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../chd.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool x,const char*s){if(!x)throw std::runtime_error(s);}
struct Memory:Snatcher::ByteSource{std::vector<uint8_t>b;uint64_t size()const override{return b.size();}bool readAt(uint64_t off,void*p,size_t n)const override{if(off>b.size()||n>b.size()-off)return false;if(n)std::memcpy(p,b.data()+size_t(off),n);return true;}};
struct File:Snatcher::ByteSource{mutable std::ifstream f;uint64_t length;File(const char*p):f(p,std::ios::binary),length(0){check(bool(f),"Cannot open probe file");f.seekg(0,std::ios::end);length=uint64_t(f.tellg());}uint64_t size()const override{return length;}bool readAt(uint64_t off,void*p,size_t n)const override{if(off>length||n>length-off)return false;f.clear();f.seekg(std::streamoff(off));return bool(f.read(static_cast<char*>(p),std::streamsize(n)));}};
struct Fault:Snatcher::ByteSource{const Snatcher::ByteSource&base;uint64_t length,patchAt;uint8_t patch;bool replace,failReads,corruptReads,missingMetadata;Fault(const Snatcher::ByteSource&s):base(s),length(s.size()),patchAt(0),patch(0),replace(false),failReads(false),corruptReads(false),missingMetadata(false){}uint64_t size()const override{return length;}bool readAt(uint64_t off,void*p,size_t n)const override{if(failReads||off>length||n>length-off||!base.readAt(off,p,n))return false;uint8_t*b=static_cast<uint8_t*>(p);if(replace&&patchAt>=off&&patchAt-off<n)b[size_t(patchAt-off)]=patch;if(corruptReads&&n)b[0]^=0x7f;if(missingMetadata&&off==0&&n>=56)std::memset(b+48,0,8);return true;}};
}
int runChdTests(){Memory bad;bad.b.resize(124);Snatcher::ChdSource chd(bad);std::string e;uint8_t out[2448];check(!chd.open(e)&&!e.empty(),"Invalid header rejected");check(!chd.readAt(0,out,1),"Unopened read rejected");check(!chd.readUnit(0,out,e),"Unopened unit rejected");chd.close();chd.close();check(!chd.open(e),"Repeated failed open safe");std::cout<<"CHD basic checks PASS\n";return 0;}
#ifdef SNATCHER_CHD_TEST_MAIN
int main(int argc,char**argv){try{runChdTests();if(argc==1)return 0;if(argc!=4){std::cerr<<"chd-tests CHD LOGICAL2448 BIN2352\n";return 2;}File compressed(argv[1]),logical(argv[2]),binary(argv[3]);Snatcher::ChdSource chd(compressed);std::string e;check(chd.open(e),e.c_str());std::vector<uint8_t>a(2448),b(2448);uint64_t rawBytes=0,projectedBytes=0;
 for(uint32_t i=0;i<chd.unitCount();++i){check(chd.readUnit(i,a.data(),e),e.c_str());check(logical.readAt(uint64_t(i)*2448,b.data(),2448),"reference read");if(a!=b){std::cerr<<"Logical mismatch unit "<<i<<'\n';return 1;}rawBytes+=2448;}
 check(rawBytes==logical.size(),"Logical length differs");
 for(uint64_t off=0;off<chd.size();off+=2352){check(chd.readAt(off,a.data(),2352),"projected read");check(binary.readAt(off,b.data(),2352),"binary read");if(!std::equal(a.begin(),a.begin()+2352,b.begin())){std::cerr<<"Projected mismatch sector "<<off/2352<<'\n';return 1;}projectedBytes+=2352;}
 // Exercise the actual logical disc reader's batched raw-sector path on CHD.
 Snatcher::DiscReader compressedDisc(chd,Snatcher::kMode1Raw2352),binaryDisc(binary,Snatcher::kMode1Raw2352);
 check(compressedDisc.open(e)&&binaryDisc.open(e),"CHD/BIN ISO catalogs open");
 check(compressedDisc.files().size()==binaryDisc.files().size(),"CHD/BIN catalog count");
 for(const auto& file:binaryDisc.files()){
  std::vector<uint8_t> x,y;
  check(compressedDisc.readFile(file.name,x,e)&&binaryDisc.readFile(file.name,y,e)&&x==y,"CHD/BIN batched resource bytes");
  check(compressedDisc.readFileAllocation(file.name,x,e)&&binaryDisc.readFileAllocation(file.name,y,e)&&x==y,"CHD/BIN batched allocation bytes");
 }
 check(projectedBytes==binary.size(),"Projected length differs");check(!chd.readAt(chd.size(),a.data(),1),"Out of range accepted");
 for(unsigned repeat=0;repeat<3;++repeat){chd.close();check(chd.open(e),e.c_str());check(chd.readAt(chd.size()-2352,a.data(),2352),"Repeated reopen last sector");check(binary.readAt(binary.size()-2352,b.data(),2352)&&std::equal(a.begin(),a.begin()+2352,b.begin()),"Last sector mismatch");}
 Snatcher::ChdSource second(compressed);check(second.open(e),e.c_str());check(second.readAt(0,a.data(),2352)&&chd.readAt(0,b.data(),2352)&&std::equal(a.begin(),a.begin()+2352,b.begin()),"Independent handle mismatch");
 // Source failure/corrupt compressed-hunk reads occur after successful open;
 // no callback invokes story logic, and failed buffers are never accepted.
 Fault fault(compressed);Snatcher::ChdSource failed(fault);check(failed.open(e),"Fault source initial open");fault.failReads=true;check(!failed.readUnit(0,a.data(),e)&&!e.empty(),"Short/failed read must fail");failed.close();fault.failReads=false;check(failed.open(e),"Reopen after stream failure");fault.corruptReads=true;check(!failed.readUnit(0,a.data(),e)&&!e.empty(),"Corrupt compressed hunk must fail");failed.close();fault.corruptReads=false;
 fault.length=123;check(!failed.open(e),"Truncated input must fail");fault.length=compressed.size();fault.missingMetadata=true;check(!failed.open(e),"Missing metadata must fail");fault.missingMetadata=false;fault.replace=true;fault.patchAt=48;fault.patch=0x7f;check(!failed.open(e),"Out-of-bounds metadata must fail");fault.patchAt=0;fault.patch='X';check(!failed.open(e),"Corrupt header must fail");
 uint8_t header[124];check(compressed.readAt(0,header,124),"Probe header read");uint64_t meta=0;for(unsigned k=48;k<56;++k)meta=meta*256+header[k];fault.patchAt=meta+16+6;fault.patch='9';check(!failed.open(e),"Invalid track index must fail");fault.replace=false;
 // Odd-byte reads straddle audio sample pairs and arbitrary sector boundaries.
 const Snatcher::ChdTrack&t=chd.tracks()[1];uint64_t audio=uint64_t(t.projectedStart+t.pregap)*2352;
 for(size_t n=1;n<20;++n){check(chd.readAt(audio+n,a.data(),31)&&binary.readAt(audio+n,b.data(),31)&&std::equal(a.begin(),a.begin()+31,b.begin()),"Odd audio read mismatch");}
 std::cout<<"PASS raw_bytes="<<rawBytes<<" projected_bytes="<<projectedBytes<<" tracks="<<chd.tracks().size()<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
#endif
