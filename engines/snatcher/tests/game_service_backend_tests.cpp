/* SPDX-License-Identifier: GPL-3.0-or-later */
// Compile the exact production algorithm against an in-memory managed transport.
// This shim supplies no filesystem, user-profile or game assets.
#include "../game_service.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
using byte=uint8_t;using uint32=uint32_t;using int64=int64_t;using uint64=uint64_t;
namespace Common {
struct String:std::string {using std::string::string;String(){}String(const std::string& s):std::string(s){}static String format(const char* pattern,...){char b[512];va_list args;va_start(args,pattern);std::vsnprintf(b,sizeof(b),pattern,args);va_end(args);return String(b);}};
enum ErrorCode{kNoError,kFailure};struct Error{ErrorCode code=kNoError;ErrorCode getCode()const{return code;}};
struct InSaveFile{std::vector<uint8_t> bytes;size_t at=0;bool bad=false;explicit InSaveFile(const std::vector<uint8_t>& b):bytes(b){}int64 size(){return bytes.size();}uint32 read(void* out,uint32 n){size_t count=std::min(size_t(n),bytes.size()-at);std::memcpy(out,bytes.data()+at,count);at+=count;return uint32(count);}bool err(){return bad;}};
struct OutSaveFile{std::vector<uint8_t>& bytes;explicit OutSaveFile(std::vector<uint8_t>& b):bytes(b){bytes.clear();}uint32 write(const void* in,uint32 n){const uint8_t* p=static_cast<const uint8_t*>(in);bytes.insert(bytes.end(),p,p+n);return n;}void finalize(){}bool err(){return false;}};
struct SaveFileManager{std::map<std::string,std::vector<uint8_t>> files;bool failPublish=false,failRollback=false,failReadPrimary=false,failOpen=false,capacityKnown=false,listFailure=false;uint64 capacity=0;unsigned renames=0,opens=0;mutable unsigned capacityQueries=0;Error failure;
 OutSaveFile* openForSaving(const String& name,bool){++opens;return failOpen?nullptr:new OutSaveFile(files[name]);}
 InSaveFile* openForLoading(const String& name){auto it=files.find(name);if(it==files.end()||(failReadPrimary&&name=="game.001"))return nullptr;return new InSaveFile(it->second);}
 bool removeSavefile(const String& name){return files.erase(name)!=0;}
 std::vector<String> listSavefiles(const String& name){if(listFailure){failure.code=kFailure;return {};}return files.count(name)?std::vector<String>{name}:std::vector<String>{};}
 bool querySavefileFreeSpace(uint64& bytes)const{++capacityQueries;bytes=capacityKnown?capacity:0;return capacityKnown;}
 bool renameSavefile(const String& from,const String& to,bool){++renames;if((failPublish&&from=="game.001.new")||(failRollback&&from=="game.001.bak"))return false;auto it=files.find(from);if(it==files.end()||files.count(to))return false;files[to]=it->second;files.erase(it);return true;}
 void clearError(){failure.code=kNoError;}Error getError(){return failure;}
};
}
#define SNATCHER_GAME_SERVICE_BACKEND_H
#define COMMON_SAVEFILE_H
#include "../game_service_backend.cpp"
namespace {unsigned checks=0;void check(bool c,const char* message){++checks;if(!c){std::fprintf(stderr,"FAIL backend: %s\n",message);std::exit(1);}}
Snatcher::GameSaveRecord record(unsigned counter){Snatcher::GameSaveRecord r;r.contextKnown=true;r.disc.assign(64,'a');r.scenarioStamps.assign(39,123);r.packed.assign(352,0);r.resource=67;r.resumeEntry=66;r.counter=uint8_t(counter);r.bookmark.known=true;return r;}
}
int main(){using namespace Snatcher;std::string error;const std::string primary="game.001",backup=primary+".bak",pending=primary+".new";GameSaveRecord previous=record(9),next=record(10),loaded;std::vector<uint8_t> old;check(encodeGameSaveRecord(previous,old,error),"valid prior generation fixture");GameSaveOutcome outcome;
 // A valid backup recovered beside a corrupt/unreadable primary remains intact.
 for(bool unreadable:{false,true}){Common::SaveFileManager manager;manager.files[primary]={1,2,3};manager.files[backup]=old;manager.failReadPrimary=unreadable;manager.failPublish=true;auto before=manager.files;check(readGameSave(manager,"game",1,previous.disc,previous.scenarioStamps,loaded,error,nullptr)&&loaded.counter==9,"verified backup recovery");check(manager.files==before,"backup read leaves existing primary untouched");check(publishGameSave(manager,"game",1,next,outcome,error)&&outcome==kGameSaveIOFailure,"unverified primary replacement refuses success");check(manager.files==before&&manager.renames==0&&!manager.files.count(pending),"corrupt/read-failed primary cannot destroy valid backup");}
 // Failure after rotating a verified primary rolls it back, or retains backup.
 for(bool failedRollback:{false,true}){Common::SaveFileManager manager;manager.files[primary]=old;manager.failPublish=true;manager.failRollback=failedRollback;check(publishGameSave(manager,"game",1,next,outcome,error)&&outcome==kGameSaveIOFailure,"injected publication failure reports IO failure");check(!manager.files.count(pending),"failed staged publication cleaned");check(manager.files[failedRollback?backup:primary]==old,"prior verified bytes retained on publication/rollback failure");check(!manager.files.count(failedRollback?primary:backup),"only verified prior generation location remains");}
 Common::SaveFileManager manager;manager.files[primary]=old;check(publishGameSave(manager,"game",1,next,outcome,error)&&outcome==kGameSaveCommitted,"verified overwrite commits");check(manager.files[backup]==old&&!manager.files.count(pending),"successful overwrite retains verified backup");check(readGameSave(manager,"game",1,next.disc,next.scenarioStamps,loaded,error,nullptr)&&loaded.counter==10,"committed generation decodes");
 std::vector<uint8_t> encoded;check(encodeGameSaveRecord(next,encoded,error),"capacity boundary uses actual encoded record size");
 for(uint64 bytes:{uint64(0),uint64(encoded.size()-1)}){
  Common::SaveFileManager full;full.capacityKnown=true;full.capacity=bytes;full.files[primary]=old;auto before=full.files;
  GameServiceState state;state.availability=kGameStorageReady;state.counterKnown=state.resultKnown=true;state.saveCounter=9;for(auto& slot:state.slots)slot.known=true;state.slots[0].occupied=true;
  GameSaveTransaction transaction;check(beginGameSaveWrite(state,1,transaction,error),"capacity failure owns a real staged service transaction");
  check(publishGameSave(full,"game",1,next,outcome,error)&&outcome==kGameSaveNoCapacity,"known insufficient staging capacity produces source result1");
  check(full.files==before&&!full.opens&&!full.renames,"capacity rejection performs no write or rotation");
  check(finishGameSaveWrite(state,transaction,outcome,error)&&state.lastWriteResult==1&&state.saveCounter==9&&state.slots[0].occupied&&!state.pending,"no-capacity result rolls counter back and retains slot fact");
 }
 for(uint64 bytes:{uint64(encoded.size()),UINT64_C(1)<<40}){Common::SaveFileManager enough;enough.capacityKnown=true;enough.capacity=bytes;check(publishGameSave(enough,"game",1,next,outcome,error)&&outcome==kGameSaveCommitted,"exact-size and64-bit capacity facts permit real publication");}
 Common::SaveFileManager unknown;unknown.capacity=0;check(publishGameSave(unknown,"game",1,next,outcome,error)&&outcome==kGameSaveCommitted,"unknown capability is not zero-space evidence");
 Common::SaveFileManager staged;staged.capacityKnown=true;staged.capacity=0;staged.files[pending]=old;check(publishGameSave(staged,"game",1,next,outcome,error)&&outcome==kGameSaveCommitted&&!staged.capacityQueries,"existing staging can reclaim allocation; no false capacity denial");
 Common::SaveFileManager io;io.capacityKnown=true;io.capacity=UINT64_C(1)<<40;io.failOpen=true;check(publishGameSave(io,"game",1,next,outcome,error)&&outcome==kGameSaveIOFailure,"actual I/O failure is not reclassified by free-space guess");
 Common::SaveFileManager noListing;noListing.listFailure=true;noListing.capacityKnown=true;noListing.capacity=0;check(publishGameSave(noListing,"game",1,next,outcome,error)&&outcome==kGameSaveCommitted,"unknown staging existence does not prove capacity denial");
 Common::SaveFileManager alreadyFull;alreadyFull.capacityKnown=true;alreadyFull.capacity=0;alreadyFull.failOpen=true;GameServiceState discovered;discovered.counterKnown=true;
 check(scanGameStorage(alreadyFull,"game",discovered,error)&&discovered.availability==kGameStorageReady,"readable namespace with affirmative zero capacity remains a present source service");
 alreadyFull.listFailure=true;check(scanGameStorage(alreadyFull,"game",discovered,error)&&discovered.availability==kGameStorageUnavailable,"unreadable namespace cannot be declared ready from quota alone");
 for(unsigned slot=1;slot<=4;++slot){Common::SaveFileManager mapped;check(publishGameSave(mapped,"mapped",slot,next,outcome,error)&&outcome==kGameSaveCommitted,"all internal slots publish through CE");check(mapped.files.count(Common::String::format("mapped.%03u",slot))==1,"internal slot maps to identical CE slot number");check(readGameSave(mapped,"mapped",slot,next.disc,next.scenarioStamps,loaded,error,nullptr)&&loaded.counter==next.counter,"mapped slot restores exact native record");}
 GameSaveRecord metadata;check(decodeGameSaveMetadata(encoded,metadata,error)&&metadata.counter==next.counter,"CE metadata validates original checkpoint codec");auto damaged=encoded;damaged.back()^=1;check(!decodeGameSaveMetadata(damaged,metadata,error),"damaged internal slot is not listed as a valid CE save");
 Common::SaveFileManager legacy;legacy.files["game-snatcher-original-1.sgr"]=old;check(readGameSave(legacy,"game",1,previous.disc,previous.scenarioStamps,loaded,error,nullptr)&&loaded.counter==9,"legacy original-slot filename remains readable");check(publishGameSave(legacy,"game",1,next,outcome,error)&&outcome==kGameSaveCommitted,"new writes use CE namespace after legacy read");check(legacy.files["game-snatcher-original-1.sgr"]==old,"historical original-slot bytes preserved");check(readGameSave(legacy,"game",1,next.disc,next.scenarioStamps,loaded,error,nullptr)&&loaded.counter==10,"canonical CE slot takes precedence over historical original slot"); Common::String recovered;Common::SaveFileManager fallback;fallback.files[primary]={1,2,3};fallback.files[backup]=old;fallback.files[primary+".ce-achievements.json"]={'N'};check(readGameSave(fallback,"game",1,previous.disc,previous.scenarioStamps,loaded,error,&recovered)&&recovered==backup,"fallback reports actual generation instead of newer primary owner");
 Common::SaveFileManager promoted;promoted.files[backup]=old;promoted.files[primary+".ce-achievements.json"]={'N'};check(readGameSave(promoted,"game",1,previous.disc,previous.scenarioStamps,loaded,error,&recovered)&&recovered==primary,"missing primary restores verified backup");check(!promoted.files.count(primary+".ce-achievements.json"),"recovered backup cannot inherit stale primary ownership"); std::printf("game service backend: %u checks passed\n",checks);return 0;}
