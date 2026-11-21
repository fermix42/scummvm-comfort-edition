/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scene_assets.h"
#include <algorithm>
#include <sstream>
namespace Snatcher {
namespace {
uint16_t word(const uint8_t *p){return uint16_t(uint16_t(p[0])<<8|p[1]);}
uint32_t longWord(const uint8_t *p){return uint32_t(word(p))<<16|word(p+2);}
bool fail(std::string& error,const char *message,size_t offset){std::ostringstream s;s<<message<<" at resource offset "<<offset;error=s.str();return false;}
bool bound(const uint8_t *data,size_t size){return data&&size&&size<=16*1024*1024;}
}
bool readSceneGraphicsTask(const uint8_t *data,size_t size,size_t begin,uint32_t base,SceneGraphicsTask& task,std::string& error){
 task=SceneGraphicsTask();error.clear();if(!bound(data,size)||begin>size||size-begin<2)return fail(error,"truncated graphics task",begin);
 SceneGraphicsTask out;out.flags=data[begin];unsigned kind=out.flags&15;if(kind!=1&&kind!=2&&kind!=3&&kind!=4&&kind!=5)return fail(error,"dependent/unsupported graphics task family",begin);
 if(kind==5){GraphicsTransfer transfer;transfer.recordOffset=begin;transfer.sourceOffset=transfer.encodedEnd=0;transfer.destination=transfer.delay=0;out.transfers.push_back(transfer);out.stagingOffsets.push_back(0);out.copyLengths.push_back(32);out.stagingSources.push_back(std::vector<uint8_t>());task=out;return true;}
 if(kind==3){size_t at=begin+2;if(size-at<12)return fail(error,"truncated shared-source graphics task",at);uint32_t address=longWord(data+at);bool reuse=(out.flags&128)!=0;size_t source=address>=base?size_t(address-base):size;if(!reuse&&(address<base||source>=size))return fail(error,"shared graphics source outside resource",at);at+=4;size_t extent=0;
  for(unsigned budget=0;budget<4096;++budget){if(size-at<2)return fail(error,"unterminated shared-source graphics task",at);if(word(data+at)==0xffff){if(out.transfers.empty())return fail(error,"empty shared-source graphics task",at);break;}if(size-at<6)return fail(error,"truncated shared graphics copy",at);uint16_t destination=word(data+at),offset=word(data+at+2),count=word(data+at+4);if(!count||offset>8192||count>8192-offset||count>65536u-destination)return fail(error,"shared graphics copy exceeds owned allocation",at);GraphicsTransfer transfer;transfer.recordOffset=at;transfer.sourceOffset=source;transfer.destination=destination;transfer.delay=0;transfer.encodedEnd=source;out.transfers.push_back(transfer);out.stagingOffsets.push_back(offset);out.copyLengths.push_back(count);out.stagingSources.push_back(std::vector<uint8_t>());if(size_t(offset)+count>extent)extent=size_t(offset)+count;at+=6;if(budget==4095)return fail(error,"shared graphics copy record budget",at);}
  if(!reuse){if(size-source<2)return fail(error,"truncated shared graphics compressed header",source);size_t used=0;std::vector<uint8_t> decoded;if(!decodeGraphics(data+source+2,size-source-2,word(data+source)?word(data+source):extent,8192,decoded,used,error,word(data+source)==0))return false;out.stagingSources[0]=decoded;for(GraphicsTransfer& transfer:out.transfers)transfer.encodedEnd=source+2+used;}
  task=out;return true;
 }
 size_t at=begin+2;for(unsigned budget=0;budget<4096;++budget){if(at>size||size-at<2)return fail(error,"unterminated graphics task",at);if(word(data+at)==0xffff){if(out.transfers.empty())return fail(error,"empty graphics task",at);if(kind==4&&out.transfers.size()!=1)return fail(error,"pixel dissolve requires one source allocation",at);task=out;return true;}if(size-at<10)return fail(error,"truncated graphics task record",at);
  uint32_t address=longWord(data+at);bool reuse=(kind==1||kind==4)&&(out.flags&128);size_t source=address>=base?size_t(address-base):size;if(!reuse&&(address<base||source>=size))return fail(error,"graphics task source outside resource owner",at);uint16_t destination=word(data+at+4),offset=word(data+at+6),count=word(data+at+8);if(kind==4&&((offset&31)||(count&31)))return fail(error,"pixel dissolve requires tile-aligned copy extent",at);if(!count||offset>8192||count>8192-offset||count>65536u-destination)return fail(error,"graphics task allocation/copy extent",at);
  GraphicsTransfer transfer;transfer.recordOffset=at;transfer.sourceOffset=source;transfer.destination=destination;transfer.delay=0;transfer.encodedEnd=source;std::vector<uint8_t> staged;
  if(kind==2){if(count>size-source)return fail(error,"raw graphics task source truncated",source);transfer.decoded.assign(data+source,data+source+count);transfer.encodedEnd=source+count;}
  else if(!reuse){if(size-source<2)return fail(error,"compressed graphics task header truncated",source);size_t used=0;if(!decodeGraphics(data+source+2,size-source-2,word(data+source)?word(data+source):size_t(offset)+count,8192,staged,used,error,word(data+source)==0))return false;if(offset<=staged.size()&&count<=staged.size()-offset)transfer.decoded.assign(staged.begin()+offset,staged.begin()+offset+count);transfer.encodedEnd=source+2+used;}
  out.transfers.push_back(transfer);out.stagingOffsets.push_back(offset);out.copyLengths.push_back(count);out.stagingSources.push_back(staged);at+=10;if(kind==4){task=out;return true;}
 }return fail(error,"graphics task record budget",at);
}
void SceneGraphicsQueue::clear(){_slots.assign(7,SceneGraphicsTask());_buffers.assign(4,std::vector<uint8_t>(8192,0));_known.assign(4,std::vector<bool>(8192,false));_zeroMask.assign(4,std::vector<uint8_t>(8192,0));}
bool SceneGraphicsQueue::setDissolvePattern(const uint8_t* data,size_t size,std::string& error){
 error.clear();if(!data||size!=16){error="pixel dissolve requires original eight-step mask data";return false;}bool seen[8]={false};for(unsigned i=0;i<8;++i){unsigned offset=data[i*2],mask=data[i*2+1];if(offset>=4||(mask!=0xf0&&mask!=0x0f)){error="invalid pixel dissolve byte/nibble step";return false;}unsigned pixel=offset*2+(mask==0xf0?1:0);if(seen[pixel]){error="duplicate pixel dissolve nibble step";return false;}seen[pixel]=true;}_dissolve.assign(data,data+size);return true;
}
bool SceneGraphicsQueue::setStaging(unsigned channel,const std::vector<uint8_t>& bytes,std::string& error){return writeStaging(channel,0,bytes,error);}
bool SceneGraphicsQueue::writeStaging(unsigned channel,unsigned offset,const std::vector<uint8_t>& bytes,std::string& error){error.clear();if(channel>=4||bytes.empty()||offset>8192||bytes.size()>8192-offset){error="invalid semantic graphics staging owner/extent";return false;}std::copy(bytes.begin(),bytes.end(),_buffers[channel].begin()+offset);for(size_t i=offset;i<offset+bytes.size();++i){_known[channel][i]=true;_zeroMask[channel][i]=0;}return true;}
bool SceneGraphicsQueue::resolveCopy(const SceneGraphicsCopy& copy,GraphicsTransfer& transfer,std::string& error)const{
 error.clear();if(!copy.count||copy.count>65536u-copy.destination||copy.channel>4){error="invalid completed graphics copy";return false;}
 GraphicsTransfer out;out.recordOffset=out.sourceOffset=out.encodedEnd=0;out.destination=copy.destination;out.delay=0;
 if(copy.channel==4){if(copy.offset||copy.bytes.size()!=copy.count){error="invalid immutable graphics copy";return false;}out.decoded=copy.bytes;}
 else{if(!copy.bytes.empty()||copy.offset>8192||copy.count>8192-copy.offset){error="invalid staged graphics copy";return false;}out.decoded.assign(_buffers[copy.channel].begin()+copy.offset,_buffers[copy.channel].begin()+copy.offset+copy.count);out.knownBytes.assign(_known[copy.channel].begin()+copy.offset,_known[copy.channel].begin()+copy.offset+copy.count);}
 transfer=out;return true;
}
size_t SceneGraphicsQueue::pending()const{size_t n=0;for(const SceneGraphicsTask& t:_slots)if(t.flags)++n;return n;}
bool SceneGraphicsQueue::enqueue(const SceneGraphicsTask& task,std::string& error){error.clear();if(!task.flags||((task.flags&15)!=1&&(task.flags&15)!=2&&(task.flags&15)!=3&&(task.flags&15)!=4&&(task.flags&15)!=5)||task.transfers.empty()||task.transfers.size()!=task.stagingOffsets.size()||task.transfers.size()!=task.copyLengths.size()||task.transfers.size()!=task.stagingSources.size()){error="invalid scene graphics task ownership";return false;}for(SceneGraphicsTask& slot:_slots)if(!slot.flags){slot=task;return true;}error="scene graphics task queue full";return false;}
bool SceneGraphicsQueue::tick(bool busy,std::vector<GraphicsTransfer>& ready,std::string& error,std::vector<SceneGraphicsCopy>* completed){
 ready.clear();error.clear();if(completed)completed->clear();if(busy)return true;SceneGraphicsQueue next=*this;std::vector<GraphicsTransfer> out;std::vector<SceneGraphicsCopy> copies;
 for(SceneGraphicsTask& task:next._slots)if(task.flags){unsigned channel=(task.flags>>4)&3,kind=task.flags&15;if(task.record>=task.transfers.size()){error="invalid queued graphics record cursor";return false;}const size_t end=kind==1?task.record+1:task.transfers.size();for(size_t i=task.record;i<end;++i){unsigned offset=task.stagingOffsets[i],count=task.copyLengths[i];if(offset>8192||count>8192-offset){error="invalid queued graphics task window";return false;}if(kind==5&&!task.progress){if(offset||count!=32||!next.setStaging(channel,std::vector<uint8_t>(32,0),error))return false;}if((kind==1||kind==4||(kind==3&&i==0))&&!(task.flags&128)){if(!next.setStaging(channel,task.stagingSources[i],error))return false;}else if(kind==2){if(task.transfers[i].decoded.size()!=count){error="invalid queued raw graphics copy";return false;}if(!next.writeStaging(channel,offset,task.transfers[i].decoded,error))return false;}
   if(kind==4||kind==5){if(next._dissolve.size()!=16||task.progress>64||(offset&31)||(count&31)){error="pixel dissolve requires owned source mask and bounded tile progress";return false;}unsigned row=task.progress&7,index=(row+((unsigned(task.progress>>3)*5)&7))&7;unsigned at=offset+row*4+next._dissolve[index*2];uint8_t mask=next._dissolve[index*2+1];for(unsigned tile=0;tile<count/32;++tile){size_t byte=at+tile*32;if(kind==5)next._buffers[channel][byte]|=uint8_t((~mask)&0x77);else next._buffers[channel][byte]&=mask;if(!next._known[channel][byte]){next._zeroMask[channel][byte]|=uint8_t(~mask);if(next._zeroMask[channel][byte]==255){next._known[channel][byte]=true;next._buffers[channel][byte]=0;}}}}
   if(kind!=4)for(unsigned n=offset;n<offset+count;++n)if(!next._known[channel][n]){error="graphics task references unestablished inherited staging bytes";return false;}GraphicsTransfer transfer=task.transfers[i];transfer.decoded.assign(next._buffers[channel].begin()+offset,next._buffers[channel].begin()+offset+count);if(kind==4)transfer.knownBytes.assign(next._known[channel].begin()+offset,next._known[channel].begin()+offset+count);out.push_back(transfer);SceneGraphicsCopy copy;copy.channel=uint8_t(channel);copy.offset=uint16_t(offset);copy.count=uint16_t(count);copy.destination=transfer.destination;copies.push_back(copy);
  }bool parallel=(task.flags&64)!=0;if(kind==4||kind==5){if(kind==4)task.flags|=128;if(++task.progress==65)task=SceneGraphicsTask();}else if(kind!=1||++task.record==task.transfers.size())task=SceneGraphicsTask();if(!parallel)break;}
 *this=next;ready.swap(out);if(completed)completed->swap(copies);return true;
}
bool readSceneDirectory(const uint8_t *data,size_t size,SceneDirectory& out,std::string& error){
 out=SceneDirectory();error.clear();if(!bound(data,size)||size<8)return fail(error,"invalid scene container extent",0);
 uint32_t first=longWord(data);if(first!=0x28008&&first!=0x2800c)return fail(error,"unestablished scene pointer-header format",0);
 size_t header=first-0x28000;if(size<header)return fail(error,"truncated scene pointer header",0);
 for(size_t i=0;i<header;i+=4){uint32_t ptr=longWord(data+i);if(ptr<0x28000+header||uint64_t(ptr)-0x28000>=size)return fail(error,"scene section pointer outside resource",i);}
 SceneDirectory staged;staged.tableOffset=longWord(data+4)-0x28000;size_t position=0,minimum=size;
 while(position+2<=size-staged.tableOffset&&staged.relativeEntries.size()<256){
  uint16_t value=word(data+staged.tableOffset+position);
  if(value>=0x8000||value<position+2||size_t(value)>=size-staged.tableOffset||(!staged.relativeEntries.empty()&&minimum<position+2))break;
  staged.relativeEntries.push_back(value);minimum=std::min(minimum,size_t(value));position+=2;if(position==minimum)break;
 }
 if(staged.relativeEntries.empty())return fail(error,"no bounded scene directory prefix",staged.tableOffset);
 staged.tableBytes=position;staged.gapBytes=minimum-position;out=staged;return true;
}
bool readGraphicsTransfers(const uint8_t *data,size_t size,size_t begin,size_t end,size_t cap,std::vector<GraphicsTransfer>& out,std::string& error,uint32_t resourceBase){
 out.clear();error.clear();if(!bound(data,size)||begin>end||end>size||end-begin>255||cap>64*1024*1024)return fail(error,"invalid transfer payload/budget",begin);
 std::vector<GraphicsTransfer> parsed;size_t cursor=begin,total=0;
 while(cursor<end){
  if(end-cursor>=2&&word(data+cursor)==0xffff){if(cursor+2!=end)return fail(error,"unowned trailing transfer payload",cursor+2);out.swap(parsed);return true;}
  if(end-cursor<8)return fail(error,"truncated graphics-transfer record",cursor);
  uint32_t ptr=longWord(data+cursor)&0xffffff;if(ptr<resourceBase||uint64_t(ptr)-resourceBase>size||size-size_t(ptr-resourceBase)<2)return fail(error,"transfer source outside resource",cursor);
  GraphicsTransfer transfer;transfer.recordOffset=cursor;transfer.sourceOffset=ptr-resourceBase;transfer.delay=word(data+cursor+4);transfer.destination=word(data+cursor+6);
  size_t expected=word(data+transfer.sourceOffset),used=0;
  if(expected>cap-total)return fail(error,"graphics-transfer decoded budget exceeded",cursor);
  std::string reason;
  if(!decodeGraphics(data+transfer.sourceOffset+2,size-transfer.sourceOffset-2,expected,cap-total,transfer.decoded,used,reason)){std::ostringstream s;s<<"graphics transfer decode failed at resource offset "<<transfer.sourceOffset<<": "<<reason;error=s.str();return false;}
  transfer.encodedEnd=transfer.sourceOffset+2+used;total+=transfer.decoded.size();parsed.push_back(transfer);cursor+=8;
 }
 return fail(error,"missing exact graphics-transfer terminator",cursor);
}
bool applySceneActorsAndPalettes(const SceneAssets& assets,ActorController& actors,PaletteController& palettes,std::string& error){
 error.clear();ActorController stagedActors=actors;PaletteController stagedPalettes=palettes;size_t a=0,p=0,l=0;
 for(size_t i=0;i<assets.commands.size();++i){const SceneAssetCommand& c=assets.commands[i];
  if(c.family==0){if(p>=assets.palettes.size()||!stagedPalettes.enqueue(assets.palettes[p++],error)){if(error.empty())error="scene palette command ownership mismatch";return false;}}
  else if(c.family==2){size_t n=(c.payloadEnd-c.payloadBegin)/12;for(size_t j=0;j<n;++j){if(a>=assets.actors.size()){error="scene actor command ownership mismatch";return false;}const ActorInitializer& actor=assets.actors[a++];if(!stagedActors.initialize(actor.actorIndex,actor.flags,error))return false;}}
  else if(c.family==3||c.family==4){size_t n=(c.payloadEnd-c.payloadBegin)/2;for(size_t j=0;j<n;++j){if(l>=assets.links.size()){error="scene link command ownership mismatch";return false;}const ActorLink& link=assets.links[l++];if(!stagedActors.link(link.parent,link.child,error))return false;}}
 }
 if(a!=assets.actors.size()||p!=assets.palettes.size()||l!=assets.links.size()){error="unowned scene actor/palette records";return false;}
 actors=stagedActors;palettes=stagedPalettes;return true;
}
namespace {
SceneAssetResult parseAssetList(const uint8_t* data,size_t size,size_t begin,uint32_t resourceBase,size_t cap,SceneAssets& out,std::string& error){
 out=SceneAssets();error.clear();if(!bound(data,size)||!cap||cap>64*1024*1024||begin>=size){fail(error,"invalid scene asset list owner/extent",begin);return kSceneAssetsInvalid;}SceneAssets staged;size_t cursor=begin;staged.listBegin=cursor;
 for(unsigned count=0;count<4096;++count){
  if(cursor>=size){fail(error,"unterminated display list",cursor);return kSceneAssetsInvalid;}
  uint8_t family=data[cursor];if(family&0x80){staged.terminated=true;staged.terminator=family;staged.listEnd=cursor+1;break;}
  if(family>9||size-cursor<2){fail(error,"invalid/truncated display command",cursor);return kSceneAssetsInvalid;}
  size_t end=cursor+2+data[cursor+1];if(end>size){fail(error,"display payload outside resource",cursor);return kSceneAssetsInvalid;}
  SceneAssetCommand command={family,cursor,cursor+2,end};staged.commands.push_back(command);

  cursor=end;
 }
 if(!staged.terminated){fail(error,"display list command budget exceeded",cursor);return kSceneAssetsInvalid;}
 if(staged.unsupportedOpcode>=0){out=staged;std::ostringstream reason;reason<<"unsupported display family "<<staged.unsupportedOpcode<<" at resource offset "<<staged.unsupportedOffset;error=reason.str();return kSceneAssetsUnsupported;}
 size_t decoded=0;
 for(size_t i=0;i<staged.commands.size();++i){const SceneAssetCommand& command=staged.commands[i];
  if(command.family==1){std::vector<GraphicsTransfer> transfers;if(!readGraphicsTransfers(data,size,command.payloadBegin,command.payloadEnd,cap-decoded,transfers,error,resourceBase))return kSceneAssetsInvalid;
   for(size_t j=0;j<transfers.size();++j){decoded+=transfers[j].decoded.size();staged.transfers.push_back(transfers[j]);}
  }else if(command.family==2){std::vector<ActorInitializer> actors;if(!readActorInitializers(data,size,command.payloadBegin,command.payloadEnd,actors,error,resourceBase))return kSceneAssetsInvalid;staged.actors.insert(staged.actors.end(),actors.begin(),actors.end());}
  else if(command.family==0){PaletteEffect effect;if(!readPaletteEffect(data,size,command.payloadBegin,command.payloadEnd,effect,error,resourceBase))return kSceneAssetsInvalid;staged.palettes.push_back(effect);}
  else if(command.family==3||command.family==4){if(command.payloadEnd==command.payloadBegin||(command.payloadEnd-command.payloadBegin)%2)return fail(error,"partial actor-link record",command.payloadBegin),kSceneAssetsInvalid;
   for(size_t at=command.payloadBegin;at<command.payloadEnd;at+=2){ActorLink link={data[at],data[at+1]};if(link.parent>=64||link.child>=64||!link.child||link.parent==link.child)return fail(error,"invalid actor-link identity",at),kSceneAssetsInvalid;staged.links.push_back(link);}
  }else{if(command.payloadEnd-command.payloadBegin<2)return fail(error,"truncated scene controller value",command.payloadBegin),kSceneAssetsInvalid;SceneControl control={command.family,command.family==8?word(data+command.payloadBegin):uint16_t(data[command.payloadBegin+1]),command.offset};staged.controls.push_back(control);}
 }
 out=staged;return kSceneAssetsReady;
}
} // asset-list parser
SceneAssetResult parseSceneAssets(const uint8_t *data,size_t size,unsigned index,size_t cap,SceneAssets& out,std::string& error){
 out=SceneAssets();error.clear();SceneDirectory directory;if(!cap||cap>64*1024*1024||index>255){fail(error,"invalid scene asset request/budget",0);return kSceneAssetsInvalid;}if(!readSceneDirectory(data,size,directory,error))return kSceneAssetsInvalid;if(index>=directory.relativeEntries.size()){fail(error,"display index outside bounded directory prefix",directory.tableOffset+size_t(index)*2);return kSceneAssetsInvalid;}return parseAssetList(data,size,directory.tableOffset+directory.relativeEntries[index],0x28000,cap,out,error);
}
SceneAssetResult parseSystemPresentationAssets(const uint8_t* data,size_t size,unsigned index,size_t cap,SceneAssets& out,std::string& error){
 out=SceneAssets();error.clear();const size_t table=0x146ce-0xd400;const unsigned entries=18;if(!bound(data,size)||size<table+entries*2||index>=entries||word(data+table)!=entries*2){fail(error,"system presentation directory extent/identity",table);return kSceneAssetsInvalid;}const size_t offset=word(data+table+index*2);if(offset<entries*2||offset>=size-table){fail(error,"system presentation list outside source",table+index*2);return kSceneAssetsInvalid;}return parseAssetList(data,size,table+offset,0xd400,cap,out,error);
}

}
