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
 bool hasFeature(MetaEngineFeature f)const override{return f==kSupportsLoadingDuringStartup||f==kSupportsListSaves||f==kSupportsDeleteSave||f==kSimpleSavesNames;}
 int getAutosaveSlot() const override { return -1; }
 int getMaximumSaveSlot()const override{return 99;}
 SaveStateList listSaves(const char* target)const override{
  SaveStateList result;auto* manager=g_system->getSavefileManager();Common::StringArray files=manager->listSavefiles(Common::String::format("%s.###",target));
  for(const Common::String& file:files){unsigned n=file.size();if(n<4)continue;int slot=(file[n-3]-'0')*100+(file[n-2]-'0')*10+file[n-1]-'0';if(slot<0||slot>99)continue;Common::InSaveFile* in=manager->openForLoading(file);std::string desc;bool valid=in&&Snatcher::nativeSaveDescription(*in,desc);delete in;if(valid)result.push_back(SaveStateDescriptor(this,slot,desc.c_str()));}
  Common::sort(result.begin(),result.end(),SaveStateDescriptorSlotComparator());return result;
 }
 bool removeSaveState(const char* target,int slot)const override{if(slot<0||slot>99)return false;auto* manager=g_system->getSavefileManager();Common::String file=Common::String::format("%s.%03d",target,slot);manager->removeSavefile(file+".bak");manager->removeSavefile(file+".new");return manager->removeSavefile(file);}
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
  const Binding bindings[]={{"PAUSE","Pause or resume","p","JOY_BACK",Common::KEYCODE_p},{"AIM","Raise or lower weapon","z","JOY_X",Common::KEYCODE_z},{"CONFIRM","Confirm","RETURN","JOY_A",Common::KEYCODE_RETURN},{"UP","Previous choice","UP","JOY_UP",Common::KEYCODE_UP},{"DOWN","Next choice","DOWN","JOY_DOWN",Common::KEYCODE_DOWN},{"LEFT","Previous choice","LEFT","JOY_LEFT",Common::KEYCODE_LEFT},{"RIGHT","Next choice","RIGHT","JOY_RIGHT",Common::KEYCODE_RIGHT},{"START","Start","SPACE","JOY_START",Common::KEYCODE_SPACE},{"CANCEL","Cancel choice","BACKSPACE","JOY_B",Common::KEYCODE_BACKSPACE}};
  for(const Binding& b:bindings){Common::Action *action=new Common::Action(b.id,Common::U32String(b.label));action->setKeyEvent(b.code);action->addDefaultInputMapping(b.key);action->addDefaultInputMapping(b.pad);map->addAction(action);}maps.push_back(map);return maps;
 }
};
#if PLUGIN_ENABLED_DYNAMIC(SNATCHER)
REGISTER_PLUGIN_DYNAMIC(SNATCHER,PLUGIN_TYPE_ENGINE,SnatcherMetaEngine);
#else
REGISTER_PLUGIN_STATIC(SNATCHER,PLUGIN_TYPE_ENGINE,SnatcherMetaEngine);
#endif
