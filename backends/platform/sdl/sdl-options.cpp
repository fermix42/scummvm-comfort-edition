/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "backends/platform/sdl/sdl.h"

#if SDL_VERSION_ATLEAST(2, 0, 5)
#include "backends/platform/sdl/window-layout.h"
#include "common/config-manager.h"
#include "common/translation.h"
#include "gui/ThemeEval.h"
#include "gui/widget.h"
#include "gui/message.h"
#include "gui/widgets/edittext.h"
#include "gui/widgets/popup.h"

class SdlWindowOptionsWidget : public GUI::OptionsContainerWidget {
public:
	SdlWindowOptionsWidget(GUI::GuiObject *boss, const Common::String &name, const Common::String &domain);
	void load() override;
	bool save() override;
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override;

private:
	void defineLayout(GUI::ThemeEval &layouts, const Common::String &name, const Common::String &overlay) const override;
	void updateEnabled();
	GUI::PopUpWidget *_mode, *_display;
	GUI::CheckboxWidget *_borderless;
	GUI::EditTextWidget *_values[4];
};

static const char *const layoutKeys[] = {
	"window_layout_width", "window_layout_height", "window_layout_x", "window_layout_y"
};
static const char *const layoutNames[] = {"Width", "Height", "X", "Y"};

SdlWindowOptionsWidget::SdlWindowOptionsWidget(GUI::GuiObject *boss, const Common::String &name, const Common::String &domain) :
	OptionsContainerWidget(boss, name, "SdlWindowOptions", domain) {
	new GUI::StaticTextWidget(widgetsBoss(), "SdlWindowOptions.ModeLabel", _("Windowed layout:"));
	_mode = new GUI::PopUpWidget(widgetsBoss(), "SdlWindowOptions.Mode", _("Applies when fullscreen is off."), 'layo');
	_mode->appendEntry(_("Normal"), WindowLayout::kNormal);
	_mode->appendEntry(_("Left two-thirds"), WindowLayout::kLeftTwoThirds);
	_mode->appendEntry(_("Right two-thirds"), WindowLayout::kRightTwoThirds);
	_mode->appendEntry(_("Custom"), WindowLayout::kCustom);
	new GUI::StaticTextWidget(widgetsBoss(), "SdlWindowOptions.DisplayLabel", _("Monitor:"));
	_display = new GUI::PopUpWidget(widgetsBoss(), "SdlWindowOptions.Display");
#if SDL_VERSION_ATLEAST(3, 0, 0)
	int count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&count);
#else
	const int count = SDL_GetNumVideoDisplays();
#endif
	for (int i = 0; i < count; ++i) {
#if SDL_VERSION_ATLEAST(3, 0, 0)
		const char *displayName = SDL_GetDisplayName(displays[i]);
#else
		const char *displayName = SDL_GetDisplayName(i);
#endif
		_display->appendEntry(Common::U32String::format("%d: %s", i + 1, displayName ? displayName : ""), i);
	}
#if SDL_VERSION_ATLEAST(3, 0, 0)
	SDL_free(displays);
#endif
	_borderless = new GUI::CheckboxWidget(widgetsBoss(), "SdlWindowOptions.Borderless", _("Borderless window"), _("Hide the title bar and borders. Disable to drag and resize normally."));
	const Common::U32String labels[] = {_("Width:"), _("Height:"), _("Left offset:"), _("Top offset:")};
	for (int i = 0; i < 4; ++i) {
		const Common::String widgetName = Common::String("SdlWindowOptions.") + layoutNames[i];
		new GUI::StaticTextWidget(widgetsBoss(), widgetName + "Label", labels[i]);
		_values[i] = new GUI::EditTextWidget(widgetsBoss(), widgetName, Common::U32String(),
			i < 2 ? _("Outer window size in desktop coordinates, including borders. 0 fills the usable area.") :
			_("Offset from the usable desktop's top-left corner. Kept within the selected monitor."));
	}
	new GUI::StaticTextWidget(widgetsBoss(), "SdlWindowOptions.Note", _("Places the window only; use Fit to window and aspect correction."));
}

