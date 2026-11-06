/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_GRID_VALIDATION_H
#define NOX_GRID_VALIDATION_H
#include "nox/nox.h"
#include "nox/host_validation.h"
#include "nox/saves.h"
#include "gui/saveload-dialog.h"

namespace Nox {
class GridValidationGui {
	bool _hadScale,_hadChooser;
	int _scale;
	Common::String _chooser;
public:
	GridValidationGui() {
		_hadScale=ConfMan.hasKey("gui_scale",Common::ConfigManager::kApplicationDomain);
		_hadChooser=ConfMan.hasKey("gui_saveload_chooser",Common::ConfigManager::kApplicationDomain);
		_scale=_hadScale?ConfMan.getInt("gui_scale",Common::ConfigManager::kApplicationDomain):100;
		_chooser=ConfMan.get("gui_saveload_chooser",Common::ConfigManager::kApplicationDomain);
		ConfMan.setInt("gui_scale",50,Common::ConfigManager::kApplicationDomain);
		ConfMan.set("gui_saveload_chooser","grid",Common::ConfigManager::kApplicationDomain);g_gui.computeScaleFactor();
	}
	~GridValidationGui() {
		if(_hadScale)ConfMan.setInt("gui_scale",_scale,Common::ConfigManager::kApplicationDomain);
		else ConfMan.removeKey("gui_scale",Common::ConfigManager::kApplicationDomain);
		if(_hadChooser)ConfMan.set("gui_saveload_chooser",_chooser,Common::ConfigManager::kApplicationDomain);
		else ConfMan.removeKey("gui_saveload_chooser",Common::ConfigManager::kApplicationDomain);
		g_gui.computeScaleFactor();
	}
};
static bool gridReport(Common::SaveFileManager &manager,const char *file,const Common::String &text) {
	Common::OutSaveFile *out=manager.openForSaving(file,false);if(!out)return false;
	out->write(text.c_str(),text.size());out->finalize();const bool ok=!out->err();delete out;return ok;
}
bool NoxEngine::validateGridWrites(MenuValidationSource &input,Common::String &error) {
#ifndef DISABLE_SAVELOADCHOOSER_GRID
	auto fail=[&](const char *text){error=text;return false;};
	Common::SaveFileManager &manager=*getSaveFileManager();
	const bool classic=_saveMode==kClassicSaveMode;
	const int slot=classic?2:0;
	const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName);
	const Common::String prefix=Saves::slotPrefix(_targetName,session,slot);
	if(!canSaveGameStateCurrently()||Saves::entries(manager,_targetName).size()!=2||
	   manager.exists(SnapshotStore(manager,prefix).filename(0))||manager.exists(SnapshotStore(manager,prefix).filename(1)))return fail("Grid caller requires an empty isolated extra slot");
	Common::String baseline;
	for(const auto &file:manager.listSavefiles(Saves::targetPrefix(_targetName)+"*")){
		Common::InSaveFile *in=manager.openForLoading(file);if(!in)return fail("Cannot hash grid baseline");
		baseline+=file+"\t"+Common::computeStreamMD5AsString(*in)+"\n";delete in;
	}
	if(!gridReport(manager,"nox-grid-base-files.tsv",baseline))return fail("Cannot record grid baseline");
	Common::MemoryWriteStreamDynamic before(DisposeAfterUse::YES),after(DisposeAfterUse::YES);
	if(!_runtime->saveState(before))return fail("Cannot capture grid caller world");
	GridValidationGui gui;
	if(GUI::getRequestedSaveLoadDialog(getMetaEngine(),_targetName.c_str())!=(classic?GUI::kSaveLoadDialogList:GUI::kSaveLoadDialogGrid))return fail("Grid caller selected wrong mode UI");
	Saves::Entry saved;Common::Array<byte> payload;
	for(uint action=0;action<2;++action){
		if(classic)input.chooser(true,1);
		else input.grid(true,action?1:0,action?"GRID-OVERWRITE":"GRID-NEW",action?"nox-grid-overwrite":"nox-grid-new");
		if(!saveGameDialog())return fail("Actual grid/list save action failed");
		if(!Saves::best(manager,prefix,_targetName,saved,payload)||saved.sequence!=action+1||saved.metadata.session!=session||
		   saved.metadata.slot!=uint(slot)||saved.metadata.mode!=_saveMode||saved.metadata.preview.empty())return fail("Grid caller committed wrong slot/session/mode/generation");
		if(!classic&&saved.metadata.description!=(action?"GRID-OVERWRITE":"GRID-NEW"))return fail("Grid description typing was not committed");
		if(uint(payload.size()-saved.offset)!=before.size()||memcmp(payload.data()+saved.offset,before.getData(),before.size()))return fail("Grid caller saved different runtime bytes");
	}
	if(!_runtime->saveState(after)||after.size()!=before.size()||memcmp(after.getData(),before.getData(),before.size())||
	   Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=session)return fail("Grid caller changed live world/session");
	if(classic&&saveGameState(3,"Forbidden third slot").getCode()==Common::kNoError)return fail("Classic grid preference allowed third slot");
	_runtime->render(_surface,0);
	Common::OutSaveFile *image=manager.openForSaving("nox-grid-saved.png",false);if(!image)return fail("Cannot capture grid world");
	bool ok=Image::writePNG(*image,_surface);image->finalize();ok=ok&&!image->err();delete image;
	const Common::String report=Common::String::format("PASS\tactual=%s\tnew_save=1\toverwrite=1\tlogical_slot=%d\tsequence=2\tworld_bytes=identical\tsession_unchanged=1\n",classic?"classic-list":"grid",slot);
	return (ok&&gridReport(manager,"nox-grid.tsv",report))||fail("Cannot persist grid caller evidence");
#else
	error="Grid diagnostic requires grid-enabled build";return false;
#endif
}
bool NoxEngine::validateGridCold(MenuValidationSource &input,Common::String &error) {
#ifndef DISABLE_SAVELOADCHOOSER_GRID
	auto fail=[&](const char *text){error=text;return false;};
	Common::SaveFileManager &manager=*getSaveFileManager();
	const bool classic=_saveMode==kClassicSaveMode;const uint slot=classic?2:0;
	Saves::Entry expected;Common::Array<byte> payload;bool found=false;
	for(const auto &entry:Saves::entries(manager,_targetName))if(entry.metadata.slot==slot){
		if(found)return fail("Grid cold fixture is ambiguous");
		if(!Saves::readGeneration(manager,entry.prefix,entry.generation,_targetName,expected,payload))return fail("Grid cold fixture unreadable");
		found=true;
	}
	if(!found||expected.sequence!=2||expected.metadata.mode!=_saveMode||
	   Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=expected.metadata.session)return fail("Grid cold fixture session/mode unavailable");
	GridValidationGui gui;
	if(classic){input.chooser(false);input.loadRow(1);}
	else input.grid(false,0,nullptr,"nox-grid-cold");
	// The chooser's default is newest-started, with stable first-record ties;
	// it need not be the currently active save owner. Select the intended run
	// through its real popup, rather than changing that policy or loading by API.
	int visibleIndex=0,ownerIndex=-1;
	for(const auto &record:Common::CEAchievements().listPlaythroughsForTarget(_targetName)){
		if(record.archived)continue;
		if(record.clientPlaythroughId==expected.metadata.session)ownerIndex=visibleIndex;
		++visibleIndex;
	}
	if(ownerIndex<0)return fail("Grid fixture owner absent from session popup");
	input.selectSession(ownerIndex);
	if(!loadGameDialog())return fail("Actual cold grid/list load failed");
	Common::MemoryWriteStreamDynamic restored(DisposeAfterUse::YES);
	if(!_runtime->saveState(restored)||restored.size()!=uint(payload.size()-expected.offset)||
	   memcmp(restored.getData(),payload.data()+expected.offset,restored.size())||_saveMode!=expected.metadata.mode||
	   Common::CEAchievements().getCurrentPlaythroughForTarget(_targetName)!=expected.metadata.session)return fail("Cold grid caller restored different world/session/mode");
	_runtime->render(_surface,0);Common::OutSaveFile *image=manager.openForSaving("nox-grid-cold.png",false);
	if(!image)return fail("Cannot capture grid cold frame");
	bool ok=Image::writePNG(*image,_surface);image->finalize();ok=ok&&!image->err();delete image;
	int handle=-1;
	for(const auto &entry:getMetaEngine()->listSaves(_targetName.c_str(),false))
		if(getMetaEngine()->getSavegameFile(entry.getSaveSlot(),_targetName.c_str())==expected.filename)handle=entry.getSaveSlot();
	if(handle<0||!getMetaEngine()->removeSaveState(_targetName.c_str(),handle)||Saves::entries(manager,_targetName).size()!=2)return fail("Grid cold fixture cleanup failed");
	return (ok&&gridReport(manager,"nox-grid-cold.tsv","PASS\tactual_load_dialog=1\truntime_bytes=identical\tsaved_mode=1\tsession=1\tfixture_deleted=1\n"))||fail("Cannot persist grid cold evidence");
#else
	error="Grid diagnostic requires grid-enabled build";return false;
#endif
}
} // namespace Nox
#endif
