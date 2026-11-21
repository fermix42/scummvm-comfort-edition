/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_service_backend.h"
#include "common/savefile.h"
namespace Snatcher {namespace {
Common::String slotName(const Common::String& target,unsigned slot){return Common::String::format("%s.%03u",target.c_str(),slot);}
bool readRecord(Common::SaveFileManager& manager,const Common::String& filename,const std::string& disc,const std::vector<uint64_t>& stamps,GameSaveRecord& record,std::string& error){
 Common::InSaveFile* file=manager.openForLoading(filename);
 if(!file){error="Original game slot cannot be opened";return false;}
 int64 length=file->size();std::vector<uint8_t> bytes;
 bool valid=length>0&&length<=4096;
 if(valid){bytes.resize(size_t(length));valid=file->read(bytes.data(),uint32(bytes.size()))==bytes.size()&&!file->err();}
 delete file;
 if(!valid){error="Original game slot has invalid extent or read failure";return false;}
 return decodeGameSaveRecord(bytes,disc,stamps,record,error);
}
}
bool scanGameStorage(Common::SaveFileManager& manager,const Common::String& target,GameServiceState& state,std::string& error){
 error.clear();if(state.pending){error="Storage refresh cannot replace an active original write";return false;}
 GameServiceState next=state;
 // Availability is established by an actual owned write/read/delete transaction,
 // not by the existence of a manager pointer or a guessed formatted-device bit.
 const Common::String probe=target+"-snatcher-original-storage-check.new";
 const byte expected[]={ 'S','G','B','K',1,0,0,0 };
 Common::OutSaveFile* out=manager.openForSaving(probe,false);bool writable=out!=nullptr;
 if(out){writable=out->write(expected,sizeof(expected))==sizeof(expected);out->finalize();writable=writable&&!out->err();delete out;}
 if(writable){Common::InSaveFile* in=manager.openForLoading(probe);byte actual[sizeof(expected)]={};writable=in&&in->size()==sizeof(expected)&&in->read(actual,sizeof(actual))==sizeof(actual)&&!in->err();delete in;for(unsigned i=0;i<sizeof(expected);++i)if(actual[i]!=expected[i])writable=false;}
 bool removed=manager.removeSavefile(probe);if(writable&&!removed)writable=false;
 next.availability=writable?kGameStorageReady:kGameStorageUnavailable;
 for(unsigned i=0;i<4;++i){manager.clearError();auto files=manager.listSavefiles(slotName(target,i+1));bool known=manager.getError().getCode()==Common::kNoError;auto backup=manager.listSavefiles(slotName(target,i+1)+".bak");known=known&&manager.getError().getCode()==Common::kNoError;auto legacy=manager.listSavefiles(Common::String::format("%s-snatcher-original-%u.sgr",target.c_str(),i+1));auto legacyBackup=manager.listSavefiles(Common::String::format("%s-snatcher-original-%u.sgr.bak",target.c_str(),i+1));next.slots[i].known=known;next.slots[i].occupied=known&&(!files.empty()||!backup.empty()||!legacy.empty()||!legacyBackup.empty());}
 // A full, accessible save namespace is present even when the eight-byte
 // write probe cannot allocate space. Keep that distinct from unknown or
 // unavailable storage so the source can receive its real capacity outcome.
 if(!writable){uint64 available=0;bool listed=true;for(const auto& slot:next.slots)listed=listed&&slot.known;if(listed&&manager.querySavefileFreeSpace(available)&&available==0)next.availability=kGameStorageReady;}
 if(!validateGameServiceState(next,error))return false;
 state=next;return true;
}
bool publishGameSave(Common::SaveFileManager& manager,const Common::String& target,unsigned slot,const GameSaveRecord& record,GameSaveOutcome& outcome,std::string& error){
 error.clear();outcome=kGameSaveIOFailure;
 if(slot<1||slot>4){error="Original game slot outside four-slot namespace";return false;}
 std::vector<uint8_t> bytes;if(!encodeGameSaveRecord(record,bytes,error))return false;
 const Common::String filename=slotName(target,slot),pending=filename+".new",backup=filename+".bak";
 manager.clearError();const bool hasPending=!manager.listSavefiles(pending).empty();const bool pendingKnown=manager.getError().getCode()==Common::kNoError;
 uint64 available=0;
 // This is a necessary-capacity check, not a reservation or an interpretation
 // of generic I/O errors. Existing staging may reclaim allocated clusters on
 // truncation, so it must not be denied using only free-byte information.
 if(pendingKnown&&!hasPending&&manager.querySavefileFreeSpace(available)&&available<bytes.size()){outcome=kGameSaveNoCapacity;error="Insufficient available capacity for original game slot";return true;}
 Common::OutSaveFile* file=manager.openForSaving(pending,false);
 if(!file){error="Cannot stage original game slot";return true;}
 bool written=file->write(bytes.data(),uint32(bytes.size()))==bytes.size();file->finalize();written=written&&!file->err();delete file;
 if(!written){manager.removeSavefile(pending);error="Cannot finalize original game slot";return true;}
 GameSaveRecord verified;
 if(!readRecord(manager,pending,record.disc,record.scenarioStamps,verified,error)){manager.removeSavefile(pending);return true;}
 bool previous=!manager.listSavefiles(filename).empty();
 // A verified backup may be the only recoverable generation when the primary
 // is corrupt or unreadable. Never discard it merely because a primary name
 // exists. Refuse replacement until the owner repairs that primary explicitly.
 if(previous){GameSaveRecord old;if(!readRecord(manager,filename,record.disc,record.scenarioStamps,old,error)){manager.removeSavefile(pending);error="Cannot replace an unverified original slot generation";return true;}}
 if(previous){manager.removeSavefile(backup);if(!manager.renameSavefile(filename,backup,false)){manager.removeSavefile(pending);error="Cannot retain previous original slot generation";return true;}}
 if(!manager.renameSavefile(pending,filename,false)){bool restored=!previous||manager.renameSavefile(backup,filename,false);manager.removeSavefile(pending);error=restored?"Cannot publish original game slot":"Cannot publish original game slot; previous generation retained in backup";return true;}
 outcome=kGameSaveCommitted;error.clear();return true;
}
bool readGameSave(Common::SaveFileManager& manager,const Common::String& target,unsigned slot,const std::string& disc,const std::vector<uint64_t>& stamps,GameSaveRecord& record,std::string& error,Common::String* restoredName){
 error.clear();if(slot<1||slot>4){error="Original game slot outside four-slot namespace";return false;}
 const Common::String filename=slotName(target,slot);Common::String restored=filename;GameSaveRecord staged;
 if(!readRecord(manager,filename,disc,stamps,staged,error)){
  if(!readRecord(manager,filename+".bak",disc,stamps,staged,error)){Common::String legacy=Common::String::format("%s-snatcher-original-%u.sgr",target.c_str(),slot);if(!readRecord(manager,legacy,disc,stamps,staged,error)){legacy+=".bak";if(!readRecord(manager,legacy,disc,stamps,staged,error))return false;}record=staged;if(restoredName)*restoredName=legacy;return true;}
  restored=filename+".bak";
  // A crash may occur between retaining the old generation and publishing the
  // new one. Only a verified old generation is recoverable; an uncommitted .new
  // file is never promoted. A read-only backend can still read the valid backup.
  if(manager.listSavefiles(filename).empty()&&manager.renameSavefile(filename+".bak",filename,false)){
   // A promoted backup cannot retain the newer primary generation's owner.
   manager.removeSavefile(filename+".ce-achievements.json");
   manager.renameSavefile(filename+".bak.ce-achievements.json",filename+".ce-achievements.json",false);restored=filename;
  }
 }
 record=staged;if(restoredName)*restoredName=restored;return true;
}
}