void SdlWindowOptionsWidget::defineLayout(GUI::ThemeEval &layouts, const Common::String &name, const Common::String &overlay) const {
	layouts.addDialog(name, overlay).addLayout(GUI::ThemeLayout::kLayoutVertical).addPadding(0, 0, 0, 0)
		.addLayout(GUI::ThemeLayout::kLayoutHorizontal)
			.addWidget("ModeLabel", "OptionsLabel").addWidget("Mode", "PopUp")
		.closeLayout()
		.addLayout(GUI::ThemeLayout::kLayoutHorizontal)
			.addWidget("DisplayLabel", "OptionsLabel").addWidget("Display", "PopUp")
		.closeLayout()
		.addWidget("Borderless", "Checkbox");
	for (int i = 0; i < 4; ++i) {
		layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal)
			.addWidget(Common::String(layoutNames[i]) + "Label", "OptionsLabel")
			.addWidget(layoutNames[i], "", -1, layouts.getVar("Globals.Line.Height"))
		.closeLayout();
	}
	layouts.addWidget("Note", "", -1, layouts.getVar("Globals.Line.Height"))
		.closeLayout().closeDialog();
}

void SdlWindowOptionsWidget::updateEnabled() {
	const uint32 mode = _mode->getSelectedTag();
	_display->setEnabled(mode != WindowLayout::kNormal);
	_borderless->setEnabled(mode != WindowLayout::kNormal);
	for (int i = 0; i < 4; ++i)
		_values[i]->setEnabled(mode == WindowLayout::kCustom);
}

void SdlWindowOptionsWidget::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	if (cmd == 'layo')
		updateEnabled();
	else
		OptionsContainerWidget::handleCommand(sender, cmd, data);
}

void SdlWindowOptionsWidget::load() {
	_mode->setSelectedTag(CLIP(ConfMan.getInt("window_layout", _domain), 0, 3));
	_display->setSelectedTag(0);
	_display->setSelectedTag(ConfMan.getInt("window_layout_display", _domain));
	_borderless->setState(ConfMan.getBool("window_layout_borderless", _domain));
	for (int i = 0; i < 4; ++i)
		_values[i]->setEditString(Common::U32String::format("%d", ConfMan.getInt(layoutKeys[i], _domain)));
	updateEnabled();
}

bool SdlWindowOptionsWidget::save() {
	bool changed = false;
	const char *const keys[] = {"window_layout", "window_layout_display"};
	const int selections[] = {(int)_mode->getSelectedTag(), (int)_display->getSelectedTag()};
	for (int i = 0; i < 2; ++i) {
		changed |= ConfMan.getInt(keys[i], _domain) != selections[i];
		ConfMan.setInt(keys[i], selections[i], _domain);
	}
	changed |= ConfMan.getBool("window_layout_borderless", _domain) != _borderless->getState();
	ConfMan.setBool("window_layout_borderless", _borderless->getState(), _domain);
	bool invalidValue = false;
	for (int i = 0; i < 4; ++i) {
		// Reject malformed/overflowing input without changing the saved value.
		const Common::U32String text = _values[i]->getEditString();
		int value = 0;
		bool valid = !text.empty();
		for (uint j = 0; valid && j < text.size(); ++j) {
			valid = text[j] >= '0' && text[j] <= '9' && value <= 1638;
			if (valid)
				value = value * 10 + text[j] - '0';
		}
		valid = valid && value <= 16384;
		invalidValue |= !valid;
		if (valid) {
			changed |= ConfMan.getInt(layoutKeys[i], _domain) != value;
			ConfMan.setInt(layoutKeys[i], value, _domain);
		}
	}
	if (invalidValue) {
		GUI::MessageDialog dialog(_("Window sizes and offsets must be whole numbers from 0 to 16384. Invalid entries kept their previous values."));
		dialog.runModal();
	}
	return changed;
}

GUI::OptionsContainerWidget *OSystem_SDL::buildBackendOptionsWidget(GUI::GuiObject *boss, const Common::String &name, const Common::String &target) const {
	if (target != Common::ConfigManager::kApplicationDomain)
		return nullptr;
	return new SdlWindowOptionsWidget(boss, name, target);
}

void OSystem_SDL::applyBackendSettings() {
	if (_window)
		_window->applyWindowLayout();
}
#endif
