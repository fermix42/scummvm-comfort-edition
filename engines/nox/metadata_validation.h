/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_METADATA_VALIDATION_H
#define NOX_METADATA_VALIDATION_H
#include "nox/nox.h"
#include "nox/saves.h"
#include "engines/metaengine.h"

namespace Nox {
// Isolated diagnostic caller. All worlds originate from normal gameplay saves;
// no RAM/quest state is fabricated and no original resources are written.
static bool validateSaveManagement(NoxEngine &engine,const Common::String &target,Common::String &error) {
	Common::SaveFileManager &manager=*g_system->getSavefileManager();
	const MetaEngine *meta=engine.getMetaEngine();
	auto fail=[&](const char *message){error=message;return false;};
	auto digest=[&](const Common::String &name){
		Common::InSaveFile *in=manager.openForLoading(name);
		if(!in)return Common::String();
		const Common::String value=Common::computeStreamMD5AsString(*in);delete in;return value;
	};
	auto handleFor=[&](const Common::String &name){
		for(const auto &save:meta->listSaves(target.c_str(),false))
			if(meta->getSavegameFile(save.getSaveSlot(),target.c_str())==name)return save.getSaveSlot();
		return -1;
	};
	const Common::Array<Saves::Entry> originals=Saves::entries(manager,target);
	if(originals.size()!=2)return fail("Metadata matrix requires two original session saves");
	Common::StringArray protectedNames,protectedDigests;
	auto protect=[&](const Saves::Entry &entry){
		for(uint generation=0;generation<2;++generation){
			const Common::String file=SnapshotStore(manager,entry.prefix).filename(generation);
			protectedNames.push_back(file);protectedDigests.push_back(digest(file));
			protectedNames.push_back(file+".ce-achievements.json");protectedDigests.push_back(digest(file+".ce-achievements.json"));
		}
	};
	protect(originals[0]);protect(originals[1]);
	Saves::Entry fixtures[2];
	for(uint owner=0;owner<2;++owner){
		const int handle=handleFor(originals[owner].filename);
		if(handle<0||engine.loadGameState(handle).getCode()!=Common::kNoError)return fail("Cannot load metadata fixture owner");
		const int slot=engine.saveMode()==kClassicSaveMode?2:999;
		const Common::String label=Common::String::format("Metadata owner %u",owner);
		if(engine.saveGameState(slot,label).getCode()!=Common::kNoError||engine.saveGameState(slot,label).getCode()!=Common::kNoError)
			return fail("Cannot create paired metadata fixture generations");
		Common::Array<byte> payload;
		if(!Saves::best(manager,Saves::slotPrefix(target,originals[owner].metadata.session,slot),target,fixtures[owner],payload))return fail("Metadata fixture missing");
		const SaveStateDescriptor info=meta->querySaveMetaInfos(target.c_str(),handleFor(fixtures[owner].filename));
		const Saves::Metadata &saved=fixtures[owner].metadata;
		SaveStateDescriptor expected(meta,0,label);expected.setSaveDate(saved.year,saved.month,saved.day);
		expected.setSaveTime(saved.hour,saved.minute);expected.setPlayTime(saved.playtime);
		const Graphics::Surface *preview=info.getThumbnail();
		if(!Saves::validDate(saved)||info.getDescription()!=label||info.getSaveDate()!=expected.getSaveDate()||
		   info.getSaveTime()!=expected.getSaveTime()||info.getPlayTime()!=expected.getPlayTime()||
		   !preview||preview->w!=Saves::kPreviewWidth||preview->h!=Saves::kPreviewHeight||preview->format.bytesPerPixel!=2)
			return fail("CE save metadata differs from actual serialized date/time/preview");
		for(uint y=0;y<Saves::kPreviewHeight;++y)for(uint x=0;x<Saves::kPreviewWidth;++x)
			if(*(const uint16 *)preview->getBasePtr(x,y)!=READ_LE_UINT16(saved.preview.data()+(y*Saves::kPreviewWidth+x)*2))
				return fail("CE thumbnail pixels differ from serialized actual-world preview");
	}
	protect(fixtures[1]);
	const SaveStateList listed=meta->listSaves(target.c_str(),false);
	if(listed.size()!=4||meta->removeSaveState(target.c_str(),-1)||meta->removeSaveState(target.c_str(),listed.size())||
	   meta->removeSaveState("nox-invalid-target",0))return fail("Invalid deletion handle or target was accepted");
	int removeHandle=-1,otherHandle=-1;
	for(const auto &save:meta->listSaves(target.c_str(),false)){
		const Common::String file=meta->getSavegameFile(save.getSaveSlot(),target.c_str());
		if(file==fixtures[0].filename)removeHandle=save.getSaveSlot();
		if(file==fixtures[1].filename)otherHandle=save.getSaveSlot();
	}
	if(removeHandle<0||otherHandle<0||!meta->removeSaveState(target.c_str(),removeHandle)||
	   meta->removeSaveState(target.c_str(),removeHandle)||
	   meta->getSavegameFile(otherHandle,target.c_str())!=fixtures[1].filename)
		return fail("Logical deletion or stable cross-session handle failed");
	for(uint generation=0;generation<2;++generation){
		const Common::String file=SnapshotStore(manager,fixtures[0].prefix).filename(generation);
		if(manager.exists(file)||manager.exists(file+".ce-achievements.json"))return fail("Logical deletion left a generation or sidecar");
	}
	for(uint i=0;i<protectedNames.size();++i)
		if(protectedDigests[i].empty()||digest(protectedNames[i])!=protectedDigests[i])return fail("Deletion altered a different session or slot");
	if(Common::CEAchievements().getCurrentPlaythroughForTarget(target)!=fixtures[1].metadata.session||
	   meta->listSaves(target.c_str(),false).size()!=3)return fail("Deletion changed active session or enumeration");
	const int finalHandle=handleFor(fixtures[1].filename);
	if(finalHandle<0||!meta->removeSaveState(target.c_str(),finalHandle)||meta->listSaves(target.c_str(),false).size()!=2)
		return fail("Second session fixture cleanup failed");
	for(uint i=0;i<8;++i)if(digest(protectedNames[i])!=protectedDigests[i])return fail("Fixture cleanup changed original saves");
	Common::OutSaveFile *out=manager.openForSaving("nox-metadata.tsv",false);
	if(!out)return fail("Cannot create metadata matrix report");
	const Common::String report="PASS\tdate=1\tplaytime=1\tactual_preview=1\tpaired_generation_deletion=2\tcross_session_isolation=1\tinvalid_handles=1\tstable_handles=1\tinitial_saves=2\tfinal_saves=2\n";
	out->write(report.c_str(),report.size());out->finalize();const bool ok=!out->err();delete out;
	return ok||fail("Cannot persist metadata matrix report");
}
} // namespace Nox
#endif
