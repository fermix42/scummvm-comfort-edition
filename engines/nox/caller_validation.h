/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_CALLER_VALIDATION_H
#define NOX_CALLER_VALIDATION_H
#include "nox/nox.h"
#include "nox/saves.h"
#include "engines/metaengine.h"

namespace Nox {
bool NoxEngine::validateSaveCallers(Common::String &error,uint32 runStarted) {
	auto fail=[&](const char *message){error=message;return false;};
	Common::SaveFileManager &manager=*getSaveFileManager();
	auto digest=[&](const Common::String &name){
		Common::InSaveFile *in=manager.openForLoading(name);if(!in)return Common::String();
		const Common::String value=Common::computeStreamMD5AsString(*in);delete in;return value;
	};
	Common::StringArray names=manager.listSavefiles(Saves::targetPrefix(_targetName)+"*");
	Common::sort(names.begin(),names.end());Common::StringArray digests;
	for(const auto &name:names)digests.push_back(digest(name));
	const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES);
	if(!canSaveGameStateCurrently()||!_runtime->saveState(before)||canSaveAutosaveCurrently()||getAutosaveSlot()!=-1||getMetaEngine()->getAutosaveSlot()!=-1)
		return fail("Manual/autosave capability policy disagrees");
	// The harness configured the actual constructor's autosave period to one
	// second. Let that interval elapse after run() (which follows construction),
	// rather than reaching into Engine's private scheduler bookkeeping.
	if(ConfMan.getInt("autosave_period")!=1)return fail("Caller fixture requires a one-second autosave period");
	while(_system->getMillis()-runStarted<=1100)_system->delayMillis(10);
	handleAutoSave();saveAutosaveIfEnabled();
	for(int slot=0;slot<=kMaximumSaveSlot;++slot)
		if(saveGameState(slot,"Rejected caller autosave",true).getCode()==Common::kNoError)return fail("Autosave accepted a manual slot");
	const int invalid[]={-1,1000,(-2147483647-1),2147483647};
	for(uint i=0;i<ARRAYSIZE(invalid);++i)
		if(saveGameState(invalid[i],"Rejected caller slot").getCode()==Common::kNoError)return fail("Out-of-range manual caller accepted");
	if(_saveMode==kClassicSaveMode){
		const int denied[]={0,3,999};
		for(uint i=0;i<ARRAYSIZE(denied);++i)
			if(saveGameState(denied[i],"Rejected quicksave slot").getCode()==Common::kNoError)return fail("Classic quicksave bypassed two-slot policy");
	}else{
		// An ordinary quicksave-style caller may use Modern slot0, through the
		// SAME session-qualified service; remove only this isolated new fixture.
		const Common::String prefix=Saves::slotPrefix(_targetName,session,0);
		if(manager.exists(SnapshotStore(manager,prefix).filename(0))||manager.exists(SnapshotStore(manager,prefix).filename(1)))return fail("Modern caller fixture slot already exists");
		if(saveGameState(0,"Caller slot0 fixture").getCode()!=Common::kNoError||saveGameState(0,"Caller slot0 fixture").getCode()!=Common::kNoError)
			return fail("Modern slot0 caller failed");
		Saves::Entry saved;Common::Array<byte> payload;
		if(!Saves::best(manager,prefix,_targetName,saved,payload)||saved.metadata.session!=session||saved.metadata.slot!=0)return fail("Modern slot0 caller escaped its session");
		getMetaEngine()->listSaves(_targetName.c_str(),true);
		if(!getMetaEngine()->removeSaveState(_targetName.c_str(),0))return fail("Modern caller fixture cleanup failed");
	}
	getMetaEngine()->listSaves(_targetName.c_str(),false);
	Common::StringArray remaining=manager.listSavefiles(Saves::targetPrefix(_targetName)+"*");
	Common::sort(remaining.begin(),remaining.end());
	if(remaining!=names||!_runtime->saveState(after)||before.size()!=after.size()||memcmp(before.getData(),after.getData(),before.size())||
	   Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=session)return fail("Caller policy changed live world/session/save set");
	Common::String fingerprints;
	for(uint i=0;i<names.size();++i){
		if(digests[i].empty()||digest(names[i])!=digests[i])return fail("Caller policy overwrote an existing generation/sidecar");
		fingerprints+=names[i]+"\t"+digests[i]+"\n";
	}
	Common::OutSaveFile *files=manager.openForSaving("nox-callers-files.tsv",false);
	if(!files)return fail("Cannot record pre-exit caller fingerprints");
	files->write(fingerprints.c_str(),fingerprints.size());files->finalize();const bool filesOk=!files->err();delete files;
	if(!filesOk)return fail("Cannot persist pre-exit caller fingerprints");
	Common::OutSaveFile *out=manager.openForSaving("nox-callers.tsv",false);
	if(!out)return fail("Cannot create caller report");
	const Common::String report=Common::String::format("PASS\tautosave_capability=disabled\telapsed_due_host_timer=1\tdirect_autosave=1\tmanual_slot_autosaves_rejected=1000\tquicksave_slot0=%s\tworld_unchanged=1\tsession_unchanged=1\tfiles_unchanged=1\n",_saveMode==kClassicSaveMode?"rejected":"session-qualified");
	out->write(report.c_str(),report.size());out->finalize();const bool ok=!out->err();delete out;
	return ok||fail("Cannot persist caller report");
}
} // namespace Nox
#endif
