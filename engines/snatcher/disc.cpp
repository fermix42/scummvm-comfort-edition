#include "disc.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
namespace Snatcher {
namespace {
bool fail(std::string &e,const char *s){e=s;return false;}
uint16_t le16(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
uint16_t be16(const uint8_t*p){return uint16_t(p[1])|uint16_t(p[0])<<8;}
uint32_t le32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t be32(const uint8_t*p){return uint32_t(p[3])|uint32_t(p[2])<<8|uint32_t(p[1])<<16|uint32_t(p[0])<<24;}
bool dual32(const uint8_t*p,uint32_t &v){v=le32(p);return v==be32(p+4);}
bool safeName(const std::string &s){return !s.empty()&&s!="."&&s!=".."&&s.find_first_of("/\\:")==std::string::npos&&s.find('\0')==std::string::npos;}
bool timeValue(const std::string&s,uint32_t &n){unsigned m,t,f; char a,b;std::istringstream i(s); if(!(i>>m>>a>>t>>b>>f)||a!=':'||b!=':'||t>=60||f>=75||m>999||i.peek()!=std::char_traits<char>::eof())return false;n=(m*60+t)*75+f;return true;}
}
DiscReader::DiscReader(const ByteSource&s,SectorLayout l,uint32_t st):_source(s),_layout(l),_start(st),_volume(0),_open(false){}
bool DiscReader::readLogical(uint64_t offset,void *data,size_t bytes) const {
 uint8_t *dst=static_cast<uint8_t*>(data);
 const uint64_t stride=_layout==kMode1Raw2352?2352:2048,sourceBytes=_source.size();
 std::vector<uint8_t> raw;
 while(bytes){
  uint64_t sector=offset/2048+_start,within=offset%2048;
  if(sector>(std::numeric_limits<uint64_t>::max()-16-within)/stride)return false;
  const uint64_t base=sector*stride;
  size_t count=0;
  if(_layout==kIso2048){
   count=std::min<size_t>(bytes,256*2048);
   const uint64_t physical=base+within;
   if(physical>sourceBytes||count>sourceBytes-physical||!_source.readAt(physical,dst,count))return false;
  }else if(!within&&bytes>=2*2048){
   // Batch full raw sectors, then strip their headers and ECC padding. The
   // same projected raw-sector contract is used by BIN/CUE and CHD sources.
   const size_t sectors=std::min<size_t>(bytes/2048,256),physicalBytes=sectors*2352;
   if(base>sourceBytes||physicalBytes>sourceBytes-base)return false;
   if(raw.size()<physicalBytes)raw.resize(physicalBytes);
   if(!_source.readAt(base,raw.data(),physicalBytes))return false;
   for(size_t n=0;n<sectors;++n)std::memcpy(dst+n*2048,raw.data()+n*2352+16,2048);
   count=sectors*2048;
  }else{
   const uint64_t physical=base+16+within;
   count=std::min<size_t>(bytes,2048-size_t(within));
   if(physical>sourceBytes||count>sourceBytes-physical||!_source.readAt(physical,dst,count))return false;
  }
  offset+=count;dst+=count;bytes-=count;
 }return true;
}
bool DiscReader::open(std::string &error){
 _open=false;_files.clear();_volume=0;error.clear();if(_layout!=kMode1Raw2352&&_layout!=kIso2048)return fail(error,"Unsupported sector layout");uint8_t pvd[2048];
 if(!readLogical(16*2048,pvd,sizeof(pvd)))return fail(error,"Cannot read ISO primary volume descriptor");
 if(pvd[0]!=1||std::memcmp(pvd+1,"CD001",5)||pvd[6]!=1)return fail(error,"Invalid ISO primary volume descriptor");
 uint32_t blocks; if(!dual32(pvd+80,blocks)||!blocks||le16(pvd+128)!=2048||be16(pvd+130)!=2048)return fail(error,"Invalid ISO volume bounds or block size");
 uint64_t stride=_layout==kMode1Raw2352?2352:2048;
 if(uint64_t(_start)+blocks>_source.size()/stride)return fail(error,"ISO volume exceeds source");_volume=blocks;
 const uint8_t*r=pvd+156;uint32_t root,rootSize;
 if(r[0]<34||r[1]||!(r[25]&2)||r[32]!=1||r[33]!=0||!dual32(r+2,root)||!dual32(r+10,rootSize)||le16(r+28)!=1||be16(r+30)!=1)return fail(error,"Invalid ISO root record");
 if(rootSize>16*1024*1024)return fail(error,"ISO root directory too large");
 std::vector<uint8_t> dir;if(!readExtent(root,rootSize,dir,error))return false;
 for(size_t pos=0;pos<dir.size();){uint8_t len=dir[pos];if(!len){pos=std::min(dir.size(),(pos/2048+1)*2048);continue;}
  if(len<34||len>dir.size()-pos||len>2048-pos%2048)return fail(error,"Truncated ISO directory record");
  const uint8_t*d=&dir[pos];uint32_t extent,size;
  if(d[1]||!dual32(d+2,extent)||!dual32(d+10,size)||le16(d+28)!=1||be16(d+30)!=1||d[26]||d[27]||d[32]>len-33)return fail(error,"Invalid ISO directory fields");
  if(uint64_t(extent)*2048+size>uint64_t(_volume)*2048)return fail(error,"ISO file extent exceeds volume");
  if(d[32]==1&&(d[33]==0||d[33]==1)){if(!(d[25]&2))return fail(error,"Invalid ISO special directory");}
  else {if(d[25]&0x82)return fail(error,"Subdirectories and multi-extent files unsupported");std::string name(reinterpret_cast<const char*>(d+33),d[32]);
   size_t semi=name.find(';');if(semi!=std::string::npos){if(name.substr(semi)!=";1")return fail(error,"Unsupported ISO file version");name.resize(semi);}
   if(!safeName(name))return fail(error,"Unsafe ISO file name");
   for(size_t j=0;j<_files.size();++j)if(_files[j].name==name)return fail(error,"Duplicate ISO file name");
   DiscFile f;f.name=name;f.extent=extent;f.size=size;_files.push_back(f);
  }pos+=len;
 }_open=true;return true;
}
bool DiscReader::readExtent(uint32_t extent,uint32_t size,std::vector<uint8_t>&out,std::string&e)const{
 out.clear();if(size>128*1024*1024)return fail(e,"Resource exceeds native read memory limit");if(uint64_t(extent)*2048+size>uint64_t(_volume)*2048)return fail(e,"Extent exceeds ISO volume");
 out.resize(size);if(size&&!readLogical(uint64_t(extent)*2048,&out[0],size)){out.clear();return fail(e,"Cannot read ISO extent");}return true;
}
bool DiscReader::readFileAllocation(const std::string& name,std::vector<uint8_t>& out,std::string& error)const{
 out.clear();error.clear();if(!_open)return fail(error,"Disc is not open");for(const DiscFile& file:_files)if(file.name==name){uint64_t rounded=(uint64_t(file.size)+2047)&~UINT64_C(2047);if(rounded>std::numeric_limits<uint32_t>::max())return fail(error,"Sector allocation extent overflows resource domain");return readExtent(file.extent,uint32_t(rounded),out,error);}return fail(error,"Resource not found");
}
bool DiscReader::readFile(const std::string&name,std::vector<uint8_t>&out,std::string&e)const{
 out.clear();if(!_open)return fail(e,"Disc is not open");for(size_t i=0;i<_files.size();++i)if(_files[i].name==name)return readExtent(_files[i].extent,_files[i].size,out,e);return fail(e,"Resource not found");
}
bool DiscReader::readBoot(std::vector<uint8_t>&out,std::string&e)const{
 out.clear();if(!_open||_volume<16)return fail(e,"Disc boot region unavailable");out.resize(32768);if(!readLogical(0,&out[0],out.size())){out.clear();return fail(e,"Cannot read disc boot region");}return true;
}
bool parseCue(const std::string&text,CueInfo&info,std::string&error){
 info=CueInfo();error.clear();std::istringstream lines(text);std::string line;bool file=false,dataIndex=false;unsigned track=0;bool trackIndex=false;uint32_t previous=0;
 while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();std::istringstream in(line);std::string command;if(!(in>>command))continue;
  if(command=="REM")continue;
  if(command=="FILE"){if(file&&(!trackIndex||info.tracks.back().fileIndex!=info.binaryFiles.size()-1))return fail(error,"Incomplete CUE FILE");in>>std::ws;std::string name,type;
   if(in.peek()=='\"'){in.get();if(!std::getline(in,name,'\"'))return fail(error,"Malformed quoted CUE path");}else in>>name;
   if(!(in>>type)||type!="BINARY"||!safeName(name))return fail(error,"Unsupported or unsafe CUE FILE");std::string extra;if(in>>extra)return fail(error,"Unexpected CUE FILE fields");for(const auto& existing:info.binaryFiles)if(existing==name)return fail(error,"Repeated CUE FILE");if(info.binaryFiles.size()>=99)return fail(error,"Too many CUE files");if(!file)info.binaryFile=name;info.binaryFiles.push_back(name);info.filesResolved=info.binaryFiles.size()==1;previous=0;file=true;
  }else if(command=="TRACK"){unsigned n;std::string mode,extra;if(!file||(track&&!trackIndex)||!(in>>n>>mode)||n!=track+1||n>99||(in>>extra))return fail(error,"Invalid CUE TRACK");if((n==1&&mode!="MODE1/2352")||(n>1&&mode!="AUDIO"))return fail(error,"Unsupported CUE track layout");track=n;trackIndex=false;CueTrack next;next.number=n;next.fileIndex=unsigned(info.binaryFiles.size()-1);next.audio=n>1;info.tracks.push_back(next);
  }else if(command=="INDEX"){unsigned n;std::string time,extra;uint32_t sector;if(!track||info.tracks.back().fileIndex!=info.binaryFiles.size()-1||!(in>>n>>time)||!timeValue(time,sector)||(in>>extra)||sector<previous)return fail(error,"Invalid CUE INDEX");if(n==0){if(track==1||trackIndex||info.tracks.back().hasIndex0)return fail(error,"Unsupported or duplicate CUE pregap index");info.tracks.back().hasIndex0=true;info.tracks.back().index0=sector;}else if(n==1){if(trackIndex)return fail(error,"Duplicate CUE INDEX 01");trackIndex=true;info.tracks.back().index1=sector;if(!info.tracks.back().hasIndex0)info.tracks.back().index0=sector;if(track==1){info.dataTrackStart=sector;dataIndex=true;}}else return fail(error,"Unsupported CUE index");previous=sector;info.lastIndex=sector;
  }else return fail(error,"Unsupported CUE directive");
 }if(!file||!dataIndex||!trackIndex||info.tracks.back().fileIndex!=info.binaryFiles.size()-1)return fail(error,"Incomplete CUE");return true;
}
bool resolveCueFiles(CueInfo& cue,const std::vector<uint64_t>& sizes,std::string& error){
 error.clear();if(sizes.empty()||sizes.size()!=cue.binaryFiles.size()||cue.tracks.empty())return fail(error,"CUE file sizes unavailable");
 if(cue.filesResolved&&sizes.size()>1)return fail(error,"CUE file indexes already resolved");
 std::vector<uint32_t> starts;uint64_t total=0;
 for(uint64_t bytes:sizes){if(!bytes||bytes%2352||bytes/2352>UINT32_MAX-total)return fail(error,"Invalid CUE file sector extent");starts.push_back(uint32_t(total));total+=bytes/2352;}
 CueInfo next=cue;
 for(size_t i=0;i<next.tracks.size();++i){CueTrack& track=next.tracks[i];unsigned file=track.fileIndex;
  if(file>=sizes.size()||track.index0>track.index1||track.index1>=sizes[file]/2352)return fail(error,"CUE track index exceeds its own file");
  if((i==0&&file!=0)||(i&&file!=next.tracks[i-1].fileIndex&&(file!=next.tracks[i-1].fileIndex+1||track.index0!=0)))return fail(error,"Unsupported CUE file ordering or leading gap");
  track.index0+=starts[file];track.index1+=starts[file];
 }
 if(next.tracks.back().fileIndex+1!=sizes.size())return fail(error,"CUE file has no tracks");
 next.dataTrackStart=next.tracks.front().index1;next.lastIndex=next.tracks.back().index1;next.filesResolved=true;
 if(!validateCueTracks(next,total*2352,error))return false;cue=next;return true;
}
bool validateCueTracks(const CueInfo& cue,uint64_t binaryBytes,std::string& error){
 error.clear();if(!cue.filesResolved)return fail(error,"CUE file indexes require resolution");if(!safeName(cue.binaryFile)||!binaryBytes||binaryBytes%2352||binaryBytes/2352>UINT32_MAX||cue.tracks.empty()||cue.tracks.size()>99)return fail(error,"Invalid CUE binary/track extent");
 uint32_t sectors=uint32_t(binaryBytes/2352);
 if(cue.dataTrackStart!=cue.tracks.front().index1||cue.lastIndex!=cue.tracks.back().index1)return fail(error,"CUE index summary disagrees with owned tracks");
 for(size_t i=0;i<cue.tracks.size();++i){const CueTrack& t=cue.tracks[i];
  if(t.number!=i+1||t.audio!=(i!=0)||t.index0>t.index1||t.index1>=sectors||(!t.hasIndex0&&t.index0!=t.index1)||(i==0&&t.hasIndex0))return fail(error,"Invalid CUE track identity or stored index");
  uint32_t end=i+1<cue.tracks.size()?cue.tracks[i+1].index0:sectors;
  if(end>sectors||end<=t.index1)return fail(error,"CUE track has empty, overlapping or out-of-file playable extent");
 }
 return true;
}
bool cueAudioFrameCount(const CueInfo& cue,uint64_t binaryBytes,unsigned track,uint32_t& frames,std::string& error){
 frames=0;if(!validateCueTracks(cue,binaryBytes,error))return false;
 if(!track||track>cue.tracks.size()||!cue.tracks[track-1].audio)return fail(error,"Invalid CUE audio track identity");
 uint32_t end=track<cue.tracks.size()?cue.tracks[track].index0:uint32_t(binaryBytes/2352);frames=end-cue.tracks[track-1].index1;return true;
}
bool readCueAudioFrames(const ByteSource& source,const CueInfo& cue,unsigned track,uint32_t first,uint32_t count,std::vector<uint8_t>& output,std::string& error){
 output.clear();uint32_t frames;if(!cueAudioFrameCount(cue,source.size(),track,frames,error))return false;
 if(!count||count>4096||first>frames||count>frames-first)return fail(error,"CUE CD audio request outside playable track");
 std::vector<uint8_t> next(size_t(count)*2352);uint64_t offset=(uint64_t(cue.tracks[track-1].index1)+first)*2352;
 if(!source.readAt(offset,next.data(),next.size()))return fail(error,"CUE CD audio source read failed");
 output.swap(next);return true;
}

}
