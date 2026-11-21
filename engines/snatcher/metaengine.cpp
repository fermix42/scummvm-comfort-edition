/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "base/plugins.h"
#include "engines/metaengine.h"
#include "snatcher/engine.h"
#include "snatcher/engine_save.h"
#include "common/system.h"
#include "common/savefile.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/action.h"
class SnatcherMetaEngine : public MetaEngine {
public:
 const char *getName() const override {return "snatcher";}
 const ExtraGuiOptions getExtraGuiOptions(const Common::String&)const override{ExtraGuiOptions options;options.push_back(ExtraGuiOption("Original controls only","Disable PC mouse interaction and use keyboard or gamepad controls","snatcher_original_controls_only",false,0,0));options.push_back(ExtraGuiOption("Decensor Alice, Lisa and Katrina", "Restore only Alice animation, Lisa artwork and Katrina shower graphics (requires local decensor data)", "snatcher_decensor", false, 0, 0));return options;}
 bool hasFeature(MetaEngineFeature f)const override{return f==kSupportsLoadingDuringStartup||f==kSupportsListSaves||f==kSupportsDeleteSave||f==kSimpleSavesNames||f==kSavesSupportMetaInfo;}
 int getAutosaveSlot() const override { return 0; }
 int getMaximumSaveSlot()const override{return 99;}
 bool isSaveSlotAllowed(const char*,int slot)const override{return slot>=5&&slot<=99;}
 SaveStateDescriptor querySaveMetaInfos(const char* target,int slot)const override{
  if(slot<0||slot>99)return SaveStateDescriptor();auto* manager=g_system->getSavefileManager();const Common::String file=Common::String::format("%s.%03d",target,slot);
  Common::InSaveFile* in=manager->openForLoading(file);if(!in&&slot>=1&&slot<=4)in=manager->openForLoading(Common::String::format("%s-snatcher-original-%d.sgr",target,slot));
  std::string description;bool valid=in&&Snatcher::nativeSaveDescription(*in,description);delete in;
  if(!valid){in=manager->openForLoading(file+".bak");valid=in&&Snatcher::nativeSaveDescription(*in,description);delete in;}
  if(!valid&&slot>=5)return SaveStateDescriptor();
  Common::String caption=description.c_str();if(slot==0&&(!valid||Snatcher::nativeAutosave(*manager,file)))caption="Autosave";
  if(slot>=1&&slot<=4)caption=Common::String::format("Internal Save Slot %d",slot);
  SaveStateDescriptor result(this,slot,caption);result.setWriteProtectedFlag(slot<=4);result.setDeletableFlag(valid);result.setAutosave(slot==0&&(!valid||Snatcher::nativeAutosave(*manager,file)));return result;
 }
 SaveStateList listSaves(const char* target)const override{
  SaveStateList result;
  for(int slot=0;slot<=99;++slot){SaveStateDescriptor desc=querySaveMetaInfos(target,slot);if(desc.isValid()&&desc.getDeletableFlag())result.push_back(desc);}
  return result;
 }
 bool removeSaveState(const char* target,int slot)const override{if(slot<0||slot>99)return false;auto* manager=g_system->getSavefileManager();Common::String file=Common::String::format("%s.%03d",target,slot);manager->removeSavefile(file+".bak");manager->removeSavefile(file+".new");manager->removeSavefile(file+".autosave");bool removed=manager->removeSavefile(file);if(slot>=1&&slot<=4){Common::String legacy=Common::String::format("%s-snatcher-original-%d.sgr",target,slot);manager->removeSavefile(legacy+".bak");removed=manager->removeSavefile(legacy)||removed;}return removed;}
 Common::Error createInstance(OSystem *s,Engine **e,const DetectedGame &game,const void *) override {
  if(game.gameSupportLevel==kUnstableGame && !Engine::warnUserAboutUnsupportedGame())
   return Common::kUserCanceled;
  *e=new Snatcher::SnatcherEngine(s);return Common::kNoError;
 }
 Common::Array<Common::Keymap *> initKeymaps(const char *) const override {
  Common::Array<Common::Keymap *> maps;
  Common::Keymap *map=new Common::Keymap(Common::Keymap::kKeymapTypeGame,"snatcher","Snatcher");
  Common::Action *exit=new Common::Action("RETURN",Common::U32String("Return to launcher"));exit->setKeyEvent(Common::KEYCODE_ESCAPE);exit->addDefaultInputMapping("ESCAPE");map->addAction(exit);
  struct Binding {const char *id,*label,*key,*pad;Common::KeyCode code;};
  const Binding bindings[]={{"SAVE","Save game","F5",nullptr,Common::KEYCODE_F5},{"LOAD","Load game","F7",nullptr,Common::KEYCODE_F7},{"PAUSE","Pause or resume","p","JOY_BACK",Common::KEYCODE_p},{"AIM","Raise or lower weapon","z","JOY_X",Common::KEYCODE_z},{"CONFIRM","Confirm","RETURN","JOY_A",Common::KEYCODE_RETURN},{"UP","Previous choice","UP","JOY_UP",Common::KEYCODE_UP},{"DOWN","Next choice","DOWN","JOY_DOWN",Common::KEYCODE_DOWN},{"LEFT","Previous choice","LEFT","JOY_LEFT",Common::KEYCODE_LEFT},{"RIGHT","Next choice","RIGHT","JOY_RIGHT",Common::KEYCODE_RIGHT},{"START","Start","SPACE","JOY_START",Common::KEYCODE_SPACE},{"CANCEL","Cancel choice","BACKSPACE","JOY_B",Common::KEYCODE_BACKSPACE}};
  for(const Binding& b:bindings){Common::Action *action=new Common::Action(b.id,Common::U32String(b.label));action->setKeyEvent(b.code);action->addDefaultInputMapping(b.key);if(b.pad)action->addDefaultInputMapping(b.pad);map->addAction(action);}maps.push_back(map);return maps;
 }
};
#if PLUGIN_ENABLED_DYNAMIC(SNATCHER)
REGISTER_PLUGIN_DYNAMIC(SNATCHER,PLUGIN_TYPE_ENGINE,SnatcherMetaEngine);
#else
REGISTER_PLUGIN_STATIC(SNATCHER,PLUGIN_TYPE_ENGINE,SnatcherMetaEngine);
#endif
