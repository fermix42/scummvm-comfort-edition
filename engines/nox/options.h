/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_OPTIONS_H
#define NOX_OPTIONS_H
#include "nox/native_combat_preferences.h"
#include "common/translation.h"
#include "gui/ThemeEval.h"
#include "gui/widget.h"
#include "gui/widgets/popup.h"
namespace Nox {
class OptionsWidget : public GUI::OptionsContainerWidget {
public:
    OptionsWidget(GUI::GuiObject *boss,const Common::String &name,const Common::String &domain)
        : OptionsContainerWidget(boss,name,"NoxGameOptionsDialog",domain) {
        new GUI::StaticTextWidget(widgetsBoss(),_dialogLayout+".CombatLabel",_("Combat mode:"));
        _combat=new GUI::PopUpWidget(widgetsBoss(),_dialogLayout+".CombatMode",_("Choose tactical combat, Quick Combat, or decide at each encounter."));
        _combat->appendEntry(_("Tactical"),Native::kPreferTactical);
        _combat->appendEntry(_("Quick Combat"),Native::kPreferQuick);
        _combat->appendEntry(_("Ask each encounter"),Native::kPreferAsk);
        _classic=new GUI::CheckboxWidget(widgetsBoss(),_dialogLayout+".ClassicSaves",_("Classic save slots for new Play Sessions"),_("Use slots 1 and 2 in new Play Sessions. Loaded saves keep their saved mode."));
    }
    void load() override {
        _combat->setSelectedTag(Native::CombatPreferences::read(ConfMan,_domain));
        _classic->setState(ConfMan.hasKey("nox_classic_saves",_domain) && ConfMan.getBool("nox_classic_saves",_domain));
    }
    bool save() override {
        Native::CombatPreferences::write(ConfMan,_domain,_combat->getSelectedTag());
        ConfMan.setBool("nox_classic_saves",_classic->getState(),_domain);return true;
    }
    void setEnabled(bool enabled) override {_combat->setEnabled(enabled);_classic->setEnabled(enabled);}
private:
    void defineLayout(GUI::ThemeEval &layouts,const Common::String &name,const Common::String &overlay) const override {
        layouts.addDialog(name,overlay).addLayout(GUI::ThemeLayout::kLayoutVertical).addPadding(16,0,0,0);
        layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal).addWidget("CombatLabel","OptionsLabel").addWidget("CombatMode","PopUp").closeLayout();
        layouts.addWidget("ClassicSaves","Checkbox").closeLayout().closeDialog();
    }
    GUI::PopUpWidget *_combat;GUI::CheckboxWidget *_classic;
};
}
#endif
