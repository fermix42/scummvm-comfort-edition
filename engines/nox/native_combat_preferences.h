/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NOX_NATIVE_COMBAT_PREFERENCES_H
#define NOX_NATIVE_COMBAT_PREFERENCES_H
#include "common/config-manager.h"
namespace Nox { namespace Native {
enum CombatPreference { kPreferTactical=0,kPreferQuick=1,kPreferAsk=2 };
class CombatPreferences {
public:
    static CombatPreference read(const Common::ConfigManager &config,const Common::String &domain,bool replay=false) {
        if (replay && (config.hasKey("nox_native_combat_ask",domain) || config.hasKey("nox_native_combat_quick",domain))) {
            if (config.hasKey("nox_native_combat_ask",domain) && config.getBool("nox_native_combat_ask",domain)) return kPreferAsk;
            return config.hasKey("nox_native_combat_quick",domain) && config.getBool("nox_native_combat_quick",domain)?kPreferQuick:kPreferTactical;
        }
        if (config.hasKey("nox_combat_mode",domain)) {
            const Common::String mode=config.get("nox_combat_mode",domain);
            return mode=="1"?kPreferQuick:mode=="2"?kPreferAsk:kPreferTactical;
        }
        // Preserve existing private installations when no normal setting exists.
        if (config.hasKey("nox_native_combat_ask",domain) && config.getBool("nox_native_combat_ask",domain)) return kPreferAsk;
        return config.hasKey("nox_native_combat_quick",domain) && config.getBool("nox_native_combat_quick",domain)?kPreferQuick:kPreferTactical;
    }
    // No prior native scroll setting exists. Start at the measured original
    // middle rate(64); retain all four original rates and +/- during play.
    static byte scrollSpeed(const Common::ConfigManager &config,const Common::String &domain) {
        return config.hasKey("nox_combat_scroll_speed",domain)?byte(CLIP<int>(config.getInt("nox_combat_scroll_speed",domain),0,3)):2;
    }
    static void write(Common::ConfigManager &config,const Common::String &domain,uint mode) {config.setInt("nox_combat_mode",mode<=2?mode:0,domain);}
};
} }
#endif
