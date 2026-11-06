/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "base/plugins.h"
#include "engines/advancedDetector.h"
#include "nox/nox.h"
#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "nox/saves.h"
#include "common/config-manager.h"
#include "common/system.h"
#include "common/translation.h"

class NoxMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override { return "nox"; }
	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		static const ADExtraGuiOptionsMap options[] = {
			{ GUIO_GAMEOPTIONS1, { _s("Classic save slots for new Play Sessions"),
				_s("Use slots 1 and 2 in new Play Sessions. Loaded saves keep their saved mode."),
				"nox_classic_saves",false,0,0 } },
			AD_EXTRA_GUI_OPTIONS_TERMINATOR
		};
		return options;
	}
	bool isSaveSlotAllowed(const char *target,int slot) const override {
		Nox::SaveMode mode=Nox::kModernSaveMode;
		if(g_engine&&g_engine->getMetaEngine()==this&&ConfMan.getActiveDomainName()==target)
			mode=static_cast<const Nox::NoxEngine *>(g_engine)->saveMode();
		else if(ConfMan.hasKey("nox_classic_saves",target)&&ConfMan.getBool("nox_classic_saves",target))mode=Nox::kClassicSaveMode;
		return Nox::allowedSaveSlot(mode,slot);
	}
	bool hasFeature(MetaEngineFeature feature) const override {
		return feature==kSupportsListSaves||feature==kSupportsLoadingDuringStartup||feature==kSupportsDeleteSave||
			feature==kSavesSupportMetaInfo||feature==kSavesSupportThumbnail||feature==kSavesSupportCreationDate||feature==kSavesSupportPlayTime;
	}
	bool hasFeatureForTarget(MetaEngineFeature feature,const char *target) const override {
		// Classic retains its exact two labeled list choices, including empty
		// slots. Metadata remains queryable; its chooser uses date/playtime.
		if(feature==kSavesSupportThumbnail&&!isSaveSlotAllowed(target,0))return false;
		return feature==kSavesUseCompactLoadList||hasFeature(feature);
	}
	int getMaximumSaveSlot() const override { return Nox::Saves::kMaximumSlot; }
	int getAutosaveSlot() const override { return -1; }
	SaveStateList listSaves(const char *target) const override { return listSaves(target,false); }
	SaveStateList listSaves(const char *target,bool saveMode) const override {
		_saveMode=saveMode;_target=target;
		_entries=Nox::Saves::entries(*g_system->getSavefileManager(),target);
		SaveStateList result;
		const Common::String session=Common::CEAchievements().getCurrentPlaythroughForTarget(target);
		for(uint i=0;i<_entries.size();++i){const Nox::Saves::Metadata &meta=_entries[i].metadata;
			if(saveMode&&meta.session!=session)continue;
			result.push_back(SaveStateDescriptor(this,saveMode?meta.slot:i,meta.description));
		}
		return result;
	}
	Common::String getSavegameFile(int slot,const char *target=nullptr) const override {
		const Common::String selected=target?target:ConfMan.getActiveDomainName();
		if(slot==kSavegameFilePattern)return Nox::Saves::targetPrefix(selected)+"*-slot###.g*";
		if(_target!=selected)listSaves(selected.c_str(),false);
		if(_saveMode){
			if(!isSaveSlotAllowed(selected.c_str(),slot))return Common::String();
			const Common::String prefix=Nox::Saves::slotPrefix(selected,Common::CEAchievements().getCurrentPlaythroughForTarget(selected),slot);
			Nox::Saves::Entry entry;Common::Array<byte> payload;
			if(Nox::Saves::best(*g_system->getSavefileManager(),prefix,selected,entry,payload))return entry.filename;
			return Nox::SnapshotStore(*g_system->getSavefileManager(),prefix).filename(0);
		}
		return slot>=0&&uint(slot)<_entries.size()?_entries[slot].filename:Common::String();
	}
	SaveStateDescriptor querySaveMetaInfos(const char *target,int slot) const override {
		const Common::String file=getSavegameFile(slot,target);
		if(file.empty())return SaveStateDescriptor();
		Nox::Saves::Entry entry;Common::Array<byte> payload;
		if(!Nox::Saves::readGeneration(*g_system->getSavefileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',target,entry,payload))return SaveStateDescriptor();
		SaveStateDescriptor result(this,slot,entry.metadata.description);
		result.setPlayTime(entry.metadata.playtime);
		if(entry.metadata.year){
			result.setSaveDate(entry.metadata.year,entry.metadata.month,entry.metadata.day);
			result.setSaveTime(entry.metadata.hour,entry.metadata.minute);
		}
		if(!entry.metadata.preview.empty()){
			Graphics::Surface *preview=new Graphics::Surface();
			preview->create(Nox::Saves::kPreviewWidth,Nox::Saves::kPreviewHeight,Graphics::PixelFormat(2,5,6,5,0,11,5,0,0));
			for(uint y=0;y<Nox::Saves::kPreviewHeight;++y)for(uint x=0;x<Nox::Saves::kPreviewWidth;++x)
				*(uint16 *)preview->getBasePtr(x,y)=READ_LE_UINT16(entry.metadata.preview.data()+(y*Nox::Saves::kPreviewWidth+x)*2);
			result.setThumbnail(preview);
		}
		return result;
	}
	bool detachSavePlaythrough(const char *target,int slot,const Common::String &session) const override {
		// Both generations keep their original commit sidecars. Removing the
		// registry owner makes them untracked, not incomplete or newly trusted.
		const Common::String file=getSavegameFile(slot,target);
		if(!file.hasSuffix(".g0")&&!file.hasSuffix(".g1"))return false;
		Nox::Saves::Entry entry;Common::Array<byte> payload;
		return Nox::Saves::readGeneration(*g_system->getSavefileManager(),file.substr(0,file.size()-3),file.lastChar()-'0',target,entry,payload)&&entry.metadata.session==session;
	}
	bool removeSaveState(const char *target,int slot) const override {
		// Resolve once. Keep cached handle tombstones until the caller next
		// enumerates, so its follow-up sidecar cleanup cannot alias another save.
		const Common::String file=getSavegameFile(slot,target);
		if(!file.hasSuffix(".g0")&&!file.hasSuffix(".g1"))return false;
		Nox::Saves::Entry entry;Common::Array<byte> payload;
		Common::SaveFileManager &manager=*g_system->getSavefileManager();
		if(!Nox::Saves::readGeneration(manager,file.substr(0,file.size()-3),file.lastChar()-'0',target,entry,payload))return false;
		return Nox::Saves::removeLogical(manager,entry);
	}
	Common::KeymapArray initKeymaps(const char *) const override {
		Common::Keymap *map = new Common::Keymap(Common::Keymap::kKeymapTypeGame, "nox", "Nox Archaist");
		const struct { const char *id; const char *label; Common::KeyCode key; const char *mapping; } actions[] = {
			{"NORTH","Move north",Common::KEYCODE_UP,"UP"},
			{"SOUTH","Move south",Common::KEYCODE_DOWN,"DOWN"},
			{"WEST","Move west",Common::KEYCODE_LEFT,"LEFT"},
			{"EAST","Move east",Common::KEYCODE_RIGHT,"RIGHT"},
			{"ACCEPT","Accept",Common::KEYCODE_RETURN,"RETURN"},
			{"CANCEL","Cancel",Common::KEYCODE_ESCAPE,"ESCAPE"}
		};
		for (uint i=0;i<ARRAYSIZE(actions);++i) {
			Common::Action *action = new Common::Action(actions[i].id, Common::U32String(actions[i].label));
			action->setKeyEvent(Common::KeyState(actions[i].key, actions[i].key == Common::KEYCODE_RETURN ? 13 : actions[i].key == Common::KEYCODE_ESCAPE ? 27 : 0));
			action->addDefaultInputMapping(actions[i].mapping);
			map->addAction(action);
		}
		return Common::Keymap::arrayOf(map);
	}
	Common::Error createInstance(OSystem *system, Engine **engine, const ADGameDescription *) const override {
		*engine = new Nox::NoxEngine(system);
		return Common::kNoError;
	}
private:
	mutable Common::String _target;
	mutable Common::Array<Nox::Saves::Entry> _entries;
	mutable bool _saveMode=false;
};
#if PLUGIN_ENABLED_DYNAMIC(NOX)
REGISTER_PLUGIN_DYNAMIC(NOX, PLUGIN_TYPE_ENGINE, NoxMetaEngine);
#else
REGISTER_PLUGIN_STATIC(NOX, PLUGIN_TYPE_ENGINE, NoxMetaEngine);
#endif
