/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "state_codec.h"
#include <algorithm>
#include <array>
namespace Snatcher {namespace {
uint32_t crc(const uint8_t*p,size_t n){static const std::array<uint32_t,256> table=[](){std::array<uint32_t,256> t{};for(unsigned i=0;i<256;++i){uint32_t v=i;for(unsigned b=0;b<8;++b)v=(v&1)?(v>>1)^0xedb88320:v>>1;t[i]=v;}return t;}();uint32_t c=0xffffffff;while(n--)c=table[(c^*p++)&255]^(c>>8);return ~c;}

const size_t kSaveMaxBytes=128*1024*1024,kSaveMaxSectionBytes=64*1024*1024,kSaveMaxSections=32,kSaveHeaderBytes=80;
size_t sectionLimit(unsigned id,bool active){if(id==kSaveMetadata)return 255;if(id==kSaveThumbnail)return 48*1024;return active?(id==kSaveInput||id==kSaveAudio?64*1024*1024:id==kSaveScene?16*1024*1024:1024*1024):1024*1024;}
bool knownSection(unsigned id){return (id>=kSaveStory&&id<=kSaveThumbnail)||id==kSaveMetadata;}
void put16(std::vector<uint8_t>& out,unsigned value){out.push_back(uint8_t(value>>8));out.push_back(uint8_t(value));}
void put32(std::vector<uint8_t>& out,uint32_t value){put16(out,value>>16);put16(out,value);}
unsigned get16(const uint8_t *p){return unsigned(p[0])<<8|p[1];}
uint32_t get32(const uint8_t *p){return uint32_t(get16(p))<<16|get16(p+2);}
bool validateRequired(const std::vector<NativeSaveSectionId>& required,std::string& error){unsigned seen=0;for(size_t i=0;i<required.size();++i){unsigned id=required[i];if(!knownSection(id)||(seen&(1u<<id))){error="invalid required native section schema";return false;}seen|=1u<<id;}return true;}
bool validDisc(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
}

bool encodeStateFragment(const std::vector<uint8_t>& state,const std::string& disc,std::vector<uint8_t>& out,std::string& e){out.clear();e.clear();if(state.size()!=352||!validDisc(disc)){e="invalid state extent or disc identity";return false;}const uint8_t magic[]={'S','N','S','F',1,0,0,0};out.assign(magic,magic+8);out.insert(out.end(),disc.begin(),disc.end());out.insert(out.end(),state.begin(),state.end());uint32_t c=crc(out.data(),out.size());for(unsigned i=0;i<4;++i)out.push_back(uint8_t(c>>(24-i*8)));return true;}
bool decodeStateFragment(const uint8_t *p,size_t n,const std::string& disc,std::vector<uint8_t>& out,std::string& e){out.clear();e.clear();const uint8_t magic[]={'S','N','S','F',1,0,0,0};if(!p||n!=428||!validDisc(disc)||!std::equal(magic,magic+8,p)){e="unsupported/truncated state fragment";return false;}if(!std::equal(disc.begin(),disc.end(),p+8)){e="state fragment belongs to another disc";return false;}uint32_t stored=0;for(unsigned i=0;i<4;++i)stored=stored<<8|p[n-4+i];if(crc(p,n-4)!=stored){e="state fragment checksum mismatch";return false;}out.assign(p+72,p+424);return true;}
namespace {
struct SaveEntry {unsigned id,version,flags;uint32_t offset,length,checksum;};
bool supportedSection(unsigned id,unsigned version){return knownSection(id)&&(version==1||(id==kSaveInput&&version>=2&&version<=13));}
bool directory(const uint8_t* p,size_t prefix,size_t total,const std::string& disc,std::vector<SaveEntry>& out,std::string& error){
 error.clear();out.clear();if(!p||prefix<80||total>kSaveMaxBytes||!validDisc(disc)||!std::equal(p,p+4,"SNSV")||get16(p+4)!=2||get16(p+6)||get16(p+14)||get32(p+8)!=total||!std::equal(disc.begin(),disc.end(),p+16)){error="Invalid indexed save identity/header";return false;}
 unsigned count=get16(p+12);size_t end=80+size_t(count)*20;if(count>32||prefix<end+4||get32(p+end)!=crc(p,end)){error="Indexed save directory checksum/extent";return false;}
 uint32_t next=uint32_t(end+4);unsigned previous=0;
 for(unsigned i=0;i<count;++i){const uint8_t* e=p+80+i*20;SaveEntry v={get16(e),get16(e+2),get16(e+4),get32(e+8),get32(e+12),get32(e+16)};
  if(v.id<=previous||get16(e+6)||(v.flags&~1u)||v.offset!=next||v.offset>total||v.length>total-v.offset||v.length>sectionLimit(v.id,true)||((v.flags&1)&&!supportedSection(v.id,v.version))){error="Invalid indexed save section";return false;}previous=v.id;next+=v.length;out.push_back(v);
 }
 if(next!=total){error="Unowned indexed save payload";return false;}return true;
}
bool encodeIndexed(const NativeSaveEnvelope& save,const std::string& disc,const std::vector<NativeSaveSectionId>& required,std::vector<uint8_t>& out,std::string& error){
 if(!validDisc(disc)||!validateRequired(required,error)||save.sections.size()>32){error="Invalid indexed save schema";return false;}
 std::vector<const NativeSaveSection*> sections;for(const auto& s:save.sections)sections.push_back(&s);std::sort(sections.begin(),sections.end(),[](const NativeSaveSection* a,const NativeSaveSection* b){return a->id<b->id;});
 size_t total=80+20*sections.size()+4;unsigned previous=0,mandatory=0;
 for(auto s:sections){if(s->id<=previous||!supportedSection(s->id,s->version)||s->bytes.size()>sectionLimit(s->id,true)||s->bytes.size()>kSaveMaxBytes-total){error="Invalid indexed save section/schema";return false;}previous=s->id;if(s->required)mandatory|=1u<<s->id;total+=s->bytes.size();}
 for(auto id:required)if(!(mandatory&(1u<<id))){error="Missing indexed save owner";return false;}
 std::vector<uint8_t> bytes;bytes.reserve(total);bytes.insert(bytes.end(),{'S','N','S','V'});put16(bytes,2);put16(bytes,0);put32(bytes,uint32_t(total));put16(bytes,unsigned(sections.size()));put16(bytes,0);bytes.insert(bytes.end(),disc.begin(),disc.end());uint32_t offset=uint32_t(80+20*sections.size()+4);
 for(auto s:sections){put16(bytes,s->id);put16(bytes,s->version);put16(bytes,s->required?1:0);put16(bytes,0);put32(bytes,offset);put32(bytes,uint32_t(s->bytes.size()));put32(bytes,crc(s->bytes.data(),s->bytes.size()));offset+=uint32_t(s->bytes.size());}
 put32(bytes,crc(bytes.data(),bytes.size()));for(auto s:sections)bytes.insert(bytes.end(),s->bytes.begin(),s->bytes.end());out.swap(bytes);return true;
}
bool decodeIndexed(const uint8_t* p,size_t size,const std::string& disc,const std::vector<NativeSaveSectionId>& required,NativeSaveEnvelope& save,std::string& error){
 std::vector<SaveEntry> entries;if(!validateRequired(required,error)||!directory(p,size,size,disc,entries,error))return false;NativeSaveEnvelope next;next.format=2;unsigned mandatory=0;
 for(const auto& e:entries){if(crc(p+e.offset,e.length)!=e.checksum){error="Indexed save payload checksum mismatch";return false;}if(supportedSection(e.id,e.version)){NativeSaveSection s(static_cast<NativeSaveSectionId>(e.id));s.version=uint16_t(e.version);s.required=e.flags&1;s.bytes.assign(p+e.offset,p+e.offset+e.length);if(s.required)mandatory|=1u<<e.id;next.sections.push_back(std::move(s));}}
 for(auto id:required)if(!(mandatory&(1u<<id))){error="Missing indexed save owner";return false;}save=std::move(next);return true;
}
}

bool encodeNativeSave(const NativeSaveEnvelope& save,const std::string& disc,const std::vector<NativeSaveSectionId>& required,std::vector<uint8_t>& out,std::string& error){
 out.clear();error.clear();if(save.format==2)return encodeIndexed(save,disc,required,out,error);if(save.format!=1){error="Unsupported save writer version";return false;}if(!validDisc(disc)||!validateRequired(required,error)){if(error.empty())error="invalid native save disc identity";return false;}
 if(save.sections.size()>kSaveMaxSections){error="native save section count exceeds limit";return false;}
 std::vector<const NativeSaveSection*> sections;for(size_t i=0;i<save.sections.size();++i)sections.push_back(&save.sections[i]);std::sort(sections.begin(),sections.end(),[](const NativeSaveSection *a,const NativeSaveSection *b){return a->id<b->id;});
 bool activeSchema=false;for(const auto* s:sections)if(s->id==kSaveInput&&s->version>=12&&s->required)activeSchema=true;const size_t envelopeLimit=activeSchema?kSaveMaxBytes:4*1024*1024;
 size_t total=kSaveHeaderBytes+4;unsigned seen=0,mandatory=0;
 for(size_t i=0;i<sections.size();++i){const NativeSaveSection& section=*sections[i];unsigned id=section.id;if(!knownSection(id)||(section.version!=1&&!(id==kSaveInput&&(section.version==2||section.version==3||section.version==4||section.version==5||section.version==6||section.version==7||section.version==8||section.version==9||section.version==10||section.version==11||section.version==12||section.version==13)))||(seen&(1u<<id))){error="unsupported or duplicate native save section";return false;}seen|=1u<<id;if(section.required)mandatory|=1u<<id;
  if(section.bytes.size()>sectionLimit(id,activeSchema)||total>envelopeLimit-12-section.bytes.size()){error="native save section/total extent exceeds limit";return false;}total+=12+section.bytes.size();}
 for(size_t i=0;i<required.size();++i)if(!(mandatory&(1u<<required[i]))){error="missing required native save section";return false;}
 std::vector<uint8_t> bytes;bytes.reserve(total);bytes.push_back('S');bytes.push_back('N');bytes.push_back('S');bytes.push_back('V');put16(bytes,1);put16(bytes,0);put32(bytes,uint32_t(total));put16(bytes,unsigned(sections.size()));put16(bytes,0);bytes.insert(bytes.end(),disc.begin(),disc.end());
 for(size_t i=0;i<sections.size();++i){const NativeSaveSection& section=*sections[i];put16(bytes,section.id);put16(bytes,section.version);put16(bytes,section.required?1:0);put16(bytes,0);put32(bytes,uint32_t(section.bytes.size()));bytes.insert(bytes.end(),section.bytes.begin(),section.bytes.end());}
 put32(bytes,crc(bytes.data(),bytes.size()));out.swap(bytes);return true;
}
bool decodeNativeSave(const uint8_t *data,size_t size,const std::string& disc,const std::vector<NativeSaveSectionId>& required,NativeSaveEnvelope& save,std::string& error){
 save.sections.clear();error.clear();if(data&&size>=80&&get16(data+4)==2)return decodeIndexed(data,size,disc,required,save,error);if(!data||size<kSaveHeaderBytes+4||size>kSaveMaxBytes||!validDisc(disc)||!validateRequired(required,error)){if(error.empty())error="invalid native save identity/extent";return false;}
 if(data[0]!='S'||data[1]!='N'||data[2]!='S'||data[3]!='V'||get16(data+4)!=1||get16(data+6)||get32(data+8)!=size||get16(data+14)){error="unsupported native save header";return false;}
 unsigned count=get16(data+12);if(count>kSaveMaxSections){error="native save section count exceeds limit";return false;}
 if(!std::equal(disc.begin(),disc.end(),data+16)){error="native save belongs to another disc";return false;}
 if(get32(data+size-4)!=crc(data,size-4)){error="native save checksum mismatch";return false;}
 // Determine the explicitly versioned active-owner budget before allocating any
 // payload sections. Historical envelopes retain their original 4 MiB/1 MiB
 // limits, including unknown optional sections.
 bool activeSchema=false;size_t preflight=kSaveHeaderBytes;for(unsigned i=0;i<count;++i){if(preflight>size-4||size-4-preflight<12){error="Truncated native save section preflight";return false;}unsigned id=get16(data+preflight),version=get16(data+preflight+2),flags=get16(data+preflight+4);size_t length=get32(data+preflight+8);preflight+=12;if(length>size-4-preflight){error="Native save section preflight extent";return false;}if(id==kSaveInput&&version>=12&&(flags&1))activeSchema=true;preflight+=length;}
 if(size>(activeSchema?kSaveMaxBytes:4*1024*1024)){error="Native save exceeds its versioned envelope budget";return false;}
 NativeSaveEnvelope result;std::vector<unsigned> seen;unsigned mandatory=0;size_t cursor=kSaveHeaderBytes;
 for(unsigned i=0;i<count;++i){if(cursor>size-4||size-4-cursor<12){error="truncated native save section header";return false;}unsigned id=get16(data+cursor),version=get16(data+cursor+2),flags=get16(data+cursor+4);uint32_t length=get32(data+cursor+8);
  if(!id||get16(data+cursor+6)||(flags&~1u)||std::find(seen.begin(),seen.end(),id)!=seen.end()){error="invalid or duplicate native save section header";return false;}seen.push_back(id);cursor+=12;
  if(length>sectionLimit(id,activeSchema)||length>size-4-cursor){error="native save section payload exceeds bounds";return false;}
  bool supported=knownSection(id)&&(version==1||(id==kSaveInput&&(version==2||version==3||version==4||version==5||version==6||version==7||version==8||version==9||version==10||version==11||version==12||version==13)));if(!supported&&(flags&1)){error="unknown required native save section/version";return false;}
  if(supported){NativeSaveSection section(static_cast<NativeSaveSectionId>(id));section.version=uint16_t(version);section.required=(flags&1)!=0;section.bytes.assign(data+cursor,data+cursor+length);result.sections.push_back(section);if(section.required)mandatory|=1u<<id;}cursor+=length;
 }
 if(cursor!=size-4){error="trailing native save section bytes";return false;}
 for(size_t i=0;i<required.size();++i)if(!(mandatory&(1u<<required[i]))){error="missing required native save section";return false;}
 save.format=1;save.sections.swap(result.sections);return true;
}

}
#ifndef SNATCHER_STATE_CORE_ONLY
#include "common/stream.h"
namespace Snatcher {
bool writeNativeSave(Common::WriteStream& stream,const NativeSaveEnvelope& save,const std::string& disc,const std::vector<NativeSaveSectionId>& required,std::string& error){std::vector<uint8_t> bytes;if(!encodeNativeSave(save,disc,required,bytes,error))return false;if(stream.write(bytes.data(),uint32(bytes.size()))!=bytes.size()||stream.err()){error="native save write failed";return false;}return true;}
bool readNativeSave(Common::SeekableReadStream& stream,const std::string& disc,const std::vector<NativeSaveSectionId>& required,NativeSaveEnvelope& save,std::string& error){save.sections.clear();error.clear();if(stream.pos()!=0||stream.size()<int64(kSaveHeaderBytes+4)||stream.size()>int64(kSaveMaxBytes)){error="native save stream extent invalid";return false;}std::vector<uint8_t> bytes(size_t(stream.size()));if(stream.read(bytes.data(),uint32(bytes.size()))!=bytes.size()||stream.err()){error="native save read failed";return false;}return decodeNativeSave(bytes.data(),bytes.size(),disc,required,save,error);}

bool readNativeSaveMetadata(Common::SeekableReadStream& stream,const std::string& disc,std::string& description,std::vector<uint8_t>* thumbnail,std::string& error){
 description.clear();if(thumbnail)thumbnail->clear();if(stream.size()<84||stream.size()>int64(kSaveMaxBytes)||!stream.seek(0)){error="Invalid indexed metadata stream";return false;}
 std::vector<uint8_t> prefix(80);if(stream.read(prefix.data(),80)!=80){error="Truncated indexed metadata header";return false;}unsigned count=get16(prefix.data()+12);if(count>32){error="Indexed metadata directory exceeds limit";return false;}prefix.resize(80+count*20+4);if(stream.read(prefix.data()+80,count*20+4)!=count*20+4){error="Truncated indexed metadata directory";return false;}
 std::vector<SaveEntry> entries;if(!directory(prefix.data(),prefix.size(),size_t(stream.size()),disc,entries,error))return false;bool found=false;
 for(const auto& e:entries)if(e.id==kSaveMetadata||(thumbnail&&e.id==kSaveThumbnail)){
  if(e.version!=1){error="Unsupported indexed metadata version";return false;}std::vector<uint8_t> data(e.length);if(!stream.seek(e.offset)||stream.read(data.data(),e.length)!=e.length||crc(data.data(),data.size())!=e.checksum){error="Indexed metadata checksum/read failure";return false;}
  if(e.id==kSaveMetadata){description.assign(data.begin(),data.end());found=true;}else thumbnail->swap(data);
 }
 if(!found)error="Indexed save has no description";return found;
}
bool writeStateFragment(Common::WriteStream& stream,const std::vector<uint8_t>& state,const std::string& disc,std::string& e){std::vector<uint8_t> encoded;if(!encodeStateFragment(state,disc,encoded,e))return false;if(stream.write(encoded.data(),uint32(encoded.size()))!=encoded.size()||stream.err()){e="state fragment write failed";return false;}return true;}
bool readStateFragment(Common::SeekableReadStream& stream,const std::string& disc,std::vector<uint8_t>& state,std::string& e){state.clear();if(stream.pos()!=0||stream.size()!=428){e="state fragment stream extent invalid";return false;}std::vector<uint8_t> bytes(428);if(stream.read(bytes.data(),428)!=428||stream.err()){e="state fragment read failed";return false;}return decodeStateFragment(bytes.data(),bytes.size(),disc,state,e);}
}
#endif
