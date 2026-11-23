#include "../disc.h"
#undef NDEBUG
#include <cassert>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
namespace {
struct Memory:Snatcher::ByteSource {std::vector<uint8_t>b;mutable size_t calls=0,largest=0;size_t failCall=0;uint64_t size()const override{return b.size();}bool readAt(uint64_t p,void*d,size_t n)const override{++calls;largest=std::max(largest,n);if(calls==failCall)return false;if(p>b.size()||n>b.size()-p)return false;if(n)std::memcpy(d,&b[size_t(p)],n);return true;}};
void dual32(uint8_t*p,uint32_t n){for(int i=0;i<4;++i){p[i]=uint8_t(n>>(8*i));p[7-i]=p[i];}}
void dual16(uint8_t*p,uint16_t n){p[0]=uint8_t(n);p[1]=uint8_t(n>>8);p[2]=p[1];p[3]=p[0];}
size_t record(uint8_t*p,uint32_t ext,uint32_t bytes,const std::string&name,bool dir){size_t len=33+name.size();if(len&1)++len;std::memset(p,0,len);p[0]=uint8_t(len);dual32(p+2,ext);dual32(p+10,bytes);p[25]=dir?2:0;dual16(p+28,1);p[32]=uint8_t(name.size());std::memcpy(p+33,name.data(),name.size());return len;}
Memory iso(){Memory m;m.b.resize(24*2048);uint8_t*p=&m.b[16*2048];p[0]=1;std::memcpy(p+1,"CD001",5);p[6]=1;dual32(p+80,24);dual16(p+128,2048);record(p+156,20,2048,std::string(1,'\0'),true);p=&m.b[20*2048];size_t n=record(p,20,2048,std::string(1,'\0'),true);n+=record(p+n,20,2048,std::string(1,'\1'),true);record(p+n,22,3,"TEST.BIN;1",false);m.b[22*2048]=9;m.b[22*2048+1]=8;m.b[22*2048+2]=7;return m;}
struct FileSource:Snatcher::ByteSource {mutable std::ifstream f;uint64_t length;FileSource(const char*p):f(p,std::ios::binary),length(0){if(f){f.seekg(0,std::ios::end);length=uint64_t(f.tellg());}}uint64_t size()const override{return length;}bool readAt(uint64_t p,void*d,size_t n)const override{if(p>length||n>length-p)return false;f.clear();f.seekg(std::streamoff(p));return bool(f.read(static_cast<char*>(d),std::streamsize(n)));}};
bool write(const std::string&p,const std::vector<uint8_t>&v){std::ofstream o(p.c_str(),std::ios::binary);if(!v.empty())o.write(reinterpret_cast<const char*>(&v[0]),v.size());return bool(o);}
}
int runDiscTests(){
 using namespace Snatcher;std::string e;std::vector<uint8_t> out;Memory m=iso();m.b[22*2048+3]=0xa5;m.b[23*2048-1]=0x5a;DiscReader d(m,kIso2048);assert(d.open(e));assert(d.files().size()==1&&d.files()[0].size==3);assert(d.readFile("TEST.BIN",out,e)&&out==std::vector<uint8_t>({9,8,7}));assert(d.readFileAllocation("TEST.BIN",out,e)&&out.size()==2048&&out[0]==9&&out[1]==8&&out[2]==7&&out[3]==0xa5&&out.back()==0x5a);assert(!d.readFileAllocation("MISSING",out,e)&&out.empty());assert(d.readBoot(out,e)&&out.size()==32768);assert(!d.readFile("MISSING",out,e)&&out.empty());
 Memory allocationSpanning=iso();dual32(&allocationSpanning.b[20*2048+68+10],2049);allocationSpanning.b[23*2048]=42;allocationSpanning.b[24*2048-1]=0xb7;DiscReader allocationSpan(allocationSpanning,kIso2048);assert(allocationSpan.open(e)&&allocationSpan.readFile("TEST.BIN",out,e)&&out.size()==2049&&out.back()==42);assert(allocationSpan.readFileAllocation("TEST.BIN",out,e)&&out.size()==4096&&out.back()==0xb7);
 Memory raw;raw.b.resize(24*2352);for(size_t s=0;s<24;++s)std::memcpy(&raw.b[s*2352+16],&m.b[s*2048],2048);DiscReader r(raw,kMode1Raw2352);assert(r.open(e)&&r.readFile("TEST.BIN",out,e)&&out.size()==3);
 Memory spanning=m;dual32(&spanning.b[20*2048+68+10],2049);spanning.b[23*2048]=42;DiscReader ds(spanning,kIso2048);assert(ds.open(e)&&ds.readFile("TEST.BIN",out,e)&&out.size()==2049&&out.back()==42);
 Memory corrupt=m;corrupt.b[16*2048+84]^=1;DiscReader c(corrupt,kIso2048);assert(!c.open(e));corrupt=m;dual32(&corrupt.b[16*2048+80],25);DiscReader c2(corrupt,kIso2048);assert(!c2.open(e));corrupt=m;corrupt.b.resize(20*2048);DiscReader c3(corrupt,kIso2048);assert(!c3.open(e));
 struct Large:ByteSource {Memory source;uint64_t size()const override{return 256*1024*1024;}bool readAt(uint64_t p,void *v,size_t n)const override{return source.readAt(p,v,n);}} large;large.source=iso();dual32(&large.source.b[16*2048+80],131072);dual32(&large.source.b[20*2048+68+10],200*1024*1024);DiscReader limited(large,kIso2048);assert(limited.open(e));assert(!limited.readFile("TEST.BIN",out,e)&&out.empty());
 // Large reads must preserve payload/tail/allocation bytes while bounding
 // physical requests. A nonzero data-track origin catches raw-sector offsets.
 {Memory bulk=iso();const unsigned sectors=2200,blocks=22+sectors;
 bulk.b.resize(size_t(blocks)*2048);dual32(bulk.b.data()+16*2048+80,blocks);
 const size_t length=size_t(sectors-1)*2048+137;
 dual32(bulk.b.data()+20*2048+68+10,uint32_t(length));
 for(size_t i=22*2048;i<bulk.b.size();++i)bulk.b[i]=uint8_t(i*31+i/2048);
 DiscReader bulkIso(bulk,kIso2048);assert(bulkIso.open(e));bulk.calls=bulk.largest=0;
 assert(bulkIso.readFile("TEST.BIN",out,e)&&out.size()==length&&std::equal(out.begin(),out.end(),bulk.b.begin()+22*2048));
 assert(bulk.calls<20&&bulk.largest<=256*2048);
 Memory raw;raw.b.assign(size_t(blocks+2)*2352,0xa7);
 for(unsigned i=0;i<blocks;++i)std::memcpy(raw.b.data()+size_t(i+2)*2352+16,bulk.b.data()+size_t(i)*2048,2048);
 DiscReader bulkRaw(raw,kMode1Raw2352,2);assert(bulkRaw.open(e));raw.calls=raw.largest=0;
 assert(bulkRaw.readFile("TEST.BIN",out,e)&&out.size()==length&&std::equal(out.begin(),out.end(),bulk.b.begin()+22*2048));
 assert(raw.calls<20&&raw.largest<=256*2352);
 assert(bulkRaw.readFileAllocation("TEST.BIN",out,e)&&out.size()==sectors*2048&&std::equal(out.begin(),out.end(),bulk.b.begin()+22*2048));
 raw.calls=0;raw.failCall=2;assert(!bulkRaw.readFile("TEST.BIN",out,e)&&out.empty());}
 CueInfo cue;assert(parseCue("FILE \"own image.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n TRACK 02 AUDIO\n INDEX 00 00:02:00\n INDEX 01 00:04:00\n",cue,e));assert(cue.binaryFile=="own image.bin"&&cue.dataTrackStart==0);

 // Multi-BIN indexes are file-relative, including stored audio pregaps.
 CueInfo multi;assert(parseCue("FILE data.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nFILE two.bin BINARY\nTRACK 02 AUDIO\nINDEX 00 00:00:00\nINDEX 01 00:00:02\nFILE three.bin BINARY\nTRACK 03 AUDIO\nINDEX 01 00:00:00\n",multi,e));
 assert(multi.binaryFiles.size()==3&&!multi.filesResolved);
 assert(!validateCueTracks(multi,10*2352,e));
 CueInfo invalidMulti=multi;assert(!resolveCueFiles(invalidMulti,{3*2352,2*2352,3*2352},e));
 invalidMulti=multi;assert(!resolveCueFiles(invalidMulti,{3*2352,4*2352},e));
 invalidMulti=multi;assert(!resolveCueFiles(invalidMulti,{3*2352,4*2352+1,3*2352},e));
 assert(resolveCueFiles(multi,{3*2352,4*2352,3*2352},e));
 assert(multi.tracks[1].index0==3&&multi.tracks[1].index1==5&&multi.tracks[2].index1==7);
 uint32_t multiFrames=0;assert(cueAudioFrameCount(multi,10*2352,2,multiFrames,e)&&multiFrames==2);
 assert(cueAudioFrameCount(multi,10*2352,3,multiFrames,e)&&multiFrames==3);
 Memory multiPcm;multiPcm.b.resize(10*2352);for(size_t i=0;i<multiPcm.b.size();++i)multiPcm.b[i]=uint8_t(i*17+i/2352);
 assert(readCueAudioFrames(multiPcm,multi,2,0,2,out,e)&&out==std::vector<uint8_t>(multiPcm.b.begin()+5*2352,multiPcm.b.begin()+7*2352));
 assert(readCueAudioFrames(multiPcm,multi,3,2,1,out,e)&&out==std::vector<uint8_t>(multiPcm.b.begin()+9*2352,multiPcm.b.end()));
 assert(!resolveCueFiles(multi,{3*2352,4*2352,3*2352},e));
 assert(!parseCue("FILE a.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nFILE a.bin BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00",invalidMulti,e));
 assert(!parseCue("FILE a.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nFILE ../two.bin BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00",invalidMulti,e));
 // Stored pregaps belong to the following track, not playable audio.
 CueInfo audio;assert(parseCue("FILE test.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 00 00:00:02\nINDEX 01 00:00:04\nTRACK 03 AUDIO\nINDEX 01 00:00:07\n",audio,e));
 Memory pcm;pcm.b.resize(10*2352);for(size_t i=0;i<pcm.b.size();++i)pcm.b[i]=uint8_t(i*31+i/2352);uint32_t frames=99;
 assert(validateCueTracks(audio,pcm.size(),e));assert(cueAudioFrameCount(audio,pcm.size(),2,frames,e)&&frames==3);assert(cueAudioFrameCount(audio,pcm.size(),3,frames,e)&&frames==3);
 assert(readCueAudioFrames(pcm,audio,2,0,3,out,e)&&out==std::vector<uint8_t>(pcm.b.begin()+4*2352,pcm.b.begin()+7*2352));assert(readCueAudioFrames(pcm,audio,3,2,1,out,e)&&out==std::vector<uint8_t>(pcm.b.begin()+9*2352,pcm.b.end()));
 for(unsigned t: {0u,1u,4u}){assert(!cueAudioFrameCount(audio,pcm.size(),t,frames,e)&&frames==0);assert(!readCueAudioFrames(pcm,audio,t,0,1,out,e)&&out.empty());}
 for(unsigned count: {0u,4097u,4u})assert(!readCueAudioFrames(pcm,audio,2,0,count,out,e)&&out.empty());assert(!readCueAudioFrames(pcm,audio,2,3,1,out,e)&&out.empty());assert(!readCueAudioFrames(pcm,audio,2,UINT32_MAX,1,out,e)&&out.empty());
 assert(!validateCueTracks(audio,pcm.size()-1,e));assert(!validateCueTracks(audio,7*2352,e));CueInfo broken=audio;broken.tracks[2].index0=4;assert(!validateCueTracks(broken,pcm.size(),e));broken=audio;broken.tracks[1].index0=5;assert(!validateCueTracks(broken,pcm.size(),e));broken=audio;broken.tracks[1].number=3;assert(!validateCueTracks(broken,pcm.size(),e));
 struct FailedRead:ByteSource {uint64_t size()const override{return 10*2352;}bool readAt(uint64_t,void*,size_t)const override{return false;}} badRead;out.assign(5,42);assert(!readCueAudioFrames(badRead,audio,2,0,1,out,e)&&out.empty());
 for(const char* bad: {"FILE a.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nFILE b.bin BINARY", "FILE a.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nPREGAP 00:02:00\nINDEX 01 00:00:04", "FILE a.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nTRACK 02 AUDIO\nINDEX 00 00:00:02\nINDEX 00 00:00:03\nINDEX 01 00:00:04"})assert(!parseCue(bad,broken,e));
 assert(!parseCue("FILE \"../x.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00",cue,e));assert(!parseCue("FILE x.bin BINARY\nTRACK 01 AUDIO\nINDEX 01 00:00:00",cue,e));assert(!parseCue("FILE x.bin BINARY\nTRACK 01 MODE1/2352",cue,e));assert(!parseCue("FILE x.bin BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:60:00",cue,e));return 0;
}
#ifdef SNATCHER_DISC_TEST_MAIN
int main(int argc,char**argv){runDiscTests();if(argc==1){std::cout<<"disc tests PASS\n";return 0;}if(argc!=4||std::string(argv[1])!="--original"){std::cerr<<"Usage: disc_tests --original BIN OUTDIR\n";return 2;}
 FileSource source(argv[2]);Snatcher::DiscReader disc(source,Snatcher::kMode1Raw2352);std::string e;if(!disc.open(e)){std::cerr<<e<<'\n';return 1;}
#ifdef _WIN32
 _mkdir(argv[3]);
#else
 mkdir(argv[3],0700);
#endif
 std::string dir=argv[3];std::ofstream manifest((dir+"/files.tsv").c_str());manifest<<"name\textent\tbytes\n";std::vector<uint8_t> data;for(size_t i=0;i<disc.files().size();++i){const Snatcher::DiscFile&f=disc.files()[i];if(!disc.readFile(f.name,data,e)||!write(dir+"/"+f.name,data))return 1;manifest<<f.name<<'\t'<<f.extent<<'\t'<<f.size<<'\n';}if(!disc.readBoot(data,e)||!write(dir+"/boot.bin",data))return 1;std::cout<<"Exported "<<disc.files().size()<<" files and boot\n";return 0;
}
#endif
